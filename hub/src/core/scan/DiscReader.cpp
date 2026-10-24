// SPDX-License-Identifier: GPL-3.0-only
#include "DiscReader.h"
#include <QElapsedTimer>
#include <QtEndian>
#include <memory>
#include <zlib.h>
namespace ac::scan::detail {
namespace {
constexpr qint64 MiB=1024*1024;
struct Disc {
 QFile &file;QString format;std::atomic_bool &cancel;QString &error;QElapsedTimer timer;
 quint64 logical=0,base=0,data=0,packed=0,entries=0,decoded=0;quint32 block=0;int shift=0;
 quint64 last=~quint64(0);QByteArray cache;
 z_stream gzip{};bool gzipReady=false;quint64 gzipEnd=0;QByteArray gzipInput,gzipWindow;
 ~Disc(){if(gzipReady)inflateEnd(&gzip);}
 Disc(QFile &f,const QString &kind,std::atomic_bool &stop,QString &issue):file(f),format(kind),cancel(stop),error(issue){timer.start();}
 QByteArray fail(const QString &why){if(error.isEmpty())error=why;return {};}
 bool stopped(){return cancel||timer.elapsed()>20000||decoded>(format=="gz"?20ULL*1024*MiB:512*quint64(MiB));}
 QByteArray physical(quint64 at,quint64 size){
  if(stopped())return fail(cancel?"Cancelled":"Disc read time/size budget exceeded");
  if(size>4*quint64(MiB)||at>quint64(file.size())||size>quint64(file.size())-at||!file.seek(qint64(at)))return fail("Compressed disc physical bounds");
  auto bytes=file.read(qint64(size));return bytes.size()==qint64(size)?bytes:fail("Truncated compressed disc");
 }
 quint32 le32(const QByteArray &b,int at){return qFromLittleEndian<quint32>(b.constData()+at);}
 quint64 le64(const QByteArray &b,int at){return qFromLittleEndian<quint64>(b.constData()+at);}
 bool init(){
  auto h=physical(0,format=="pbp"?40:format=="gcz"?32:24);if(h.isEmpty())return false;
  if(format=="gz")return h.startsWith(QByteArray("\x1f\x8b",2));
  if(format=="cso"){
   if(!h.startsWith("CISO")||quint8(h[20])>1||quint8(h[21])>16)return !fail("Unsupported CISO header").isEmpty();
   base=le32(h,4);logical=le64(h,8);block=le32(h,16);shift=quint8(h[21]);
   if(base<24)return !fail("Invalid CISO index offset").isEmpty();
   entries=block?(logical+block-1)/block:0;
  }else if(format=="gcz"){
   if(le32(h,0)!=0xb10bc001||le32(h,4)>1)return !fail("Unsupported GCZ header").isEmpty();
   packed=le64(h,8);logical=le64(h,16);block=le32(h,24);entries=le32(h,28);base=32;data=32+entries*12;
   if(data>quint64(file.size())||packed>quint64(file.size())-data)return !fail("GCZ data outside file").isEmpty();
  }else if(format=="pbp"){
   if(h.left(4)!=QByteArray("\0PBP",4))return !fail("Invalid PBP header").isEmpty();
   quint64 disc=le32(h,36);auto magic=physical(disc,16);
   if(magic.startsWith("PSTITLEIMG000000")){
    auto first=physical(disc+0x200,4);if(first.size()!=4)return false;
    const auto relative=le32(first,0);if(!relative||relative==0x44475000)return !fail("Encrypted/empty PBP disc table").isEmpty();
    disc+=relative;magic=physical(disc,16);
   }
   if(!magic.startsWith("PSISOIMG0000"))return !fail("Encrypted or unsupported PBP image").isEmpty();
   const auto marker=physical(disc+0x400,4),offset=physical(disc+0xbfc,4);
   if(marker.size()!=4||offset.size()!=4)return false;
   if(marker==QByteArray("\0PGD",4))return !fail("Encrypted PBP image unsupported").isEmpty();
   const auto relative=le32(offset,0);if(relative<0x100000)return !fail("Invalid PBP data offset").isEmpty();
   base=disc+0x4000;data=disc+relative;block=16*2352;entries=32256;logical=entries*block;
  }else return false;
  if(block<512||block>2*MiB||!logical||logical>100ULL*1024*MiB||!entries||entries>16*quint64(MiB))return !fail("Compressed disc dimensions exceed limits").isEmpty();
  if(format=="gcz"&&entries!=(logical+block-1)/block)return !fail("GCZ block count mismatch").isEmpty();
  const quint64 tableBytes=format=="pbp"?entries*32:format=="gcz"?entries*12:(entries+1)*4;
  if(base>quint64(file.size())||tableBytes>quint64(file.size())-base)return !fail("Compressed disc index outside file").isEmpty();
  return true;
 }
 QByteArray inflateBlock(const QByteArray &input,int window,quint32 expected){
  QByteArray result(qsizetype(expected)+1,0);z_stream stream{};
  if(inflateInit2(&stream,window)!=Z_OK)return fail("Inflater initialization failed");
  stream.next_in=reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));stream.avail_in=uInt(input.size());
  stream.next_out=reinterpret_cast<Bytef *>(result.data());stream.avail_out=uInt(result.size());
  const auto status=inflate(&stream,Z_FINISH);const auto length=stream.total_out;inflateEnd(&stream);
  if(status!=Z_STREAM_END||length!=expected)return fail("Invalid compressed block length");
  result.resize(qsizetype(length));return result;
 }
 QByteArray gzipAt(quint64 at,quint64 count){
  if(at+count>10ULL*1024*MiB)return fail("Gzip logical read exceeds 10 GiB budget");
  if(!gzipReady||at<gzipEnd-quint64(gzipWindow.size())){
   if(gzipReady)inflateEnd(&gzip);gzip={};gzipReady=false;gzipEnd=0;gzipWindow.clear();
   if(!file.seek(0)||inflateInit2(&gzip,31)!=Z_OK)return fail("Gzip inflater initialization failed");
   gzipReady=true;gzipInput.resize(64*1024);
  }
  QByteArray buffer(64*1024,0);int status=Z_OK;
  while(gzipEnd<at+count&&!stopped()){
   if(!gzip.avail_in){const auto n=file.read(gzipInput.data(),gzipInput.size());if(n<=0)break;gzip.next_in=reinterpret_cast<Bytef *>(gzipInput.data());gzip.avail_in=uInt(n);}
   gzip.next_out=reinterpret_cast<Bytef *>(buffer.data());gzip.avail_out=uInt(buffer.size());
   const auto before=gzip.avail_in;status=inflate(&gzip,Z_NO_FLUSH);const auto n=quint64(buffer.size())-gzip.avail_out;decoded+=n;gzipEnd+=n;
   gzipWindow+=buffer.left(qsizetype(n));if(gzipWindow.size()>4*MiB)gzipWindow.remove(0,gzipWindow.size()-4*MiB);
   if(status!=Z_OK||(n==0&&gzip.avail_in==before))break;
  }
  const auto start=gzipEnd-quint64(gzipWindow.size());
  if(gzipEnd<at+count||at<start)return fail(stopped()?"Gzip time/size budget or cancellation":"Invalid or truncated gzip stream");
  return gzipWindow.mid(qsizetype(at-start),qsizetype(count));
 }
 QByteArray read(qint64 offset,qint64 count){
  if(stopped())return fail(cancel?"Cancelled":"Disc read time/size budget exceeded");
  if(offset<0||count<0||count>2*MiB)return fail("Compressed disc read bounds");
  const auto at=quint64(offset),size=quint64(count);
  if(format=="gz")return gzipAt(at,size);
  if(at>logical||size>logical-at)return fail("Compressed disc logical bounds");
  QByteArray result;
  while(count>0&&!stopped()){
   const auto number=quint64(offset)/block,within=quint64(offset)%block;
   if(last!=number){
    quint64 start=0,length=0;bool raw=false;
    if(format=="cso"){
     const auto pair=physical(base+number*4,8);if(pair.size()!=8)return {};
     const auto a=le32(pair,0),b=le32(pair,4);raw=(a&0x80000000U)!=0;start=quint64(a&0x7fffffffU)<<shift;const auto end=quint64(b&0x7fffffffU)<<shift;
     if(end<start)return fail("CISO block order");length=end-start;
    }else if(format=="gcz"){
     const auto a=physical(base+number*8,8);if(a.size()!=8)return {};
     const auto pointer=le64(a,0);raw=(pointer>>63)!=0;start=pointer&0x7fffffffffffffffULL;
     quint64 end=packed;if(number+1<entries){const auto b=physical(base+(number+1)*8,8);if(b.size()!=8)return {};end=le64(b,0)&0x7fffffffffffffffULL;}
     if(end<start||end>packed)return fail("GCZ block order");length=end-start;start+=data;
    }else{
     const auto row=physical(base+number*32,8);if(row.size()!=8)return {};
     start=data+le32(row,0);length=qFromLittleEndian<quint16>(row.constData()+4);raw=length==block;
    }
    const auto expected=quint32(qMin(quint64(block),logical-number*block));
    if(!length||length>quint64(block)+65536||(raw&&length<expected))return fail("Compressed block size invalid");
    const auto bytes=physical(start,raw?expected:length);if(bytes.isEmpty())return {};
    if(format=="gcz"){
     const auto hash=physical(32+entries*8+number*4,4);if(hash.size()!=4)return {};
     if(le32(hash,0)!=quint32(adler32(0,reinterpret_cast<const Bytef *>(bytes.constData()),uInt(bytes.size()))))return fail("GCZ block checksum mismatch");
    }
    cache=raw?bytes:inflateBlock(bytes,format=="gcz"?15:-15,expected);if(cache.isEmpty())return {};
    decoded+=expected;last=number;
   }
   const auto n=qMin(count,qint64(cache.size()-qsizetype(within)));if(n<=0)return fail("Compressed block bounds");
   result+=cache.mid(qsizetype(within),n);offset+=n;count-=n;
  }
  return count?QByteArray():result;
 }
};
}
DiscReader compressedReader(QFile &file,const QString &format,std::atomic_bool &cancel,QString &error){
 auto disc=std::make_shared<Disc>(file,format,cancel,error);if(!disc->init())return {};
 return [disc](qint64 offset,qint64 count){return disc->read(offset,count);};
}
}
