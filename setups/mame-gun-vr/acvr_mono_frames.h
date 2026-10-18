// license:BSD-3-Clause
// ACVR mono frame + gun protocol v4 ("ACVRMG"), shared by MAME and a viewer.
// Same style as model2_frames.h: one named file mapping per MAME process,
// "Local\ACVRMG-<pid>", guarded by the mutex "<name>-lock", lock-or-skip.
// No MAME headers here so a separate viewer or test tool can include it.
#ifndef MAME_FRONTEND_ACVR_MONO_FRAMES_H
#define MAME_FRONTEND_ACVR_MONO_FRAMES_H
#include <cstdint>
#include <cstring>
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace acvr_mono {
constexpr unsigned max_width = 1024, max_height = 1024, players = 2;
constexpr std::uint32_t magic = 0x474d5641; // "AVMG" little-endian
constexpr std::uint32_t protocol_version = 4;

// Written by the viewer. u,v: 0..1 on the displayed picture, top-left origin.
struct controls
{
	std::uint64_t heartbeat_ms;      // GetTickCount64() of the writer; stale after 250 ms
	std::uint32_t valid, off_screen, trigger, button2, start, coin;
	float u, v;
};

// Written by MAME: what it found and what the game now reads.
struct gun_state
{
	std::uint32_t has_x, has_y;      // aim fields found (lightgun or AD stick)
	std::uint32_t has_trigger, has_button2, has_start, has_coin;
	std::int32_t x_min, x_max, y_min, y_max;   // field ranges (adjusted)
	std::int32_t x_set, y_set;       // value given to set_value
	std::int32_t x_read, y_read;     // bits the game reads back from the port
	float cross_u, cross_v;          // MAME crosshair position mapped to displayed u,v
	std::uint32_t buttons;           // bit0 trigger, 1 button2, 2 start, 3 coin (as applied)
	std::uint32_t active;            // overrides currently applied
};

struct frame
{
	std::uint32_t tag, version, w, h, stride; // stride in pixels (== w)
	std::uint64_t serial;            // +1 per published frame
	float physical_aspect;           // displayed width / height (use this, not w/h)
	std::uint32_t orientation;       // MAME ORIENTATION_* bits applied to the picture
	float refresh_hz;
	std::uint32_t exit_request;      // viewer sets 1 -> MAME exits cleanly
	double emulated_seconds;
	std::uint64_t frame_number;      // MAME screen frame number
	controls input[players];
	gun_state gun[players];
	std::uint32_t pixels[max_width * max_height]; // XRGB8888, rows of 'stride'
};

inline void mapping_name(char *out, std::size_t size, unsigned long pid)
{
	std::snprintf(out, size, "Local\\ACVRMG-%lu", pid);
}

#ifdef _WIN32
class publisher
{
public:
	publisher()
	{
		char name[80], lock_name[96];
		mapping_name(name, sizeof(name), GetCurrentProcessId());
		std::snprintf(lock_name, sizeof(lock_name), "%s-lock", name);
		m_lock = CreateMutexA(nullptr, TRUE, lock_name);
		m_mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(frame), name);
		if (m_mapping) m_frame = static_cast<frame *>(MapViewOfFile(m_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(frame)));
		if (m_frame) { std::memset(m_frame, 0, sizeof(frame) - sizeof(m_frame->pixels)); m_frame->tag = magic; m_frame->version = protocol_version; }
		if (m_lock) ReleaseMutex(m_lock);
	}
	~publisher() { if (m_frame) UnmapViewOfFile(m_frame); if (m_mapping) CloseHandle(m_mapping); if (m_lock) CloseHandle(m_lock); }
	bool ok() const { return m_frame && m_lock; }

	// Lock-or-skip: returns the locked frame or nullptr; call unlock() after.
	frame *try_lock()
	{
		if (!ok()) return nullptr;
		const DWORD result = WaitForSingleObject(m_lock, 0);
		if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) return nullptr;
		return m_frame;
	}
	void unlock() { ReleaseMutex(m_lock); }

	static bool fresh(const controls &input)
	{
		const auto now = GetTickCount64();
		return input.valid && input.heartbeat_ms <= now && now - input.heartbeat_ms < 250;
	}
private:
	HANDLE m_mapping = nullptr, m_lock = nullptr;
	frame *m_frame = nullptr;
};
#else
class publisher
{
public:
	bool ok() const { return false; }
	frame *try_lock() { return nullptr; }
	void unlock() { }
	static bool fresh(const controls &) { return false; }
};
#endif
} // namespace acvr_mono
#endif
