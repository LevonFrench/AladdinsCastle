// SPDX-License-Identifier: GPL-3.0-only
#include "UiController.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <algorithm>
namespace ac {
namespace {
QString str(const Json &j,const char *key){return j.contains(key)&&j[key].is_string()?QString::fromStdString(j[key].get<std::string>()):QString();}
QString eventText(const QVariantMap &e){auto kind=e.value("kind").toString(), text=e.value("text").toString();if(kind=="step")return QString("--- [%1/%2] %3 ---").arg(e.value("step").toInt()).arg(e.value("total").toInt()).arg(text);const QMap<QString,QString> prefixes{{"ok"," [OK] "},{"warn"," [!!] "},{"fail"," [XX] "},{"work"," [..] "},{"detail","  "},{"prompt"," >>> "},{"done"," >>> "}};return prefixes.value(kind,"  ")+text;}
}
UiController::UiController(GameListModel *games,FilterSortModel *filter,UiSettings *settings,QObject *parent):QObject(parent),m_games(games),m_filter(filter),m_settings(settings){
 connect(filter,&FilterSortModel::facetsChanged,this,&UiController::facetsChanged);
 connect(filter,&FilterSortModel::visibleCountChanged,this,&UiController::facetsChanged);
 connect(games,&QAbstractItemModel::dataChanged,this,[this]{emit detailChanged();emit facetsChanged();});
 connect(settings,&UiSettings::error,this,&UiController::message);
 connect(settings,&UiSettings::gameSaved,this,&UiController::writeConfigRequested);
}
QString UiController::vrLabel(int badge)const{const QStringList labels{"FLAT","TRUE 3D","THEATRE","PLANNED"};return labels.value(badge,"FLAT");}
QString UiController::primaryLabel(const QString &id)const{auto g=m_games->find(id);if(!g)return "No setup yet";if(g->roles.value("state").toInt()==4){const auto *v=variant(*g);return v?v->title:"Play";}if(g->roles.value("state").toInt()==6)return "Locate / install emulator";return g->roles.value("stateLabel").toString();}
QString UiController::appVersion()const{return QCoreApplication::applicationVersion();}
void UiController::message(const QString &text){m_status=text;emit statusChanged();}
QVariantMap UiController::game(const QString &id)const {auto g=m_games->find(id);return g?g->roles:QVariantMap{};}
QVariantMap UiController::filteredGame(int row)const{auto i=m_filter->mapToSource(m_filter->index(row,0));return i.isValid()?m_games->records()[i.row()].roles:QVariantMap{};}
void UiController::openDetail(const QString &id){if(!m_games->find(id))return;m_detailId=id;m_variantId.clear();emit detailChanged();}
void UiController::selectVariant(const QString &id){auto g=m_games->find(m_detailId);if(!g)return;for(const auto &v:g->variants)if(v.id==id){m_variantId=id;emit detailChanged();return;}}
const Variant *UiController::variant(const GameRecord &g) const{for(const auto &v:g.variants)if(v.id==m_variantId)return &v;for(const auto &v:g.variants)if(v.id==g.runtime.selectedVariantId)return &v;for(const auto &v:g.variants)if(v.generated)return &v;for(const auto &v:g.variants)if(v.status!="planned")return &v;return g.variants.isEmpty()?nullptr:&g.variants.first();}
QString UiController::safeMarkdown(QString text){
 text.remove(QRegularExpression("!\\[[^\\]]*\\]\\([^)]*\\)"));
 text.replace("![","[");
 text.remove(QRegularExpression("<img\\b[^>]*>",QRegularExpression::CaseInsensitiveOption));
 return text;
}
QString UiController::readme(const GameRecord &g)const {
 QFile f(QDir(g.folder).filePath("README.md"));if(!f.open(QIODevice::ReadOnly))return "No setup documentation yet. The catalog metadata is available above.";
 QString text=safeMarkdown(QString::fromUtf8(f.readAll()));
 // D40: Markdown images never trigger remote loads, including relative/HTML images.
 text.remove(QRegularExpression("!\\[[^\\]]*\\]\\([^)]*\\)"));text.remove(QRegularExpression("<img\\b[^>]*>",QRegularExpression::CaseInsensitiveOption));
 // Installer owns hand-run instructions. Keep all other H2 content.
 text.remove(QRegularExpression("(?im)^## How to use[^\\n]*\\n[\\s\\S]*?(?=^## |\\z)"));return text;
}
QVariantMap UiController::detail()const{
 auto source=m_games->find(m_detailId);if(!source)return {};auto g=*source;const auto *v=variant(g);
 if(v){g.runtime.selectedVariantId=v->id;resolveState(g);}QVariantMap result=g.roles;
 QVariantList variants,needs,controls,components,similar;QStringList supported;
 for(const auto &item:g.variants){QVariantMap row{{"id",item.id},{"title",item.title},{"quality",item.quality},{"status",item.status},{"generated",item.generated},{"m1Available",item.generated}};variants<<row;}
 if(v){result["variantId"]=v->id;result["variantTitle"]=v->title;result["m1Available"]=v->generated;
 for(const auto &id:v->media)needs<<QVariantMap{{"kind","media"},{"name",id},{"status",g.runtime.mediaFound.contains(id)?"Found":"Missing"},{"found",g.runtime.mediaFound.contains(id)}};
 for(const auto &id:v->tools)needs<<QVariantMap{{"kind","tool"},{"name",id},{"status",g.runtime.toolsOk.contains(id)?"Found":g.runtime.toolsOlder.contains(id)?"Found (older)":"Missing"},{"found",g.runtime.toolsOk.contains(id)}};
 if(v->quality=="true3d"||v->quality=="theatre")needs<<QVariantMap{{"kind","runtime"},{"name","VR runtime"},{"status","Checked when launching"},{"found",false}};
 if(v->raw.contains("step")&&v->raw["step"].is_array())for(const auto &step:v->raw["step"])if(str(step,"do")=="write-config"&&step.contains("set")&&step["set"].is_array())for(const auto &entry:step["set"])supported<<str(entry,"from");
 components<<QVariantMap{{"name",v->title},{"license",str(v->raw,"license")},{"upstream",str(v->raw,"upstream")},{"version",str(v->raw,"version")},{"role",v->generated?"Flat emulator route":"VR setup (outside M1)"}};
 }
 if(g.setup.is_object())for(const auto &setup:g.setup.items()){const auto &value=setup.value();if(!value.is_object())continue;if(value.contains("element")&&value["element"].is_array())for(const auto &e:value["element"])controls<<QVariantMap{{"action",str(e,"id")},{"input",str(e,"source").isEmpty()?str(e,"type"):str(e,"source")},{"notes",e.contains("output")?QString::fromStdString(e["output"].dump()):QString()}};}
 if(controls.isEmpty())controls<<QVariantMap{{"action",g.roles.value("controlsLabel")},{"input","Emulator bindings"},{"notes","Use the emulator's input settings. No per-game control set yet."}};
 QVector<QPair<int,QString>> scores; QHash<QString,QString> titles;
 for(const auto &other:m_games->records()){titles.insert(other.id,other.roles.value("title").toString());if(other.id==g.id)continue;int score=0;auto match=[&](const char *key,int weight){auto a=g.roles.value(key).toString();if(!a.isEmpty()&&a==other.roles.value(key).toString())score+=weight;};match("series",6);match("hardwareFamily",4);match("manufacturerId",2);match("controlsType",2);int y=qAbs(g.roles.value("year").toInt()-other.roles.value("year").toInt());score+=y<=3?2:y<=8?1:0;int sub=0;for(const auto &s:g.roles.value("subgenreIds").toStringList())if(other.roles.value("subgenreIds").toStringList().contains(s))sub+=2;score+=std::min(4,sub);if(score)scores<<qMakePair(score,other.id);}
 std::sort(scores.begin(),scores.end(),[&titles](const auto &a,const auto &b){return a.first==b.first?titles.value(a.second)<titles.value(b.second):a.first>b.first;});for(int i=0;i<std::min(12,static_cast<int>(scores.size()));++i)similar<<game(scores[i].second);
 result["variants"]=variants;result["needs"]=needs;result["controls"]=controls;result["components"]=components;result["similar"]=similar;result["settingsSupported"]=supported;result["readme"]=readme(g);result["notice"]=str(g.raw,"notice");result["quip"]=str(g.raw,"quip");result["settings"]=m_settings->game(g.id);return result;
}
void UiController::primary(const QString &id){auto g=m_games->find(id);if(!g)return;if(g->roles.value("playing").toBool()){message("Playing "+g->roles.value("title").toString());return;}if(m_detailId!=id)openDetail(id);auto d=detail();int state=d.value("state").toInt();if(state==5){emit locationRequested("media");message("Choose folders containing your own media.");}else if(state==6){emit locationRequested("tool");message("Locate an existing emulator or install its pinned release.");}else if(state==4)play(id,d.value("variantId").toString());else if(state==3||state==7)startInstall(id,d.value("variantId").toString());else if(state==2)retryInstall(false,{});else message(d.value("stateLabel").toString());}
void UiController::scan(const QStringList &roots){if(m_scanning){emit cancelScanRequested();return;}if(roots.isEmpty()){message("Choose at least one folder to scan.");return;}message("Scan requested; waiting for scanner.");emit scanRequested(roots);}
void UiController::startInstall(const QString &id,const QString &variantId){auto g=m_games->find(id);if(!g)return;for(const auto &v:g->variants)if(v.id==variantId){if(!v.generated){message("This VR setup is outside M1. Select a flat emulator route.");return;}message("Install requested; waiting for installer.");emit installRequested(id,variantId);return;}}
void UiController::play(const QString &id,const QString &variantId){auto g=m_games->find(id);if(!g)return;for(const auto &v:g->variants)if(v.id==variantId){if(!v.generated){message("This VR setup is outside M1. Select a flat emulator route.");return;}emit playRequested(id,variantId);return;}}
void UiController::cancelInstall(){emit cancelInstallRequested();message("Cancel requested; the installer stops between steps.");}
void UiController::retryInstall(bool fromStart,const QString &handover){emit retryInstallRequested(fromStart,handover);}
void UiController::answerPrompt(bool proceed){emit promptAnswered(proceed);}
void UiController::skipStep(){if(m_recovery.value("canSkip").toBool())emit skipStepRequested();}
void UiController::uninstall(const QString &id,const QString &variantId){emit uninstallPreviewRequested(id,variantId);message("Waiting for the ownership manifest preview before removal.");}
void UiController::toggleFacet(const QString &key,const QString &value){
 auto list=m_filter->facet(key).toStringList();
 if(key=="hardwareIds"){
  QMap<QString,QStringList> nodes; for(const auto &g:m_games->records()){
   auto id=g.roles.value("hardwareId").toString(),kind=g.roles.value("hardwareKind").toString(),family=g.roles.value("hardwareFamily").toString();
   for(const auto &node:{id,kind,family,kind+"/"+family})if(!nodes[node].contains(id))nodes[node]<<id;
  }
  QStringList selectedIds;for(const auto &node:list)for(const auto &id:nodes.value(node))if(!selectedIds.contains(id))selectedIds<<id;
  const auto targets=nodes.value(value);bool all=!targets.isEmpty();for(const auto &id:targets)all=all&&selectedIds.contains(id);
  for(const auto &id:targets)if(all)selectedIds.removeAll(id);else if(!selectedIds.contains(id))selectedIds<<id;
  list.clear();for(const auto &kind:{"arcade","console","pc"}){const auto ids=nodes.value(kind);bool full=!ids.isEmpty();for(const auto &id:ids)full=full&&selectedIds.contains(id);if(full){list<<kind;for(const auto &id:ids)selectedIds.removeAll(id);}}
  for(auto it=nodes.begin();it!=nodes.end();++it)if(it.key().contains('/')){bool full=!it.value().isEmpty();for(const auto &id:it.value())full=full&&selectedIds.contains(id);if(full){list<<it.key();for(const auto &id:it.value())selectedIds.removeAll(id);}}
  list<<selectedIds;
 }else if(list.contains(value))list.removeAll(value);else list<<value;
 m_filter->setFacet(key,list);
}
bool UiController::selected(const QString &key,const QString &value)const{return m_filter->facet(key).toStringList().contains(value);}
QVariantList UiController::activeFacets()const{QVariantList out;for(const auto &key:{"genre","statePills","inLibraryOnly","hardwareIds","manufacturerIds","yearMin","yearMax","graphicsIds","vrKeys","playersBuckets","controlsTypes","decades"}){auto value=m_filter->facet(key);if(!value.isValid())continue;if(value.metaType().id()==QMetaType::Bool){if(value.toBool())out<<QVariantMap{{"key",key},{"value","true"},{"label","In my library"}};}else if(value.metaType().id()==QMetaType::Int)out<<QVariantMap{{"key",key},{"value",value.toString()},{"label",QString(key)+": "+value.toString()}};else for(const auto &item:value.toStringList())out<<QVariantMap{{"key",key},{"value",item},{"label",QString(key)+": "+item}};}return out;}
QVariantList UiController::hardwareTree()const{QVariantList result;QMap<QString,int> counts;for(const auto &choice:m_filter->choices("hardwareIds")){auto row=choice.toMap();counts[row.value("id").toString()]=row.value("count").toInt();}auto vocab=m_games->catalog().vocab;if(!vocab.contains("hardware"))return result;const auto &hw=vocab["hardware"];for(const auto &kind:{"arcade","console","pc"}){QVariantList families;QStringList names;for(const auto &item:hw.items())if(str(item.value(),"kind")==kind&&!names.contains(str(item.value(),"family")))names<<str(item.value(),"family");names.sort();for(const auto &family:names){QVariantList boards;for(const auto &item:hw.items())if(str(item.value(),"kind")==kind&&str(item.value(),"family")==family){QString id=QString::fromStdString(item.key());int count=counts.value(id);if(count)boards<<QVariantMap{{"id",id},{"label",str(item.value(),"label")},{"count",count}};}if(!boards.isEmpty())families<<QVariantMap{{"id",QString(kind)+"/"+family},{"label",family},{"children",boards}};}result<<QVariantMap{{"id",kind},{"label",QString(kind).toUpper()},{"children",families}};}return result;}
int UiController::hardwareSelection(const QString &node)const{int count=0,on=0;auto list=m_filter->facet("hardwareIds").toStringList();for(const auto &g:m_games->records()){auto id=g.roles.value("hardwareId").toString(),kind=g.roles.value("hardwareKind").toString(),family=g.roles.value("hardwareFamily").toString();if(node==id||node==kind||node==kind+"/"+family){++count;if(list.contains(id)||list.contains(kind)||list.contains(family)||list.contains(kind+"/"+family))++on;}}return on==0?0:on==count?2:1;}
QVariantList UiController::sections()const{QVariantList result;for(const auto &genre:{"gun","racing"}){QVariantList rows;for(int i=0;i<m_filter->rowCount();++i){auto g=filteredGame(i);if(g.value("genreId")==genre)rows<<g;}result<<QVariantMap{{"id",genre},{"label",QString(genre)=="gun"?"Light guns":"Racing"},{"count",rows.size()},{"games",rows}};}return result;}
QVariantList UiController::featured()const{QVariantList out;for(const auto &g:m_games->records())if(g.roles.value("featuredEligible").toBool())out<<g.roles;if(out.isEmpty()&&!m_games->records().isEmpty())out<<m_games->records().first().roles;return out;}
QVariantList UiController::recent()const{QVariantList out;QVector<GameRecord> sorted=m_games->records();std::sort(sorted.begin(),sorted.end(),[](const auto &a,const auto &b){return a.runtime.lastPlayed>b.runtime.lastPlayed;});for(const auto &g:sorted)if(g.runtime.lastPlayed>0&&out.size()<8&&!m_settings->get("hideRecent-"+g.id,false).toBool())out<<g.roles;return out;}
QVariantList UiController::exploreRows()const{QVariantList out;const QStringList names{"Rail shooters","Cover and sniper","Horror","Hunting, party and water","Circuit and street","Rally and off-road","Kart and futuristic","Bikes, boats and more"};for(int b=0;b<8;++b){QVariantList games;for(int i=0;i<m_filter->rowCount();++i){auto g=filteredGame(i);if((g.value("bucketMask").toInt()&(1<<b))!=0)games<<g;}if(!games.isEmpty())out<<QVariantMap{{"label",names[b]},{"id",b},{"games",games},{"count",games.size()}};}return out;}
void UiController::copyLog()const{QStringList lines;for(const auto &e:m_events)lines<<e.toMap().value("line").toString();QGuiApplication::clipboard()->setText(lines.join('\n'));}
bool UiController::openLink(const QString &url)const{QUrl u(url);return (u.scheme()=="https"||u.scheme()=="http")&&QDesktopServices::openUrl(u);}
void UiController::openLocation(const QString &kind){emit locationRequested(kind);}
void UiController::removeRecent(const QString &id){m_settings->set("hideRecent-"+id,true);emit detailChanged();}
void UiController::scanStarted(){m_scanning=true;emit scanChanged();message("Scanning…");}
void UiController::scanProgress(const QVariantMap &p){message(p.value("text").toString());}
void UiController::scanFinished(bool success){m_scanning=false;emit scanChanged();if(success)m_filter->setScanComplete(true);message(success?"Scan complete.":"Scan stopped. Existing results were kept.");}
void UiController::installEvent(const QVariantMap &event){auto e=event;e["line"]=eventText(e);m_events<<e;if(e.value("kind")=="step"){m_installing=true;m_recovery.clear();}if(e.value("kind")=="fail"){m_installing=false;m_recovery=e;}emit installChanged();}
void UiController::installFinished(bool success,const QString &text){m_installing=false;if(success)m_recovery.clear();else if(m_recovery.isEmpty())m_recovery={{"text",text},{"kind","fail"}};emit installChanged();message(text);}
void UiController::launchStarted(const QString &id){message("Playing "+game(id).value("title").toString());}
void UiController::launchFinished(const QString &,const QString &error){message(error.isEmpty()?"Game closed.":error);}
void UiController::applyRuntimeStates(const QVector<RuntimeState> &states){m_games->queueRuntimeStates(states);}
}
