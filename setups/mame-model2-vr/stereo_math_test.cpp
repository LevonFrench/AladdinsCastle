// license:BSD-3-Clause
#include "model2_stereo.h"
#include <cassert>
#include <cmath>

int main()
{
	// A 600-pixel focal length, 64 mm IPD, 100 units/m and a plane at 4 m.
	// At 1 m the right-eye point moves -14.4 px; at 8 m it moves +2.4 px.
	const auto project = [](float z, double eye) {
		return model2_stereo::focused_x(0, z, 600, eye, 400) / z;
	};
	assert(std::abs(project(100, 3.2) + 14.4) < 0.0001);
	assert(std::abs(project(800, 3.2) - 2.4) < 0.0001);
	assert(project(400, 3.2) == 0);
	assert(std::abs(project(100, 3.2) + project(100, -3.2)) < 0.0001);
	assert(std::abs(project(100, 6.4) - 2 * project(100, 3.2)) < 0.0001);
	assert(model2_stereo::focused_x(12, 100, 600, 0, 400) == 12);
	// Scaling both world distances and units/m leaves the projected depth unchanged.
	assert(std::abs(model2_stereo::focused_x(0, 1000, 600, 32, 4000) / 1000 + 14.4) < 0.0001);
	return 0;
}
