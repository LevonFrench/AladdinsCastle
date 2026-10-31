// SPDX-License-Identifier: GPL-3.0-only
#include "RomList.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <windows.h>
#include <oleauto.h>
#endif
namespace ac::pinball {
namespace {
QString setName(QString name){name=name.trimmed().toCaseFolded();static const QRegularExpression valid("^[a-z0-9_]{1,64}$");return valid.match(name).hasMatch()?name:QString();}
constexpr int maxSetsPerCall=400,maxRomsPerSet=256;
#ifdef Q_OS_WIN
// Owns one COM interface pointer.
template<class T> struct Com {
 T *p=nullptr;
 Com()=default;Com(const Com&)=delete;Com &operator=(const Com&)=delete;
 ~Com(){if(p)p->Release();}
};
// Owns one VARIANT.
struct Var {
 VARIANT v;
 Var(){VariantInit(&v);}Var(const Var&)=delete;Var &operator=(const Var&)=delete;
 ~Var(){VariantClear(&v);}
};
HRESULT invoke(IDispatch *object,DISPID id,VARIANT *result,VARIANT *argument=nullptr,WORD flags=DISPATCH_PROPERTYGET){
 DISPPARAMS params{argument,nullptr,argument?1u:0u,0u};
 return object->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,flags,&params,result,nullptr,nullptr);
}
HRESULT property(IDispatch *object,const wchar_t *name,VARIANT *result){
 DISPID id=0;auto *n=const_cast<LPOLESTR>(name);
 const auto hr=object->GetIDsOfNames(IID_NULL,&n,1,LOCALE_USER_DEFAULT,&id);
 return FAILED(hr)?hr:invoke(object,id,result);
}
// The interface a VARIANT holds, with its own reference, or null.
IDispatch *dispatchOf(const VARIANT &v){if(v.vt!=VT_DISPATCH||!v.pdispVal)return nullptr;v.pdispVal->AddRef();return v.pdispVal;}
QString text(IDispatch *object,const wchar_t *name){
 Var v;if(FAILED(property(object,name,&v.v))||FAILED(VariantChangeType(&v.v,&v.v,0,VT_BSTR))||!v.v.bstrVal)return {};
 return QString::fromWCharArray(v.v.bstrVal,int(SysStringLen(v.v.bstrVal)));
}
quint32 number(IDispatch *object,const wchar_t *name){
 Var v;if(FAILED(property(object,name,&v.v))||FAILED(VariantChangeType(&v.v,&v.v,0,VT_I4)))return 0;
 return quint32(v.v.lVal);
}
QString registryText(HKEY root,const QString &key,const wchar_t *value){
 wchar_t buffer[2048];DWORD size=sizeof(buffer);
 if(RegGetValueW(root,reinterpret_cast<const wchar_t*>(key.utf16()),value,RRF_RT_REG_SZ|RRF_RT_REG_EXPAND_SZ,nullptr,buffer,&size)!=ERROR_SUCCESS)return {};
 return QString::fromWCharArray(buffer);
}
QString folderKey(const QString &path){return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toCaseFolded();}
// Games.Item(name): VPinMAME declares its key as [in] VARIANT*, so it is passed by reference, then by value.
IDispatch *game(IDispatch *games,const QString &set){
 Var key;key.v.vt=VT_BSTR;key.v.bstrVal=SysAllocString(reinterpret_cast<const wchar_t*>(set.utf16()));
 VARIANT reference;VariantInit(&reference);reference.vt=VT_VARIANT|VT_BYREF;reference.pvarVal=&key.v;
 {Var result;if(SUCCEEDED(invoke(games,DISPID_VALUE,&result.v,&reference)))if(auto *found=dispatchOf(result.v))return found;}
 Var result;if(SUCCEEDED(invoke(games,DISPID_VALUE,&result.v,&key.v)))return dispatchOf(result.v);
 return nullptr;
}
std::optional<ExpectedSet> setOf(IDispatch *games,const QString &set){
 Com<IDispatch> found;found.p=game(games,set);if(!found.p)return std::nullopt;
 if(setName(text(found.p,L"Name"))!=set)return std::nullopt; // an unknown name must not stand for another set
 ExpectedSet out;out.cloneOf=setName(text(found.p,L"CloneOf"));
 Com<IDispatch> roms;{Var v;if(FAILED(property(found.p,L"Roms",&v.v)))return std::nullopt;roms.p=dispatchOf(v.v);}if(!roms.p)return std::nullopt;
 Var enumerator;if(FAILED(invoke(roms.p,DISPID_NEWENUM,&enumerator.v,nullptr,DISPATCH_PROPERTYGET|DISPATCH_METHOD)))return std::nullopt;
 if(enumerator.v.vt!=VT_UNKNOWN&&enumerator.v.vt!=VT_DISPATCH)return std::nullopt;
 IUnknown *unknown=enumerator.v.vt==VT_UNKNOWN?enumerator.v.punkVal:enumerator.v.pdispVal;if(!unknown)return std::nullopt;
 Com<IEnumVARIANT> each;if(FAILED(unknown->QueryInterface(IID_IEnumVARIANT,reinterpret_cast<void**>(&each.p)))||!each.p)return std::nullopt;
 for(int i=0;i<maxRomsPerSet;++i){
  Var item;ULONG fetched=0;if(each.p->Next(1,&item.v,&fetched)!=S_OK||fetched!=1)break;
  Com<IDispatch> rom;rom.p=dispatchOf(item.v);if(!rom.p)continue;
  ExpectedRom r;r.name=text(rom.p,L"Name");r.length=number(rom.p,L"ExpLength");r.crc=number(rom.p,L"ExpChecksum");r.flags=number(rom.p,L"Flags");
  if(!r.name.isEmpty())out.roms<<r;
 }
 return out;
}
#endif
}

ExpectedSets expectedSetsFromJson(const QByteArray &json){
 ExpectedSets out;const auto sets=QJsonDocument::fromJson(json).object().value("sets").toObject();
 for(auto it=sets.begin();it!=sets.end();++it){
  const auto name=setName(it.key());if(name.isEmpty())continue;
  const auto o=it.value().toObject();ExpectedSet set;set.cloneOf=setName(o.value("cloneOf").toString());
  for(const auto &value:o.value("roms").toArray()){const auto r=value.toObject();
   set.roms<<ExpectedRom{r.value("name").toString(),quint32(r.value("length").toDouble()),quint32(r.value("crc").toDouble()),quint32(r.value("flags").toDouble())};}
  out.insert(name,set);
 }
 return out;
}

QByteArray expectedSetsToJson(const ExpectedSets &sets,const QStringList &unknown){
 QJsonObject all;
 for(auto it=sets.cbegin();it!=sets.cend();++it){
  QJsonArray roms;for(const auto &r:it.value().roms)roms<<QJsonObject{{"name",r.name},{"length",double(r.length)},{"crc",double(r.crc)},{"flags",double(r.flags)}};
  all.insert(it.key(),QJsonObject{{"cloneOf",it.value().cloneOf},{"roms",roms}});
 }
 return QJsonDocument(QJsonObject{{"source","VPinMAME"},{"sets",all},{"unknown",QJsonArray::fromStringList(unknown)}}).toJson(QJsonDocument::Compact);
}

ExpectedSets expectedRomSets(const QStringList &sets,const QString &pinmameFolder,QString &why){
 const auto fixture=qEnvironmentVariable("AC_TEST_PINMAME_ROM_LIST");
 if(!fixture.isEmpty()){QFile file(fixture);if(!file.open(QIODevice::ReadOnly)){why="The test ROM list cannot be read.";return {};}return expectedSetsFromJson(file.readAll());}
 if(sets.isEmpty())return {};
#ifdef Q_OS_WIN
 if(pinmameFolder.isEmpty()){why="No VPinMAME folder is known, so ROM files are checked by name only.";return {};}
 const auto tool=QDir(QCoreApplication::applicationDirPath()).filePath("hubtool.exe");
 if(!QFileInfo(tool).isFile()){why="hubtool.exe is not beside the Hub, so ROM files are checked by name only.";return {};}
 ExpectedSets out;
 for(qsizetype at=0;at<sets.size();at+=maxSetsPerCall){
  QProcess process;process.setProgram(tool);process.setArguments(QStringList{"pinmame-roms",QDir::toNativeSeparators(pinmameFolder)}+sets.mid(at,maxSetsPerCall));
  process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments){arguments->flags|=CREATE_NO_WINDOW|BELOW_NORMAL_PRIORITY_CLASS;});
  process.start();
  if(!process.waitForStarted(10000)){why="The ROM list helper did not start.";return {};}
  if(!process.waitForFinished(60000)){process.kill();process.waitForFinished(5000);why="VPinMAME did not list the ROM files in time.";return {};}
  if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0){why=QString::fromUtf8(process.readAllStandardError()).simplified().left(300);if(why.isEmpty())why="VPinMAME could not list the ROM files.";return {};}
  const auto part=expectedSetsFromJson(process.readAllStandardOutput());for(auto it=part.cbegin();it!=part.cend();++it)out.insert(it.key(),it.value());
 }
 return out;
#else
 Q_UNUSED(pinmameFolder);why="ROM files are checked with VPinMAME, which is Windows only; here they are checked by name only.";return {};
#endif
}

ExpectedSets vpinmameRomSets(const QStringList &sets,const QString &pinmameFolder,QStringList &unknown,QString &error){
#ifdef Q_OS_WIN
 CLSID clsid;if(FAILED(CLSIDFromProgID(L"VPinMAME.Controller",&clsid))){error="VPinMAME is not registered on this PC.";return {};}
 wchar_t id[64];if(!StringFromGUID2(clsid,id,64)){error="VPinMAME's registration cannot be read.";return {};}
 const auto dll=registryText(HKEY_CLASSES_ROOT,QString("CLSID\\%1\\InprocServer32").arg(QString::fromWCharArray(id)),nullptr);
 if(dll.isEmpty()||folderKey(QFileInfo(dll).absolutePath())!=folderKey(pinmameFolder)){error="The registered VPinMAME is not the one in the selected Visual Pinball folder.";return {};}
 HKEY settings=nullptr;
 if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Freeware\\Visual PinMame\\default",0,KEY_QUERY_VALUE,&settings)!=ERROR_SUCCESS){error="VPinMAME has not been set up yet: start a table with it once.";return {};}
 RegCloseKey(settings);
 const auto initialised=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);const bool uninitialise=SUCCEEDED(initialised);
 ExpectedSets out;
 {
  Com<IDispatch> controller;
  if(FAILED(CoCreateInstance(clsid,nullptr,CLSCTX_INPROC_SERVER,IID_IDispatch,reinterpret_cast<void**>(&controller.p)))||!controller.p){error="VPinMAME could not be created.";}
  else{
   Com<IDispatch> games;{Var v;if(SUCCEEDED(property(controller.p,L"Games",&v.v)))games.p=dispatchOf(v.v);}
   if(!games.p)error="VPinMAME did not give its list of games.";
   else for(const auto &raw:sets){
    const auto set=setName(raw);if(set.isEmpty())continue;
    if(const auto found=setOf(games.p,set))out.insert(set,*found);else unknown<<set;
   }
  }
 }
 if(uninitialise)CoUninitialize();
 return out;
#else
 Q_UNUSED(sets);Q_UNUSED(pinmameFolder);Q_UNUSED(unknown);error="VPinMAME is Windows only.";return {};
#endif
}
}
