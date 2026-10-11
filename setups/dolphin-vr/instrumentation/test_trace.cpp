// Copyright 2026 AladdinsCastle contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ACDrawTrace.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

void Require(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}

struct CommaDecimals : std::numpunct<char>
{
  char do_decimal_point() const override { return ','; }
};

int main(int argc, char** argv)
{
  try
  {
    Require(argc == 2, "Supply a private synthetic output directory");
    const std::filesystem::path directory(argv[1]);
    std::filesystem::create_directories(directory);
    Require(std::abs(ACDrawTrace::FovDegrees(1, 0) - 90) < 1e-8, "Symmetric FoV");
    Require(std::abs(ACDrawTrace::FovDegrees(1, 0.5f) - 82.8749836510982) < 1e-8,
            "Off-centre FoV");
    Require(std::isnan(ACDrawTrace::FovDegrees(0, 0)), "Invalid scale");
    Require(std::isnan(ACDrawTrace::FovDegrees(1, INFINITY)), "Invalid offset");
    ACDrawTrace::Sample sample{7, 2, 12, 0, {1, 0, 1, 0, -1, -2},
                               {320, -264, 1, 320, 264, 1}, 1, 2, 3, 4, 5, 6, 123, 456};
    const auto path = directory / "synthetic-draws.csv";
    const auto previous_locale = std::locale();
    std::locale::global(std::locale(previous_locale, new CommaDecimals));
    {
      ACDrawTrace::Logger trace(path, 2, 2);
      trace.Record(sample);             // Frame 0: perspective.
      trace.EndFrame();
      trace.Record(sample);             // Frame 1: excluded by sampling.
      trace.EndFrame();
      sample.projection_type = 1;
      trace.Record(sample);             // Frame 2: orthographic.
      trace.Record(sample);             // Excluded by row cap.
      trace.EndFrame();
    }
    std::locale::global(previous_locale);
    std::ifstream file(path);
    std::vector<std::vector<std::string>> rows;
    for (std::string line; std::getline(file, line);)
    {
      std::vector<std::string> columns;
      std::istringstream stream(line);
      for (std::string cell; std::getline(stream, cell, ',');)
        columns.push_back(cell);
      rows.push_back(columns);
    }
    Require(rows.size() == 3, "Sampling and record cap");
    for (const auto& row : rows)
      Require(row.size() == 29, "CSV field alignment");
    Require(rows[1][0] == "0" && rows[2][0] == "2", "Frame accounting");
    Require(rows[1][2] == "0" && rows[2][2] == "0", "Submission counter resets");
    Require(std::stod(rows[1][13]) == 90 && std::stod(rows[1][14]) == 90,
            "Perspective columns");
    Require(std::isnan(std::stod(rows[2][13])) && std::isnan(std::stod(rows[2][14])),
            "Orthographic has no FoV");
    Require(rows[1][3] == "7" && rows[1][26] == "6", "State columns");
    Require(rows[1][27] == "123" && rows[1][28] == "456", "Shader UID columns");
    {
      ACDrawTrace::Logger disabled(std::filesystem::path{});
      disabled.Record(sample);
      disabled.EndFrame();
    }
    std::cout << "Synthetic draw trace checks passed\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
