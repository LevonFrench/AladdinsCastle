// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
class ShotFeedback
{
public:
  template<class Queue> void observe(std::uint64_t serial,std::uint64_t timestamp,std::uint64_t now,bool live,Queue queue)
  {
    if(!m_baseline || serial<m_last) { m_last=serial; m_baseline=true; return; }
    const auto edges=serial-m_last; m_last=serial; acknowledged+=edges;
    // Never replay old events after attaching late, losing focus, or stalling.
    if(!live || edges>8 || timestamp>now || now-timestamp>=250) return;
    for(std::uint64_t i=0;i<edges;++i) { queue(); ++queued; }
  }
  std::uint64_t acknowledged=0,queued=0;
private:
  std::uint64_t m_last=0;
  bool m_baseline=false;
};
