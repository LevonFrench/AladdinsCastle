// SPDX-License-Identifier: GPL-3.0-only
#include "Support.h"
#include <QRegularExpression>
#include <QtEndian>
#include <cmath>
#include <map>
#include <set>
#include <toml++/toml.hpp>
namespace ac::install {
namespace {
struct Text {
  QString value, newline;
  bool utf16 = false, bom = false;
  explicit Text(const QByteArray &bytes) {
    utf16 = bytes.startsWith(QByteArray::fromHex("fffe"));
    bom = utf16 || bytes.startsWith(QByteArray::fromHex("efbbbf"));
    if (utf16) {
      if (bytes.size() % 2)
        throw Error("E_CONFIG_PARSE", "Odd UTF-16 byte count");
      for (qsizetype i = 2; i < bytes.size(); i += 2)
        value += QChar(qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(bytes.constData() + i)));
    } else {
      if (bytes.startsWith(QByteArray::fromHex("feff")))
        throw Error("E_CONFIG_PARSE", "UTF-16BE not supported");
      value = QString::fromUtf8(bytes.mid(bom ? 3 : 0));
      if (value.toUtf8() != bytes.mid(bom ? 3 : 0))
        throw Error("E_CONFIG_PARSE", "Invalid UTF-8");
    }
    newline = value.contains("\r\n") ? "\r\n" : "\n";
  }
  QByteArray encode() const {
    if (!utf16)
      return (bom ? QByteArray::fromHex("efbbbf") : QByteArray()) +
             value.toUtf8();
    QByteArray out = QByteArray::fromHex("fffe");
    for (const auto c : value) {
      char b[2];
      qToLittleEndian<quint16>(c.unicode(), reinterpret_cast<uchar *>(b));
      out.append(b, 2);
    }
    return out;
  }
};
QString scalar(const Json &v, const QString &format) {
  if (v.is_string())
    return format == "toml" || format == "json"
               ? QString::fromStdString(v.dump())
               : QString::fromStdString(v.get<std::string>());
  if (v.is_primitive() && !v.is_null())
    return QString::fromStdString(v.dump());
  throw Error("E_CONFIG_PARSE", "Config value must be scalar");
}
Json resolve(const Json &e, const Json &settings, const Json &profile) {
  if (e.contains("value"))
    return e["value"];
  const auto source = string(e, "from");
  const auto v =
      dotted(Json{{"settings", settings}, {"profile", profile}}, source);
  if (e.contains("map")) {
    const auto key = v.is_string() ? v.get<std::string>() : v.dump();
    if (e["map"].contains(key))
      return e["map"][key];
    if (e.contains("default"))
      return e["default"];
    throw Error("E_CONFIG_PARSE", "Unmapped config value");
  }
  if (e.contains("bool")) {
    if (!v.is_boolean())
      throw Error("E_CONFIG_PARSE", "Boolean map requires a boolean");
    return e["bool"][v.get<bool>() ? "on" : "off"];
  }
  const auto type = string(e, "type", "string");
  if (type == "string" || type == "path") {
    if (!v.is_string())
      throw Error("E_CONFIG_PARSE", "String config value required");
    return v;
  }
  if (type == "int") {
    if (!v.is_number_integer())
      throw Error("E_CONFIG_PARSE", "Integer config value required");
    return v;
  }
  if (type == "float") {
    if (!v.is_number())
      throw Error("E_CONFIG_PARSE", "Numeric config value required");
    const auto scale = std::pow(10.0, e.value("decimals", 2));
    return std::round(v.get<double>() * scale) / scale;
  }
  if (type == "bool") {
    if (!v.is_boolean())
      throw Error("E_CONFIG_PARSE", "Boolean config value required");
    return v;
  }
  throw Error("E_CONFIG_PARSE", "Unknown config type");
}
struct LineKey {
  int index = -1, insertAt = -1;
  QString prior, prefix, suffix;
};
LineKey find(const QStringList &lines, const QString &section,
             const QString &key) {
  QString current;
  LineKey result;
  bool seen = section.isEmpty();
  static const QRegularExpression header(
      "^\\s*\\[([^\\]]+)\\]\\s*(?:[#;].*)?$");
  const QRegularExpression entry("^(\\s*" + QRegularExpression::escape(key) +
                                 "\\s*=\\s*)(.*)$");
  for (int i = 0; i < lines.size(); ++i) {
    const auto h = header.match(lines[i]);
    if (h.hasMatch()) {
      if (current == section)
        result.insertAt = i;
      current = h.captured(1);
      if (current == section)
        seen = true;
      continue;
    }
    if (current != section)
      continue;
    const auto m = entry.match(lines[i]);
    if (!m.hasMatch())
      continue;
    if (result.index >= 0)
      throw Error("E_AMBIGUOUS_KEY", "Duplicate config key: " + key);
    result.index = i;
    result.prefix = m.captured(1);
    const auto body = m.captured(2);
    bool quoted = false, escapeNext = false;
    QChar quote;
    int comment = -1;
    for (int p = 0; p < body.size(); ++p) {
      const auto c = body[p];
      if (escapeNext) {
        escapeNext = false;
        continue;
      }
      if (c == '\\' && quoted) {
        escapeNext = true;
        continue;
      }
      if (quoted) {
        if (c == quote)
          quoted = false;
      } else if (c == '"' || c == '\'') {
        quoted = true;
        quote = c;
      } else if ((c == '#' || c == ';') && (p == 0 || body[p - 1].isSpace())) {
        comment = p;
        break;
      }
    }
    const auto raw = comment >= 0 ? body.left(comment) : body;
    result.prior = raw.trimmed();
    result.suffix = raw.mid(raw.trimmed().size()) +
                    (comment >= 0 ? body.mid(comment) : QString());
  }
  if (seen && result.insertAt < 0)
    result.insertAt = lines.size();
  return result;
}
void setJson(Json &j, const QString &key, const Json &value,
             bool remove = false) {
  auto parts = key.split('.');
  Json *at = &j;
  for (int i = 0; i < parts.size() - 1; ++i) {
    const auto k = parts[i].toStdString();
    if (!at->contains(k))
      (*at)[k] = Json::object();
    if (!(*at)[k].is_object())
      throw Error("E_CONFIG_PARSE", "Dotted key crosses scalar");
    at = &(*at)[k];
  }
  if (remove)
    at->erase(parts.last().toStdString());
  else
    (*at)[parts.last().toStdString()] = value;
}
} // namespace
ConfigEdit editConfig(const QByteArray &before, const QString &format,
                      const Json &entries, const Json &settings,
                      const Json &profile) {
  if (!QSet<QString>{"ini", "cfg", "toml", "json"}.contains(format))
    throw Error("E_CONFIG_PARSE", "Unsupported M1 config format");
  Text text(before);
  ConfigEdit out;
  if (format == "json") {
    Json j;
    std::map<int, std::set<std::string>> objectKeys;
    const auto rejectDuplicate = [&](int depth, Json::parse_event_t event,
                                     Json &value) {
      if (event == Json::parse_event_t::object_start)
        objectKeys[depth + 1].clear();
      if (event == Json::parse_event_t::key) {
        const auto key = value.get<std::string>();
        if (objectKeys[depth].contains(key))
          throw Error("E_AMBIGUOUS_KEY", "Duplicate JSON key");
        objectKeys[depth].insert(key);
      }
      return true;
    };
    try {
      j = before.isEmpty()
              ? Json::object()
              : Json::parse(text.value.toStdString(), rejectDuplicate);
    } catch (const Error &) {
      throw;
    } catch (const std::exception &) {
      throw Error("E_CONFIG_PARSE", "Invalid JSON config");
    }
    for (const auto &e : entries) {
      const auto key = string(e, "key");
      Json prior;
      bool present = true;
      try {
        prior = dotted(j, key);
      } catch (const Error &) {
        present = false;
      }
      const auto after = resolve(e, settings, profile);
      setJson(j, key, after);
      out.keys.push_back(Json{{"key", key.toStdString()},
                              {"prior", present ? "value" : "absent"},
                              {"prior_value", present ? prior.dump() : ""},
                              {"after_value", after.dump()}});
    }
    text.value = QString::fromStdString(j.dump(2)) + text.newline;
    out.bytes = text.encode();
    out.reserialized = true;
    return out;
  }
  if (format == "toml" && !before.isEmpty())
    try {
      [[maybe_unused]] const auto parsed =
          toml::parse(text.value.toStdString());
    } catch (const std::exception &) {
      throw Error("E_CONFIG_PARSE", "Invalid TOML config");
    }
  auto lines = text.value.split(text.newline);
  const bool hadEnd = text.value.endsWith(text.newline);
  if (hadEnd)
    lines.removeLast();
  for (const auto &e : entries) {
    auto key = string(e, "key"), section = string(e, "section");
    if (key.isEmpty())
      throw Error("E_CONFIG_PARSE", "Empty managed key");
    if (format == "toml" && section.isEmpty() && key.contains('.')) {
      const auto p = key.lastIndexOf('.');
      section = key.left(p);
      key = key.mid(p + 1);
    }
    const auto found = find(lines, section, key);
    const auto value = resolve(e, settings, profile);
    auto after = scalar(value, format);
    if (string(e, "type") == "float")
      after = QString::number(value.get<double>(), 'f', e.value("decimals", 2));
    if (string(e, "type") == "bool" && (format == "ini" || format == "cfg")) {
      const auto style = string(e, "style", "truefalse");
      after = style == "01"      ? (value.get<bool>() ? "1" : "0")
              : style == "yesno" ? (value.get<bool>() ? "yes" : "no")
                                 : (value.get<bool>() ? "true" : "false");
    }
    out.keys.push_back(Json{{"key", key.toStdString()},
                            {"section", section.toStdString()},
                            {"prior", found.index < 0 ? "absent" : "value"},
                            {"prior_value", found.prior.toStdString()},
                            {"after_value", after.toStdString()}});
    if (found.index >= 0)
      lines[found.index] = found.prefix + after + found.suffix;
    else {
      if (found.insertAt < 0) {
        if (!lines.isEmpty() && !lines.last().isEmpty())
          lines << "";
        lines << "[" + section + "]";
        lines << key + " = " + after;
      } else
        lines.insert(found.insertAt, key + " = " + after);
    }
  }
  text.value = lines.join(text.newline) +
               (hadEnd || before.isEmpty() ? text.newline : QString());
  out.bytes = text.encode();
  return out;
}
QByteArray undoConfig(const QByteArray &current, const QString &format,
                      const Json &keys, bool &kept) {
  Text text(current);
  if (format == "json") {
    Json j;
    try {
      j = Json::parse(text.value.toStdString());
    } catch (const std::exception &) {
      kept = true;
      return current;
    }
    for (const auto &k : keys) {
      try {
        const auto key = string(k, "key");
        if (dotted(j, key).dump() != k["after_value"].get<std::string>()) {
          kept = true;
          continue;
        }
        setJson(j, key,
                string(k, "prior") == "absent"
                    ? Json()
                    : Json::parse(k["prior_value"].get<std::string>()),
                string(k, "prior") == "absent");
      } catch (const Error &) {
        kept = true;
      }
    }
    text.value = QString::fromStdString(j.dump(2)) + text.newline;
    return text.encode();
  }
  auto lines = text.value.split(text.newline);
  for (const auto &k : keys) {
    const auto found = find(lines, string(k, "section"), string(k, "key"));
    if (found.index < 0 || found.prior != string(k, "after_value")) {
      kept = true;
      continue;
    }
    if (string(k, "prior") == "absent")
      lines.removeAt(found.index);
    else
      lines[found.index] =
          found.prefix + string(k, "prior_value") + found.suffix;
  }
  text.value = lines.join(text.newline);
  return text.encode();
}
} // namespace ac::install
