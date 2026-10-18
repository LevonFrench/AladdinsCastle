// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
class CleanQuit
{
public:
  enum class Reason { none, stop_file, y_hold, escape, time_limit };
  void request(Reason reason) { if(m_reason==Reason::none) m_reason=reason; }
  void poll(std::uint64_t now,bool yHeld,bool stopFile)
  {
    if(stopFile) request(Reason::stop_file);
    if(!yHeld) m_holding=false;
    else if(!m_holding || now<m_since) { m_holding=true; m_since=now; }
    else if(now-m_since>=3000) request(Reason::y_hold);
  }
  bool requested() const { return m_reason!=Reason::none; }
  const char *reason() const {
    switch(m_reason) {
      case Reason::stop_file:return "stop-file";
      case Reason::y_hold:return "Y held for 3 seconds";
      case Reason::escape:return "Escape";
      case Reason::time_limit:return "viewer time limit";
      default:return "none";
    }
  }
private:
  Reason m_reason=Reason::none;
  bool m_holding=false;
  std::uint64_t m_since=0;
};
