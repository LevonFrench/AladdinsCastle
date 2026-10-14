// SPDX-License-Identifier: GPL-3.0-or-later
#include "WinGL.h"
#include "ACVRStereo.h"
#include "model2_frames.h"
#include <vector>
#include <memory>
#include <stdexcept>
#include <cstdlib>
#include <cstring>

ACVRStereoState g_acvrStereo;

// Headset-free proof still writes the real shared-memory controller return path.
// Timing follows emulated seconds during a bounded unthrottled run.
model2_frames::controls ScriptedInput(double seconds)
{
	model2_frames::controls input{};
	input.valid=1; input.u=.5f; input.v=.5f;
	auto pulse=[seconds](double time) { return seconds>=time && seconds<time+.18; };
	input.coin=pulse(20); input.start=pulse(22);
	if (pulse(24)) { input.trigger=1; input.u=.25f; }
	const double shots[] = {35,36,37,38,39,40,41,42,43};
	const float points[][2] = {{.53f,.5f},{.5f,.55f},{.5f,.5f},{.65f,.5f},{.35f,.5f},{.53f,.55f},{.5f,.4f},{.65f,.55f},{.5f,.55f}};
	for (unsigned i=0; i<sizeof(shots)/sizeof(shots[0]); ++i) if (pulse(shots[i])) {
		input.trigger=1; input.u=points[i][0]; input.v=points[i][1];
	}
	if (pulse(40.5)) { input.trigger=1; input.off_screen=1; }
	const double pictures[] = {19.5,21,23,25,32,34.9,35.10,35.9,36.10,36.9,37.10,37.9,38.10,39.10,40.10,40.60,40.9,41.10,42.10,43.10};
	for (double time : pictures) if (seconds>=time) ++input.snapshot_request;
	return input;
}

int main(int argc, char **argv)
{
	if (argc < 2) return 2;
	const DWORD pid = std::strtoul(argv[1], nullptr, 10);
	const bool scripted = argc > 2 && !std::strcmp(argv[2], "--scripted-input");
	const bool no_xr = scripted || (argc > 2 && !std::strcmp(argv[2], "--no-xr"));
	const DWORD limit = scripted ? 240000 : no_xr ? 20000 : 3600000;
	SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
	char name[80]; std::snprintf(name, sizeof(name), "Local\\ACVRM2-%lu", pid);
	HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, pid), mapping = nullptr, mutex = nullptr;
	const ULONGLONG start = GetTickCount64();
	while (!mapping && GetTickCount64() - start < 15000 && parent && WaitForSingleObject(parent, 0) == WAIT_TIMEOUT)
	{ mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name); if (!mapping) Sleep(20); }
	std::strcat(name, "-lock"); mutex = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, name);
	auto *shared = mapping ? static_cast<model2_frames::frame *>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(model2_frames::frame))) : nullptr;
	if (!shared || !mutex || !parent) { std::fprintf(stderr, "Frame transport unavailable (Win32 %lu)\n", GetLastError()); return 3; }
	auto local = std::make_unique<model2_frames::frame>();
	std::vector<unsigned> flipped(model2_frames::width * model2_frames::height), readback(flipped.size());
	Host_Window *window = nullptr; GLuint tex[2] = {}, fbo[2] = {};
	bool started = false, checked = false; unsigned frames = 0; std::uint64_t serial = 0; int result = 0;
	try
	{
		while (WaitForSingleObject(parent, 0) == WAIT_TIMEOUT && GetTickCount64() - start < limit)
		{
			MSG message; while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageA(&message); }
			const DWORD lock = WaitForSingleObject(mutex, 20);
			if (lock != WAIT_OBJECT_0 && lock != WAIT_ABANDONED) continue;
			if (GetAsyncKeyState(VK_ESCAPE) & 1) { shared->exit_request = 1; ReleaseMutex(mutex); break; }
			if (shared->tag != model2_frames::magic || shared->version != model2_frames::protocol_version) { ReleaseMutex(mutex); throw std::runtime_error("Unexpected frame transport format"); }
			model2_frames::controls input{};
			if (scripted) input=ScriptedInput(shared->emulated_seconds);
			else if (!no_xr) {
				const auto controller=ACVR_XR_ControllerInput();
				input.valid=controller.active; input.off_screen=controller.offScreen; input.u=controller.u; input.v=controller.v;
				input.trigger=controller.trigger; input.start=controller.start; input.coin=controller.coin;
			}
			input.heartbeat_ms=GetTickCount64(); shared->input=input;
			std::memcpy(local.get(), shared, sizeof(*local)); ReleaseMutex(mutex);
			if (local->w != model2_frames::width || local->h != model2_frames::height)
				throw std::runtime_error("Unexpected frame transport format");
			if (!local->serial || local->serial == serial) { Sleep(5); continue; }
			serial = local->serial;
			if (scripted) { ++frames; Sleep(1); continue; }
			if (!window)
			{
				window = Host_CreateWindow("Model 2 viewer", 0, 0, 16, 16, Host_WINDOW_HIDDEN);
				if (!window || !Host_GL_CreateContext(window) || !Host_LoadGL()) throw std::runtime_error("Hidden OpenGL context/entry points unavailable");
				std::printf("Hidden GL context: %s\n", glGetString(GL_VERSION));
				glGenTextures(2, tex); glGenFramebuffers(2, fbo);
				for (int eye = 0; eye < 2; ++eye)
				{
					glBindTexture(GL_TEXTURE_2D, tex[eye]); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, local->w, local->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glBindFramebuffer(GL_FRAMEBUFFER, fbo[eye]); glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[eye], 0);
				}
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				if (!no_xr)
				{
					char distance[40]; std::snprintf(distance, sizeof(distance), "%.9g", local->distance); _putenv_s("ACVR_XR_DISTANCE", distance);
					ACVR_XR_UpdateFov(local->tan_l, local->tan_r, local->tan_b, local->tan_t);
					started = ACVR_XR_Start(window); if (!started) throw std::runtime_error("Presenter context creation failed");
				}
			}
			for (int eye = 0; eye < 2; ++eye)
			{
				for (unsigned y = 0; y < local->h; ++y) std::memcpy(flipped.data() + (local->h - 1 - y) * local->w, local->pixels[eye][y], local->w * 4);
				glBindTexture(GL_TEXTURE_2D, tex[eye]); glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, local->w, local->h, GL_BGRA, GL_UNSIGNED_BYTE, flipped.data());
				if (!checked) { glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, readback.data()); if (readback != flipped) throw std::runtime_error("Bitmap orientation/upload roundtrip failed"); }
			}
			if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL bitmap upload failed");
			checked = true;
			if (started) { ACVR_XR_UpdateFov(local->tan_l, local->tan_r, local->tan_b, local->tan_t); ACVR_XR_Publish(fbo[0], fbo[1], local->w, local->h); }
			++frames;
		}
		std::printf("Received %u stereo frames; serial=%llu; bitmap roundtrip=%s; scripted return=%s; XR=%s; FOV tangents=(%.6f,%.6f,%.6f,%.6f)\n",
			frames, static_cast<unsigned long long>(serial), checked ? "passed" : "not used", scripted ? "enabled" : "disabled", no_xr ? "disabled" : "owner test", local->tan_l, local->tan_r, local->tan_b, local->tan_t);
		if (!frames) result = 4;
	}
	catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); result = 5; }
	if (started) ACVR_XR_Stop();
	if (window) { glDeleteFramebuffers(2, fbo); glDeleteTextures(2, tex); Host_DestroyWindow(window); }
	if (WaitForSingleObject(mutex, 100)==WAIT_OBJECT_0) { shared->input={}; ReleaseMutex(mutex); }
	UnmapViewOfFile(shared); CloseHandle(mutex); CloseHandle(mapping); CloseHandle(parent);
	return result;
}
