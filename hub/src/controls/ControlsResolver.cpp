// SPDX-License-Identifier: GPL-3.0-only
#include "ControlsResolver.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <toml++/toml.hpp>

namespace ac {
namespace {
using Map=QVariantMap;
using List=QVariantList;
void check(bool ok,const QString &message) { if (!ok) throw std::runtime_error(message.toStdString()); }
bool isMap(const QVariant &v) { return v.metaType().id()==QMetaType::QVariantMap; }
bool isList(const QVariant &v) { return v.metaType().id()==QMetaType::QVariantList; }
bool isString(const QVariant &v) { return v.metaType().id()==QMetaType::QString; }
bool nonempty(const QVariant &v) { return isString(v) && !v.toString().trimmed().isEmpty(); }
bool number(const QVariant &v) {
    const int type=v.metaType().id();
    return type==QMetaType::Int||type==QMetaType::UInt||type==QMetaType::LongLong||type==QMetaType::ULongLong||type==QMetaType::Double;
}
bool integer(const QVariant &v) {
    if (!number(v)||v.metaType().id()==QMetaType::Double) return false;
    const double n=v.toDouble();
    return std::isfinite(n)&&n>=0&&std::floor(n)==n;
}
bool identifier(const QVariant &v) { static const QRegularExpression id("^[a-z0-9]+(?:-[a-z0-9]+)*$"); return nonempty(v)&&id.match(v.toString()).hasMatch(); }
const QMap<QString,QSet<QString>> &vocabulary() {
    static const QMap<QString,QSet<QString>> values{
        {"gun",{"trigger","reload"}},
        {"runtime",{"laser_toggle","recenter","pause","hand_switch","join"}},
        {"axis",{"steering","accelerator","brake","lean","pedal_speed","stick_x","stick_y","lever","cover_pedal","rear_brake"}},
        {"button",{"gear_low","gear_high","gear_n","gear_r","gear_1","gear_2","gear_3","gear_4","gear_5","gear_6",
                   "shift_up","shift_down","view","start","coin","handbrake","view_1","view_2","view_3","view_4"}}};
    return values;
}
QVariant convert(const toml::node &node,unsigned depth=0) {
    check(depth<64,"TOML nesting limit exceeded");
    if (const auto *table=node.as_table()) {
        Map result;
        for (const auto &[key,value]:*table) result[QString::fromUtf8(key.str().data(),qsizetype(key.str().size()))]=convert(value,depth+1);
        return result;
    }
    if (const auto *array=node.as_array()) {
        List result; check(array->size()<=10000,"TOML array limit exceeded");
        for (const auto &value:*array) result.append(convert(value,depth+1));
        return result;
    }
    if (node.is_string()) return QString::fromStdString(node.value<std::string>().value());
    if (node.is_boolean()) return node.value<bool>().value();
    if (node.is_integer()) return QVariant::fromValue<qlonglong>(node.value<int64_t>().value());
    if (node.is_floating_point()) return node.value<double>().value();
    // Match the Hub catalog converter for TOML date/time extension scalars.
    std::ostringstream text; text<<toml::toml_formatter{node};
    return QString::fromStdString(text.str());
}
List list(const Map &m,const QString &key) { const auto value=m.value(key); check(isList(value),key+" must be an array"); return value.toList(); }
List optionalList(const Map &m,const QString &key) { return m.contains(key)?list(m,key):List{}; }
QMap<QString,Map> indexed(const List &rows,const QString &context,bool strict=true) {
    QMap<QString,Map> result;
    for (const auto &value:rows) {
        check(isMap(value),context+": table required"); const auto row=value.toMap();
        check(nonempty(row.value("id"))&&(!strict||identifier(row.value("id"))),context+": valid id required"); const QString id=row.value("id").toString();
        check(!result.contains(id),context+": duplicate id "+id); result[id]=row;
    }
    return result;
}
QVariant merge(const QVariant &base,const QVariant &later,unsigned depth=0) {
    check(depth<64,"Layer nesting limit exceeded");
    if (isMap(base)&&isMap(later)) {
        Map result=base.toMap(); const auto update=later.toMap();
        for (auto it=update.begin();it!=update.end();++it) {
            if (isString(it.value())&&it.value().toString()=="!delete") result.remove(it.key());
            else result[it.key()]=result.contains(it.key())?merge(result.value(it.key()),it.value(),depth+1):it.value();
        }
        return result;
    }
    if (isList(base)&&isList(later)&&!later.toList().isEmpty()) {
        const auto a=base.toList(),b=later.toList();
        const auto keyed=[](const List &rows) { return std::all_of(rows.begin(),rows.end(),[](const auto &v){return isMap(v)&&v.toMap().contains("id");}); };
        if (keyed(a)&&keyed(b)) {
            indexed(a,"inherited array",false); auto updates=indexed(b,"override array",false); List result;
            for (const auto &value:a) {
                const QString id=value.toMap().value("id").toString();
                result.append(updates.contains(id)?merge(value,updates.take(id),depth+1):value);
            }
            // Keep declaration order of new IDs, not QMap sorting.
            for (const auto &value:b) if (updates.contains(value.toMap().value("id").toString())) result.append(value);
            return result;
        }
    }
    return later;
}
QString confined(const QString &root,const QString &relative,bool optional) {
    check(!relative.isEmpty()&&!QDir::isAbsolutePath(relative)&&!relative.contains('\\')&&!relative.contains(':')&&
          !relative.split('/').contains(".."),"Configuration path escape rejected");
    const QString canonicalRoot=QFileInfo(root).canonicalFilePath(); check(!canonicalRoot.isEmpty(),"Library root is unavailable");
    const QString absolute=QDir(root).absoluteFilePath(relative); QFileInfo leaf(absolute),parent(absolute);
    while (!parent.exists()) {
        const QString previous=parent.absoluteFilePath(); parent=QFileInfo(parent.dir().absolutePath());
        check(parent.absoluteFilePath()!=previous,"No existing configuration ancestor");
    }
#ifdef Q_OS_WIN
    constexpr auto sensitivity=Qt::CaseInsensitive;
#else
    constexpr auto sensitivity=Qt::CaseSensitive;
#endif
    const QString canonical=parent.canonicalFilePath();
    check(canonical==canonicalRoot||canonical.startsWith(canonicalRoot+'/',sensitivity),"Configuration symlink/ancestor escape rejected");
    if (!leaf.exists()) { check(optional,"Required configuration file is missing: "+relative); return {}; }
    check(leaf.isFile(),"Configuration path is not a file: "+relative); return leaf.canonicalFilePath();
}
Map read(const QString &root,const QString &relative,bool optional=false) {
    const QString path=confined(root,relative,optional); if (path.isEmpty()) return {};
    QFile file(path); check(file.open(QIODevice::ReadOnly)&&file.size()<=4*1024*1024,"Configuration file unavailable/oversized");
    const auto bytes=file.readAll(); check(file.error()==QFileDevice::NoError,"Configuration read failed");
    const auto table=toml::parse(std::string_view(bytes.constData(),size_t(bytes.size())));
    return convert(table).toMap();
}
void validateModelPath(const QString &root,const QString &relative) {
    static const QRegularExpression allowed("^(assets/guns/|user/guns/|packs/[a-z0-9]+(?:-[a-z0-9]+)*/assets/guns/).+\\.glb$");
    check(allowed.match(relative).hasMatch(),"Model path must name a GLB inside a gun-asset directory");
    confined(root,relative,true);
}
QStringList names(const QString &root,const QString &relative,const QStringList &filters,QDir::Filters flags) {
    const QString canonical=QFileInfo(QDir(root).filePath(relative)).canonicalFilePath();
    check(!canonical.isEmpty(),"Configuration directory missing: "+relative);
#ifdef Q_OS_WIN
    constexpr auto sensitivity=Qt::CaseInsensitive;
#else
    constexpr auto sensitivity=Qt::CaseSensitive;
#endif
    check(canonical.startsWith(QFileInfo(root).canonicalFilePath()+'/',sensitivity),"Configuration directory escape rejected");
    // Validate every entry through confined(), including junctioned directories.
    const auto result=QDir(canonical).entryList(filters,flags|QDir::NoDotAndDotDot,QDir::Name);
    check(result.size()<=2000,"Configuration entry limit exceeded"); return result;
}
List scalarList(const QVariant &v) { return isList(v)?v.toList():List{v}; }
bool match(const Map &want,const Map &game,const QString &kind) {
    const auto controls=game.value("controls").toMap();
    for (auto it=want.begin();it!=want.end();++it) {
        if (it.key()=="gun") {
            check(isString(it.value()),"Gun match must be a regular-expression string");
            QRegularExpression expression(it.value().toString(),QRegularExpression::CaseInsensitiveOption);
            check(expression.isValid(),"Invalid gun rule expression");
            if (!expression.match(controls.value("gun").toString()).hasMatch()) return false;
        } else {
            check(QSet<QString>{"id","hardware","manufacturer","kind"}.contains(it.key()),"Unknown gun rule match key");
            const QVariant actual=it.key()=="kind"?QVariant(kind):game.value(it.key());
            if (!scalarList(it.value()).contains(actual)) return false;
        }
    }
    return true;
}
struct Selection { QString model,provenance; bool review=false; };
Selection selectModel(const Map &game,const Map &games,const Map &models,const Map &defaults,const Map &hardware,unsigned depth=0) {
    const auto explicitModel=game.value("controls").toMap().value("gun_model");
    if (explicitModel.isValid()) { check(identifier(explicitModel)&&models.contains(explicitModel.toString()),"Unknown/invalid gun_model"); return {explicitModel.toString(),"game.toml",false}; }
    const QString kind=hardware.value(game.value("hardware").toString()).toMap().value("kind").toString();
    const auto rules=list(defaults,"rule");
    for (qsizetype i=0;i<rules.size();++i) {
        check(isMap(rules[i]),"Gun rule must be a table"); const auto rule=rules[i].toMap();
        check(isMap(rule.value("match")),"Gun rule match must be a table");
        if (match(rule.value("match").toMap(),game,kind)) {
            const QString model=rule.value("model").toString(); check(models.contains(model),"Rule names unknown gun model");
            return {model,QString("rule %1").arg(i+1),rule.value("review").toBool()};
        }
    }
    if (depth==0&&defaults.value("inherit_original").toList().contains(game.value("hardware"))) {
        const QString original=game.value("original").toString();
        if (games.contains(original)) { auto selected=selectModel(games.value(original).toMap(),games,models,defaults,hardware,1); selected.provenance="original "+original; return selected; }
    }
    const QString fallback=defaults.value("fallback").toString(); check(models.contains(fallback),"Unknown gun fallback"); return {fallback,"fallback",true};
}
void validateBinding(const Map &binding) {
    check(QSet<QString>{"slot","left","right","either"}.contains(binding.value("hand").toString()),"Invalid binding hand");
    check(QSet<QString>{"trigger","grip","primary","secondary","thumbstick_click","offscreen_trigger","flick_up","pump","slide","menu_chord"}.contains(binding.value("control").toString()),"Invalid binding control");
    check(QSet<QString>{"hold","press","toggle"}.contains(binding.value("mode").toString()),"Invalid binding mode");
    if (binding.contains("threshold")) check(number(binding.value("threshold"))&&std::isfinite(binding.value("threshold").toDouble())&&binding.value("threshold").toDouble()>=0&&binding.value("threshold").toDouble()<=1,"Invalid trigger/grip threshold");
    if (binding.contains("invert")) check(binding.value("invert").metaType().id()==QMetaType::Bool,"Binding invert must be boolean");
}
QSet<QString> physicalHands(const Map &row,const Map &binding,const QString &primaryHand) {
    const QString hand=binding.value("hand").toString();
    if (hand=="slot") return {row.value("slot").toInt()==0?primaryHand:(primaryHand=="right"?"left":"right")};
    if (hand=="either") return {"right","left"};
    return {hand};
}
void validateSet(const Map &data,const Map &model,const std::optional<QSet<QString>> &nodes,bool collisions) {
    check(data.value("version").toString()=="0.1"&&identifier(data.value("id"))&&nonempty(data.value("title")),"Invalid control set version/id/title");
    if (data.contains("gun_model")) check(data.value("gun_model")==model.value("id"),"Control-set model differs from selected gun");
    if (data.contains("policy")) check(isMap(data.value("policy")),"Policy must be a table");
    const auto policy=data.value("policy").toMap();
    const QString primaryHand=policy.value("p1_hand","right").toString();
    check(QSet<QString>{"left","right"}.contains(primaryHand),"Invalid profile primary hand");
    const auto elements=list(data,"element"),parts=optionalList(data,"unmapped_part");
    indexed(elements,"element"); indexed(parts,"unmapped_part");
    QMap<QString,Map> seen,fallbackSeen; QSet<QString> covered;
    for (const auto &value:elements) {
        const auto row=value.toMap(); const QString id=row.value("id").toString();
        check(nonempty(row.value("label"))&&nonempty(row.value("part"))&&isString(row.value("node")),id+": missing part/label/node");
        check(integer(row.value("slot"))&&integer(row.value("player"))&&row.value("slot").toULongLong()<2&&row.value("slot")==row.value("player"),id+": invalid gun slot/player");
        check(isMap(row.value("input"))&&isMap(row.value("binding")),id+": input/binding table required");
        const auto input=row.value("input").toMap(),binding=row.value("binding").toMap();
        check(vocabulary().value(input.value("kind").toString()).contains(input.value("semantic").toString()),id+": unknown ABI semantic");
        validateBinding(binding); const QString control=binding.value("control").toString(),node=row.value("node").toString(); covered.insert(node);
        if (!node.isEmpty()&&nodes) check(nodes->contains(node),id+": missing decoded model node");
        const auto fallback=row.value("fallback_binding").toMap();
        if (control=="pump"||control=="slide") check(!fallback.isEmpty(),id+": one-handed fallback required");
        if (row.contains("fallback_binding")) {
            check(isMap(row.value("fallback_binding")),"Fallback must be a table"); validateBinding(fallback);
            check(fallback.value("hand").toString()=="slot"&&fallback.value("control").toString()!="pump"&&fallback.value("control").toString()!="slide","Fallback must be slot-local and one-handed");
        }
        if (control=="offscreen_trigger") check(input==Map{{"kind","gun"},{"semantic","reload"}},"Offscreen trigger is only a composite reload");
        if (collisions&&data.value("configured_slots",2).toInt()==2&&QSet<QString>{"trigger","reload","cover_pedal"}.contains(input.value("semantic").toString())) check(binding.value("hand").toString()=="slot","Two-gun action must be slot-local");
        for (const auto &hand:physicalHands(row,binding,primaryHand)) {
            const QString key=hand+'/'+control; const auto old=seen.value(key);
            if (collisions&&!old.isEmpty()) check(old.value("input")==row.value("input")&&old.value("player")==row.value("player")&&old.value("binding")==row.value("binding"),"Controller binding collision");
            seen[key]=row;
        }
        const Map active=fallback.isEmpty()?binding:fallback;
        for (const auto &hand:physicalHands(row,active,primaryHand)) {
            const QString key=hand+'/'+active.value("control").toString(); const auto old=fallbackSeen.value(key);
            if (collisions&&!old.isEmpty()) {
                const Map oldActive=old.contains("fallback_binding")?old.value("fallback_binding").toMap():old.value("binding").toMap();
                check(old.value("input")==row.value("input")&&old.value("player")==row.value("player")&&oldActive==active,"Fallback binding collision");
            }
            fallbackSeen[key]=row;
        }
    }
    for (const auto &value:parts) {
        const auto row=value.toMap();
        check(nonempty(row.value("label"))&&nonempty(row.value("reason"))&&nonempty(row.value("requested_semantic"))&&isString(row.value("node"))&&integer(row.value("slot"))&&integer(row.value("player"))&&row.value("slot").toULongLong()<2&&row.value("slot")==row.value("player"),"Invalid unresolved part");
        const QString node=row.value("node").toString(); covered.insert(node);
        if (!node.isEmpty()&&nodes) check(nodes->contains(node),"Unresolved part names missing model node");
    }
    for (const auto &button:model.value("button").toList()) check(covered.contains(button.toMap().value("node").toString()),"Model button lacks mapping or explicit unresolved part");
    const auto outputs=optionalList(data,"output"); indexed(outputs,"output");
    for (const auto &value:outputs) {
        const auto output=value.toMap();
        check(QSet<QString>{"solenoid","lamp","ffb"}.contains(output.value("kind").toString()),"Invalid output kind");
        for (const QString &key:{QString("slot"),QString("player"),QString("channel")}) check(integer(output.value(key)),"Invalid output index");
        check(output.value("slot").toULongLong()<2&&output.value("player").toULongLong()<2&&model.value("motion").toMap().contains(output.value("motion").toString()),"Invalid output slot/motion");
        const auto amp=output.value("amplitude"); check(number(amp)&&std::isfinite(amp.toDouble())&&amp.toDouble()>=0&&amp.toDouble()<=1,"Invalid output amplitude");
        check(integer(output.value("duration_ms"))&&output.value("duration_ms").toLongLong()>0,"Invalid output duration");
    }
    if (policy.contains("two_guns")) check(QSet<QString>{"off","on_join","always"}.contains(policy.value("two_guns").toString()),"Invalid two-gun policy");
}
void validateBackend(const std::optional<Map> &backend) {
    if (!backend) return;
    const auto &b=*backend;
    check(integer(b.value("guns"))&&integer(b.value("players"))&&b.value("shared_view").metaType().id()==QMetaType::Bool,"Invalid backend counts/shared_view");
    QSet<QString> seen;
    for (const auto &value:optionalList(b,"control")) {
        const auto row=value.toMap(); const QString kind=row.value("kind").toString(),semantic=row.value("semantic").toString();
        check(kind!="runtime"&&vocabulary().value(kind).contains(semantic)&&integer(row.value("player"))&&row.value("player").toLongLong()<b.value("players").toLongLong(),"Invalid backend control declaration");
        const QString key=kind+'/'+semantic+'/'+row.value("player").toString(); check(!seen.contains(key),"Duplicate backend declaration"); seen.insert(key);
    }
    for (const auto &action:optionalList(b,"runtime_actions")) check(vocabulary().value("runtime").contains(action.toString()),"Invalid runtime action declaration");
}
Map availability(const Map &row,const std::optional<Map> &backend,int slotCount) {
    if (!backend) return {{"state","unverified"},{"reason","No backend/runtime input declaration supplied"}};
    if (row.value("slot").toInt()>=slotCount) return {{"state","unavailable"},{"reason","Backend gun/player/shared-view limits exclude this slot"}};
    const auto input=row.value("input").toMap(); bool supported=false;
    if (input.value("kind").toString()=="runtime") supported=backend->value("runtime_actions").toList().contains(input.value("semantic"));
    else for (const auto &value:backend->value("control").toList()) {
        const auto control=value.toMap(); if (control.value("kind")==input.value("kind")&&control.value("semantic")==input.value("semantic")&&control.value("player")==row.value("player")) supported=true;
    }
    return {{"state",supported?"available":"unavailable"},{"reason",supported?"Declared by supplied backend/profile":"Not declared by supplied backend/profile"}};
}
List filterSlots(const List &rows,int slotCount) {
    List result; for (const auto &row:rows) if (row.toMap().value("slot").toInt()<slotCount) result.append(row); return result;
}
}

bool ControlsResolver::mergeLayers(const QVariant &base,const QVariant &later,QVariant &out,QString &error) {
    try { const auto result=merge(base,later); out=result; error.clear(); return true; }
    catch (const std::exception &e) { error=QString::fromUtf8(e.what()); return false; }
}
bool ControlsResolver::readConfinedToml(const QString &root,const QString &relative,Map &out,QString &error,bool optional) {
    try { const auto result=read(root,relative,optional); out=result; error.clear(); return true; }
    catch (const std::exception &e) { error=QString::fromUtf8(e.what()); return false; }
}
bool ControlsResolver::loadLibrary(const QString &root,QString &error) {
    try {
        const QString canonical=QFileInfo(root).canonicalFilePath(); check(!canonical.isEmpty(),"Library root unavailable");
        Map games,models,sets;
        for (const auto &gid:names(canonical,"games",{},QDir::Dirs)) {
            check(identifier(gid),"Invalid game folder id"); auto game=read(canonical,"games/"+gid+"/game.toml");
            check(game.value("id")==gid,"Game id differs from folder"); games[gid]=game;
        }
        for (const auto &file:names(canonical,"data/guns",{"*.toml"},QDir::Files)) if (file!="defaults.toml") {
            const QString id=QFileInfo(file).completeBaseName(); const auto model=read(canonical,"data/guns/"+file);
            check(identifier(id)&&model.value("id")==id,"Gun id differs from filename");
            validateModelPath(canonical,model.value("model").toString()); models[id]=model;
        }
        for (const auto &file:names(canonical,"data/controls",{"*.toml"},QDir::Files)) if (file!="defaults.toml") {
            const QString id=QFileInfo(file).completeBaseName(); const auto set=read(canonical,"data/controls/"+file);
            check(identifier(id)&&set.value("id")==id,"Control set id differs from filename"); sets[id]=set;
        }
        const auto defaults=read(canonical,"data/controls/defaults.toml"),guns=read(canonical,"data/guns/defaults.toml"),hardware=read(canonical,"data/vocab/hardware.toml");
        check(defaults.value("version").toString()=="0.1"&&sets.contains(defaults.value("fallback").toString()),"Invalid controls defaults");
        const auto rules=list(defaults,"rule"); indexed(rules,"control rule"); QSet<QString> coverage;
        for (const auto &value:rules) { const auto rule=value.toMap(); check(models.contains(rule.value("gun_model").toString())&&sets.contains(rule.value("control_set").toString()),"Unknown default model/set"); coverage.insert(rule.value("gun_model").toString()); }
        check(coverage==QSet<QString>(models.keyBegin(),models.keyEnd()),"Every model requires an explicit control-set rule");
        m_root=canonical; m_games=games; m_models=models; m_sets=sets; m_defaults=defaults; m_gunDefaults=guns; m_hardware=hardware; error.clear(); return true;
    } catch (const std::exception &e) { error=QString::fromUtf8(e.what()); return false; }
}

bool ControlsResolver::resolveGame(const QString &id,const ControlsResolveOptions &options,Map &out,QString &error) const {
    try {
        check(!m_root.isEmpty()&&identifier(id)&&m_games.contains(id),"Unknown/invalid gun game");
        Map game=m_games.value(id).toMap(); check(game.value("genre").toString()=="gun","Requested game is not a gun game");
        validateBackend(options.backend); QStringList packs;
        for (const auto &pack:options.packIds) { check(identifier(pack)&&!packs.contains(pack),"Invalid/duplicate pack id"); packs.append(pack); }
        if (options.useUserOverrides) confined(m_root,options.userOverrideDirectory+"/games/"+id+"/game.toml",true);
        for (const auto &pack:packs) game=merge(game,read(m_root,"packs/"+pack+"/games/"+id+"/game.toml",true)).toMap();
        if (options.useUserOverrides) game=merge(game,read(m_root,options.userOverrideDirectory+"/games/"+id+"/game.toml",true)).toMap();
        check(game.value("id").toString()==id&&game.value("genre").toString()=="gun","Layer changed game identity/genre");
        check(isMap(game.value("controls")),"Game controls must be a table");
        const bool profileSelected=!game.value("controls").toMap().contains("gun_model")&&!options.profileModel.isEmpty();
        if (profileSelected) {
            check(identifier(options.profileModel)&&m_models.contains(options.profileModel),"Unknown profile default model");
            auto controls=game.value("controls").toMap(); controls["gun_model"]=options.profileModel; game["controls"]=controls;
        }
        const auto selection=selectModel(game,m_games,m_models,m_gunDefaults,m_hardware);
        Map model=m_models.value(selection.model).toMap(); QString sid=m_defaults.value("fallback").toString();
        for (const auto &value:m_defaults.value("rule").toList()) { const auto rule=value.toMap(); if (rule.value("gun_model").toString()==selection.model) { sid=rule.value("control_set").toString(); break; } }
        for (const auto &pack:packs) model=merge(model,read(m_root,"packs/"+pack+"/data/guns/"+selection.model+".toml",true)).toMap();
        if (options.useUserOverrides) model=merge(model,read(m_root,options.userOverrideDirectory+"/data/guns/"+selection.model+".toml",true)).toMap();
        check(model.value("id").toString()==selection.model,"Layer changed model identity"); validateModelPath(m_root,model.value("model").toString());
        Map data=merge(m_sets.value(sid),options.profileDefaults).toMap(); List layers{QString("data/controls/%1.toml").arg(sid)};
        if (!options.profileDefaults.isEmpty()) layers.append("caller defaults");
        const QString canonical="games/"+id+"/setup/controls.toml"; const auto own=read(m_root,canonical,true);
        if (!own.isEmpty()) { check(own.value("version").toString()=="0.1","Legacy gun overrides cannot be silently migrated"); data=merge(data,own).toMap(); layers.append(canonical); }
        for (qsizetype index=0;index<packs.size();++index) {
            data=merge(data,read(m_root,"packs/"+packs[index]+"/data/controls/"+sid+".toml",true)).toMap();
            data=merge(data,read(m_root,"packs/"+packs[index]+"/games/"+id+"/setup/controls.toml",true)).toMap();
            layers.append(QString("pack %1").arg(index+1));
        }
        if (options.useUserOverrides) {
            const auto shared=read(m_root,options.userOverrideDirectory+"/data/controls/"+sid+".toml",true);
            data=merge(data,shared).toMap();
            const auto user=read(m_root,options.userOverrideDirectory+"/games/"+id+"/setup/controls.toml",true);
            data=merge(data,user).toMap();
            if (!user.isEmpty()||!shared.isEmpty()) layers.append("user override");
        }
        const auto controls=game.value("controls").toMap();
        check(integer(controls.value("guns",1))&&integer(game.value("players",1)),"Invalid catalog gun/player count");
        const bool separate=m_gunDefaults.value("two_guns").toMap().value("separate_views").toList().contains(id);
        const bool eligible=controls.value("guns",1).toInt()>=2&&game.value("players",1).toInt()>=2&&!separate;
        const QString policy=controls.value("two_guns",data.value("policy").toMap().value("two_guns","on_join")).toString();
        check(QSet<QString>{"off","on_join","always"}.contains(policy),"Invalid two-gun option");
        const int slotCount=eligible&&policy!="off"?2:1; data["configured_slots"]=slotCount;
        validateSet(data,model,std::nullopt,false);
        data["element"]=filterSlots(data.value("element").toList(),slotCount); data["unmapped_part"]=filterSlots(data.value("unmapped_part").toList(),slotCount);
        if (data.contains("output")) data["output"]=filterSlots(data.value("output").toList(),slotCount);
        std::optional<QSet<QString>> nodes;
        if (options.decodedNodesForModel) nodes=options.decodedNodesForModel(model);
        else if (options.decodedModelNodes.contains(selection.model)) nodes=options.decodedModelNodes.value(selection.model);
        validateSet(data,model,nodes,true);
        const int declared=options.backend?std::min({slotCount,options.backend->value("guns").toInt(),options.backend->value("players").toInt(),options.backend->value("shared_view").toBool()?2:1}):0;
        List elements,gaps;
        for (const auto &value:data.value("unmapped_part").toList()) { const auto p=value.toMap(); gaps.append(Map{{"category","unmapped-part"},{"id",p.value("id")},{"reason",p.value("reason")},{"node",p.value("node")},{"requested_semantic",p.value("requested_semantic")}}); }
        for (const auto &value:data.value("element").toList()) { auto row=value.toMap(); row["availability"]=availability(row,options.backend,declared); elements.append(row); }
        data["element"]=elements;
        for (int slot=0;slot<slotCount;++slot) {
            const bool found=std::any_of(elements.begin(),elements.end(),[slot](const auto &v){const auto r=v.toMap();return r.value("slot").toInt()==slot&&r.value("input").toMap()==Map{{"kind","gun"},{"semantic","trigger"}};});
            if (!found) gaps.append(Map{{"category","game-mapping"},{"id",QString("p%1-trigger").arg(slot+1)},{"reason","Configured gun slot has no fire mapping"}});
        }
        for (const auto &value:elements) { const auto row=value.toMap(),state=row.value("availability").toMap(); if (state.value("state").toString()!="available") gaps.append(Map{{"category","backend-binding"},{"id",row.value("id")},{"reason",state.value("reason")}}); }
        if (!nodes) gaps.append(Map{{"category","asset"},{"id",selection.model},{"reason","GLB not built; actual model nodes not checked"}});
        const bool needsCover=controls.value("pedals").toList().contains("cover-pedal")||m_defaults.value("required_cover").toList().contains(id);
        const bool hasCover=std::any_of(elements.begin(),elements.end(),[](const auto &v){return v.toMap().value("input").toMap()==Map{{"kind","axis"},{"semantic","cover_pedal"}};});
        if (needsCover&&!hasCover) gaps.append(Map{{"category","game-mapping"},{"id","cover-pedal"},{"reason","Catalog/lead requires cover pedal; game override not yet supplied"}});
        if (options.backend) for (const auto &value:options.backend->value("control").toList()) {
            const auto declaration=value.toMap(); const int player=declaration.value("player").toInt();
            const Map wanted{{"kind",declaration.value("kind")},{"semantic",declaration.value("semantic")}};
            if (player<slotCount&&!std::any_of(elements.begin(),elements.end(),[&](const auto &v){const auto r=v.toMap();return r.value("player").toInt()==player&&r.value("input").toMap()==wanted;})) gaps.append(Map{{"category","declared-input"},{"id",QString("p%1-%2").arg(player+1).arg(declaration.value("semantic").toString())},{"reason","Backend declares an input with no control element"}});
        }
        Map result{{"game_id",id},{"model",selection.model},{"model_provenance",profileSelected?"caller model default":selection.provenance},{"shape_review",selection.review},
            {"control_set",sid},{"layers",layers},{"data",data},{"model_metadata",model},{"two_gun_eligible",eligible},{"separate_views",separate},{"configured_slots",slotCount},
            {"declared_active_slots",declared},{"node_validation",nodes?"built-references-checked":"not-built"},{"gaps",gaps}};
        out=result; error.clear(); return true;
    } catch (const std::exception &e) { error=QString::fromUtf8(e.what()); return false; }
}
}
