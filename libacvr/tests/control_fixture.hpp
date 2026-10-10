// SPDX-License-Identifier: MIT
#pragma once
#include <string>
inline std::string control_header() {return "version='0.1'\nid='synthetic'\ntitle='Synthetic controls'\n[future]\nretained=true\n";}
inline std::string control_row(const std::string &id,const std::string &kind,const std::string &semantic,const std::string &control,const std::string &mode="hold",const std::string &hand="slot") {
    return "\n[[element]]\nid='"+id+"'\nlabel='Synthetic'\npart='fixture'\nnode=''\nslot=0\nplayer=0\ninput={kind='"+kind+"',semantic='"+semantic+"'}\nbinding={hand='"+hand+"',control='"+control+"',mode='"+mode+"'}\n";
}
inline std::string control_fixture() {return control_header()+control_row("fire","gun","trigger","trigger")+control_row("reload","gun","reload","offscreen_trigger","press")+control_row("cover","axis","cover_pedal","grip")+control_row("coin","button","coin","secondary","press");}
