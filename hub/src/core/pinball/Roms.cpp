// SPDX-License-Identifier: GPL-3.0-only
#include "Roms.h"
#include <QCryptographicHash>
#include <QByteArrayView>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QtEndian>
#include <algorithm>
namespace ac::pinball {
namespace {
constexpr int maxDepth=8,maxEntries=20000,maxAliases=4096;
constexpr qint64 maxZipMetadata=64*1024*1024;
bool link(const QFileInfo &f){return f.isSymLink()||f.isJunction();}
QString romName(QString name){name=name.trimmed().toCaseFolded();static const QRegularExpression valid("^[a-z0-9_]{1,64}$");return valid.match(name).hasMatch()?name:QString();}
bool romPayload(const QFileInfo &f){
 if(!f.isFile()||f.size()<=0||f.suffix().compare("zip",Qt::CaseInsensitive)==0)return false;
 static const QSet<QString> excluded{"txt","md","nfo","ini","cfg","json","xml","png","jpg","jpeg","webp","vpx","pdf","url","lnk"};
 const auto stem=f.completeBaseName().toCaseFolded();return !excluded.contains(f.suffix().toCaseFolded())&&!stem.startsWith("readme")&&!stem.startsWith("license")&&!stem.startsWith("copying");
}
quint16 u16(const QByteArray &b,qsizetype p){return qFromLittleEndian<quint16>(b.constData()+p);}
quint32 u32(const QByteArray &b,qsizetype p){return qFromLittleEndian<quint32>(b.constData()+p);}
void stamp(QCryptographicHash &hash,const QString &path,const QFileInfo &f){hash.addData(path.toCaseFolded().toUtf8());hash.addData(QByteArrayView("\0",1));hash.addData(QByteArray::number(f.size()));hash.addData(QByteArrayView("\0",1));hash.addData(QByteArray::number(f.lastModified().toMSecsSinceEpoch()));hash.addData(QByteArrayView("\n",1));}
// A differently named ZIP can identify a ROM only when all nonempty files are
// under the same explicitly named root directory. Member bytes are not read.
QString zipRoot(const QFileInfo &info,qint64 &budget,bool &complete){
 const auto unavailable=[&]()->QString{complete=false;return {};};
 QFile f(info.absoluteFilePath());if(!f.open(QIODevice::ReadOnly)||f.size()<22)return unavailable();
 // Most archives have no ZIP comment: read their fixed footer first so a large
 // collection does not spend the metadata budget on compressed ROM payloads.
 if(budget<22||!f.seek(f.size()-22))return unavailable();budget-=22;auto tail=f.read(22);if(tail.size()!=22)return unavailable();
 if(u32(tail,0)!=0x06054b50U||u16(tail,20)!=0){const qint64 tailSize=std::min<qint64>(f.size(),65557);if(tailSize>budget)return unavailable();budget-=tailSize;
  if(!f.seek(f.size()-tailSize))return unavailable();tail=f.read(tailSize);if(tail.size()!=tailSize)return unavailable();
 }
 qsizetype end=-1;for(qsizetype p=tail.size()-22;p>=0;--p){if(u32(tail,p)==0x06054b50U&&p+22+u16(tail,p+20)==tail.size()){end=p;break;}}
 if(end<0||u16(tail,end+4)||u16(tail,end+6)||u16(tail,end+8)!=u16(tail,end+10))return unavailable();
 const quint16 count=u16(tail,end+10);const quint32 size=u32(tail,end+12),offset=u32(tail,end+16);
 if(!count||count>4096||size>4*1024*1024||quint64(offset)+size>quint64(f.size())||!f.seek(offset))return unavailable();
 if(size>budget){complete=false;return {};}budget-=size;
 const auto directory=f.read(size);if(directory.size()!=qsizetype(size))return unavailable();
 qsizetype p=0;QString root;int files=0;bool singleRoot=true;
 for(int i=0;i<count;++i){
  if(p+46>directory.size()||u32(directory,p)!=0x02014b50U)return unavailable();
  const quint16 flags=u16(directory,p+8),method=u16(directory,p+10);const quint32 packed=u32(directory,p+20),unpacked=u32(directory,p+24);
  const quint16 nameSize=u16(directory,p+28),extra=u16(directory,p+30),comment=u16(directory,p+32);
  if(flags&1||(method!=0&&method!=8)||p+46+nameSize+extra+comment>directory.size())return unavailable();
  const auto raw=directory.mid(p+46,nameSize);const auto name=(flags&0x800)?QString::fromUtf8(raw):QString::fromLatin1(raw);
  p+=46+nameSize+extra+comment;
  if(name.contains('\\')||name.startsWith('/')||name.contains(":")||name.contains("//")||name.contains("../")||name.startsWith("../"))return unavailable();
  if(name.endsWith('/'))continue;
  if(!packed||!unpacked)continue;
  const auto slash=name.indexOf('/');if(slash<=0||slash==name.size()-1){singleRoot=false;continue;}
  const auto next=romName(name.left(slash));if(next.isEmpty()||(!root.isEmpty()&&next!=root)){singleRoot=false;continue;}
  root=next;++files;
 }
 if(p!=directory.size())return unavailable();
 return singleRoot&&files?root:QString();
}
void readAliases(const QString &pinmameFolder,RomInventory &out,QCryptographicHash &hash){
 if(pinmameFolder.isEmpty())return;
 hash.addData(QFileInfo(pinmameFolder).absoluteFilePath().toCaseFolded().toUtf8());hash.addData(QByteArrayView("\n",1));
 const QFileInfo base(pinmameFolder);if(!base.isDir()||link(base)){out.complete=false;return;}
 QFileInfo alias(QDir(pinmameFolder).filePath("VPMAlias.txt"));
 if(!alias.exists())return;
 if(!alias.isFile()||link(alias)||alias.size()>1024*1024){out.complete=false;return;}
 QFile file(alias.absoluteFilePath());if(!file.open(QIODevice::ReadOnly)){out.complete=false;return;}
 const auto content=file.readAll();hash.addData("aliases\n");hash.addData(QCryptographicHash::hash(content,QCryptographicHash::Sha256));
 if(content.size()!=alias.size()){out.complete=false;return;}
 const auto lines=QString::fromUtf8(content).split('\n');int mappings=0;
 for(auto line:lines){line=line.trimmed();if(line.startsWith(QChar(0xfeff)))line.remove(0,1);if(line.isEmpty()||line.startsWith('#')||line.startsWith(';')||line.startsWith("//"))continue;
  if(++mappings>maxAliases){out.complete=false;return;}
  const auto parts=line.split(',');if(parts.size()!=2){out.complete=false;continue;}
  const auto aliasName=romName(parts[0]),target=romName(parts[1]);
  if(aliasName.isEmpty()){out.complete=false;continue;}
  if(target.isEmpty()||aliasName==target){out.aliases.remove(aliasName);out.unresolvedAliases.insert(aliasName);continue;}
  if(out.unresolvedAliases.contains(aliasName))continue;
  if(out.aliases.contains(aliasName)&&out.aliases[aliasName]!=target){out.aliases.remove(aliasName);out.unresolvedAliases.insert(aliasName);continue;}
  out.aliases.insert(aliasName,target);
 }
}
QString resolve(const QString &name,const RomInventory &inventory,bool &safe){
 QString current=name;QSet<QString> seen;
 while(inventory.aliases.contains(current)){
  if(seen.contains(current)||inventory.unresolvedAliases.contains(current)){safe=false;return {};}
  seen.insert(current);current=inventory.aliases.value(current);
 }
 if(inventory.unresolvedAliases.contains(current)){safe=false;return {};}
 return current;
}
bool relatedArchive(const QString &name,const QSet<QString> &present){
 // A revision/clone may use a merged parent archive. Names alone cannot prove
 // that coverage, but a related installed archive prevents a definite missing verdict.
 static const QRegularExpression revision("_[a-z]*[0-9]+[a-z0-9_]*$"),numbered("[0-9]+[a-z]*$"),letters("^[a-z]{1,2}$");
 const auto family=[&](QString s){s.remove(revision);s.remove(numbered);return s;};const auto base=family(name);
 for(const auto &other:present){if(base.size()>=2&&base==family(other))return true;
  if(other.size()>=3&&name.startsWith(other)&&letters.match(name.mid(other.size())).hasMatch())return true;
  if(name.size()>=3&&other.startsWith(name)&&letters.match(other.mid(name.size())).hasMatch())return true;
 }return false;
}
}
RomInventory collectRoms(const QString &romFolder,const QString &pinmameFolder){
 RomInventory out;QCryptographicHash hash(QCryptographicHash::Sha256);hash.addData("rom-inventory-1\n");
 if(!romFolder.isEmpty()){
  const QFileInfo base(romFolder);hash.addData(base.absoluteFilePath().toCaseFolded().toUtf8());hash.addData(QByteArrayView("\n",1));out.configured=base.isDir()&&!link(base);
  if(out.configured){
   out.complete=true;QList<QPair<QString,int>> pending{{base.absoluteFilePath(),0}};int entries=0;QSet<QString> nonemptyDirs; qint64 zipBudget=maxZipMetadata;
   while(!pending.isEmpty()){
    const auto [folder,depth]=pending.takeFirst();if(!QFileInfo(folder).isReadable()){out.complete=false;continue;}
    QDirIterator it(folder,QDir::Files|QDir::Dirs|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot);
    QFileInfoList children;while(it.hasNext()&&entries+children.size()<maxEntries)children<<it.nextFileInfo();
    if(it.hasNext()){out.complete=false;pending.clear();break;}
    std::sort(children.begin(),children.end(),[](const QFileInfo &a,const QFileInfo &b){const int folded=a.fileName().compare(b.fileName(),Qt::CaseInsensitive);return folded?folded<0:a.fileName()<b.fileName();});
    entries+=int(children.size());
    for(const auto &f:children){
     if(link(f)){out.complete=false;continue;}
     const auto relative=QDir(base.absoluteFilePath()).relativeFilePath(f.absoluteFilePath());stamp(hash,relative,f);
     if(romPayload(f)){auto parts=relative.split('/');if(parts.size()>=2){const auto name=romName(parts[parts.size()-2]);if(!name.isEmpty())nonemptyDirs.insert(name);for(int i=0;i<parts.size()-2;++i){const auto ancestor=romName(parts[i]);if(!ancestor.isEmpty())out.possible.insert(ancestor);}}}
     if(f.isFile()&&f.suffix().compare("zip",Qt::CaseInsensitive)==0&&f.size()>0){
      bool archiveReadable=true;const auto rooted=zipRoot(f,zipBudget,archiveReadable);
      if(!archiveReadable){out.complete=false;continue;}
      const auto stem=romName(f.completeBaseName());if(!stem.isEmpty())out.present.insert(stem);
      if(!rooted.isEmpty()&&rooted!=stem)out.possible.insert(rooted);
     }
     if(f.isDir()){
      if(depth<maxDepth)pending<<qMakePair(f.absoluteFilePath(),depth+1);else out.complete=false;
     }
    }
   }
   out.present.unite(nonemptyDirs);if(out.present.isEmpty())out.complete=false;
  }
 }
 if(pinmameFolder.isEmpty())out.complete=false;
 readAliases(pinmameFolder,out,hash);
 QStringList possible=out.possible.values();std::sort(possible.begin(),possible.end());for(const auto &name:possible){hash.addData(name.toUtf8());hash.addData(QByteArrayView("\n",1));}
 out.fingerprint={{"romInventory",QString::fromLatin1(hash.result().toHex())},{"romEntries",out.present.size()},{"romPossible",out.possible.size()},{"romConfigured",out.configured},{"romComplete",out.complete}};
 return out;
}
QVariantMap applyRomState(QVariantMap row,const RomInventory &inventory){
 if(!row.value("error").toString().isEmpty()){row["romState"]="unknown";return row;}
 if(row.value("romState")=="needs no ROM")return row;
 QStringList candidates=row.value("romCandidates").toStringList();const auto oldRom=row.value("rom").toString();
 if(candidates.isEmpty()&&!oldRom.isEmpty())candidates<<oldRom;
 if(candidates.isEmpty()){row["romState"]="unknown";return row;}
 const bool certain=row.value("romCandidatesComplete",true).toBool();
 const bool pinmame=row.value("romRequiresPinmame").toBool()||(!oldRom.isEmpty()&&row.value("romState")=="ROM missing");
 int available=0;bool safe=true,alternate=false,related=false;QString unsafeReason;
 for(const auto &candidate:candidates){
  if(!safe)break;
  const auto name=romName(candidate);if(name.isEmpty()){safe=false;unsafeReason="The ROM name is unsafe or unsupported";continue;}
  const auto resolved=resolve(name,inventory,safe);if(!safe){unsafeReason="The ROM alias is ambiguous, cyclic, or unsafe";continue;}
  if(resolved!=name){if(inventory.present.contains(resolved))++available;else if(inventory.present.contains(name)){safe=false;unsafeReason="An alias ZIP exists, but the mapped ROM target is absent";}}
  else if(inventory.present.contains(name))++available;
  if(inventory.possible.contains(resolved)||inventory.possible.contains(name))alternate=true;
  if(!inventory.present.contains(resolved)&&relatedArchive(resolved,inventory.present))related=true;
 }
 if(!safe){row["romState"]="unknown";row["romReason"]=unsafeReason;return row;}
 if(!inventory.configured){row["romState"]="unknown";row["romReason"]="ROM folder is not configured or readable";return row;}
 if(!certain||!pinmame){row["romState"]="unknown";if(row.value("romReason").toString().isEmpty())row["romReason"]="ROM evidence is incomplete";return row;}
 if(available==candidates.size()){row["romState"]="ready";row.remove("romReason");return row;}
 if(available){row["romState"]="unknown";row["romReason"]="Only some possible PinMAME ROMs are installed";return row;}
 if(alternate){row["romState"]="unknown";row["romReason"]="ROM files may exist in a nested or differently named location, but searchability is unverified";return row;}
 if(related){row["romState"]="unknown";row["romReason"]="A related ROM archive is installed; exact version or merged-set coverage is unverified";return row;}
 if(!inventory.complete){row["romState"]="unknown";row["romReason"]="ROM folder is empty or could not be fully inspected";return row;}
 row["romState"]="ROM missing";row["romReason"]="No certain PinMAME ROM candidate was found";return row;
}
}
