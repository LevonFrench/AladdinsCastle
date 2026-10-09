// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QVariantMap>
namespace ac {
class UiSettings : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
 public:
 explicit UiSettings(QString userDir,QObject *parent=nullptr);
 QVariantMap values() const {return m_values;}
 Q_INVOKABLE QVariant get(const QString &key,const QVariant &fallback={}) const {return m_values.value(key,fallback);}
 Q_INVOKABLE bool set(const QString &key,const QVariant &value);
 Q_INVOKABLE QVariantMap game(const QString &id) const {return m_games.value(id).toMap();}
 Q_INVOKABLE bool saveGame(const QString &id,const QVariantMap &value);
 signals: void changed(); void error(const QString &message); void gameSaved(const QString &id,const QVariantMap &value);
 private: bool save(); QString m_path; QVariantMap m_values,m_games;
};
}
