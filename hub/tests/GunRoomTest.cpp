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
  Fixture() {
    save(binary,"synthetic executable"); save(loader,"synthetic loader");
    const auto hash=QCryptographicHash::hash("synthetic executable",QCryptographicHash::Sha256).toHex();
    save(binary+".launch.json",QJsonDocument(QJsonObject{{"schema","gun-room-launch/1"},{"sha256",QString::fromLatin1(hash)},{"control_stdin",true},{"source_commit",QString(40,'a')}}).toJson());
    save(folder.filePath("data/product.json"),QJsonDocument(QJsonObject{{"schema",1},{"display_name",ac::product::display_name}}).toJson());
    save(folder.filePath("assets/hands/assembly.json"),R"({"schema":"hand-assembly/1","models":{"synthetic-pistol":{"hands":{"left":{},"right":{}},"primary_mount_by_hand":{"left":{},"right":{}}}}})");
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
