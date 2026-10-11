// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QVariantList>
namespace ac::gunroom {
struct Inputs {
  QString root, executable, loader, model, hand;
  bool supported = false;
  bool calibrateGrip = false;
};
struct Plan {
  QString executable, root, loader, model, hand, error;
  QStringList arguments;
  bool calibrationCapable = false;
  bool available() const { return error.isEmpty(); }
};
QStringList modelIds(const QString &root);
QVariantList models(const QString &root);
Inputs systemInputs(const QString &root, const QString &executable,
                    const QString &model, const QString &hand);
Plan plan(const Inputs &inputs);
class Service : public QObject {
  Q_OBJECT
public:
  explicit Service(QObject *parent = nullptr);
  ~Service() override;
  bool busy() const { return active_; }
  bool closing() const { return closing_; }
  QString output() const { return output_; }
  bool start(const Plan &plan);
  void close();
signals:
  void started();
  void changed();
  void finished(const QString &error);
private:
  void sendClose();
  void complete(const QString &error);
  QProcess process_;
  QString output_;
  bool active_ = false, closing_ = false;
};
}
