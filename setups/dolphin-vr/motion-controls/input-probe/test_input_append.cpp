// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone CPU parser/inbox checks. No Dolphin headers, processes or runtime.
#include "ACInputAppend.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
  using namespace ACInputProbe::Append;
  Command command;
  std::string reason;
  auto parse = [&](const std::string& text) { return ParseLine(text, 10, command, reason); };
  assert(parse("12 12.3 R primary 1") == Result::Accept);
  assert(command.hand == 1 && command.value == 1);
  assert(parse("12 14 L u -.44") == Result::Accept);
  assert(command.hand == 0 && command.control == "u");
  assert(parse("shot 12 menu-01") == Result::Accept);
  assert(command.shot && command.name == "menu-01");
  assert(parse("# ignored") == Result::Ignore);
  assert(parse(" \t") == Result::Ignore);
  for (const std::string text : {
         "9 12 R primary 1", "12 12 R primary 1", "12 200 R primary 1",
         "12 13 X primary 1", "12 13 R menu 0", "12 13 L menu 1",
         "12 13 R primary .5", "12 13 R trigger 2", "12 13 R stick_x -2",
         "12 13 R u 6", "12 13 R yaw 361", "12 13 R ang_x 101",
         "12 13 R unknown 1", "12 13 R primary nan", "12 13 R primary 1 extra",
         "shot 9 old", "shot 700 too-far", "shot 12 ../escape", "shot 12 CON",
         "shot 12 lpt9", "shot 12 bad.name", "shot 12 extra token"})
    assert(parse(text) == Result::Reject);
  assert(parse("shot 12 " + std::string(65, 'a')) == Result::Reject);
  assert(parse("shot 12 " + std::string(64, 'a')) == Result::Accept);
  assert(ParseLine("12 13 R primary 1", std::numeric_limits<double>::infinity(), command, reason) == Result::Reject);
  assert(SameName("Capture-01", "capture-01"));
  assert(!SameName("a", "b"));

  // Explicit caller-owned temporary directory; refuse existing fixture files.
  assert(argc == 2);
  const auto root = std::filesystem::path(argv[1]);
  assert(std::filesystem::is_directory(root));
  const auto file = root / "append-fixture.txt";
  assert(!std::filesystem::exists(file));
  std::vector<Command> accepted;
  std::vector<std::string> logs;
  auto accept = [&](const Command& item) { accepted.push_back(item); return true; };
  auto log = [&](const std::string& text) { logs.push_back(text); };
  { std::ofstream output(file, std::ios::binary); output << "12 12.3 R primary 1\nshot 13 partial"; }
  {
    Inbox inbox;
    inbox.Poll(file.string(), 10, accept, log);
    assert(inbox.Ready() && accepted.size() == 1);
    assert(logs.front() == "AC_INPUT_APPEND ready");
    { std::ofstream output(file, std::ios::app | std::ios::binary); output << '\n'; }
    inbox.Poll(file.string(), 10.5, accept, log);  // one-second throttle
    assert(accepted.size() == 1);
    inbox.Poll(file.string(), 11, accept, log);
    assert(accepted.size() == 2 && accepted.back().name == "partial");
    inbox.Poll(file.string(), 12, accept, log);
    assert(accepted.size() == 2);  // no re-reading/replaying old commands
    { std::ofstream output(file, std::ios::binary | std::ios::trunc); }
    inbox.Poll(file.string(), 13, accept, log);
    assert(inbox.Disabled());
    { std::ofstream output(file, std::ios::app | std::ios::binary); output << "shot 20 reset\n"; }
    inbox.Poll(file.string(), 14, accept, log);
    assert(accepted.size() == 2);  // truncation never resets the cursor
  }
  std::filesystem::remove(file);

  { std::ofstream output(file, std::ios::binary); output << std::string(MAX_BYTES + 1, '#'); }
  {
    Inbox inbox;
    inbox.Poll(file.string(), 10, accept, log);
    assert(inbox.Disabled());
  }
  std::filesystem::remove(file);
  { std::ofstream output(file, std::ios::binary); for (std::size_t i = 0; i <= MAX_LINES; ++i) output << "#\n"; }
  {
    Inbox inbox;
    inbox.Poll(file.string(), 10, accept, log);
    assert(inbox.Disabled());
  }
  std::filesystem::remove(file);
  accepted.clear();
  { std::ofstream output(file, std::ios::binary); for (std::size_t i = 0; i <= MAX_ACTIONS; ++i) output << "20 21 R primary 1\n"; }
  {
    Inbox inbox;
    inbox.Poll(file.string(), 10, accept, log);
    assert(inbox.Disabled() && accepted.size() == MAX_ACTIONS);
  }
  std::filesystem::remove(file);
  { std::ofstream output(file, std::ios::binary); output << "shot 20 late\n"; }
  {
    Inbox inbox;
    inbox.Poll(file.string(), 10, accept, log);
    inbox.Poll(file.string(), 9, accept, log);
    assert(inbox.Disabled());  // no reactivation after emulated clock rewind
  }
  std::filesystem::remove(file);
  return 0;
}
