// SPDX-License-Identifier: GPL-3.0-only
#include "app/HubServices.h"
#include "models/GameListModel.h"
#include "ui/UiController.h"
#include "ui/UiSettings.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

namespace {
bool writeFile(const QString &path,const QByteArray &bytes){
    if(!QDir().mkpath(QFileInfo(path).absolutePath()))return false;
    QFile file(path);return file.open(QIODevice::WriteOnly|QIODevice::Truncate)&&file.write(bytes)==bytes.size();
}

// A tiny synthetic CFB with one CODE record in GameStg/GameData. It contains
// only metadata statements and has no real table content.
QByteArray syntheticTable(const QByteArray &script){
    QByteArray cfb(5632,'\0');cfb.replace(0,8,QByteArray::fromHex("d0cf11e0a1b11ae1"));
    const auto u16=[&](int at,quint16 n){qToLittleEndian(n,cfb.data()+at);};
    const auto u32=[&](int at,quint32 n){qToLittleEndian(n,cfb.data()+at);};
    u16(26,3);u16(28,0xfffe);u16(30,9);u16(32,6);u32(44,1);u32(48,0);u32(56,4096);u32(60,0xfffffffe);u32(68,0xfffffffe);
    for(int i=0;i<109;++i)u32(76+4*i,i==0?9:0xffffffff);
    cfb[512+66]=5;u32(512+76,1);u32(512+116,0xfffffffe);
    for(int e=1;e<3;++e){
        const int at=512+e*128;const QString name=e==1?"GameStg":"GameData";
        for(int i=0;i<name.size();++i)u16(at+2*i,name[i].unicode());
        u16(at+64,static_cast<quint16>((name.size()+1)*2));cfb[at+66]=e==1?1:2;
        u32(at+68,0xffffffff);u32(at+72,0xffffffff);u32(at+76,e==1?2:0xffffffff);
        u32(at+116,e==1?0xfffffffe:1);u32(at+120,e==1?0:4096);
    }
    QByteArray record;const auto append32=[&](quint32 n){char word[4];qToLittleEndian(n,word);record.append(word,4);};
    append32(4);record.append("CODE",4);append32(static_cast<quint32>(script.size()));record.append(script);
    cfb.replace(1024,record.size(),record);
    for(int i=0;i<128;++i)u32(5120+4*i,i==0||i==8?0xfffffffe:i==9?0xfffffffd:i<8?static_cast<quint32>(i+1):0xffffffff);
    return cfb;
}

QString romState(const ac::UiController &ui){
    const auto tables=ui.pinballData().value("tables").toList();
    return tables.isEmpty()?QString():tables.first().toMap().value("romState").toString();
}

void configure(ac::UiSettings &settings,const QString &root){
    settings.set("pinballFolder",QDir(root).filePath("tables"));
    settings.set("pinballRoms",QDir(root).filePath("roms"));
    settings.set("pinballExecutable",QDir(root).filePath("vpx/VPX.exe"));
}
}

class PinballCacheTest:public QObject{
    Q_OBJECT
private slots:
    void scanUsesExecutableVPinMameFolderAndAlias(){
        QTemporaryDir temp;QVERIFY(temp.isValid());
        const auto root=temp.path();const auto table=QDir(root).filePath("tables/Synthetic Table.vpx");
        QVERIFY(writeFile(table,syntheticTable("Set engine=CreateObject(\"VPinMAME.Controller\")\nengine.GameName=\"alias\"\nLoadVPM \"x\",\"fixture.vbs\",1\n")));
        QVERIFY(writeFile(QDir(root).filePath("roms/chip/chip.rom"),"synthetic ROM filename fixture"));
        QVERIFY(writeFile(QDir(root).filePath("vpx/VPX.exe"),"synthetic executable marker"));
        QVERIFY(writeFile(QDir(root).filePath("vpx/VPinMAME/VPMAlias.txt"),"alias,chip\n"));

        ac::GameListModel games(ac::CatalogData{});ac::FilterSortModel filter;filter.setSourceModel(&games);
        ac::UiSettings settings(QDir(root).filePath("user"));configure(settings,root);
        ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,root);
        ui.scanPinball();
        QTRY_VERIFY_WITH_TIMEOUT(!ui.pinballScanning()&&!ui.pinballData().value("tables").toList().isEmpty(),10000);
        QCOMPARE(romState(ui),QString("ready"));
        QFile saved(QDir(root).filePath("user/cache/pinball-scan.json"));QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto cache=QJsonDocument::fromJson(saved.readAll()).object();
        QVERIFY(cache.value("fingerprint").toObject().value("romComplete").toBool());
    }

    void unchangedRestoreUsesSavedScanWithoutScanning(){
        QTemporaryDir temp;QVERIFY(temp.isValid());const auto root=temp.path();
        const auto table=QDir(root).filePath("tables/Synthetic Table.vpx");
        QVERIFY(writeFile(table,syntheticTable("Set engine=CreateObject(\"VPinMAME.Controller\")\nengine.GameName=\"alias\"\nLoadVPM \"x\",\"fixture.vbs\",1\n")));
        QVERIFY(writeFile(QDir(root).filePath("roms/chip/chip.rom"),"synthetic ROM filename fixture"));
        QVERIFY(writeFile(QDir(root).filePath("vpx/VPX.exe"),"synthetic executable marker"));
        QVERIFY(writeFile(QDir(root).filePath("vpx/VPinMAME/VPMAlias.txt"),"alias,chip\n"));
        {
            ac::GameListModel games(ac::CatalogData{});ac::FilterSortModel filter;filter.setSourceModel(&games);
            ac::UiSettings settings(QDir(root).filePath("user"));configure(settings,root);
            ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,root);
            ui.scanPinball();QTRY_VERIFY_WITH_TIMEOUT(!ui.pinballScanning()&&!ui.pinballData().value("tables").toList().isEmpty(),10000);
        }
        ac::GameListModel games(ac::CatalogData{});ac::FilterSortModel filter;filter.setSourceModel(&games);
        ac::UiSettings settings(QDir(root).filePath("user"));configure(settings,root);
        ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,root);
        QSignalSpy scans(&ui,&ac::UiController::pinballScanRequested);
        ui.openPinball();
        QTRY_COMPARE_WITH_TIMEOUT(ui.pinballData().value("tables").toList().size(),1,10000);
        QTest::qWait(200);QCOMPARE(scans.count(),0);QCOMPARE(romState(ui),QString("ready"));
    }

    void aliasEditInvalidatesSavedScanAndRechecksAvailability(){
        QTemporaryDir temp;QVERIFY(temp.isValid());const auto root=temp.path();
        const auto table=QDir(root).filePath("tables/Synthetic Table.vpx");
        QVERIFY(writeFile(table,syntheticTable("Set engine=CreateObject(\"VPinMAME.Controller\")\nengine.GameName=\"alias\"\nLoadVPM \"x\",\"fixture.vbs\",1\n")));
        QVERIFY(writeFile(QDir(root).filePath("roms/chip/chip.rom"),"synthetic ROM filename fixture"));
        QVERIFY(writeFile(QDir(root).filePath("vpx/VPX.exe"),"synthetic executable marker"));
        const auto aliases=QDir(root).filePath("vpx/VPinMAME/VPMAlias.txt");
        QVERIFY(writeFile(aliases,"alias,chip\n"));
        {
            ac::GameListModel games(ac::CatalogData{});ac::FilterSortModel filter;filter.setSourceModel(&games);
            ac::UiSettings settings(QDir(root).filePath("user"));configure(settings,root);
            ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,root);
            ui.scanPinball();QTRY_VERIFY_WITH_TIMEOUT(!ui.pinballScanning()&&!ui.pinballData().value("tables").toList().isEmpty(),10000);
            QCOMPARE(romState(ui),QString("ready"));
        }
        QVERIFY(writeFile(aliases,"alias,other\n"));
        ac::GameListModel games(ac::CatalogData{});ac::FilterSortModel filter;filter.setSourceModel(&games);
        ac::UiSettings settings(QDir(root).filePath("user"));configure(settings,root);
        ac::UiController ui(&games,&filter,&settings);ac::HubServices services(&games,&filter,&ui,&settings,root);
        QSignalSpy scans(&ui,&ac::UiController::pinballScanRequested);
        ui.openPinball();
        QTRY_COMPARE_WITH_TIMEOUT(scans.count(),1,10000);
        QTRY_VERIFY_WITH_TIMEOUT(!ui.pinballScanning()&&!ui.pinballData().value("tables").toList().isEmpty(),10000);
        QCOMPARE(romState(ui),QString("ROM missing"));
    }
};

QTEST_GUILESS_MAIN(PinballCacheTest)
#include "PinballCacheTest.moc"
