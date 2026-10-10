// SPDX-License-Identifier: GPL-3.0-only
#include "CatalogLoader.h"
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <sstream>
#include <toml++/toml.hpp>
namespace ac {
namespace {
QString str(const Json &j, const char *key, const QString &fallback = {}) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? QString::fromStdString(it->get<std::string>())
                                            : fallback;
}
QStringList list(const Json &j, const char *key) {
    QStringList out;
    auto it = j.find(key);
    if (it != j.end() && it->is_array())
        for (const auto &v : *it)
            if (v.is_string())
                out << QString::fromStdString(v.get<std::string>());
    return out;
}
Json object(const Json &j, const char *key) {
    auto it = j.find(key);
    return it != j.end() && it->is_object() ? *it : Json::object();
}
int integer(const Json &j, const char *key, int fallback = 0) {
    auto it = j.find(key);
    return it != j.end() && it->is_number_integer() ? it->get<int>() : fallback;
}
bool boolean(const Json &j, const char *key, bool fallback = false) {
    auto it = j.find(key);
    return it != j.end() && it->is_boolean() ? it->get<bool>() : fallback;
}
Json convert(const toml::node &node) {
    if (const auto *table = node.as_table()) {
        // toml++ stores tables in key order; restore declaration order for hardware sorting.
        std::vector<std::pair<std::string, const toml::node *>> entries;
        for (const auto &[key, value] : *table)
            entries.emplace_back(std::string(key.str()), &value);
        std::stable_sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
            return a.second->source().begin < b.second->source().begin;
        });
        Json out = Json::object();
        for (const auto &[key, value] : entries)
            out[key] = convert(*value);
        return out;
    }
    if (const auto *array = node.as_array()) {
        Json out = Json::array();
        for (const auto &v : *array)
            out.push_back(convert(v));
        return out;
    }
    if (auto v = node.value<std::string>())
        return *v;
    if (node.is_boolean())
        return node.value<bool>().value();
    if (node.is_integer())
        return node.value<int64_t>().value();
    if (node.is_floating_point())
        return node.value<double>().value();
    std::ostringstream text;
    text << toml::toml_formatter{node};
    return text.str();
}
QString pointerKey(const std::string &key) {
    auto k = QString::fromStdString(key);
    k.replace("~", "~0");
    k.replace("/", "~1");
    return k;
}
bool idArray(const Json &v) {
    if (!v.is_array() || v.empty())
        return false;
    return std::all_of(v.begin(), v.end(), [](const Json &e) {
        return e.is_object() && e.contains("id") && e["id"].is_string();
    });
}
void eraseProvenance(Json &p, const QString &prefix) {
    for (auto it = p.begin(); it != p.end();) {
        const auto key = QString::fromStdString(it.key());
        if (key == prefix || key.startsWith(prefix + "/"))
            it = p.erase(it);
        else
            ++it;
    }
}
void mark(Json &p, const Json &v, const QString &path, const QString &source) {
    if (v.is_object())
        for (auto it = v.begin(); it != v.end(); ++it)
            mark(p, it.value(), path + "/" + pointerKey(it.key()), source);
    else if (v.is_array() && !v.empty())
        for (size_t i = 0; i < v.size(); ++i)
            mark(p, v[i], path + "/" + QString::number(i), source);
    else
        p[path.toStdString()] = source.toStdString();
}
void mergeTracked(Json &base, const Json &layer, Json &provenance, const QString &path,
                  const QString &source) {
    if (layer.is_object()) {
        if (!base.is_object())
            base = Json::object();
        for (auto it = layer.begin(); it != layer.end(); ++it) {
            const auto child = path + "/" + pointerKey(it.key());
            if (it.value().is_string() && it.value() == "!delete") {
                base.erase(it.key());
                eraseProvenance(provenance, child);
                continue;
            }
            if (!base.contains(it.key()))
                base[it.key()] = nullptr;
            mergeTracked(base[it.key()], it.value(), provenance, child, source);
        }
    } else if (idArray(base) && idArray(layer)) {
        for (const auto &entry : layer) {
            size_t i = 0;
            while (i < base.size() && base[i]["id"] != entry["id"])
                ++i;
            if (i == base.size())
                base.push_back(Json::object());
            mergeTracked(base[i], entry, provenance, path + "/" + QString::number(i), source);
        }
    } else {
        base = layer;
        eraseProvenance(provenance, path);
        mark(provenance, base, path, source);
    }
}
QString label(const Json &vocab, const QString &id) {
    auto it = vocab.find(id.toStdString());
    return it == vocab.end() ? "Unknown (" + id + ")" : str(*it, "label", id);
}
void validate(GameRecord &g, const CatalogData &data, const QSet<QString> &ids) {
    const auto &j = g.raw;
    auto warn = [&](const QString &s, bool error = false) {
        g.warnings << s;
        if (error)
            g.validationErrors << s;
    };
    for (const auto *key : {"id", "title", "genre", "year", "manufacturer", "hardware", "graphics"})
        if (!j.contains(key))
            warn(QString("missing required field '%1'").arg(key), true);
    if (str(j, "id") != g.id)
        warn("id does not match folder", true);
    if (!QRegularExpression("^[a-z0-9][a-z0-9-]*$").match(g.id).hasMatch())
        warn("folder/id must be lowercase kebab-case", true);
    for (const auto *key : {"id", "title", "genre", "manufacturer", "hardware", "graphics"})
        if (j.contains(key) && !j[key].is_string())
            warn(QString("%1 must be text").arg(key), true);
    if (!j.contains("hub") || !j["hub"].is_object())
        warn("hub table missing or invalid");
    const auto genres = object(data.vocab, "genres"), hardware = object(data.vocab, "hardware");
    for (const auto *key : {"genre", "manufacturer", "hardware", "graphics"}) {
        const Json registry =
            QString(key) == "genre"
                ? object(genres, "genre")
                : object(data.vocab, QString(key) == "manufacturer" ? "manufacturers" : key);
        if (!registry.contains(str(j, key).toStdString()))
            warn(QString("%1 not in vocab").arg(key), true);
    }
    for (const auto &sg : list(j, "subgenre")) {
        auto registry = object(genres, "subgenre");
        if (!registry.contains(sg.toStdString()))
            warn("subgenre not in vocab: " + sg, true);
        else if (str(registry[sg.toStdString()], "genre") != str(j, "genre"))
            warn("subgenre belongs to another genre: " + sg);
    }
    if (!j.contains("year") || !j["year"].is_number_integer() || integer(j, "year") < 1970 ||
        integer(j, "year") > 2026)
        warn("year is not a plausible integer year", true);
    const QMap<QString, QString> prefixes{{"sony-ps1", "ps1-"},
                                          {"sony-ps2", "ps2-"},
                                          {"sony-ps3", "ps3-"},
                                          {"sega-saturn", "sat-"},
                                          {"sega-dreamcast", "dc-"},
                                          {"nintendo-64", "n64-"},
                                          {"nintendo-gamecube", "gc-"},
                                          {"nintendo-wii", "wii-"},
                                          {"nintendo-wii-u", "wiiu-"},
                                          {"nintendo-switch", "switch-"},
                                          {"microsoft-xbox", "xbox-"},
                                          {"microsoft-xbox-360", "x360-"},
                                          {"pc-windows", "pc-"},
                                          {"nintendo-3ds", "3ds-"},
                                          {"nintendo-ds", "nds-"},
                                          {"sega-genesis", "md-"},
                                          {"sega-cd", "scd-"},
                                          {"super-nes", "snes-"},
                                          {"sega-master-system", "sms-"},
                                          {"sega-game-gear", "gg-"}};
    const auto hw = str(j, "hardware");
    if (prefixes.contains(hw) && !g.id.startsWith(prefixes[hw]))
        warn("hardware id prefix should be " + prefixes[hw]);
    if (hardware.contains(hw.toStdString()) && str(hardware[hw.toStdString()], "kind") == "arcade")
        for (const auto &prefix : prefixes)
            if (g.id.startsWith(prefix)) {
                warn("arcade entry uses console id prefix");
                break;
            }
    QSet<QString> mediaIds;
    const auto media = j.value("media", Json::array());
    if (!media.is_array()) warn("media must be an array of tables", true);
    else {
        qsizetype index = 0;
        for (const auto &m : media) {
            const auto requirement = mediaRequirementId(m, g.id, index++);
            if (!m.is_object()) { warn("media item must be a table", true); continue; }
            if (m.contains("optional") && !m["optional"].is_boolean())
                warn("media.optional must be boolean", true);
            for (const auto *key : {"set", "serial", "id"})
                if (m.contains(key) && !m[key].is_string())
                    warn(QString("media.%1 must be text").arg(key), true);
            if (mediaIds.contains(requirement)) warn("duplicate media requirement: " + requirement, true);
            mediaIds.insert(requirement);
        }
    }
    const auto variants = object(g.install, "variant");
    if (g.install.contains("variant") && !g.install["variant"].is_object())
        warn("variant must be a table", true);
    for (auto it = variants.begin(); it != variants.end(); ++it) {
        const auto name = QString::fromStdString(it.key());
        if (!it.value().is_object()) { warn("variant." + name + " must be a table", true); continue; }
        if (it.value().contains("needs") && !it.value()["needs"].is_object())
            warn("variant." + name + ".needs must be a table", true);
        const auto needs = object(it.value(), "needs");
        if (needs.contains("media") && (!needs["media"].is_array() ||
            std::any_of(needs["media"].begin(), needs["media"].end(),
                        [](const Json &id) { return !id.is_string(); })))
            warn("needs.media must be an array of text", true);
        for (const auto &id : list(needs, "media"))
            if (!mediaIds.contains(id))
                warn("unresolved needs.media: " + id, true);
    }
    const auto routes = object(j, "routes");
    for (auto it = routes.begin(); it != routes.end(); ++it) {
        if (it.key() == "vr") {
            if (!QStringList{"true3d", "theatre", "none"}.contains(str(it.value(), "best")))
                warn("routes.vr.best should be true3d|theatre|none");
        } else if (it.value().is_string() &&
                   !QStringList{"working", "imperfect", "not-working", "profile", "playable",
                                "unknown"}
                        .contains(QString::fromStdString(it.value().get<std::string>())))
            warn("route status invalid: " + QString::fromStdString(it.key()));
    }
    auto hub = object(j, "hub");
    for (const auto *key : {"pill", "colour", "accent", "blurb"})
        if (!hub.contains(key))
            warn(QString("hub.%1 missing").arg(key));
    if (str(hub, "pill").size() > 12)
        warn("hub.pill longer than 12 chars");
    for (const auto *key : {"colour", "accent"})
        if (hub.contains(key) &&
            !QRegularExpression("^#[0-9a-fA-F]{6}$").match(str(hub, key)).hasMatch())
            warn(QString("hub.%1 is not #rrggbb").arg(key));
    if (list(object(j, "meta"), "sources").isEmpty())
        warn("meta.sources empty");
    if (!str(j, "original").isEmpty() && !ids.contains(str(j, "original")))
        warn("original has no game folder");
    if (!j.contains("players") || !j["players"].is_number_integer() || integer(j, "players") < 1)
        warn("players missing or invalid");
    const auto ct = str(object(j, "controls"), "type");
    if (j.contains("controls") && !j["controls"].is_object())
        warn("controls must be a table", true);
    const auto controls = object(j, "controls");
    if (controls.contains("gun_model") &&
        (!controls["gun_model"].is_string() ||
         !QRegularExpression("\\A[a-z0-9][a-z0-9-]*\\z").match(str(controls, "gun_model")).hasMatch()))
        warn("controls.gun_model must be a lowercase kebab-case id", true);
    if (controls.contains("two_guns") &&
        (!controls["two_guns"].is_string() ||
         !QStringList{"on_join", "always", "off"}.contains(str(controls, "two_guns"))))
        warn("controls.two_guns must be on_join|always|off", true);
    if (!ct.isEmpty() && !QStringList{"gun", "wheel", "handlebars", "bike", "ski", "joystick",
                                      "yoke", "boat", "other"}
                              .contains(ct))
        warn("controls.type unknown: " + ct);
}
void derive(GameRecord &g, const CatalogData &data) {
    const auto &j = g.raw;
    auto &r = g.roles;
    const auto hub = object(j, "hub"), controls = object(j, "controls"),
               vr = object(object(j, "routes"), "vr"), genres = object(data.vocab, "genres");
    r["gameId"] = g.id;
    for (const auto *key : {"title", "series", "developer"})
        r[key] = str(j, key, QString(key) == "title" ? g.id : QString());
    r["altTitles"] = list(j, "alt_titles");
    r["blurb"] = str(hub, "blurb");
    r["originalId"] = str(j, "original");
    const auto genre = str(j, "genre"), maker = str(j, "manufacturer"), hw = str(j, "hardware"),
               graphics = str(j, "graphics");
    r["genreId"] = genre;
    r["genreLabel"] = label(object(genres, "genre"), genre);
    r["manufacturerId"] = maker;
    r["manufacturerLabel"] = label(object(data.vocab, "manufacturers"), maker);
    const auto makerRecord =
        object(object(data.vocab, "manufacturers"), maker.toUtf8().constData());
    r["manufacturerShort"] = str(makerRecord, "short", r["manufacturerLabel"].toString());
    r["developerShown"] = !str(j, "developer").isEmpty() &&
                          folded(str(j, "developer")) != folded(r["manufacturerLabel"].toString());
    const auto hardware = object(data.vocab, "hardware");
    auto hardwareRecord = object(hardware, hw.toUtf8().constData());
    r["hardwareId"] = hw;
    r["hardwareLabel"] = label(hardware, hw);
    r["hardwareFamily"] = str(hardwareRecord, "family");
    r["hardwareKind"] = str(hardwareRecord, "kind");
    int order = 0;
    for (auto it = hardware.begin(); it != hardware.end(); ++it, ++order)
        if (it.key() == hw.toStdString())
            break;
    r["hardwareOrder"] = order;
    r["graphicsId"] = graphics;
    r["graphicsLabel"] = label(object(data.vocab, "graphics"), graphics);
    const auto subgenres = list(j, "subgenre");
    QStringList labels;
    for (const auto &sg : subgenres)
        labels << label(object(genres, "subgenre"), sg);
    r["subgenreIds"] = subgenres;
    r["subgenreLabels"] = labels;
    const QList<QStringList> buckets{
        {"rail-shooter"},
        {"cover-shooter", "sniper"},
        {"horror"},
        {"hunting", "machine-gun", "party", "water-gun"},
        {"circuit", "street"},
        {"rally", "truck"},
        {"kart", "futuristic", "vehicle-combat"},
        {"motorcycle", "bicycle", "boat", "ski", "mission-driving", "flying"}};
    int mask = 0;
    for (int i = 0; i < buckets.size(); ++i)
        for (const auto &sg : subgenres)
            if (buckets[i].contains(sg))
                mask |= 1 << i;
    r["bucketMask"] = mask;
    r["year"] = integer(j, "year");
    r["decade"] = integer(j, "year") / 10 * 10;
    r["players"] = integer(j, "players");
    r["playersBucket"] = integer(j, "players") <= 1 ? "1" : "2+";
    const auto ct = str(controls, "type", "other");
    r["controlsType"] = ct;
    r["controlsLabel"] = ct == "other" ? "Unknown" : ct;
    const auto best = str(vr, "best", "none");
    const bool planned = str(vr, "planned") == "true3d-planned" || boolean(vr, "planned");
    r["vrBest"] = best;
    r["vrPlanned"] = planned;
    r["vrBadge"] = static_cast<int>(best == "true3d"    ? VrBadge::True3D
                                    : best == "theatre" ? VrBadge::Theatre
                                    : planned           ? VrBadge::Planned
                                                        : VrBadge::None);
    r["featuredEligible"] =
        boolean(hub, "featured", true) && (best == "true3d" || best == "theatre");
    const auto tags = list(hub, "badges");
    bool wip = tags.contains("wip"), quest = tags.contains("quest-standalone");
    for (const auto &v : g.variants) {
        wip |= v.status == "wip";
        quest |= object(v.raw, "needs").contains("headset");
    }
    QStringList badges;
    if (QStringList{"gun", "wheel", "handlebars", "bike"}.contains(ct))
        badges << ct.toUpper();
    if (tags.contains("roomscale"))
        badges << "ROOMSCALE";
    if (tags.contains("seated"))
        badges << "SEATED";
    if (integer(j, "players") >= 2)
        badges << "2 PLAYERS";
    if (wip)
        badges << "WIP";
    if (quest)
        badges << "QUEST STANDALONE";
    r["badges"] = badges;
    r["isWip"] = wip;
    r["quest"] = quest;
    r["roomscale"] = tags.contains("roomscale");
    r["seated"] = tags.contains("seated");
    r["hasRecipe"] = g.hasRecipe;
    r["variantCount"] = g.variants.size();
    r["pill"] = str(hub, "pill", genre.toUpper()).toUpper().left(12);
    const auto colours = object(data.theme, "color");
    QColor genreColour(str(object(colours, "genre"), genre.toUtf8().constData(),
                           genre == "gun" ? "#e0302a" : "#3a8add"));
    QVariantMap stateColours;
    for (const auto *key : {"planned_neon", "installed_neon", "retry_line", "update_neon",
                            "ready_neon", "needs_neon", "needs_emulator"})
        stateColours[key] = QColor(str(object(colours, "state"), key, "#8a93a6"));
    r["_stateColours"] = stateColours;
    QColor base(str(hub, "colour")), accent(str(hub, "accent"));
    if (!base.isValid())
        base = QColor(str(object(colours, "surface"), "card", "#0c0c10"));
    if (!accent.isValid())
        accent = genreColour;
    r["genreColour"] = genreColour;
    r["colourBase"] = base;
    r["accent"] = accent;
    const double lum = .299 * accent.red() + .587 * accent.green() + .114 * accent.blue();
    const double scale = lum > 0 ? 140.0 / lum : 1;
    r["neonAccent"] = QColor(std::min(255, static_cast<int>(accent.red() * scale)),
                             std::min(255, static_cast<int>(accent.green() * scale)),
                             std::min(255, static_cast<int>(accent.blue() * scale)));
    r["loadWarning"] = g.warnings.join("\n");
    r["loadError"] = g.errors.join("\n");
    for (const auto *kind : {"Banner", "Tile", "Portrait"})
        r[QString("art") + kind] = "image://art/" + g.id + "/" + QString(kind).toLower();
    QVariantMap search;
    const QMap<QString, QString> searchFields{
        {"title", r["title"].toString()},
        {"alt", list(j, "alt_titles").join(" ")},
        {"series", str(j, "series")},
        {"developer", str(j, "developer")},
        {"manufacturer", maker + " " + r["manufacturerLabel"].toString()},
        {"hardware", hw + " " + r["hardwareLabel"].toString()},
        {"subgenre", subgenres.join(" ") + " " + labels.join(" ")},
        {"blurb", str(hub, "blurb")},
        {"badges", badges.join(" ")},
        {"vr", best + (planned ? " planned" : "")},
        {"genre", genre + " " + r["genreLabel"].toString()},
        {"id", g.id}};
    for (auto it = searchFields.begin(); it != searchFields.end(); ++it)
        search[it.key()] = folded(it.value());
    r["searchIndex"] = search;
    resolveState(g);
}
} // namespace
Json CatalogLoader::parseToml(const QString &file) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        throw std::runtime_error("cannot open metadata file");
    return convert(toml::parse(f.readAll().toStdString()));
}
void CatalogLoader::merge(Json &base, const Json &layer) {
    Json p = Json::object();
    mergeTracked(base, layer, p, {}, {});
}
const GameRecord *CatalogData::find(const QString &id) const {
    for (const auto &g : games)
        if (g.id == id)
            return &g;
    return nullptr;
}
CatalogData CatalogLoader::load(const QString &root) const {
    CatalogData data;
    data.root=QDir(root).absolutePath();
    const auto themePath = QFileInfo::exists(root + "/docs/ui/theme.toml")
                               ? root + "/docs/ui/theme.toml"
                               : QString(":/resources/theme.toml");
    try {
        if (QFileInfo::exists(themePath))
            data.theme = parseToml(themePath);
    } catch (const std::exception &) {
        data.report.messages << "Theme metadata invalid; using neutral defaults";
        ++data.report.warnings;
    }
    QStringList layers{QDir(root).absolutePath()};
    struct Pack {
        QString name, path;
        int priority;
    };
    QList<Pack> packs;
    QDir packDir(QDir(root).filePath("packs"));
    for (const auto &name : packDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        const auto path = packDir.filePath(name);
        int priority = 0;
        try {
            if (QFileInfo::exists(path + "/pack.toml"))
                priority = integer(parseToml(path + "/pack.toml"), "priority");
        } catch (const std::exception &e) {
            data.report.messages << "pack.toml: " + QString::fromUtf8(e.what());
            ++data.report.warnings;
        }
        packs << Pack{name, path, priority};
    }
    std::stable_sort(packs.begin(), packs.end(), [](const Pack &a, const Pack &b) {
        return a.priority == b.priority ? a.name < b.name : a.priority < b.priority;
    });
    for (const auto &p : packs) {
        data.packIds << p.name;
        layers << p.path;
    }
    layers << QDir(root).filePath("user/overrides");
    QMap<QString, GameRecord> records;
    for (const auto &layer : layers) {
        for (const auto &kind : {QString("vocab"), QString("emulators"), QString("setups")}) {
            QDir dir(kind == "setups" ? layer + "/setups" : layer + "/data/" + kind);
            for (const auto &file : dir.entryList({"*.toml"}, QDir::Files, QDir::Name)) {
                try {
                    auto value = parseToml(dir.filePath(file));
                    const auto key = QFileInfo(file).baseName().toStdString();
                    auto &target = kind == "vocab"       ? data.vocab
                                   : kind == "emulators" ? data.emulators
                                                         : data.sharedSetups;
                    if (!target.contains(key))
                        target[key] = Json::object();
                    merge(target[key], value);
                } catch (const std::exception &e) {
                    data.report.messages << kind + "/" + file + ": " + QString::fromUtf8(e.what());
                    ++data.report.warnings;
                }
            }
        }
        QDir dir(layer + "/games");
        for (const auto &id : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const auto folder = dir.filePath(id);
            if (!records.contains(id) && !QFileInfo::exists(folder + "/game.toml"))
                continue;
            auto &g = records[id];
            g.id = id;
            if (g.folder.isEmpty())
                g.folder = folder;
            g.runtime.gameId = id;
            auto read = [&](const QString &path, Json &target, Json &provenance, bool fatal) {
                if (!QFileInfo::exists(path))
                    return;
                try {
                    mergeTracked(target, parseToml(path), provenance, {},
                                 QDir(root).relativeFilePath(path));
                } catch (const std::exception &e) {
                    const auto message = QDir(root).relativeFilePath(path) +
                                         ": TOML error: " + QString::fromUtf8(e.what());
                    g.warnings << message;
                    if (fatal)
                        g.errors << message;
                }
            };
            read(folder + "/game.toml", g.raw, g.provenance, true);
            if (QFileInfo::exists(folder + "/install.toml"))
                g.hasRecipe = true;
            // A broken authored recipe warns without hiding a working generated
            // flat route. Recipe execution still requires a parsed variant.
            read(folder + "/install.toml", g.install, g.installProvenance, false);
            QDir setup(folder + "/setup");
            for (const auto &file : setup.entryList({"*.toml"}, QDir::Files, QDir::Name)) {
                const auto key = QFileInfo(file).baseName().toStdString();
                if (!g.setup.contains(key))
                    g.setup[key] = Json::object();
                Json prov = Json::object();
                const auto prefix = "/" + QString::fromStdString(key);
                for (auto it = g.setupProvenance.begin(); it != g.setupProvenance.end(); ++it) {
                    auto pointer = QString::fromStdString(it.key());
                    if (pointer.startsWith(prefix + "/"))
                        prov[pointer.mid(prefix.size()).toStdString()] = it.value();
                }
                read(setup.filePath(file), g.setup[key], prov, false);
                eraseProvenance(g.setupProvenance, prefix);
                for (auto it = prov.begin(); it != prov.end(); ++it)
                    g.setupProvenance["/" + key + it.key()] = it.value();
            }
        }
    }
    QSet<QString> ids;
    for (auto it = records.begin(); it != records.end(); ++it)
        ids.insert(it.key());
    QSet<QString> declaredIds;
    for (auto it = records.begin(); it != records.end(); ++it) {
        auto &g = it.value();
        validate(g, data, ids);
        const auto declaredId = str(g.raw, "id");
        if (!declaredId.isEmpty()) {
            if (declaredIds.contains(declaredId)) {
                g.errors << "duplicate declared id: " + declaredId;
                g.warnings << g.errors.last();
            }
            declaredIds.insert(declaredId);
        }
        const auto variants = object(g.install, "variant");
        for (auto v = variants.begin(); v != variants.end(); ++v) {
            // Validation already warns on non-table rows; they cannot shadow
            // a working generated flat route with the same id.
            if (!v.value().is_object()) continue;
            Variant record;
            record.id = QString::fromStdString(v.key());
            record.raw = v.value();
            record.title = str(record.raw, "title", record.id);
            record.status = str(record.raw, "status");
            record.quality = str(record.raw, "quality");
            record.installedWhen = str(record.raw, "installed_when");
            auto needs = object(record.raw, "needs");
            record.media = list(needs, "media");
            qsizetype mediaIndex = 0;
            for (const auto &m : (g.raw.contains("media") && g.raw["media"].is_array()
                                     ? g.raw["media"] : Json::array())) {
                const auto requirement = mediaRequirementId(m, g.id, mediaIndex++);
                if (optionalMedia(m)) record.media.removeAll(requirement);
            }
            record.tools = list(needs, "tools");
            g.variants << record;
        }
        // Basic flat route metadata for lane E/G; no install or launch occurs here.
        const auto routes = object(g.raw, "routes");
        for (auto route = routes.begin(); route != routes.end(); ++route) {
            if (!route.value().is_string() ||
                !QStringList{"working", "imperfect", "playable"}.contains(
                    QString::fromStdString(route.value().get<std::string>())) ||
                !data.emulators.contains(route.key()))
                continue;
            // VR tools are authored setup variants, never automatic flat routes.
            if (QStringList{"vr-tool", "vr-port"}.contains(
                    str(data.emulators[route.key()], "kind")))
                continue;
            const auto id = QString::fromStdString(route.key());
            bool exists = false;
            for (const auto &v : g.variants)
                exists |= v.id == id;
            if (exists)
                continue;
            Variant v;
            v.id = id;
            v.title = "Play in " + str(data.emulators[route.key()], "name", id);
            v.quality = "flat";
            v.status = "stable";
            v.generated = true;
            v.tools << id;
            if (g.raw.contains("media") && g.raw["media"].is_array()) {
                qsizetype mediaIndex = 0;
                for (const auto &m : g.raw["media"]) {
                    const auto requirement = mediaRequirementId(m, g.id, mediaIndex++);
                    if (m.is_object() && !optionalMedia(m) && mediaAppliesToRoute(m, id, str(g.raw, "hardware")))
                        v.media << requirement;
                }
            }
            v.raw = Json{
                {"title", v.title.toStdString()},
                {"quality", "flat"},
                {"status", "stable"},
                {"generated", true},
                {"needs",
                 {{"media",
                   v.media.isEmpty()
                       ? Json::array()
                       : Json::parse(QJsonDocument::fromVariant(v.media).toJson().toStdString())},
                  {"tools", Json::array({route.key()})}}},
                {"launch", object(data.emulators[route.key()], "launch")}};
            g.variants << v;
        }
        derive(g, data);
        data.report.errors += static_cast<int>(g.errors.size() + g.validationErrors.size());
        data.report.warnings += static_cast<int>(g.warnings.size());
        for (const auto &message : g.warnings) {
            data.report.messages << g.id + ": " + message;
            ++data.report.counts[message.section(':', 0, 0)];
        }
        data.games << g;
    }
    data.report.records = static_cast<int>(data.games.size());
    return data;
}
} // namespace ac
