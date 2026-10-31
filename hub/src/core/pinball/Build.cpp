// SPDX-License-Identifier: GPL-3.0-only
#include "Build.h"
#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QList>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
namespace ac::pinball {
QString productVersion(const QString &executable){
 const QFileInfo info(executable);
 if(!info.isFile())return {};
#ifdef Q_OS_WIN
 const auto path=QDir::toNativeSeparators(info.absoluteFilePath());
 DWORD unused=0;
 const DWORD size=GetFileVersionInfoSizeW(reinterpret_cast<LPCWSTR>(path.utf16()),&unused);
 if(!size||size>4u*1024u*1024u)return {};
 QByteArray block(static_cast<qsizetype>(size),0);
 if(!GetFileVersionInfoW(reinterpret_cast<LPCWSTR>(path.utf16()),0,size,block.data()))return {};
 // The string tables the file lists, then the usual US English ones.
 struct Translation { WORD language, codepage; };
 QList<Translation> tables;
 Translation *listed=nullptr;UINT length=0;
 if(VerQueryValueW(block.data(),L"\\VarFileInfo\\Translation",reinterpret_cast<void **>(&listed),&length)&&listed)
  for(UINT i=0;i<length/sizeof(Translation);++i)tables<<listed[i];
 tables<<Translation{0x0409,0x04b0}<<Translation{0x0409,0x04e4};
 for(const auto &t:tables){
  const auto key=QString("\\StringFileInfo\\%1%2\\ProductVersion").arg(t.language,4,16,QChar('0')).arg(t.codepage,4,16,QChar('0'));
  wchar_t *text=nullptr;UINT chars=0;
  if(VerQueryValueW(block.data(),reinterpret_cast<LPCWSTR>(key.utf16()),reinterpret_cast<void **>(&text),&chars)&&text&&chars)
   return QString::fromWCharArray(text).trimmed();
 }
#endif
 return {};
}
bool isPatchedVersion(const QString &productVersion){return productVersion.trimmed().endsWith("-acvr",Qt::CaseInsensitive);}
bool isPatchedBuild(const QString &executable){return isPatchedVersion(productVersion(executable));}
}
