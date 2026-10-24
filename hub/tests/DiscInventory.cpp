// SPDX-License-Identifier: GPL-3.0-only
// Offline proof driver. Output may contain private paths; never commit its output.
#include "core/scan/Scan.h"
#include "core/scan/DiscIndex.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QTextStream>
#include <QFileInfo>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);const auto args=app.arguments();if(args.size()!=2)return 2;
 std::atomic_bool stop=false;QElapsedTimer clock;clock.start();
 const auto files=ac::scan::walk({args[1]},{"iso","bin","img","cue","gdi","cdi","chd","cso","gz","pbp","m3u","wbfs","rvz","wia","gcz","gcm","wad","nkit"},16,stop);
 QVariantList rows;int read=0,matched=0;
 for(const auto &path:files){
  const auto id=ac::scan::Scanner::inspect(path,stop);const auto row=ac::scan::DiscIndex::match(id.system,id.identity);
  const bool identified=id.error.isEmpty()&&!id.identity.isEmpty()&&!id.system.isEmpty();read+=identified;matched+=identified&&!row.isEmpty();
  rows<<QVariantMap{{"file",path},{"system",id.system},{"identity",id.identity},{"error",id.error},{"matched",identified&&!row.isEmpty()},{"title",row.value("title")},{"game",row.value("game")}};
 }
 QTextStream(stdout)<<QJsonDocument::fromVariant(QVariantMap{{"files",files.size()},{"idsRead",read},{"matched",matched},{"unknown",files.size()-matched},{"seconds",clock.elapsed()/1000.0},{"rows",rows}}).toJson();
 return 0;
}
