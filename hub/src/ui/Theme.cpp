// SPDX-License-Identifier: GPL-3.0-only
#include "Theme.h"
#include <algorithm>
namespace ac {
QVariant Theme::get(const QString &path) const {
 QVariant v=m_tokens; for(const auto &part:path.split('.')){if(v.metaType().id()==QMetaType::QVariantList){bool ok=false;int index=part.toInt(&ok);auto list=v.toList();v=ok&&index>=0&&index<list.size()?list[index]:QVariant{};}else v=v.toMap().value(part);} return v;
}
QColor Theme::mix(QColor a,QColor b,double t) const {
 t=std::clamp(t,0.0,1.0); return QColor::fromRgbF(a.redF()*(1-t)+b.redF()*t,a.greenF()*(1-t)+b.greenF()*t,a.blueF()*(1-t)+b.blueF()*t,a.alphaF()*(1-t)+b.alphaF()*t);
}
QColor Theme::alpha(QColor c,double value) const {c.setAlphaF(std::clamp(value,0.0,1.0));return c;}
QColor Theme::neon(QColor c) const {
 double lum=c.red()*get("formula.luminance.weight_r").toDouble()+c.green()*get("formula.luminance.weight_g").toDouble()+c.blue()*get("formula.luminance.weight_b").toDouble();
 double lift=lum>0?std::max(1.0,get("formula.neon.lift_min_lum").toDouble()/lum):1.0;
 double dim=get("formula.neon.dim").toDouble();
 return QColor::fromRgbF(std::min(1.0,c.redF()*lift)*dim,std::min(1.0,c.greenF()*lift)*dim,std::min(1.0,c.blueF()*lift)*dim);
}
}
