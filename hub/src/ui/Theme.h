// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "core/catalog/GameRecord.h"
#include <QObject>
#include <QColor>
namespace ac {
class Theme : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantMap tokens READ tokens CONSTANT)
 public:
 explicit Theme(const Json &data, QObject *parent=nullptr):QObject(parent),m_tokens(jsonVariant(data).toMap()) {}
 QVariantMap tokens() const {return m_tokens;}
 Q_INVOKABLE QVariant get(const QString &path) const;
 Q_INVOKABLE QColor mix(QColor a,QColor b,double amount) const;
 Q_INVOKABLE QColor alpha(QColor color,double value) const;
 Q_INVOKABLE QColor neon(QColor color) const;
 private: QVariantMap m_tokens;
};
}
