// SPDX-License-Identifier: GPL-3.0-only
#include "Wii.h"
#include "core/install/Install.h"
#include "core/scan/Scan.h"
#include "core/scan/DiscIndex.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
namespace ac::wii {
namespace {
// Universal controls contract v1 (U3 recentre, U4 quit) as Dolphin hotkey expressions. A test checks
// these strings against setups/dolphin-vr/motion-controls/universal_controls.py.
const QString kResetBase = QStringLiteral("@(Shift+R)");
const QString kResetChord = QString::fromUtf8(R"x(pulse(!`OpenXR/0/OpenXR Controller:Left Button Thumbstick` & !`OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 0.1) & pulse(pulse(`OpenXR/0/OpenXR Controller:Left Button Thumbstick`, 0.15) & pulse(`OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 0.15) & `OpenXR/0/OpenXR Controller:Left Button Thumbstick` & `OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 1.0))x");
const QString kResetChordOld = QString::fromUtf8(R"x(pulse(!`OpenXR/0/OpenXR Controller:Left Button Thumbstick` & !`OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 0.1) & pulse(pulse(`OpenXR/0/OpenXR Controller:Left Button Thumbstick`, 0.25) & pulse(`OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 0.25) & `OpenXR/0/OpenXR Controller:Left Button Thumbstick` & `OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 1.0))x");
const QString kExitChord = QString::fromUtf8(R"x(hold(`OpenXR/0/OpenXR Controller:Left Button Thumbstick` & `OpenXR/0/OpenXR Controller:Right Button Thumbstick`, 3.0))x");
const QString kBackup = QStringLiteral(".before-universal-controls");
struct Ini {
  QStringList lines;
  QString eol = "\n";
  bool trailing = true;
};
Ini splitIni(const QString &text) {
  Ini ini;
  ini.eol = text.contains("\r\n") ? "\r\n" : "\n";
  QString norm = text;
  norm.replace("\r\n", "\n");
  if (norm.isEmpty())
    return ini;
  ini.trailing = norm.endsWith('\n');
  ini.lines = norm.split('\n');
  if (ini.trailing)
    ini.lines.removeLast();
  return ini;
}
QString joinIni(const Ini &ini) {
  const auto out = ini.lines.join(ini.eol);
  return ini.trailing && !ini.lines.isEmpty() ? out + ini.eol : out;
}
bool sectionBounds(const Ini &ini, const QString &section, int &start, int &end) {
  start = -1;
  for (int i = 0; i < ini.lines.size(); ++i) {
    const auto line = ini.lines[i].trimmed();
    if (line.startsWith('[') && line.endsWith(']')) {
      if (start >= 0) {
        end = i;
        return true;
      }
      if (line == "[" + section + "]")
        start = i;
    }
  }
  end = int(ini.lines.size());
  return start >= 0;
}
int findKey(const Ini &ini, int start, int end, const QString &key) {
  for (int i = start + 1; i < end; ++i) {
    const auto eq = ini.lines[i].indexOf('=');
    if (eq >= 0 && ini.lines[i].left(eq).trimmed() == key)
      return i;
  }
  return -1;
}
// The value of a key; an empty string when the section or the key is missing.
QString getValue(const Ini &ini, const QString &section, const QString &key) {
  int start = 0, end = 0;
  if (!sectionBounds(ini, section, start, end))
    return QString();
  const auto i = findKey(ini, start, end, key);
  return i < 0 ? QString() : ini.lines[i].mid(ini.lines[i].indexOf('=') + 1).trimmed();
}
bool hasKey(const Ini &ini, const QString &section, const QString &key) {
  int start = 0, end = 0;
  return sectionBounds(ini, section, start, end) && findKey(ini, start, end, key) >= 0;
}
void putValue(Ini &ini, const QString &section, const QString &key, const QString &value) {
  const auto line = key + " = " + value;
  int start = 0, end = 0;
  if (!sectionBounds(ini, section, start, end)) {
    ini.lines << "[" + section + "]" << line;
    return;
  }
  const auto at = findKey(ini, start, end, key);
  if (at >= 0) {
    ini.lines[at] = line;
    return;
  }
  auto insert = end;
  while (insert > start + 1 && ini.lines[insert - 1].trimmed().isEmpty())
    --insert;
  ini.lines.insert(insert, line);
}
// The new value for one hotkey; an empty string when the existing one is already right.
QString mergeHotkey(const QString &existing, const QString &base, const QString &chord, const QString &oldChord) {
  const auto current = existing.trimmed();
  if (current.isEmpty())
    return base.isEmpty() ? chord : base + " | (" + chord + ")";
  if (current.contains(chord))
    return QString();
  if (!oldChord.isEmpty() && current.contains(oldChord)) {
    auto upgraded = current;
    return upgraded.replace(oldChord, chord);
  }
  if (!base.isEmpty() && current == base)
    return base + " | (" + chord + ")";
  return "(" + current + ") | (" + chord + ")";
}
QString mergeHotkeysText(const QString &text, QStringList &changes) {
  auto ini = splitIni(text);
  const auto before = changes.size();
  if (getValue(ini, "Hotkeys", "Device").isEmpty()) {
    putValue(ini, "Hotkeys", "Device", "DInput/0/Keyboard Mouse");
    changes << "Hotkeys.ini Device added";
  }
  struct Row {
    QString key, base, chord, old;
  };
  const QList<Row> rows{{"VR/Reset VR Position", kResetBase, kResetChord, kResetChordOld}, {"General/Exit", QString(), kExitChord, QString()}};
  for (const auto &row : rows) {
    const auto existing = getValue(ini, "Hotkeys", row.key);
    const auto value = mergeHotkey(existing, row.base, row.chord, row.old);
    if (value.isEmpty())
      continue;
    putValue(ini, "Hotkeys", row.key, value);
    changes << "Hotkeys.ini " + row.key + (existing.trimmed().isEmpty() ? " added" : " updated");
  }
  return changes.size() == before ? text : joinIni(ini);
}
QString mergeDolphinText(const QString &text, QStringList &changes) {
  auto ini = splitIni(text);
  const auto before = changes.size();
  if (getValue(ini, "Interface", "ConfirmStop").toLower() != "false") {
    putValue(ini, "Interface", "ConfirmStop", "False");
    changes << "Dolphin.ini ConfirmStop set to False";
  }
  if (!hasKey(ini, "General", "HotkeysRequireFocus")) {
    putValue(ini, "General", "HotkeysRequireFocus", "False");
    changes << "Dolphin.ini HotkeysRequireFocus added";
  }
  return changes.size() == before ? text : joinIni(ini);
}
// Merge one file in place. A missing file is created from createText when that is not empty, else skipped.
void reseedFile(const QString &path, QString (*merge)(const QString &, QStringList &), const QString &createText, QStringList &changes) {
  QByteArray raw;
  bool bom = false;
  QString text = createText;
  if (QFileInfo::exists(path)) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      return;
    raw = file.readAll();
    file.close();
    bom = raw.startsWith("\xef\xbb\xbf");
    text = QString::fromUtf8(bom ? raw.mid(3) : raw);
  } else if (createText.isEmpty()) {
    return;
  }
  const auto count = changes.size();
  const auto merged = merge(text, changes);
  const auto undo = [&] { changes.erase(changes.begin() + count, changes.end()); };
  if (changes.size() == count)
    return;
  if (!raw.isEmpty() && !QFileInfo::exists(path + kBackup) && !QFile::copy(path, path + kBackup))
    return undo(); // No backup, no change.
  QSaveFile out(path);
  if (!out.open(QIODevice::WriteOnly))
    return undo();
  out.write((bom ? QByteArray("\xef\xbb\xbf") : QByteArray()) + merged.toUtf8());
  if (!out.commit())
    undo();
}
} // namespace
QVariantList games(){return scan::DiscIndex::games("wii");}
QString discId(const QString &path,std::atomic_bool &cancel){const auto read=scan::Scanner::inspect(path,cancel);return read.error.isEmpty()&&read.system=="wii"?read.identity:QString();}
QString discId(const QString &path){std::atomic_bool cancel{false};return discId(path,cancel);}
QString gameFor(const QString &id){return scan::DiscIndex::match("wii",id).value("game").toString();}
QVariantMap identify(const QString &path,std::atomic_bool &cancel){
 const QFileInfo info(path);const auto id=discId(info.absoluteFilePath(),cancel);const auto row=scan::DiscIndex::match("wii",id);
 return {{"file",QDir::fromNativeSeparators(info.absoluteFilePath())},{"id",id},{"game",row.value("game",QString())},{"title",row.value("title",QString())},{"verified",!row.isEmpty()}};
}
QVariantMap identify(const QString &path){std::atomic_bool cancel{false};return identify(path,cancel);}
QVariantList discs(const QString &folder, std::atomic_bool &cancel) {
  QVariantList found;
  if (folder.isEmpty())
    return found;
  for (const auto &entry : QDir(folder).entryInfoList({"*.iso", "*.wbfs", "*.rvz", "*.wia", "*.gcz", "*.wad", "*.nkit", "*.m3u"}, QDir::Files, QDir::Name)) {
    if (cancel)
      break;
    if (entry.isSymLink() || entry.isJunction())
      continue;
    found << identify(entry.absoluteFilePath(),cancel);
  }
  return found;
}
QStringList missing(const QString &game, const QString &id, const QString &executable) {
  QStringList result;
  QVariantMap row;
  for(const auto &entry:games())if(entry.toMap().value("id")==game)row=entry.toMap();
  if(row.isEmpty())return result;
  const QDir user(QFileInfo(executable).absolutePath() + "/User");
  if (!id.isEmpty() && !QFileInfo(user.filePath("GameSettingsVR/" + id + ".ini")).isFile())
    result << QString("VR settings of its own (GameSettingsVR/%1.ini), so it starts with DolphinXR's default VR screen").arg(id);
  if (row.value("hold").toString() == "docked")
    result << "docked controls: it is meant to be held with both hands on one gun, and the build is set for a gun in one hand";
  return result;
}
QVariantMap launchPlan(const QString &file, const QString &executable) {
  if (executable.isEmpty() || !QFileInfo(executable).isFile())
    throw install::Error("E_WII_SETUP", "The DolphinXR build is not set up. Choose its Dolphin.exe in Set-up.");
  if (file.isEmpty() || !QFileInfo(file).isFile())
    throw install::Error("E_WII_DISC", "The disc image is no longer where it was. Use Rescan in Set-up.");
  const QFileInfo program(executable);
  const auto native = [](const QString &path) { return QDir::toNativeSeparators(path); };
  return {{"executable", native(program.absoluteFilePath())},
          {"cwd", native(program.absolutePath())},
          {"args", QStringList{"--user", native(program.absolutePath() + "/User"), "-C", "Graphics.VR.EnableOpenXR=True", "-C",
                               "Graphics.VR.ResolutionScale=1.0", "-C", "Graphics.VR.EagerHeartbeat=False", "-C",
                               "Graphics.Stereoscopy.StereoMode=0", "-C", "Dolphin.Interface.ConfirmStop=False", "--exec",
                               native(QFileInfo(file).absoluteFilePath())}}};
}
QStringList refreshUniversalControls(const QString &userFolder) {
  QStringList changes;
  const QDir config(userFolder + "/Config");
  if (!config.exists())
    return changes;
  reseedFile(config.filePath("Hotkeys.ini"), mergeHotkeysText, QStringLiteral("[Hotkeys]\n"), changes);
  reseedFile(config.filePath("Dolphin.ini"), mergeDolphinText, QString(), changes);
  return changes;
}
} // namespace ac::wii
