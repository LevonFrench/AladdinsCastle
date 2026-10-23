// SPDX-License-Identifier: GPL-3.0-only
#include "Script.h"
#include <QMap>
#include <QSet>
#include <QRegularExpression>
#include <QStringList>
#include <functional>
#include <algorithm>
namespace ac::pinball {
namespace {
const auto ci=QRegularExpression::CaseInsensitiveOption;
struct Statement {QString text,syntax;bool conditional=false;};
QList<Statement> statements(QString script,bool &incomplete){
 QList<Statement> out;Statement line;bool quoted=false,comment=false;
 const auto flush=[&]{const auto text=line.text.trimmed();if(quoted)incomplete=true;if(!text.isEmpty()&&!text.startsWith("Rem ",Qt::CaseInsensitive)){
  if(out.size()<100000)out<<line;else incomplete=true;
 }line={};};
 for(qsizetype i=0;i<script.size();++i){const auto ch=script[i];
  if(ch=='\n'||ch=='\r'){
   if(ch=='\r'&&i+1<script.size()&&script[i+1]=='\n')++i;
   const auto trimmed=line.syntax.trimmed();const auto tail=line.syntax.lastIndexOf('_');
   if(!quoted&&!comment&&trimmed.endsWith('_')&&tail>0&&line.syntax[tail-1].isSpace()){line.text[tail]=' ';line.syntax[tail]=' ';continue;}
   flush();quoted=false;comment=false;continue;
  }
  if(comment)continue;
  if(ch=='\''&&!quoted){comment=true;continue;}
  if(ch=='"'){
   line.text+=ch;line.syntax+=' ';
   if(quoted&&i+1<script.size()&&script[i+1]=='"'){line.text+=script[++i];line.syntax+=' ';}else quoted=!quoted;
   continue;
  }
  if(ch==':'&&!quoted){flush();continue;}
  line.text+=ch;line.syntax+=quoted?QChar(' '):ch;
 }
 flush();return out;
}
QStringList split(const QString &text,QChar separator){
 QStringList parts;int depth=0;bool quoted=false;qsizetype start=0;
 for(qsizetype i=0;i<text.size();++i){auto ch=text[i];if(ch=='"'){if(quoted&&i+1<text.size()&&text[i+1]=='"')++i;else quoted=!quoted;}
  else if(!quoted){if(ch=='(')++depth;else if(ch==')')--depth;else if(ch==separator&&depth==0){parts<<text.mid(start,i-start).trimmed();start=i+1;}}
 }parts<<text.mid(start).trimmed();return parts;
}
struct Values {QStringList names;bool complete=true;};
}
QVariantMap scriptMetadata(const QString &script,bool locked){
 QVariantMap result{{"romState","unknown"},{"romRequiresPinmame",false},{"romCandidatesComplete",false}};
 if(locked){result["romReason"]="The table's author locked its script; its ROM requirement cannot be read.";return result;}
 bool incomplete=false;const auto parsed=statements(script,incomplete);QList<Statement> lines;
 if(incomplete){result["romReason"]="The script could not be classified within the metadata reader's limits.";return result;}
 // Tables often embed controller.vbs verbatim, including unused PinMAME loaders.
 // Classify the calls to these standard loaders, not the alternative COM factories in their bodies.
 static const QRegularExpression procedure("^\\s*(?:(?:Public|Private)\\s+)?(?:Sub|Function|Property\\s+(?:Get|Let|Set))\\s+([A-Za-z_]\\w*)",ci),procedureEnd("^\\s*End\\s+(?:Sub|Function|Property)\\b",ci);
 static const QRegularExpression event("_(?:Init|Timer|Hit|Unhit|Spin|Slingshot|Collide|Animate|KeyDown|KeyUp|Exit|Paused|UnPaused|MusicDone|Initialize|Terminate)$",ci);
 static const QRegularExpression blockStart("^\\s*(?:If\\b.*\\bThen\\s*$|Select\\s+Case\\b|For\\b|Do\\b|While\\b)",ci),blockEnd("^\\s*(?:End\\s+(?:If|Select)\\b|Next\\b|Loop\\b|Wend\\b)",ci);
 QMap<QString,QList<Statement>> procedures;QString scope;int blockDepth=0;
 for(auto line:parsed){const auto p=procedure.match(line.syntax);if(p.hasMatch()){scope=p.captured(1).toCaseFolded();procedures[scope];blockDepth=0;continue;}
  if(procedureEnd.match(line.syntax).hasMatch()){scope.clear();blockDepth=0;continue;}
  if(blockEnd.match(line.syntax).hasMatch())blockDepth=std::max(0,blockDepth-1);
  line.conditional=blockDepth>0||(!scope.isEmpty()&&!event.match(scope).hasMatch());procedures[scope]<<line;
  if(blockStart.match(line.syntax).hasMatch())++blockDepth;
 }
 // VPX invokes Object_Event callbacks; other helpers need an actual reference from a reachable body.
 QSet<QString> reached,queued{QString()};QStringList pending{QString()};for(auto it=procedures.cbegin();it!=procedures.cend();++it)if(event.match(it.key()).hasMatch()){pending<<it.key();queued.insert(it.key());}
 static const QRegularExpression identifier("(?<![\\w.])[A-Za-z_]\\w*");
 while(!pending.isEmpty()){const auto name=pending.takeLast();if(reached.contains(name))continue;reached.insert(name);
  for(const auto &line:procedures.value(name)){auto words=identifier.globalMatch(line.syntax);while(words.hasNext()){const auto word=words.next().captured().toCaseFolded();if(procedures.contains(word)&&!queued.contains(word)){pending<<word;queued.insert(word);}}}
 }
 const QStringList loaders{"loadvpm","loadvpmalt","loadvpinmame","loadcontroller","loadem","loadproc"};
 for(auto it=procedures.cbegin();it!=procedures.cend();++it)if(reached.contains(it.key())&&!loaders.contains(it.key()))lines+=it.value();
 const auto standardLoader=[&](const QString &name){if(!procedures.contains(name))return true;
  static const QRegularExpression factory("^\\s*Set\\s+\\w+\\s*=\\s*CreateObject\\s*\\(\\s*\"VPinMAME\\.Controller\"\\s*\\)",ci),vpmCall("^\\s*(?:Call\\s+)?LoadVPinMAME\\b",ci),controllerCall("^\\s*(?:Call\\s+)?LoadController\\s*\\(?\\s*\"VPM(?:ALT)?\"",ci);
  for(const auto &line:procedures.value(name))if(factory.match(line.text).hasMatch()||vpmCall.match(line.syntax).hasMatch()||controllerCall.match(line.text).hasMatch())return true;
  return false;
 };
 QMap<QString,QStringList> assignments;QMap<QString,QString> objects;QSet<QString> certainAssignments,uncertainObjects;bool pinmame=false,dmd=false,unconditionalPinmame=false,dynamicController=false;
 static const QRegularExpression load("(?:^|\\bThen\\s+|\\bElse\\s+)\\s*(?:Call\\s+)?(?:LoadVPM|LoadVPMALT|LoadVPinMAME)\\b",ci),controllerLoad("^\\s*(?:Call\\s+)?LoadController\\b",ci),controllerKind("^\\s*(?:Call\\s+)?LoadController\\s*\\(?\\s*\"(EM|PROC|VPM|VPMALT)\"\\s*\\)?\\s*$",ci);
 static const QRegularExpression create("^\\s*Set\\s+(\\w+)\\s*=\\s*CreateObject\\s*\\(\\s*\"([^\"]+)\"\\s*\\)",ci);
 static const QRegularExpression assignment("^\\s*(?:(?:Const|Dim)\\s+)?([A-Za-z_]\\w*)(?:\\(\\s*([0-9]+)\\s*\\))?\\s*=\\s*(.*)$",ci);
 static const QRegularExpression branch("\\b(?:Then|Else)\\s+",ci);
 for(const auto &line:lines){const auto conditional=branch.match(line.syntax);const auto call=load.match(line.syntax);if(call.hasMatch()){
   const auto name=call.captured().trimmed().section(QRegularExpression("\\s+"),-1).toCaseFolded();
   if(standardLoader(name)){pinmame=true;unconditionalPinmame|=!conditional.hasMatch()&&!line.conditional;}else dynamicController=true;
  }
  auto text=line.text;if(conditional.hasMatch())text=text.mid(conditional.capturedEnd());
  if(controllerLoad.match(conditional.hasMatch()?line.syntax.mid(conditional.capturedEnd()):line.syntax).hasMatch()){
   const auto kind=controllerKind.match(text);if(!kind.hasMatch())dynamicController=true;
   else if(kind.captured(1).startsWith("VPM",Qt::CaseInsensitive)){pinmame=true;unconditionalPinmame|=!conditional.hasMatch()&&!line.conditional;}
  }
  const auto object=create.match(text);if(object.hasMatch()){
   const auto name=object.captured(1).toCaseFolded(),kind=object.captured(2).toCaseFolded();
   if(conditional.hasMatch()||line.conditional||(objects.contains(name)&&objects[name]!=kind))uncertainObjects.insert(name);
   objects[name]=kind;pinmame|=kind=="vpinmame.controller";unconditionalPinmame|=kind=="vpinmame.controller"&&!conditional.hasMatch()&&!line.conditional;dmd|=kind.startsWith("flexdmd.")||kind.startsWith("ultradmd.");
  }
  // Comma-separated Const declarations are common; commas inside Array(...) are retained.
  for(const auto &part:split(text,',')){const auto m=assignment.match(part);if(m.hasMatch()){
   auto name=m.captured(1).toCaseFolded();if(!m.captured(2).isEmpty())name+='('+m.captured(2)+')';assignments[name]<<m.captured(3).trimmed();
   if(!line.conditional&&!conditional.hasMatch())certainAssignments.insert(name);
  }}
 }
 if(pinmame&&!objects.contains("controller"))objects["controller"]="vpinmame.controller";
 std::function<Values(QString,QSet<QString>,int)> resolve;int resolutions=0;
 resolve=[&](QString expression,QSet<QString> seen,int depth)->Values{
  expression=expression.trimmed();if(depth>16||++resolutions>8192)return {{},false};
  static const QRegularExpression literal("^\"([^\"\\r\\n]{1,128})\"$"),reference("^([A-Za-z_]\\w*)(?:\\((.*)\\))?$"),array("^Array\\s*\\((.*)\\)$",ci);
  auto m=literal.match(expression);if(m.hasMatch())return {{m.captured(1).toCaseFolded()},true};
  m=array.match(expression);if(m.hasMatch()){Values v;for(const auto &part:split(m.captured(1),',')){const auto item=resolve(part,seen,depth+1);v.names+=item.names;v.complete&=item.complete;if(v.names.size()>64)return {{},false};}return v;}
  const auto concat=split(expression,'&');if(concat.size()>1){Values v{{QString()},true};for(const auto &part:concat){const auto item=resolve(part,seen,depth+1);v.complete&=item.complete;QStringList names;for(const auto &a:v.names)for(const auto &b:item.names){if(names.size()>=64)return {{},false};names<<a+b;}v.names=names;}return v;}
  m=reference.match(expression);if(!m.hasMatch())return {{},false};auto name=m.captured(1).toCaseFolded();const auto index=m.captured(2).trimmed();
  if(!index.isEmpty()&&assignments.contains(name+'('+index+')'))name+='('+index+')';
  if(seen.contains(name))return {{},false};
  seen.insert(name);Values values;
  if(assignments.contains(name)){values.complete=certainAssignments.contains(name);if(assignments[name].size()>128)return {{},false};for(const auto &rhs:assignments[name]){const auto v=resolve(rhs,seen,depth+1);values.names+=v.names;values.complete&=v.complete;if(values.names.size()>64)return {{},false};}}
  else {values.complete=false;for(auto it=assignments.cbegin();it!=assignments.cend();++it)if(it.key().startsWith(name+'(')){for(const auto &rhs:it.value())values.names+=resolve(rhs,seen,depth+1).names;}}
  if(!index.isEmpty()&&name==m.captured(1).toCaseFolded()&&assignments.value(name).size()==1){bool ok=false;const int i=index.toInt(&ok);if(ok){if(i>=0&&i<values.names.size())values.names={values.names[i]};else return {{},false};}}
  // An array's indexed writes can change any runtime-selected member; a base Array(...) is not the complete set.
  if(name==m.captured(1).toCaseFolded())for(auto it=assignments.cbegin();it!=assignments.cend();++it)if(it.key().startsWith(name+'(')){values.complete=false;break;}
  if(values.names.size()>64)return {{},false};
  return values;
 };
 QStringList with;QStringList sinks;bool otherSink=false,uncertainSink=false;
 static const QRegularExpression withStart("^\\s*With\\s+([A-Za-z_]\\w*)\\s*$",ci),withEnd("^\\s*End\\s+With\\b",ci);
 static const QRegularExpression property("(?:^|\\bThen\\s+|\\bElse\\s+)\\s*(?:([A-Za-z_]\\w*)\\s*)?\\.\\s*GameName\\s*=\\s*(.*)$",ci);
 for(const auto &line:lines){auto m=withStart.match(line.syntax);if(m.hasMatch()){with<<m.captured(1).toCaseFolded();continue;}if(withEnd.match(line.syntax).hasMatch()){if(!with.isEmpty())with.removeLast();continue;}
  m=property.match(line.syntax);if(!m.hasMatch())continue;const auto receiver=m.captured(1).isEmpty()?with.value(with.size()-1):m.captured(1).toCaseFolded();
  if(uncertainObjects.contains(receiver)||line.conditional||branch.match(line.syntax).hasMatch())uncertainSink=true;
  if(objects.value(receiver)=="vpinmame.controller")sinks<<line.text.mid(line.syntax.indexOf('=',m.capturedStart())+1).trimmed();else otherSink=true;
 }
 if(sinks.isEmpty()&&!(pinmame&&otherSink)){if(assignments.contains("cgamename"))sinks<<"cgamename";else if(assignments.contains("gamename"))sinks<<"gamename";}
 Values roms;for(const auto &sink:sinks){const auto v=resolve(sink,{},0);roms.names+=v.names;roms.complete&=v.complete;}
 roms.names.removeDuplicates();std::sort(roms.names.begin(),roms.names.end());
 result["romRequiresPinmame"]=pinmame;
 if(dynamicController){result["romReason"]="The script selects its controller dynamically.";return result;}
 if(!pinmame){
  if(!sinks.isEmpty()&&!roms.complete&&!dmd){result["romReason"]="The script chooses its game name dynamically.";return result;}
  result["romState"]="needs no ROM";result["romReason"]=dmd?"The script uses a DMD display without PinMAME.":"The script does not use a PinMAME controller.";return result;
 }
 result["romCandidatesComplete"]=roms.complete&&!roms.names.isEmpty()&&unconditionalPinmame&&!uncertainSink;
 if(!roms.names.isEmpty())result["romCandidates"]=roms.names;
 if(result["romCandidatesComplete"].toBool()&&roms.names.size()==1)result["rom"]=roms.names.first();
 result["romReason"]=!unconditionalPinmame||uncertainSink?"The script selects its controller conditionally.":roms.names.isEmpty()?"The PinMAME game name could not be resolved.":!roms.complete?"The script chooses its ROM dynamically.":roms.names.size()>1?"The script offers more than one ROM choice.":"The ROM has not been checked.";
 // Availability is a separate decision made against a read-only ROM inventory.
 return result;
}
bool canStart(const QVariantMap &row){return row.value("error").toString().isEmpty()&&row.value("romState").toString()!="ROM missing";}
}
