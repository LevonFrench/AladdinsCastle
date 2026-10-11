// SPDX-License-Identifier: GPL-3.0-only
#include "GunRoom.h"
#include "core/install/Support.h"
#include "ProductIdentity.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <toml++/toml.hpp>
namespace ac::gunroom {
namespace {
void require(bool ok,const QString &reason) {
  if (!ok) throw install::Error("E_GUN_ROOM",reason);
}
QString localFile(const QString &path,const QString &kind) {
  require(QDir::isAbsolutePath(path),kind+" must be an absolute local path.");
  const auto checked=install::scopedPath(path,QFileInfo(path).absolutePath());
  require(QFileInfo(checked).isFile(),kind+" is unavailable at the selected path.");
  return checked;
}
QByteArray read(const QString &path,qint64 limit) {
  QFile file(path);
  require(file.open(QIODevice::ReadOnly)&&file.size()>0&&file.size()<=limit,
          "Required gun-room metadata is unreadable or too large.");
  const auto bytes=file.readAll();
  require(bytes.size()==file.size(),"Required gun-room metadata could not be read.");
  return bytes;
}
QJsonObject object(const QString &path,qint64 limit) {
  QJsonParseError error;
  const auto document=QJsonDocument::fromJson(read(path,limit),&error);
  require(error.error==QJsonParseError::NoError&&document.isObject(),"Invalid gun-room metadata.");
  return document.object();
}
}
QVariantList models(const QString &root) {
  QVariantList out;
  try {
    require(QDir::isAbsolutePath(root)&&QFileInfo(root).isDir(),"Gun-room data root is unavailable.");
    const auto document=object(install::scopedPath("assets/hands/assembly.json",root),1024*1024);
    require(document.value("schema")=="hand-assembly/1","Unsupported hand assembly.");
    const auto entries=document.value("models").toObject();
    for(auto it=entries.begin();it!=entries.end();++it) {
      if(!QRegularExpression("^[a-z0-9-]{1,64}$").match(it.key()).hasMatch()) continue;
      const auto entry=it.value().toObject();
      const auto hands=entry.value("hands").toObject(),mounts=entry.value("primary_mount_by_hand").toObject();
      if(!hands.contains("left")||!hands.contains("right")||!mounts.contains("left")||!mounts.contains("right")) continue;
      try {
        localFile(install::scopedPath("assets/guns/"+it.key()+".glb",root),"Gun model");
        const auto file=install::scopedPath("data/guns/"+it.key()+".toml",root);
        const auto descriptor=toml::parse(read(file,64*1024).toStdString());
        if(descriptor["id"].value_or(std::string{})!=it.key().toStdString()) continue;
        const auto name=QString::fromStdString(descriptor["name"].value_or(it.key().toStdString())).left(128);
        out<<QVariantMap{{"id",it.key()},{"title",name}};
      } catch(const std::exception &) { /* A broken model does not hide the others. */ }
    }
  } catch(const std::exception &) { }
  return out;
}
QStringList modelIds(const QString &root) {
  QStringList out;
  for(const auto &choice:models(root)) out<<choice.toMap().value("id").toString();
  return out;
}
Inputs systemInputs(const QString &root, const QString &executable,
                    const QString &model, const QString &hand) {
  Inputs out{root,executable,qEnvironmentVariable("ACVR_OPENXR_LOADER_DLL"),model,hand,false};
#ifdef Q_OS_WIN
  out.supported=true;
  if(out.loader.isEmpty()) out.loader=QDir(qEnvironmentVariable("SystemRoot")).filePath("System32/openxr_loader.dll");
#endif
  return out;
}
Plan plan(const Inputs &in) {
  Plan out;
  try {
    require(in.supported,"The native gun room requires Windows desktop OpenXR.");
    require(in.hand=="left"||in.hand=="right","Choose a left or right hand explicitly.");
    require(QDir::isAbsolutePath(in.root)&&QFileInfo(in.root).isDir(),"Gun-room data root is unavailable.");
    out.root=install::scopedPath(".",in.root);
    require(modelIds(out.root).contains(in.model),"Choose an available gun model with authored hand mounts.");
    const auto identity=object(install::scopedPath("data/product.json",out.root),16*1024);
    require(identity.value("schema")==1&&identity.value("display_name").toString()==QString::fromUtf8(product::display_name),
            "Gun-room product identity differs from this Hub build.");
    out.executable=localFile(in.executable,"Reviewed gun-room executable");
    out.loader=localFile(in.loader,"OpenXR loader");
    const auto receiptPath=out.executable+".launch.json";
    require(QFileInfo(receiptPath).isFile(),"The reviewed gun-room launch receipt is unavailable beside the executable.");
    const auto receipt=object(localFile(receiptPath,"Launch receipt"),16*1024);
    require(receipt.value("schema")=="gun-room-launch/1"&&receipt.value("control_stdin")==true&&
            QRegularExpression("^[a-f0-9]{40}$").match(receipt.value("source_commit").toString()).hasMatch(),
            "The gun-room launch receipt does not admit cooperative stdin close.");
    const auto expected=receipt.value("sha256").toString();
    require(QRegularExpression("^[a-f0-9]{64}$").match(expected).hasMatch(),"Invalid executable hash in gun-room launch receipt.");
    QFile binary(out.executable);QCryptographicHash hash(QCryptographicHash::Sha256);
    require(binary.open(QIODevice::ReadOnly)&&binary.size()>0&&binary.size()<=256*1024*1024&&hash.addData(&binary)&&
            QString::fromLatin1(hash.result().toHex())==expected,
            "Gun-room executable changed or differs from its frozen launch receipt.");
    out.model=in.model;out.hand=in.hand;
    out.arguments={"--root",out.root,"--xr","--model",in.model,"--hand",in.hand,"--control-stdin"};
  } catch(const std::exception &error) { out.error=QString::fromUtf8(error.what()); }
  return out;
}
Service::Service(QObject *parent):QObject(parent) {
  process_.setInputChannelMode(QProcess::ManagedInputChannel);
  process_.setProcessChannelMode(QProcess::MergedChannels);
  connect(&process_,&QProcess::readyReadStandardOutput,this,[this]{output_=(output_+QString::fromUtf8(process_.readAllStandardOutput())).right(4096);});
  connect(&process_,&QProcess::started,this,[this]{emit started();if(closing_)sendClose();});
  connect(&process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
    if(error==QProcess::FailedToStart) complete("Gun-room process could not start: "+process_.errorString());
  });
  connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
    output_=(output_+QString::fromUtf8(process_.readAllStandardOutput())).right(4096);
    complete(code==0&&status==QProcess::NormalExit?QString():QString("Gun room exited with code %1.\n%2").arg(code).arg(output_));
  });
}
Service::~Service() {
  // QProcess destruction kills a live child. Retain it through orderly exit,
  // including an unexpected Hub shutdown; a blocked XR call can keep us waiting.
  if(process_.state()!=QProcess::NotRunning) { close();process_.waitForFinished(-1); }
}
bool Service::start(const Plan &plan) {
  if(active_||!plan.available()) return false;
  active_=true;closing_=false;output_.clear();
  process_.setWorkingDirectory(plan.root);
  auto environment=QProcessEnvironment::systemEnvironment();
  if(!plan.loader.isEmpty()) environment.insert("ACVR_OPENXR_LOADER_DLL",plan.loader);
  process_.setProcessEnvironment(environment);
  emit changed();process_.start(plan.executable,plan.arguments,QIODevice::ReadWrite);return true;
}
void Service::close() {
  if(!active_||closing_) return;
  closing_=true;emit changed();
  if(process_.state()==QProcess::Running) sendClose();
}
void Service::sendClose() {
  process_.write("close\n",6);
  // Qt closes only after queued bytes drain. EOF is also a native close command.
  process_.closeWriteChannel();
}
void Service::complete(const QString &error) {
  if(!active_) return;
  active_=false;closing_=false;emit changed();emit finished(error);
}
}
