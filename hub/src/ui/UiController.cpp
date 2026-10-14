// SPDX-License-Identifier: GPL-3.0-only
#include "UiController.h"
#include "core/pinball/Pinball.h"
#include "core/model3/Model3.h"
#include "core/setup/Setup.h"
#include "core/dreamcast/Dreamcast.h"
#include "core/model2/Model2.h"
#include <QFileInfo>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QClipboard>
#include <QGuiApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QTimer>
#include <algorithm>
namespace ac {
QString UiController::pinballLaunchHelp()const{return pinball::launchGuidance().value("help").toString();}
namespace {
QString str(const Json &j,const char *key){return j.contains(key)&&j[key].is_string()?QString::fromStdString(j[key].get<std::string>()):QString();}
QString eventText(const QVariantMap &e){auto kind=e.value("kind").toString(), text=e.value("text").toString();if(kind=="step")return QString("--- [%1/%2] %3 ---").arg(e.value("index",e.value("step")).toInt()).arg(e.value("total").toInt()).arg(text);const QMap<QString,QString> prefixes{{"ok"," [OK] "},{"warn"," [!!] "},{"fail"," [XX] "},{"work"," [..] "},{"detail","  "},{"prompt"," >>> "},{"done"," >>> "}};return prefixes.value(kind,"  ")+text;}
}
SectionedGridModel::SectionedGridModel(GameListModel *games,FilterSortModel *filter,int columns,QObject *parent):QAbstractListModel(parent),m_games(games),m_filter(filter),m_columns(std::max(1,columns)){
 connect(filter,&QAbstractItemModel::modelReset,this,[this]{scheduleRefresh();});connect(filter,&QAbstractItemModel::layoutChanged,this,[this]{scheduleRefresh();});
 connect(filter,&QAbstractItemModel::rowsInserted,this,[this]{scheduleRefresh();});connect(filter,&QAbstractItemModel::rowsRemoved,this,[this]{scheduleRefresh();});connect(filter,&QAbstractItemModel::dataChanged,this,[this]{scheduleRefresh();});
 connect(games,&QAbstractItemModel::dataChanged,this,[this]{scheduleRefresh();});refresh();
}
QVariant SectionedGridModel::data(const QModelIndex &index,int role)const{return index.isValid()&&index.row()>=0&&index.row()<count()&&(role==Qt::UserRole+1||role==Qt::DisplayRole)?QVariant(m_rows[index.row()].data):QVariant{};}
void SectionedGridModel::setColumns(int columns){columns=std::max(1,columns);if(columns==m_columns)return;m_columns=columns;emit columnsChanged();scheduleRefresh();}
void SectionedGridModel::scheduleRefresh(){if(m_queued)return;m_queued=true;QTimer::singleShot(0,this,[this]{m_queued=false;refresh();});}
void SectionedGridModel::refresh(){
 QVector<Row> desired;
 for(const auto &genre:{"gun","racing"}){
  QVariantList games;QStringList ids;
  for(int i=0;i<m_filter->rowCount();++i){const auto source=m_filter->mapToSource(m_filter->index(i,0));if(!source.isValid())continue;const auto &g=m_games->records()[source.row()];if(g.roles.value("genreId")!=genre)continue;games<<g.roles;ids<<g.id;}
  if(games.isEmpty())continue;
  desired<<Row{QString("header:")+genre,{{"header",true},{"label",QString(genre)=="gun"?"Light guns":"Racing"},{"count",games.size()},{"id",genre}}};
  for(int i=0;i<games.size();i+=m_columns){QVariantList cards;QStringList keys;for(int j=i;j<std::min(i+m_columns,static_cast<int>(games.size()));++j){cards<<games[j];keys<<ids[j];}desired<<Row{"games:"+keys.join('|'),{{"header",false},{"games",cards}}};}
 }
 // Move/insert/remove rows without resets; unchanged cards keep their indexes.
 for(int i=0;i<desired.size();++i){
  if(i>=m_rows.size()||m_rows[i].key!=desired[i].key){int existing=-1;for(int j=i+1;j<m_rows.size();++j)if(m_rows[j].key==desired[i].key){existing=j;break;}
   if(existing>=0){beginMoveRows({},existing,existing,{},i);m_rows.move(existing,i);endMoveRows();}
   else{beginInsertRows({},i,i);m_rows.insert(i,desired[i]);endInsertRows();}
  }
  if(m_rows[i].data!=desired[i].data){m_rows[i].data=desired[i].data;emit dataChanged(index(i),index(i),{Qt::UserRole+1});}
 }
 if(m_rows.size()>desired.size()){beginRemoveRows({},static_cast<int>(desired.size()),count()-1);m_rows.resize(desired.size());endRemoveRows();}
 emit countChanged();
}
QObject *UiController::createGridModel(int columns){return new SectionedGridModel(m_games,m_filter,columns,this);}
void UiController::releaseGridModel(QObject *model){if(auto grid=qobject_cast<SectionedGridModel *>(model);grid&&grid->parent()==this)grid->deleteLater();}
UiController::UiController(GameListModel *games,FilterSortModel *filter,UiSettings *settings,QObject *parent):QObject(parent),m_detailControls(this),m_games(games),m_filter(filter),m_settings(settings){
 connect(&m_detailControls,&DetailControlsController::changed,this,&UiController::detailControlsChanged);
 connect(settings,&UiSettings::changed,this,&UiController::model3Changed);
 connect(settings,&UiSettings::changed,this,&UiController::setupChanged);
 m_detailControls.configure(games->catalog().root,games->catalog().packIds);
 ControlsResolver pinballLibrary;QVariantMap pinballResolved;
 if(!pinballLibrary.loadLibrary(games->catalog().root,m_pinballControlsStatus)||
    !pinballLibrary.resolveRoute("vpx-openxr",pinballResolved,m_pinballControlsStatus)||
    !m_pinballControls.loadResolved(pinballResolved))m_pinballControls.clear();
 connect(filter,&FilterSortModel::facetsChanged,this,&UiController::scheduleFacetsChanged);
 connect(filter,&FilterSortModel::visibleCountChanged,this,&UiController::scheduleFacetsChanged);
 connect(games,&QAbstractItemModel::dataChanged,this,[this]{if(m_detailControlsVisible)syncDetailControls(false);emit detailChanged();scheduleFacetsChanged();});
 connect(settings,&UiSettings::error,this,[this](const QString &text){showError("Settings",text);});
 connect(settings,&UiSettings::gameSaved,this,&UiController::writeConfigRequested);
 connect(settings,&UiSettings::gameSaved,this,[this](const QString &id,const QVariantMap &){if(m_detailControlsVisible&&id==m_detailId)refreshDetailControls();});
}
void UiController::scheduleFacetsChanged(){if(m_facetsQueued)return;m_facetsQueued=true;QTimer::singleShot(0,this,[this]{m_facetsQueued=false;emit facetsChanged();});}
QString UiController::vrLabel(int badge)const{const QStringList labels{"FLAT","TRUE 3D · CATALOG","THEATRE · CATALOG","PLANNED"};return labels.value(badge,"FLAT");}
QString UiController::primaryLabel(const QString &id)const{auto g=m_games->find(id);if(!g)return "No setup yet";if(g->roles.value("playing").toBool())return "Playing";if(g->roles.value("state").toInt()==4){const auto *v=variant(*g);return v?v->title:"Play";}if(g->roles.value("state").toInt()==6)return "Locate / install emulator";return g->roles.value("stateLabel").toString();}
QString UiController::appVersion()const{return QCoreApplication::applicationVersion();}
void UiController::message(const QString &text){m_status=text;emit statusChanged();}
QVariantMap UiController::game(const QString &id)const {auto g=m_games->find(id);return g?g->roles:QVariantMap{};}
QVariantMap UiController::filteredGame(int row)const{auto i=m_filter->mapToSource(m_filter->index(row,0));return i.isValid()?m_games->records()[i.row()].roles:QVariantMap{};}
void UiController::openDetail(const QString &id){if(!m_games->find(id))return;m_detailId=id;m_variantId.clear();if(m_detailControlsVisible)refreshDetailControls();emit detailChanged();}
void UiController::selectVariant(const QString &id){auto g=m_games->find(m_detailId);if(!g)return;for(const auto &v:g->variants)if(v.id==id){m_variantId=id;if(m_detailControlsVisible)refreshDetailControls();emit detailChanged();return;}}
void UiController::refreshDetailControls(){
 m_detailControlsVisible=true;
 syncDetailControls(true);
}
void UiController::syncDetailControls(bool force){
 const auto game=m_games->find(m_detailId);const auto selected=game?variant(*game):nullptr;
 const auto hardware=game?str(game->raw,"hardware"):QString();
 const auto route=selected&&hardware=="nintendo-wii"&&selected->generated&&selected->id=="dolphin"?QString("flat-dolphin"):selected?selected->id:QString();
 const auto gameId=game?game->id:QString(),variantId=selected?selected->id:QString();
 if(force||!m_detailControls.selectionMatches(gameId,variantId,hardware,route))
   m_detailControls.selectGame(gameId,variantId,hardware,route);
}
const Variant *UiController::variant(const GameRecord &g) const{for(const auto &v:g.variants)if(g.id==m_detailId&&v.id==m_variantId)return &v;for(const auto &v:g.variants)if(v.id==g.runtime.selectedVariantId)return &v;for(const auto &v:g.variants)if(v.id==g.roles.value("preferredVariantId").toString())return &v;for(const auto &v:g.variants)if(v.generated)return &v;for(const auto &v:g.variants)if(v.status!="planned")return &v;return g.variants.isEmpty()?nullptr:&g.variants.first();}
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
 for(const auto &item:g.variants){QVariantMap row{{"id",item.id},{"title",item.title},{"quality",item.quality},{"status",item.status},{"generated",item.generated},{"m1Available",item.generated||authoredLaunchSupported(item)}};variants<<row;}
 if(v){result["variantId"]=v->id;result["variantTitle"]=v->title;result["m1Available"]=v->generated||authoredLaunchSupported(*v);
 for(const auto &id:v->media)needs<<QVariantMap{{"kind","media"},{"name",id},{"status",g.runtime.mediaFound.contains(id)?"Found":"Missing"},{"found",g.runtime.mediaFound.contains(id)}};
 for(const auto &id:v->tools)needs<<QVariantMap{{"kind","tool"},{"name",id},{"status",g.runtime.toolsOk.contains(id)?"Found":g.runtime.toolsOlder.contains(id)?"Found (older)":"Missing"},{"found",g.runtime.toolsOk.contains(id)}};
 if(v->quality=="true3d"||v->quality=="theatre")needs<<QVariantMap{{"kind","runtime"},{"name","VR runtime"},{"status","Checked when launching"},{"found",false}};
 if(v->raw.contains("step")&&v->raw["step"].is_array())for(const auto &step:v->raw["step"])if(str(step,"do")=="write-config"&&step.contains("set")&&step["set"].is_array())for(const auto &entry:step["set"])supported<<str(entry,"from");
 components<<QVariantMap{{"name",v->title},{"license",str(v->raw,"license")},{"upstream",str(v->raw,"upstream")},{"version",str(v->raw,"version")},{"role",v->generated?"Flat emulator route":authoredLaunchSupported(*v)?"Authored setup (experimental)":"Authored setup recipe (Hub launch unavailable)"}};
 }
 const bool wii=str(g.raw,"hardware")=="nintendo-wii";
 if(wii)controls<<QVariantMap{{"action","Controller mapping"},{"input",v&&v->id=="dolphinxr"?"DolphinXR input settings":"Selected route"},{"notes",v&&v->id=="dolphinxr"?"Configure native controller mapping inside DolphinXR.":"Only route-matched controls are shown above."}};
 else if(g.setup.is_object())for(const auto &setup:g.setup.items()){const auto &value=setup.value();if(!value.is_object())continue;if(value.contains("element")&&value["element"].is_array())for(const auto &e:value["element"])controls<<QVariantMap{{"action",str(e,"id")},{"input",str(e,"source").isEmpty()?str(e,"type"):str(e,"source")},{"notes",e.contains("output")?QString::fromStdString(e["output"].dump()):QString()}};}
 if(controls.isEmpty())controls<<QVariantMap{{"action",g.roles.value("controlsLabel")},{"input","Emulator bindings"},{"notes","Use the emulator's input settings. No per-game control set yet."}};
 QVector<QPair<int,QString>> scores; QHash<QString,QString> titles;
 for(const auto &other:m_games->records()){titles.insert(other.id,other.roles.value("title").toString());if(other.id==g.id)continue;int score=0;auto match=[&](const char *key,int weight){auto a=g.roles.value(key).toString();if(!a.isEmpty()&&a==other.roles.value(key).toString())score+=weight;};match("series",6);match("hardwareFamily",4);match("manufacturerId",2);match("controlsType",2);int y=qAbs(g.roles.value("year").toInt()-other.roles.value("year").toInt());score+=y<=3?2:y<=8?1:0;int sub=0;for(const auto &s:g.roles.value("subgenreIds").toStringList())if(other.roles.value("subgenreIds").toStringList().contains(s))sub+=2;score+=std::min(4,sub);if(score)scores<<qMakePair(score,other.id);}
 std::sort(scores.begin(),scores.end(),[&titles](const auto &a,const auto &b){return a.first==b.first?titles.value(a.second)<titles.value(b.second):a.first>b.first;});for(int i=0;i<std::min(12,static_cast<int>(scores.size()));++i)similar<<game(scores[i].second);
 result["variants"]=variants;result["needs"]=needs;result["controls"]=controls;result["components"]=components;result["similar"]=similar;result["settingsSupported"]=supported;result["readme"]=readme(g);result["notice"]=str(g.raw,"notice");result["quip"]=str(g.raw,"quip");result["settings"]=m_settings->game(g.id);return result;
}
void UiController::primary(const QString &id){auto g=m_games->find(id);if(!g)return;if(g->roles.value("playing").toBool()){message("Playing "+g->roles.value("title").toString());return;}if(m_detailId!=id)openDetail(id);auto d=detail();int state=d.value("state").toInt();if(state==5){emit locationRequested("media");message("Choose folders containing your own media.");}else if(state==6){emit locationRequested("tool");message("Locate an existing emulator or install its pinned release.");}else if(state==4)play(id,d.value("variantId").toString());else if(state==3||state==7)startInstall(id,d.value("variantId").toString());else if(state==2)retryGameInstall(id,d.value("variantId").toString(),false,{});else message(d.value("stateLabel").toString());}
void UiController::scan(const QStringList &roots){startScan(roots);}
void UiController::startScan(const QStringList &roots){if(m_scanning)return;if(roots.isEmpty()){message("Choose at least one folder to scan.");return;}m_scanning=true;emit scanChanged();message("Scan requested; waiting for scanner.");emit scanRequested(roots);}
void UiController::cancelScan(){if(!m_scanning)return;emit cancelScanRequested();message("Scan cancellation requested.");}
void UiController::showError(const QString &operation,const QString &text){message(operation+": "+text);}
void UiController::overlayFallback(const QString &reason){showError("Overlay unavailable",reason);}
void UiController::settingsSaved(const QString &id){emit detailChanged();message("Settings saved for "+game(id).value("title").toString()+". They apply to the next owned install or repair; existing emulator profiles are preserved.");}
void UiController::startInstall(const QString &id,const QString &variantId){auto g=m_games->find(id);if(!g)return;for(const auto &v:g->variants)if(v.id==variantId){if(!v.generated&&!authoredLaunchSupported(v)){message("Installing and launching this authored setup through the Hub is not enabled yet. Select an available flat route.");return;}message("Install requested; waiting for installer.");emit installRequested(id,variantId);return;}}
void UiController::play(const QString &id,const QString &variantId){auto g=m_games->find(id);if(!g)return;for(const auto &v:g->variants)if(v.id==variantId){if(!v.generated&&!authoredLaunchSupported(v)){message("Installing and launching this authored setup through the Hub is not enabled yet. Select an available flat route.");return;}emit playRequested(id,variantId);return;}}
void UiController::cancelInstall(){emit cancelInstallRequested();message("Cancel requested; the installer stops between steps.");}
bool UiController::retryGameAvailable()const{
 const QRegularExpression id("^[a-z0-9]+(?:-[a-z0-9]+)*$");
 return m_recovery.value("retryKind")=="game"&&m_recovery.value("retryOperation")=="install"&&id.match(m_recovery.value("retryGameId").toString()).hasMatch()&&id.match(m_recovery.value("retryVariantId").toString()).hasMatch();
}
bool UiController::retryToolAvailable()const{
 const QRegularExpression id("^[a-z0-9]+(?:-[a-z0-9]+)*$");
 return m_recovery.value("retryKind")=="tool"&&m_recovery.value("retryOperation")=="install"&&id.match(m_recovery.value("retryGameId").toString()).hasMatch()&&id.match(m_recovery.value("retryVariantId").toString()).hasMatch();
}
void UiController::retryInstall(bool fromStart,const QString &handover){
 const bool recovery=!m_recovery.isEmpty();
 if(recovery&&!retryGameAvailable()){message("Nothing to retry: there is no identified failed game installation.");return;}
 if(recovery){retryGameInstall(m_recovery.value("retryGameId").toString(),m_recovery.value("retryVariantId").toString(),fromStart,handover);return;}
 auto g=m_games->find(m_detailId);const auto *v=g?variant(*g):nullptr;retryGameInstall(m_detailId,v?v->id:QString(),fromStart,handover);
}
void UiController::retryGameInstall(const QString &gameId,const QString &variantId,bool fromStart,const QString &handover){
 auto g=m_games->find(gameId);const Variant *v=nullptr;
 if(g)for(const auto &candidate:g->variants)if(candidate.id==variantId)v=&candidate;
 if(!g||!v||(!v->generated&&!authoredLaunchSupported(*v))||!g->install.is_object()||!g->install.contains("variant")||!g->install["variant"].contains(v->id.toStdString())){message("Nothing to retry: the selected game and variant have no M1 owned installation recipe.");return;}
 emit retryInstallRequested(g->id,v->id,fromStart,handover);
}
void UiController::retryFailedTool(bool fromStart,const QString &handover){
 if(!retryToolAvailable()){message("There is no identified failed tool installation to retry.");return;}
 emit retryToolInstallRequested(m_recovery.value("retryGameId").toString(),m_recovery.value("retryVariantId").toString(),fromStart,handover);
}
void UiController::stopLaunch(){
 if(m_nowPlaying.contains("pinballId")){
  cancelPendingRestart();
  if(!m_nowPlaying.value("closeUnavailable").toString().isEmpty()){
   message("Close the table in VPX; normal close is unavailable from this Hub.");return;
  }
  if(m_nowPlaying.value("closing").toBool())return;
  const bool running=m_nowPlaying.value("running").toBool();
  launchClosing();emit closeLaunchRequested();
  message(running?"Close requested; waiting for VPX to exit.":"Pinball launch cancellation requested.");return;
 }
 emit stopLaunchRequested();message("Stop requested; the game is asked to close, then forced to stop after the timeout.");
}
void UiController::answerPrompt(bool proceed){emit promptAnswered(proceed);}
void UiController::skipStep(){if(m_recovery.value("canSkip").toBool())emit skipStepRequested();}
void UiController::uninstall(const QString &id,const QString &variantId){message("Waiting for the ownership manifest preview before removal.");emit uninstallPreviewRequested(id,variantId);}
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
QVariantList UiController::hardwareTree()const{
 QVariantList result;QMap<QString,int> counts;
 for(const auto &choice:m_filter->choices("hardwareIds")){auto row=choice.toMap();counts[row.value("id").toString()]=row.value("count").toInt();}
 auto vocab=m_games->catalog().vocab;if(!vocab.contains("hardware"))return result;const auto &hw=vocab["hardware"];
 for(const auto &kind:{"arcade","console","pc"}){
  QVariantList families;QStringList names;int kindCount=0;
  for(const auto &item:hw.items())if(str(item.value(),"kind")==kind&&!names.contains(str(item.value(),"family")))names<<str(item.value(),"family");
  names.sort();for(const auto &family:names){QVariantList boards;int familyCount=0;
   for(const auto &item:hw.items())if(str(item.value(),"kind")==kind&&str(item.value(),"family")==family){QString id=QString::fromStdString(item.key());int count=counts.value(id);if(count>0){boards<<QVariantMap{{"id",id},{"label",str(item.value(),"label")},{"count",count}};familyCount+=count;}}
   if(!boards.isEmpty()){families<<QVariantMap{{"id",QString(kind)+"/"+family},{"label",family},{"count",familyCount},{"children",boards}};kindCount+=familyCount;}
  }
  if(!families.isEmpty())result<<QVariantMap{{"id",kind},{"label",QString(kind).toUpper()},{"count",kindCount},{"children",families}};
 }
 return result;
}
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
void UiController::installStarted(){m_installing=true;m_recovery.clear();emit installChanged();message("Preparing install.");}
void UiController::installEvent(const QVariantMap &event){auto e=event;e["line"]=eventText(e);m_events<<e;if(e.value("kind")=="step"){m_installing=true;m_recovery.clear();}if(e.value("kind")=="fail"){m_installing=false;m_recovery=e;}emit installChanged();}
void UiController::installFinished(bool success,const QString &text){m_installing=false;if(success)m_recovery.clear();else if(m_recovery.isEmpty())m_recovery={{"text",text},{"kind","fail"}};emit installChanged();message(text);}
void UiController::launchPreparing(const QString &id,const QString &text){message("Preparing "+game(id).value("title").toString()+": "+text);}
void UiController::pinballLoaded(const QVariantMap &data){
 m_pinballRaw=data;m_pinballData=data;m_pinballScanning=false;m_pinballSelection.clear();m_pinballPlan.clear();
 auto overrides=m_settings->get("pinballOverrides",QVariantMap{}).toMap();auto favourites=m_settings->get("pinballFavourites",QVariantMap{}).toMap();
 const auto recent=m_settings->get("pinballRecent",QVariantMap{}).toMap();
 QVariantList rows;int ready=0;for(auto value:data.value("tables").toList()){
  auto row=value.toMap();const auto id=row.value("id").toString();
  if(overrides.contains(id)){row["vrReady"]=overrides[id].toBool();row["evidence"]=overrides[id].toBool()?QStringList{"marked by you"}:QStringList{};}
  row["lastPlayed"]=recent.value(id,0);
  row["favourite"]=favourites.value(id).toBool();if(row.value("vrReady").toBool())++ready;rows<<row;
 }m_pinballData["tables"]=rows;m_pinballData["vrReady"]=ready;emit pinballChanged();
}
void UiController::selectPinball(const QString &id){m_pinballSelection.clear();m_pinballPlan.clear();for(const auto &value:m_pinballData.value("tables").toList()){auto row=value.toMap();if(row.value("id")==id){m_pinballSelection=row;break;}}emit pinballChanged();}
void UiController::scanPinball(){if(m_pinballScanning)return;const auto folder=m_settings->get("pinballFolder",QString()).toString();if(folder.isEmpty()){message("Choose your Pinball table folder first.");return;}m_pinballScanning=true;emit pinballChanged();emit pinballScanRequested(folder,m_settings->get("pinballDatabase",QString()).toString(),m_settings->get("pinballRoms",QString()).toString(),m_settings->get("pinballMedia",QString()).toString());}
void UiController::openPinball(){const auto folder=m_settings->get("pinballFolder",QString()).toString();if(m_pinballScanning||m_pinballRaw.contains("tables")||folder.isEmpty())return;emit pinballRestoreRequested(folder,m_settings->get("pinballDatabase",QString()).toString(),m_settings->get("pinballRoms",QString()).toString(),m_settings->get("pinballMedia",QString()).toString());}
void UiController::pinballFailed(const QString &error){m_pinballScanning=false;emit pinballChanged();message(error);}
void UiController::markPinball(bool ready){const auto id=m_pinballSelection.value("id").toString();if(id.isEmpty())return;auto overrides=m_settings->get("pinballOverrides",QVariantMap{}).toMap();overrides[id]=ready;m_settings->set("pinballOverrides",overrides);pinballLoaded(m_pinballRaw);selectPinball(id);}
void UiController::favouritePinball(){const auto id=m_pinballSelection.value("id").toString();if(id.isEmpty())return;auto favourites=m_settings->get("pinballFavourites",QVariantMap{}).toMap();favourites[id]=!favourites.value(id).toBool();m_settings->set("pinballFavourites",favourites);pinballLoaded(m_pinballRaw);selectPinball(id);}
void UiController::planPinball(){if(m_pinballSelection.isEmpty())return;emit pinballPlanRequested(m_pinballSelection.value("path").toString(),m_settings->get("pinballExecutable",QString()).toString(),m_pinballSelection.value("id").toString());}
void UiController::playPinball(){if(!m_pinballSelection.isEmpty())emit pinballPlayRequested(m_pinballSelection.value("id").toString());}
void UiController::launchPinballQueued(const QVariantMap &row,const QString &closeUnavailable){
 launchQueued("pinball-"+row.value("id").toString(),"vpx",closeUnavailable);
 m_nowPlaying["title"]=row.value("title");m_nowPlaying["pinballId"]=row.value("id");
 if(m_settings->get("pinballHeadset",true).toBool())m_nowPlaying["hint"]="Visual Pinball was asked to start in the headset. If the table does not appear there, look at the PC monitor: Visual Pinball asks on it whether to retry the headset. In the table, Y exits and X opens the menu.";
 emit nowPlayingChanged();
}
QVariantMap UiController::setupKinds()const{
 // Until the user says otherwise, a kind is ticked when one of its systems already has a folder.
 const auto saved=m_settings->get("setupKinds",QVariantMap{}).toMap();
 QVariantMap kinds{{"arcade",false},{"pinball",false},{"consoles",false}};
 for(const auto &value:setup::systems()){const auto s=value.toMap();const auto key=s.value("folderKey").toString().isEmpty()?s.value("programKey").toString():s.value("folderKey").toString();if(!m_settings->get(key,QString()).toString().isEmpty())kinds[s.value("kind").toString()]=true;}
 for(auto it=saved.begin();it!=saved.end();++it)if(kinds.contains(it.key()))kinds[it.key()]=it.value().toBool();
 return kinds;
}
QVariantList UiController::setupSystems()const{
 const auto kinds=setupKinds();const auto saved=m_settings->get("setupSystems",QVariantMap{}).toMap();
 QStringList known;for(const auto &value:setup::systems())if(!value.toMap().value("folderKey").toString().isEmpty())known<<m_settings->get(value.toMap().value("folderKey").toString(),QString()).toString();
 QVariantList rows;
 for(const auto &value:setup::systems()){
  auto row=value.toMap();const auto id=row.value("id").toString();
  row["folder"]=row.value("folderKey").toString().isEmpty()?QString():m_settings->get(row.value("folderKey").toString(),QString()).toString();
  row["program"]=m_settings->get(row.value("programKey").toString(),QString()).toString();
  // A system follows its kind unless the user ticked or cleared it itself.
  row["chosen"]=kinds.value(row.value("kind").toString()).toBool()&&saved.value(id,true).toBool();
  row["guess"]=row.value("folder").toString().isEmpty()&&!row.value("folderKey").toString().isEmpty()?setup::guessFolder(id,known):QString();
  rows<<row;
 }
 return rows;
}
void UiController::setSetupKind(const QString &kind,bool on){auto kinds=m_settings->get("setupKinds",QVariantMap{}).toMap();kinds[kind]=on;m_settings->set("setupKinds",kinds);}
void UiController::setSetupSystem(const QString &id,bool on){auto systems=m_settings->get("setupSystems",QVariantMap{}).toMap();systems[id]=on;m_settings->set("setupSystems",systems);}
void UiController::setSetupFolder(const QString &id,const QString &folder){for(const auto &value:setup::systems())if(value.toMap().value("id")==id&&!value.toMap().value("folderKey").toString().isEmpty())m_settings->set(value.toMap().value("folderKey").toString(),folder);}
void UiController::setSetupProgram(const QString &id,const QString &program){for(const auto &value:setup::systems())if(value.toMap().value("id")==id)m_settings->set(value.toMap().value("programKey").toString(),program);}
void UiController::acceptSetupGuesses(){for(const auto &value:setupSystems()){const auto row=value.toMap();if(row.value("chosen").toBool()&&!row.value("guess").toString().isEmpty())setSetupFolder(row.value("id").toString(),row.value("guess").toString());}}
void UiController::finishSetup(){m_settings->set("setupDone",true);}
QVariantList UiController::otherGames()const{
 const auto chosen=m_settings->get("gameViews",QVariantMap{}).toMap();QVariantList rows;
 const auto add=[&](QVariantMap row,const QVariantList &views){
  auto view=chosen.value(row.value("id").toString()).toString();bool known=false;
  for(const auto &value:views)if(value.toMap().value("id")==view)known=true;
  if(!known)view=views.first().toMap().value("id").toString();
  for(const auto &value:views)if(value.toMap().value("id")==view)row["viewLabel"]=value.toMap().value("label");
  row["view"]=view;row["views"]=views;rows<<row;
 };
 // Model 2: checked as it is now, like Model 3. One view, and no controls yet.
 const auto model2Program=m_settings->get("model2Executable",QString()).toString(),model2Folder=m_settings->get("model2GameFolder",QString()).toString();
 if(!model2Program.isEmpty()||!model2Folder.isEmpty())for(const auto &value:model2::games()){
  const auto game=value.toMap();
  add({{"id","model2-"+game.value("id").toString()},{"system","model2"},{"pill","MODEL 2"},{"title",game.value("title")},{"year",game.value("year")},{"file",QString()},
   {"warning","View only for now: the game plays its attract sequence and there are no controls yet."},{"problem",model2::problem(game.value("id").toString(),model2Program,model2Folder)}},
   {QVariantMap{{"id","screen"},{"label","3D screen"},{"help","A 3D screen in the headset. View only for now: no controls yet."}}});
 }
 // Time Crisis: one native program, checked as it is now.
 const auto program=m_settings->get("system22Executable",QString()).toString();
 if(!program.isEmpty())add({{"id","system22-timecris"},{"system","system22"},{"pill","SYSTEM 22"},{"title","Time Crisis"},{"year",1996},{"file",QString()},{"warning",QString()},
  {"problem",QFileInfo(program).isFile()?QString():QString("Time Crisis VR build not found. Use Set-up.")}},
  {QVariantMap{{"id","vr"},{"label","In the headset"},{"help","The whole game around you, with a tracked gun. Start your headset's runtime first."}}});
 // Dreamcast: the discs of the last scan. A disc without a VR profile starts behind a warning.
 const auto dreamcastProgram=m_settings->get("dreamcastExecutable",QString()).toString();
 for(const auto &system:m_setupReport.value("systems").toList()){
  if(system.toMap().value("id")!="dreamcast")continue;
  QHash<QString,int> years;for(const auto &value:dreamcast::games())years.insert(value.toMap().value("id").toString(),value.toMap().value("year").toInt());
  const auto row=[&](const QVariantMap &entry){return QVariantMap{{"id","dreamcast-"+entry.value("id").toString()},{"system","dreamcast"},{"pill","DREAMCAST"},{"title",entry.value("title")},{"year",years.value(entry.value("id").toString())},{"file",entry.value("file")},{"warning",QString()},{"problem",QString()}};};
  for(const auto &value:system.toMap().value("ready").toList()){auto r=row(value.toMap());if(!QFileInfo(dreamcastProgram).isFile())r["problem"]="Dreamcast VR build not found. Use Set-up.";add(r,dreamcast::views());}
  for(const auto &value:system.toMap().value("notRight").toList()){const auto entry=value.toMap();auto r=row(entry);
   if(entry.value("startable").toBool()&&QFileInfo(dreamcastProgram).isFile())r["warning"]=entry.value("reason");else r["problem"]=entry.value("reason");add(r,dreamcast::views());}
  for(const auto &value:system.toMap().value("missing").toList()){const auto entry=value.toMap();auto r=row(entry);r["problem"]="Not found. The Hub looks for: "+entry.value("expected").toString();add(r,dreamcast::views());}
 }
 return rows;
}
void UiController::chooseGameView(const QString &id,const QString &view){auto chosen=m_settings->get("gameViews",QVariantMap{}).toMap();if(chosen.value(id)==view)return;chosen[id]=view;m_settings->set("gameViews",chosen);}
void UiController::playOtherGame(const QString &id){for(const auto &value:otherGames()){const auto row=value.toMap();if(row.value("id")==id&&row.value("problem").toString().isEmpty()){emit otherPlayRequested(id,row.value("view").toString(),row.value("file").toString());return;}}}
void UiController::launchOtherQueued(const QString &id,const QString &view,const QString &title,const QString &hint,const QString &closeUnavailable){
 launchQueued(id,view,closeUnavailable);m_nowPlaying["title"]=title;if(!hint.isEmpty())m_nowPlaying["hint"]=hint;emit nowPlayingChanged();
}
void UiController::scanSetup(){
 if(m_setupScanning)return;
 QStringList ids;for(const auto &value:setupSystems())if(value.toMap().value("chosen").toBool())ids<<value.toMap().value("id").toString();
 m_setupScanning=true;m_setupProgress="Starting";emit setupChanged();emit setupScanRequested(ids);
}
QVariantList UiController::model3Views()const{return model3::views();}
QVariantList UiController::model3Games()const{
 const auto executable=m_settings->get("model3Executable",QString()).toString(),folder=m_settings->get("model3GameFolder",QString()).toString();
 const auto chosen=m_settings->get("model3Views",QVariantMap{}).toMap();
 QVariantList rows;
 for(const auto &value:model3::games()){
  auto row=value.toMap();const auto id=row.value("id").toString();
  auto view=chosen.value(id).toString();if(model3::viewLabel(view).isEmpty())view="screen";
  row["view"]=view;row["viewLabel"]=model3::viewLabel(view);
  row["problem"]=model3::problem(id,executable,folder);
  // A file that is there but wrong for this emulator is known only from the last scan.
  if(row.value("problem").toString().isEmpty())for(const auto &system:m_setupReport.value("systems").toList())if(system.toMap().value("id")=="model3")
   for(const auto &entry:system.toMap().value("notRight").toList())if(entry.toMap().value("id")==id)row["problem"]="The game file is not right. "+entry.toMap().value("reason").toString();
  row["card"]=game(id); // Catalog colours and art where the catalog has this title.
  rows<<row;
 }
 return rows;
}
void UiController::chooseModel3View(const QString &id,const QString &view){
 if(model3::viewLabel(view).isEmpty())return;
 auto chosen=m_settings->get("model3Views",QVariantMap{}).toMap();
 if(chosen.value(id)==view)return;
 chosen[id]=view;m_settings->set("model3Views",chosen);
}
void UiController::playModel3(const QString &id){
 for(const auto &value:model3Games()){const auto row=value.toMap();if(row.value("id")==id){emit model3PlayRequested(id,row.value("view").toString());return;}}
}
void UiController::launchModel3Queued(const QString &id,const QString &view,const QString &closeUnavailable){
 launchQueued("model3-"+id,view,closeUnavailable);
 for(const auto &value:model3::games())if(value.toMap().value("id")==id)m_nowPlaying["title"]=value.toMap().value("title").toString()+" · "+model3::viewLabel(view);
 m_nowPlaying["hint"]="Home = set straight ahead. 1 = start, 3 = coin, Esc = quit. The picture is also on the PC monitor.";
 emit nowPlayingChanged();
}
void UiController::launchQueued(const QString &id,const QString &variant,const QString &closeUnavailable){
 m_restartAfterClose=false;
 m_nowPlaying={{"gameId",id},{"variantId",variant},{"title",game(id).value("title",QString("Pinball"))},{"running",false},{"closing",false},{"closeUnavailable",closeUnavailable}};
 emit nowPlayingChanged();
}
void UiController::closeLaunch(){
 if(!m_nowPlaying.value("running").toBool()||m_nowPlaying.value("closing").toBool()||!m_nowPlaying.value("closeUnavailable").toString().isEmpty())return;
 m_nowPlaying["closing"]=true;emit nowPlayingChanged();emit closeLaunchRequested();
}
void UiController::launchRoomQueued(const QString &model,const QString &hand,bool calibrateGrip){
 launchQueued("__gun_room",model+"|"+hand+(calibrateGrip?"|calibrate":""),QString());
 m_nowPlaying["title"]=appName()+" Gun Room · "+model+" · "+hand+(calibrateGrip?" · Grip calibration":"");
 m_nowPlaying["calibrateGrip"]=calibrateGrip;
 m_nowPlaying["gunRoom"]=true;emit nowPlayingChanged();
}
void UiController::launchClosing(){
 if(m_nowPlaying.isEmpty()||m_nowPlaying.value("closing").toBool())return;
 m_nowPlaying["closing"]=true;emit nowPlayingChanged();
}
void UiController::restartLaunch(){
 if(!m_nowPlaying.value("running").toBool()||m_nowPlaying.value("closing").toBool()||!m_nowPlaying.value("closeUnavailable").toString().isEmpty())return;
 m_restartAfterClose=true;closeLaunch();
}
void UiController::launchStarted(const QString &id){
 if(m_nowPlaying.value("gameId")==id){
  m_nowPlaying["running"]=true;emit nowPlayingChanged();
  const auto tableId=m_nowPlaying.value("pinballId").toString();
  if(!tableId.isEmpty()){
   auto recent=m_settings->get("pinballRecent",QVariantMap{}).toMap();const auto now=QDateTime::currentSecsSinceEpoch();
   recent[tableId]=now;m_settings->set("pinballRecent",recent);
   auto rows=m_pinballData.value("tables").toList();for(auto &value:rows){auto row=value.toMap();if(row.value("id")==tableId){row["lastPlayed"]=now;value=row;}}
   m_pinballData["tables"]=rows;if(m_pinballSelection.value("id")==tableId)m_pinballSelection["lastPlayed"]=now;emit pinballChanged();
  }
 }
 message((id=="__gun_room"?"Opening ":"Playing ")+m_nowPlaying.value("title",game(id).value("title")).toString());
}
void UiController::launchFinished(const QString &id,const QString &error){
 if(!m_nowPlaying.isEmpty()&&m_nowPlaying.value("gameId")!=id)return;
 const auto variant=m_nowPlaying.value("variantId").toString();const bool restart=m_restartAfterClose&&error.isEmpty();
 m_restartAfterClose=false;m_nowPlaying.clear();emit nowPlayingChanged();clearDetailControlStates();message(error.isEmpty()?"Game closed.":error);
 if(restart)emit playRequested(id,variant);
}
void UiController::applyRuntimeStates(const QVector<RuntimeState> &states){m_games->queueRuntimeStates(states);}
}
