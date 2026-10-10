// SPDX-License-Identifier: GPL-3.0-only
#include "Support.h"
#include "core/catalog/CatalogLoader.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QLockFile>
#include <QScopeGuard>
#include <QUuid>
#include <vector>
#include <limits>
#include <QSaveFile>
#include <QtEndian>
#include <sstream>
#include <toml++/toml.hpp>
#ifdef Q_OS_WIN
#include <io.h>
#include <qt_windows.h>
#include <shlobj.h>
#include <sddl.h>
#include <aclapi.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <signal.h>
#endif
namespace ac::install {
namespace {
enum class ChildState { Gone, Alive, Unknown };
struct ChildIdentity { ChildState state=ChildState::Unknown; QString creation; };
ChildIdentity childIdentity(qint64 pid) {
  if(pid<=0) return {};
#ifdef Q_OS_WIN
  if(static_cast<quint64>(pid)>MAXDWORD) return {};
  HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,static_cast<DWORD>(pid));
  if(!process) return {GetLastError()==ERROR_INVALID_PARAMETER?ChildState::Gone:ChildState::Unknown,{}};
  const auto close=qScopeGuard([&]{CloseHandle(process);});
  const auto wait=WaitForSingleObject(process,0);
  if(wait==WAIT_OBJECT_0) return {ChildState::Gone,{}};
  if(wait!=WAIT_TIMEOUT) return {};
  FILETIME creation{},exit{},kernel{},user{};
  if(!GetProcessTimes(process,&creation,&exit,&kernel,&user)) return {};
  const auto stamp=(static_cast<quint64>(creation.dwHighDateTime)<<32U)|creation.dwLowDateTime;
  return {ChildState::Alive,QString::number(stamp)};
#else
  if(pid>std::numeric_limits<pid_t>::max()) return {};
  QFile status(QString("/proc/%1/stat").arg(pid));
  if(!status.open(QIODevice::ReadOnly)) {
    if(::kill(static_cast<pid_t>(pid),0)!=0 && errno==ESRCH) return {ChildState::Gone,{}};
    return {};
  }
  const auto raw=status.read(4096); const auto close=raw.lastIndexOf(')');
  if(close<0) return {};
  const auto fields=raw.mid(close+2).split(' '); // state is field 3; start time is field 22.
  if(fields.size()<=19) return {};
  if(fields[0]=="Z" || fields[0]=="X") return {ChildState::Gone,{}};
  bool ok=false; const auto start=fields[19].toULongLong(&ok);
  if(!ok) return {};
  return {ChildState::Alive,QString::number(start)};
#endif
}
bool childStillUses(qint64 pid,const QString &creation) {
  const auto current=childIdentity(pid);
  if(current.state==ChildState::Unknown) return true;
  return current.state==ChildState::Alive && (creation.isEmpty() || creation==current.creation);
}
bool recordChildStillUses(const QString &path) {
  try {
    if(QFileInfo(path).size()>4096) throw Error("E_LOCKED","Shared child lease evidence is too large");
    const auto record=Json::parse(readBytes(path).toStdString());
    if(record.is_object() && record.value("format",0)==1 && record.value("phase",std::string())=="pending")
      throw Error("E_LOCKED","A launch has no registered child identity yet. Wait for its Hub/game to stop; if the Hub exited, an operator must review the pending lease. Automatic cleanup is refused");
    if(!record.is_object() || record.value("format",0)!=1 || !record.contains("child_pid") ||
       !record["child_pid"].is_number_integer() || !record.contains("child_identity") || !record["child_identity"].is_string())
      throw Error("E_LOCKED","Shared child lease evidence is invalid");
    const auto pid=record["child_pid"].get<qint64>(); const auto creation=string(record,"child_identity");
    if(pid<=0 || (!creation.isEmpty() && !QRegularExpression("\\A[0-9]{1,20}\\z").match(creation).hasMatch()))
      throw Error("E_LOCKED","Shared child lease evidence is invalid");
    return childStillUses(pid,creation);
  } catch(const Error &error) {
    if(error.code=="E_LOCKED") throw;
    throw Error("E_LOCKED","Shared child lease evidence cannot be verified; retained for safety");
  }
  catch(const std::exception &) {
    throw Error("E_LOCKED","Shared child lease evidence cannot be verified; retained for safety");
  }
}
QString resourceIdentity(const QString &input) {
  if (!QDir::isAbsolutePath(input))
    throw Error("E_PATH_OUTSIDE_ROOT", "Shared resource paths must be absolute");
  const auto absolute=QDir::cleanPath(QDir::fromNativeSeparators(input));
  QString ancestor=absolute; QStringList suffix;
  while(!QFileInfo::exists(ancestor)) {
    suffix.prepend(QFileInfo(ancestor).fileName());
    const auto parent=QFileInfo(ancestor).absolutePath();
    if(parent==ancestor) throw Error("E_PATH_OUTSIDE_ROOT","Shared resource has no existing ancestor");
    ancestor=parent;
  }
  scopedPath(absolute,ancestor); // Reject linked ancestors before canonicalizing.
  auto identity=QFileInfo(ancestor).canonicalFilePath();
  if(identity.isEmpty()) throw Error("E_PATH_OUTSIDE_ROOT","Cannot identify shared resource");
  if(!suffix.isEmpty()) identity += "/"+suffix.join('/');
  identity=QDir::cleanPath(identity);
#ifdef Q_OS_WIN
  identity=identity.toCaseFolded();
#endif
  return identity;
}
#ifdef Q_OS_WIN
void privateDirectory(const QString &path) {
  HANDLE token=nullptr;
  if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))
    throw Error("E_WRITE_DENIED","Cannot identify shared-lock OS user");
  const auto closeToken=qScopeGuard([&]{CloseHandle(token);});
  DWORD bytes=0; GetTokenInformation(token,TokenUser,nullptr,0,&bytes);
  std::vector<unsigned char> info(bytes);
  if(bytes==0 || !GetTokenInformation(token,TokenUser,info.data(),bytes,&bytes))
    throw Error("E_WRITE_DENIED","Cannot identify shared-lock OS user");
  const auto sid=reinterpret_cast<TOKEN_USER *>(info.data())->User.Sid;
  LPWSTR text=nullptr;
  if(!ConvertSidToStringSidW(sid,&text)) throw Error("E_WRITE_DENIED","Cannot protect shared locks");
  const auto freeText=qScopeGuard([&]{LocalFree(text);});
  const auto sddl="O:"+QString::fromWCharArray(text)+"D:P(A;OICI;FA;;;"+QString::fromWCharArray(text)+")(A;OICI;FA;;;SY)";
  PSECURITY_DESCRIPTOR descriptor=nullptr;
  if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(reinterpret_cast<LPCWSTR>(sddl.utf16()),SDDL_REVISION_1,&descriptor,nullptr))
    throw Error("E_WRITE_DENIED","Cannot protect shared locks");
  const auto freeDescriptor=qScopeGuard([&]{LocalFree(descriptor);});
  SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),descriptor,FALSE};
  const auto native=QDir::toNativeSeparators(path).toStdWString();
  if(!CreateDirectoryW(native.c_str(),&attributes) && GetLastError()!=ERROR_ALREADY_EXISTS)
    throw Error("E_WRITE_DENIED","Cannot create private shared-lock folder");
  scopedPath(path,QFileInfo(path).absolutePath());
  PSID owner=nullptr; PSECURITY_DESCRIPTOR current=nullptr;
  if(GetNamedSecurityInfoW(const_cast<LPWSTR>(native.c_str()),SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION,&owner,nullptr,nullptr,nullptr,&current)!=ERROR_SUCCESS)
    throw Error("E_WRITE_DENIED","Cannot verify shared-lock ownership");
  const auto freeCurrent=qScopeGuard([&]{LocalFree(current);});
  if(!owner || !EqualSid(owner,sid)) throw Error("E_WRITE_DENIED","Shared locks belong to a different OS user");
  BOOL present=FALSE,defaulted=FALSE; PACL dacl=nullptr;
  if(!GetSecurityDescriptorDacl(descriptor,&present,&dacl,&defaulted) || !present || !dacl ||
     SetNamedSecurityInfoW(const_cast<LPWSTR>(native.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,
                         nullptr,nullptr,dacl,nullptr)!=ERROR_SUCCESS)
    throw Error("E_WRITE_DENIED","Cannot restrict shared-lock access");
}
QString sharedLockRoot() {
  PWSTR folder=nullptr;
  if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_DONT_VERIFY,nullptr,&folder)))
    throw Error("E_WRITE_DENIED","OS per-user shared-lock location is unavailable");
  const auto freeFolder=qScopeGuard([&]{CoTaskMemFree(folder);});
  const auto base=QString::fromWCharArray(folder);
  const auto root=scopedPath(base+"/AladdinsCastle-shared-locks",base);
  privateDirectory(root); return root;
}
#else
void privateDirectory(const QString &path) {
  const auto encoded=QFile::encodeName(path);
  if(::mkdir(encoded.constData(),0700)!=0 && errno!=EEXIST)
    throw Error("E_WRITE_DENIED","Cannot create private shared-lock folder");
  struct stat info{};
  if(::lstat(encoded.constData(),&info)!=0 || !S_ISDIR(info.st_mode) || info.st_uid!=::geteuid())
    throw Error("E_WRITE_DENIED","Shared-lock folder is unsafe or belongs to another OS user");
  if((info.st_mode&0077)!=0 && ::chmod(encoded.constData(),info.st_mode&0700)!=0)
    throw Error("E_WRITE_DENIED","Cannot restrict shared-lock access");
  scopedPath(path,QFileInfo(path).absolutePath());
}
QString sharedLockRoot() {
  // Fixed system temporary base, not TMPDIR/TEMP/TMP; UID comes from the kernel.
  const auto root=QString("/tmp/aladdinscastle-shared-locks-%1").arg(::geteuid());
  privateDirectory(root); return root;
}
#endif
} // namespace
struct ResourceLocks::Impl {
  std::vector<std::unique_ptr<QLockFile>> held;
  QStringList childRecords;
  qint64 childPid=0;
  QString childCreation;
  bool pendingChild=false;
};
QString resourceLockDirectory(const QString &resource) {
  const auto root=sharedLockRoot();
  const auto directory=scopedPath(root+"/"+sha256(resourceIdentity(resource).toUtf8()),root);
  privateDirectory(directory); return directory;
}
ResourceLocks::ResourceLocks(QStringList resources, ResourceAccess access):impl_(std::make_unique<Impl>()) {
  QMap<QString,QString> ordered;
  for(const auto &resource:resources) ordered[resourceIdentity(resource)]=resource;
  for(auto it=ordered.cbegin();it!=ordered.cend();++it) {
    const auto directory=resourceLockDirectory(it.value());
    auto guard=std::make_unique<QLockFile>(scopedPath(directory+"/mutation.lock",directory));
    guard->setStaleLockTime(0);
    if(!guard->tryLock(0)) throw Error("E_LOCKED","Another Hub is using or changing this resource; retry after it stops");
    // Recheck link/alias identity under the guard before proceeding.
    if(resourceIdentity(it.value())!=it.key()) throw Error("E_PATH_OUTSIDE_ROOT","Shared resource identity changed while locking");
    scopedPath(directory,QFileInfo(directory).absolutePath());
    if(access==ResourceAccess::Mutation) {
      QSet<QString> uses;
      for(const auto &entry:QDir(directory).entryInfoList({"use-*.lock","use-*.lock.child.json"},QDir::Files|QDir::Hidden|QDir::System)) {
        auto lease=scopedPath(entry.absoluteFilePath(),directory);
        if(lease.endsWith(".child.json")) lease.chop(QString(".child.json").size());
        uses.insert(lease);
      }
      for(const auto &lease:uses) {
        const auto record=lease+".child.json";
        // A crashed Hub's child may still read its payload. Verify creation
        // identity before reclaiming the holder's stale QLockFile.
        if(QFileInfo::exists(record) && recordChildStillUses(record))
          throw Error("E_LOCKED","Another Hub child is still using this resource; stop its game before changing it");
        QLockFile use(scopedPath(lease,directory)); use.setStaleLockTime(0);
        if(!use.tryLock(0)) throw Error("E_LOCKED","Another Hub is using this resource; stop its game before changing it");
        if(QFileInfo::exists(record) && !QFile::remove(record))
          throw Error("E_LOCKED","Cannot reclaim finished child lease evidence");
        // The holder is gone and its recorded child is gone or a different
        // creation. Unlock removes the reclaimed temporary QLockFile.
      }
      impl_->held.push_back(std::move(guard));
    } else {
      const auto lease=scopedPath(directory+"/use-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".lock",directory);
      auto use=std::make_unique<QLockFile>(lease);
      use->setStaleLockTime(0);
      if(!use->tryLock(0)) throw Error("E_LOCKED","Cannot reserve shared resource use");
      impl_->childRecords << lease+".child.json";
      impl_->held.push_back(std::move(use));
      // Registration occurs under the mutation guard; readers coexist afterward.
    }
  }
}
ResourceLocks::~ResourceLocks() {
  // Also preserve a still-running child if a caller releases use prematurely.
  const bool retain=impl_->childPid>0 ? childStillUses(impl_->childPid,impl_->childCreation) : impl_->pendingChild;
  if(!retain) for(const auto &record:impl_->childRecords) QFile::remove(record);
}
void ResourceLocks::prepareChildLaunch() {
  if(impl_->childRecords.isEmpty()) return;
  if(impl_->childPid>0 || impl_->pendingChild)
    throw Error("E_PLAN_INVALID","Child launch evidence is already reserved");
  impl_->pendingChild=true;
  const Json record{{"format",1},{"phase","pending"}};
  // The caller must not spawn unless every durable write succeeds. A partial
  // failure is only cleared by the still-live holder after proving no spawn.
  for(const auto &path:impl_->childRecords) atomicWrite(path,QByteArray::fromStdString(record.dump()));
}
void ResourceLocks::clearPendingChildLaunch() {
  if(!impl_->pendingChild || impl_->childPid>0) return;
  impl_->pendingChild=false;
  for(const auto &path:impl_->childRecords) QFile::remove(path);
}
void ResourceLocks::trackChild(qint64 pid) {
  if(impl_->childRecords.isEmpty()) return;
  if(pid<=0) throw Error("E_PLAN_INVALID","Invalid tracked child process");
  const auto identity=childIdentity(pid);
  if(identity.state==ChildState::Gone) {clearPendingChildLaunch();return;}
  impl_->childPid=pid; impl_->childCreation=identity.creation;
  const Json record{{"format",1},{"child_pid",pid},{"child_identity",identity.creation.toStdString()}};
  // Empty creation means the query failed: keep the evidence conservative.
  for(const auto &path:impl_->childRecords) atomicWrite(path,QByteArray::fromStdString(record.dump()));
  impl_->pendingChild=false;
}
QString toolPayloadRoot(const QString &executable, const QString &toolId) {
  auto directory=QFileInfo(executable).absolutePath();
  for(auto parent=directory;;parent=QFileInfo(parent).absolutePath()) {
    const auto base=QFileInfo(parent).fileName(), container=QFileInfo(QFileInfo(parent).absolutePath()).fileName();
#ifdef Q_OS_WIN
    const bool matches=base.compare(toolId,Qt::CaseInsensitive)==0 && container.compare("emulators",Qt::CaseInsensitive)==0;
#else
    const bool matches=base==toolId && container=="emulators";
#endif
    if(matches) return scopedPath(parent,QFileInfo(parent).absolutePath());
    if(QFileInfo(parent).absolutePath()==parent) break;
  }
  return scopedPath(directory,QFileInfo(directory).absolutePath());
}
QString string(const Json &j, const char *key, const QString &fallback) {
  return j.contains(key) && j[key].is_string()
             ? QString::fromStdString(j[key].get<std::string>())
             : fallback;
}
QString sha256(const QByteArray &bytes) {
  return QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QByteArray readBytes(const QString &path) {
  scopedPath(path, QFileInfo(path).absolutePath());
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    throw Error("E_READ", "Cannot read " + path);
  return f.readAll();
}
QString hashFile(const QString &path) {
  scopedPath(path, QFileInfo(path).absolutePath());
  QFile f(path);
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!f.open(QIODevice::ReadOnly) || !hash.addData(&f))
    throw Error("E_READ", "Cannot hash " + path);
  return QString::fromLatin1(hash.result().toHex());
}
static void syncFile(QFileDevice &f) {
  if (!f.flush())
    throw Error("E_WRITE_DENIED", "Cannot flush file");
#ifdef Q_OS_WIN
  if (_commit(static_cast<int>(f.handle())) != 0)
    throw Error("E_WRITE_DENIED", "Cannot durably flush file");
#else
  if (::fsync(static_cast<int>(f.handle())) != 0)
    throw Error("E_WRITE_DENIED", "Cannot durably flush file");
#endif
}
void atomicWrite(const QString &path, const QByteArray &bytes) {
  scopedPath(path, QFileInfo(path).absolutePath());
  if (!QDir().mkpath(QFileInfo(path).absolutePath()))
    throw Error("E_WRITE_DENIED", "Cannot create parent folder");
  QSaveFile f(path);
  f.setDirectWriteFallback(false);
  if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size())
    throw Error("E_WRITE_DENIED", "Cannot write " + path);
  syncFile(f);
  if (!f.commit())
    throw Error("E_FILE_IN_USE", "Cannot replace " + path);
}
void atomicCopy(const QString &source, const QString &destination) {
  scopedPath(source, QFileInfo(source).absolutePath());
  scopedPath(destination, QFileInfo(destination).absolutePath());
  if (!QDir().mkpath(QFileInfo(destination).absolutePath()))
    throw Error("E_WRITE_DENIED", "Cannot create streamed copy folder");
  QFile input(source);
  QSaveFile output(destination);
  output.setDirectWriteFallback(false);
  if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
    throw Error("E_WRITE_DENIED", "Cannot open streamed copy");
  while (!input.atEnd()) {
    const auto chunk = input.read(128 * 1024);
    if (chunk.isEmpty() && input.error() != QFileDevice::NoError)
      throw Error("E_READ", "Streamed copy read failed");
    if (output.write(chunk) != chunk.size())
      throw Error("E_WRITE_DENIED", "Streamed copy write failed");
  }
  syncFile(output);
  if (!output.commit())
    throw Error("E_FILE_IN_USE", "Cannot replace streamed copy");
}
void durableAppend(const QString &path, const Json &record) {
  scopedPath(path, QFileInfo(path).absolutePath());
  QFile f(path);
  const auto bytes = QByteArray::fromStdString(record.dump()) + '\n';
  if (!f.open(QIODevice::WriteOnly | QIODevice::Append) ||
      f.write(bytes) != bytes.size())
    throw Error("E_WRITE_DENIED", "Cannot journal");
  syncFile(f);
}
static Json sorted(const Json &v) {
  if (v.is_number_float())
    throw Error("E_STATE_FORMAT", "State canonical form forbids floats");
  if (v.is_object()) {
    std::map<std::string, Json> keys;
    for (auto it = v.begin(); it != v.end(); ++it)
      keys.emplace(it.key(), sorted(it.value()));
    Json out = Json::object();
    for (const auto &[k, value] : keys)
      out[k] = value;
    return out;
  }
  if (v.is_array()) {
    Json out = Json::array();
    for (const auto &x : v)
      out.push_back(sorted(x));
    return out;
  }
  return v;
}
QByteArray canonicalJson(const Json &data) {
  // nlohmann uses short control escapes; canonical contract requires uXXXX.
  const auto raw = QByteArray::fromStdString(sorted(data).dump());
  QByteArray out;
  for (qsizetype i = 0; i < raw.size(); ++i) {
    if (raw[i] == '\\' && i + 1 < raw.size()) {
      const char c = raw[++i];
      const QMap<char, QByteArray> escapes{{'b', "\\u0008"},
                                           {'f', "\\u000c"},
                                           {'n', "\\u000a"},
                                           {'r', "\\u000d"},
                                           {'t', "\\u0009"}};
      out += escapes.contains(c) ? escapes[c] : QByteArray("\\") + c;
    } else
      out += raw[i];
  }
  return out;
}
static toml::table toTable(const Json &j);
static void insert(toml::table &t, const std::string &k, const Json &v) {
  if (v.is_object())
    t.insert(k, toTable(v));
  else if (v.is_array()) {
    toml::array a;
    for (const auto &x : v) {
      if (x.is_object())
        a.push_back(toTable(x));
      else if (x.is_string())
        a.push_back(x.get<std::string>());
      else if (x.is_boolean())
        a.push_back(x.get<bool>());
      else if (x.is_number_integer())
        a.push_back(x.get<int64_t>());
      else
        throw Error("E_STATE_FORMAT", "Unsupported state array value");
    }
    t.insert(k, std::move(a));
  } else if (v.is_string())
    t.insert(k, v.get<std::string>());
  else if (v.is_boolean())
    t.insert(k, v.get<bool>());
  else if (v.is_number_integer())
    t.insert(k, v.get<int64_t>());
  else
    throw Error("E_STATE_FORMAT", "Unsupported state value");
}
static toml::table toTable(const Json &j) {
  toml::table t;
  for (auto it = j.begin(); it != j.end(); ++it)
    insert(t, it.key(), it.value());
  return t;
}
void writeEnvelope(const QString &path, const Json &data) {
  if (QFileInfo::exists(path)) {
    QStringList recovered;
    readEnvelope(path, &recovered);
    // Recovery may have read .previous; do not overwrite it with corrupt main bytes.
    if (recovered.isEmpty()) atomicWrite(path + ".previous", readBytes(path));
  }
  const Json envelope{
      {"schema", 1},
      {"checksum", ("sha256:" + sha256(canonicalJson(data))).toStdString()},
      {"data", data}};
  std::ostringstream s;
  s << toTable(envelope);
  atomicWrite(path, QByteArray::fromStdString(s.str()));
}
Json readEnvelope(const QString &path, QStringList *warnings) {
  if (!QFileInfo::exists(path))
    return Json::object();
  for (int generation = 0; generation < 2; ++generation) {
    try {
      const auto j =
          CatalogLoader::parseToml(path + (generation ? ".previous" : ""));
      if (j.value("schema", 0) > 1)
        throw Error("E_SCHEMA_NEWER", "Newer state schema; read-only");
      if (j.value("schema", 0) != 1 || !j.contains("data") ||
          string(j, "checksum") != "sha256:" + sha256(canonicalJson(j["data"])))
        throw Error("E_STATE_CHECKSUM", "State checksum failed");
      if (generation && warnings)
        *warnings << "Recovered previous state generation";
      return j["data"];
    } catch (const Error &e) {
      if (e.code == "E_SCHEMA_NEWER")
        throw;
    } catch (const std::exception &) {
    }
  }
  throw Error("E_STATE_CHECKSUM",
              "Both state generations invalid; repair needed");
}
void validateRelative(const QString &input) {
  QString path = input;
  path.replace('\\', '/');
  if (path.isEmpty() || path.contains(QChar(0)) || path.startsWith('/') ||
      QRegularExpression("^[A-Za-z]:").match(path).hasMatch())
    throw Error("E_ARCHIVE_UNSAFE", "Absolute or empty path");
  static const QRegularExpression reserved(
      "^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\..*)?$",
      QRegularExpression::CaseInsensitiveOption);
  for (const auto &part : path.split('/', Qt::SkipEmptyParts)) {
    if (part == ".." || part.contains(':') || part.endsWith('.') ||
        part.endsWith(' ') || reserved.match(part).hasMatch())
      throw Error("E_PATH_OUTSIDE_ROOT", "Unsafe name: " + part);
  }
}
QString scopedPath(const QString &input, const QString &root) {
  QString path = input;
  path.replace('\\', '/');
  const auto base = QDir(root).absolutePath();
  if (!QDir::isAbsolutePath(path))
    path = QDir(base).filePath(path);
  for (QString parent = QDir::cleanPath(path); !parent.isEmpty();) {
    const QFileInfo info(parent);
    if (info.isSymLink() || info.isJunction())
      throw Error("E_PATH_OUTSIDE_ROOT",
                  "Symbolic link/junction in target path refused");
    const auto up = info.absolutePath();
    if (up == parent)
      break;
    parent = up;
  }
  if (path.startsWith("//") ||
      QRegularExpression("^[A-Za-z]:[^/]").match(path).hasMatch())
    throw Error("E_PATH_OUTSIDE_ROOT", "UNC or drive-relative path");
  if (!QDir::isAbsolutePath(path))
    path = QDir(base).filePath(path);
  const auto rel = QDir(base).relativeFilePath(path);
  if (rel == ".") {
    if (QFileInfo(base).isSymLink())
      throw Error("E_PATH_OUTSIDE_ROOT", "Symbolic root refused");
    return base;
  }
  validateRelative(rel);
  const auto cleaned = QDir::cleanPath(path);
  QString ancestor =
      QFileInfo::exists(cleaned) ? cleaned : QFileInfo(cleaned).absolutePath();
  while (!QFileInfo::exists(ancestor)) {
    const auto up = QFileInfo(ancestor).absolutePath();
    if (up == ancestor)
      break;
    ancestor = up;
  }
  const auto real = QFileInfo(ancestor).canonicalFilePath();
  const auto realRoot = QFileInfo(base).canonicalFilePath();
#ifdef Q_OS_WIN
  constexpr auto pathCase = Qt::CaseInsensitive;
#else
  constexpr auto pathCase = Qt::CaseSensitive;
#endif
  if (!realRoot.isEmpty() && !(real.compare(realRoot, pathCase) == 0 ||
                               real.startsWith(realRoot + "/", pathCase)))
    throw Error("E_PATH_OUTSIDE_ROOT", "Parent junction escapes allowed root");
  if (QFileInfo(cleaned).isSymLink())
    throw Error("E_PATH_OUTSIDE_ROOT", "Symbolic target refused");
  return cleaned;
}
QString stateBase(const Request &r) {
  validateRelative(r.gameId);
  validateRelative(r.variantId);
  if (r.gameId.contains('/') || r.variantId.contains('/') ||
      r.gameId.contains('\\') || r.variantId.contains('\\'))
    throw Error("E_PLAN_INVALID", "IDs must be one path component");
  for (const auto *folder :
       {"user/state", "user/logs", "user/cache", "user/backups",
        "user/emulator-profiles", "user/state/locks", "user/state/staging",
        "user/state/backups", "user/logs/install", "user/cache/artifacts"})
    scopedPath(QString::fromLatin1(folder), r.root);
  return scopedPath("user/state/installs/" + r.gameId + "/" + r.variantId,
                    r.root);
}
Json dotted(const Json &value, const QString &path) {
  const Json *at = &value;
  for (const auto &part : path.split('.')) {
    const auto k = part.toStdString();
    if (!at->is_object() || !at->contains(k))
      throw Error("E_EXPAND_UNKNOWN", "Unknown variable: " + path);
    at = &(*at)[k];
  }
  return *at;
}
QString expand(const QString &text, const QMap<QString, QString> &vars,
               bool path) {
  QString out;
  const auto escaped = text;
  int pos = 0;
  static const QSet<QString> env{
      "LOCALAPPDATA",      "APPDATA",       "USERPROFILE", "ProgramFiles",
      "ProgramFiles(x86)", "XDG_DATA_HOME", "HOME"};
  while (pos < escaped.size()) {
    if (escaped.mid(pos, 3) == "$${") {
      out += "${";
      pos += 3;
      continue;
    }
    if (escaped.mid(pos, 2) != "${") {
      out += escaped[pos++];
      continue;
    }
    const auto end = escaped.indexOf('}', pos + 2);
    if (end < 0)
      throw Error("E_EXPAND_UNKNOWN", "Unclosed variable");
    const auto key = escaped.mid(pos + 2, end - pos - 2);
    if (key.startsWith("env:")) {
      const auto name = key.mid(4);
      if (!path || !env.contains(name))
        throw Error("E_EXPAND_UNKNOWN", "Environment read forbidden");
      out += qEnvironmentVariable(name.toUtf8().constData());
    } else {
      if (!vars.contains(key))
        throw Error("E_EXPAND_UNKNOWN", "Unknown variable: " + key);
      out += vars[key];
    }
    pos = end + 1;
  }
  return out;
}
Json expandJson(const Json &j, const QMap<QString, QString> &vars,
                const QString &key) {
  static const QSet<QString> literal{"url",    "repo",    "tag",   "asset",
                                     "sha256", "package", "hosts", "version"};
  if (j.is_string()) {
    const auto s = QString::fromStdString(j.get<std::string>());
    if (literal.contains(key)) {
      if (s.contains("${"))
        throw Error("E_PLAN_INVALID", "Templated security field");
      return j;
    }
    return expand(
               s, vars,
               QSet<QString>{"from", "to", "file", "exe", "cwd"}.contains(key))
        .toStdString();
  }
  Json out = j;
  if (j.is_object())
    for (auto it = j.begin(); it != j.end(); ++it)
      out[it.key()] =
          expandJson(it.value(), vars, QString::fromStdString(it.key()));
  if (j.is_array())
    for (size_t i = 0; i < j.size(); ++i)
      out[i] = expandJson(j[i], vars, key);
  return out;
}
namespace {
void validateGuard(const Json &guard) {
  if (!guard.is_object() || !guard.contains("format") ||
      !guard["format"].is_number_integer() || guard["format"] != 1)
    throw Error("E_CONTENT_GUARD", "Content guard format 1 is required");
  for (const auto *key : {"extensions", "names", "sha256"}) {
    if (!guard.contains(key) || !guard[key].is_array())
      throw Error("E_CONTENT_GUARD", "Content guard lists are required");
    for (const auto &value : guard[key]) {
      if (!value.is_string() || value.get<std::string>().empty())
        throw Error("E_CONTENT_GUARD", "Content guard list entry is invalid");
      const auto text = QString::fromStdString(value.get<std::string>());
      if (QString(key) == "extensions" &&
          !QRegularExpression("^[a-zA-Z0-9]+$").match(text).hasMatch())
        throw Error("E_CONTENT_GUARD", "Content guard extension is invalid");
      if (QString(key) == "sha256" &&
          !QRegularExpression("^[a-fA-F0-9]{64}$").match(text).hasMatch())
        throw Error("E_CONTENT_GUARD", "Content guard hash is invalid");
    }
  }
  if (guard["extensions"].empty())
    throw Error("E_CONTENT_GUARD", "Content guard extensions cannot be empty");
}
} // namespace
Json loadContentGuard(const QString &catalogRoot) {
  try {
    const auto guard = CatalogLoader::parseToml(catalogRoot + "/data/content-guard.toml");
    validateGuard(guard);
    return guard;
  } catch (const std::exception &) {
    throw Error("E_CONTENT_GUARD", "Content guard is missing or malformed; install is blocked");
  }
}
void contentGuard(const QString &name, const Json &guard) {
  validateGuard(guard);
  const auto file = QFileInfo(name).fileName().toLower(),
             ext = QFileInfo(name).suffix().toLower();
  for (const auto &extension : guard["extensions"])
    if (ext == QString::fromStdString(extension.get<std::string>()).toLower())
      throw Error("E_CONTENT_GUARD", "Game content extension refused");
  for (const auto &n : guard["names"])
    if (file == QString::fromStdString(n.get<std::string>()).toLower())
      throw Error("E_CONTENT_GUARD", "Known game/BIOS file refused");
}
bool verifyPe64(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return false;
  const auto h = f.read(64);
  if (h.size() < 64 || !h.startsWith("MZ"))
    return false;
  const auto off = qFromLittleEndian<quint32>(
      reinterpret_cast<const uchar *>(h.constData() + 60));
  if (off > static_cast<quint64>(f.size()) || !f.seek(off))
    return false;
  const auto pe = f.read(26);
  return pe.size() == 26 && pe.left(4) == QByteArray("PE\0\0", 4) &&
         qFromLittleEndian<quint16>(
             reinterpret_cast<const uchar *>(pe.constData() + 4)) == 0x8664 &&
         qFromLittleEndian<quint16>(
             reinterpret_cast<const uchar *>(pe.constData() + 24)) == 0x20b;
}
} // namespace ac::install
