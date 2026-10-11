// SPDX-License-Identifier: GPL-3.0-only
#include "core/gunroom/GunRoom.h"
#include "ProductIdentity.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScopeGuard>
#include <iostream>
#include <thread>
#include <chrono>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace {
void save(const QString &path,const QByteArray &bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly));
  QCOMPARE(file.write(bytes),bytes.size());
}
struct Fixture {
  QTemporaryDir folder;
  QString root=folder.path(),binary=folder.filePath("reviewed room.exe"),loader=folder.filePath("loader.dll");
  ac::gunroom::Inputs inputs{root,binary,loader,"synthetic-pistol","left",true};
  QJsonObject assembly;
  Fixture() {
    save(binary,"synthetic executable"); save(loader,"synthetic loader");
    const auto hash=QCryptographicHash::hash("synthetic executable",QCryptographicHash::Sha256).toHex();
    save(binary+".launch.json",QJsonDocument(QJsonObject{{"schema","gun-room-launch/1"},{"sha256",QString::fromLatin1(hash)},{"control_stdin",true},{"source_commit",QString(40,'a')}}).toJson());
    save(folder.filePath("data/product.json"),QJsonDocument(QJsonObject{{"schema",1},{"display_name",ac::product::display_name}}).toJson());
    const QJsonObject mount{{"node","grip"},{"translation",QJsonArray{0,0,0}},
                           {"rotation_xyzw",QJsonArray{0,0,0,1}}};
    auto right=mount,left=mount;
    right["asset"]="assets/hands/grip-hand-right.glb";
    left["asset"]="assets/hands/grip-hand-left.glb";
    const QJsonObject model{{"hands",QJsonObject{{"left",left},{"right",right}}},
      {"primary_mount_by_hand",QJsonObject{{"left",mount},{"right",mount}}},
      {"label_anchors",QJsonObject{}}};
    assembly={{"schema","hand-assembly/1"},{"units","metres"},{"quaternion_order","xyzw"},
      {"hand_root","grip"},{"axes",QJsonObject{{"up","+Y"},{"forward","-Z"},{"handedness","right"}}},
      {"models",QJsonObject{{"synthetic-pistol",model}}}};
    save(folder.filePath("assets/hands/assembly.json"),QJsonDocument(assembly).toJson());
    save(folder.filePath("assets/guns/synthetic-pistol.glb"),"synthetic asset");
    save(folder.filePath("data/guns/synthetic-pistol.toml"),"id=\"synthetic-pistol\"\nname=\"Synthetic pistol\"\n");
  }
};
}
class GunRoomTest : public QObject {
  Q_OBJECT
private slots:
  void explicitModelHandAndPaths() {
    Fixture f; const auto plan=ac::gunroom::plan(f.inputs);
    QVERIFY2(plan.available(),qPrintable(plan.error));
    QCOMPARE(plan.arguments,QStringList({"--root",f.root,"--xr","--model","synthetic-pistol","--hand","left","--control-stdin"}));
    QCOMPARE(plan.executable,f.binary); QCOMPARE(plan.loader,f.loader);
    QCOMPARE(ac::gunroom::models(f.root)[0].toMap().value("title").toString(),QString("Synthetic pistol"));
    auto in=f.inputs; in.hand.clear(); QVERIFY(ac::gunroom::plan(in).error.contains("hand",Qt::CaseInsensitive));
    in.hand="LEFT"; QVERIFY(!ac::gunroom::plan(in).available());
    in=f.inputs;in.model="../synthetic-pistol";QVERIFY(!ac::gunroom::plan(in).available());
    in.model="unknown-pistol";QVERIFY(!ac::gunroom::plan(in).available());
    in=f.inputs;in.executable="reviewed room.exe";QVERIFY(!ac::gunroom::plan(in).available());
    in=f.inputs;in.loader="loader.dll";QVERIFY(!ac::gunroom::plan(in).available());
    in=f.inputs;in.supported=false;QVERIFY(!ac::gunroom::plan(in).available());
  }
  void unavailableAndStaleBinaryReasons() {
    Fixture f;
    save(f.binary,"older executable");
    QVERIFY(ac::gunroom::plan(f.inputs).error.contains("changed",Qt::CaseInsensitive));
    save(f.binary,"synthetic executable");
    QFile::remove(f.binary); QVERIFY(ac::gunroom::plan(f.inputs).error.contains("executable",Qt::CaseInsensitive));
    save(f.binary,"synthetic executable");QFile::remove(f.loader);
    QVERIFY(ac::gunroom::plan(f.inputs).error.contains("loader",Qt::CaseInsensitive));
    save(f.loader,"synthetic loader");QFile::remove(f.binary+".launch.json");
    QVERIFY(ac::gunroom::plan(f.inputs).error.contains("receipt",Qt::CaseInsensitive));
    save(f.binary+".launch.json",R"({"schema":"gun-room-launch/1","control_stdin":true,"sha256":"invalid"})");
    QVERIFY(!ac::gunroom::plan(f.inputs).available());
    save(f.folder.filePath("assets/hands/assembly.json"),R"({"schema":"hand-assembly/1","models":{"synthetic-pistol":{}}})");
    QVERIFY(ac::gunroom::models(f.root).isEmpty());
  }
  void missingRootNeverBorrowsCurrentDirectory() {
    Fixture f;const auto previous=QDir::currentPath();
    const auto restore=qScopeGuard([&]{QDir::setCurrent(previous);});
    QVERIFY(QDir::setCurrent(f.root));
    QVERIFY(ac::gunroom::models(QString()).isEmpty());
    auto inputs=f.inputs;inputs.root.clear();QVERIFY(!ac::gunroom::plan(inputs).available());
  }
  void malformedHandsAndMountsRefused() {
    Fixture f;
    for(int mode=0;mode<14;++mode){
      auto document=f.assembly;
      auto models=document["models"].toObject();auto model=models["synthetic-pistol"].toObject();
      auto hands=model["hands"].toObject(),mounts=model["primary_mount_by_hand"].toObject();
      auto hand=hands["left"].toObject(),mount=mounts["right"].toObject();
      if(mode==0)hand=QJsonObject{};
      if(mode==1)mount=QJsonObject{};
      if(mode==2)hand["asset"]=123;
      if(mode==3)hand["translation"]=QJsonArray{0,0};
      if(mode==4)mount["rotation_xyzw"]=QJsonArray{0,0,0,0};
      if(mode==5)mount["rotation_xyzw"]=QJsonArray{0,0,0,"1"};
      if(mode==6)mount["node"]="unknown-node";
      if(mode==7)hand["asset"]="../unowned.glb";
      if(mode==8)hand["rotation_xyzw"]=QJsonArray{0,0,0,2};
      if(mode==9)mount["translation"]=QJsonArray{0,0,1e6};
      if(mode==10)document["axes"]=QJsonObject{{"up","+Y"},{"forward","+Z"},{"handedness","right"}};
      if(mode==11)document["units"]="millimetres";
      hands["left"]=mode==12?QJsonValue(QJsonArray{}):QJsonValue(hand);
      mounts["right"]=mode==13?QJsonValue(false):QJsonValue(mount);
      model["hands"]=hands;model["primary_mount_by_hand"]=mounts;models["synthetic-pistol"]=model;document["models"]=models;
      save(f.folder.filePath("assets/hands/assembly.json"),QJsonDocument(document).toJson());
      QVERIFY2(ac::gunroom::modelIds(f.root).isEmpty(),qPrintable(QString("Malformed metadata mode %1 was listed").arg(mode)));
      QVERIFY(!ac::gunroom::plan(f.inputs).available());
    }
  }
  void brokenModelDoesNotHideHealthyMetadata() {
    Fixture f;auto document=f.assembly;auto entries=document["models"].toObject();
    const auto healthy=entries["synthetic-pistol"].toObject();auto broken=healthy;
    broken["hands"]=QJsonObject{{"left",QJsonObject{}},{"right",QJsonObject{}}};
    entries["synthetic-pistol"]=broken;entries["healthy-pistol"]=healthy;document["models"]=entries;
    save(f.folder.filePath("assets/hands/assembly.json"),QJsonDocument(document).toJson());
    save(f.folder.filePath("assets/guns/healthy-pistol.glb"),"synthetic asset");
    save(f.folder.filePath("data/guns/healthy-pistol.toml"),"id=\"healthy-pistol\"\nname=\"Healthy fixture\"\n");
    QCOMPARE(ac::gunroom::modelIds(f.root),QStringList{"healthy-pistol"});
    auto input=f.inputs;input.model="healthy-pistol";QVERIFY(ac::gunroom::plan(input).available());
  }
  void retainedPipeCloseAndConfirmedExit() {
    ac::gunroom::Service service;
    ac::gunroom::Plan fixture;
    fixture.executable=QCoreApplication::applicationFilePath();
    fixture.root=QDir::tempPath();fixture.arguments={"--pipe-fixture"};
    QSignalSpy started(&service,&ac::gunroom::Service::started),finished(&service,&ac::gunroom::Service::finished);
    QVERIFY(service.start(fixture));QVERIFY(service.busy());QVERIFY(!service.start(fixture));
    QTRY_COMPARE(started.size(),1);
    service.close();service.close();QVERIFY(service.busy());QVERIFY(service.closing());
    QCOMPARE(finished.size(),0);
    QTRY_COMPARE(finished.size(),1);QVERIFY(finished[0][0].toString().isEmpty());
    QVERIFY(!service.busy());QVERIFY(service.output().contains("exact-close-and-eof"));
  }
  void closeWhileStartingAndFailedStart() {
    ac::gunroom::Service service;ac::gunroom::Plan fixture;
    fixture.executable=QCoreApplication::applicationFilePath();fixture.root=QDir::tempPath();fixture.arguments={"--pipe-fixture"};
    QSignalSpy finished(&service,&ac::gunroom::Service::finished);
    QVERIFY(service.start(fixture));service.close();
    QTRY_COMPARE(finished.size(),1);QVERIFY(finished[0][0].toString().isEmpty());
    fixture.executable=QDir(QDir::tempPath()).filePath("absent-gun-room-executable.exe");
    QVERIFY(service.start(fixture));QTRY_COMPARE(finished.size(),2);
    QVERIFY(!finished[1][0].toString().isEmpty());QVERIFY(!service.busy());
  }
  void destructionRetainsChildThroughOrderlyExit() {
    auto service=std::make_unique<ac::gunroom::Service>();ac::gunroom::Plan fixture;
    fixture.executable=QCoreApplication::applicationFilePath();fixture.root=QDir::tempPath();fixture.arguments={"--pipe-fixture"};
    bool finished=false;QString error;
    connect(service.get(),&ac::gunroom::Service::finished,this,[&](const QString &value){finished=true;error=value;});
    QVERIFY(service->start(fixture));service.reset();
    QVERIFY(finished);QVERIFY2(error.isEmpty(),qPrintable(error));
  }
};
int main(int argc,char **argv) {
  if(argc==2&&QByteArray(argv[1])=="--pipe-fixture") {
#ifdef Q_OS_WIN
    if(GetFileType(GetStdHandle(STD_INPUT_HANDLE))!=FILE_TYPE_PIPE)return 7;
#endif
    std::string bytes((std::istreambuf_iterator<char>(std::cin)),{});
    if(bytes!="close\n")return 9;
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    std::cout<<"exact-close-and-eof\n";return 0;
  }
  QCoreApplication app(argc,argv);GunRoomTest test;return QTest::qExec(&test,argc,argv);
}
#include "GunRoomTest.moc"
