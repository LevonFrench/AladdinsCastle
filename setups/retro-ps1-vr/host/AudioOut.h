// SPDX-License-Identifier: GPL-3.0-or-later
// Stereo 16-bit output through waveOut. The emulator thread paces itself by
// waiting while too much audio is queued, which keeps the game at its own rate.
// Buffers are 20 ms; after the queue runs dry the device is paused until a few
// buffers are queued again, so a late frame costs one gap instead of crackling.
// A device that refuses buffers or stops returning them (unplugged, or the default
// device switched) is closed, so the caller can fall back to timer pacing.
#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <cstdint>
#include <cstring>
#include <vector>

class AudioOut
{
public:
  bool open(unsigned rate)
  {
    WAVEFORMATEX f{}; f.wFormatTag = WAVE_FORMAT_PCM; f.nChannels = 2; f.nSamplesPerSec = rate;
    f.wBitsPerSample = 16; f.nBlockAlign = 4; f.nAvgBytesPerSec = rate * 4;
    if (waveOutOpen(&m_out, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) { m_out = nullptr; return false; }
    m_rate = rate;
    m_chunk = rate / 50;                  // 20 ms per buffer
    for (auto &b : m_buffers) { b.data.assign(m_chunk * 2, 0); b.header = {}; }
    waveOutPause(m_out); m_paused = true; // start once kPreroll buffers are queued
    return true;
  }
  void close()
  {
    if (!m_out) return;
    waveOutReset(m_out);
    for (auto &b : m_buffers) if (b.header.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(m_out, &b.header, sizeof(WAVEHDR));
    waveOutClose(m_out); m_out = nullptr;
  }
  ~AudioOut() { close(); }
  bool active() const { return m_out != nullptr; }

  // Appends interleaved stereo frames; full buffers are queued to the device.
  void write(const std::int16_t *frames, std::size_t count)
  {
    if (!m_out) return;
    while (count) {
      Buffer &b = m_buffers[m_next];
      if (b.header.dwFlags & WHDR_PREPARED) {
        // Wait for the device to return this buffer (bounded so a stuck device cannot hang the game).
        for (int i = 0; i < 200 && !(b.header.dwFlags & WHDR_DONE); ++i) Sleep(1);
        if (!(b.header.dwFlags & WHDR_DONE)) { m_fill = 0; ++m_dropped; if (++m_stalls >= 3) fail(); return; }
        m_stalls = 0;
        waveOutUnprepareHeader(m_out, &b.header, sizeof(WAVEHDR));
        b.header = {};
      }
      const std::size_t take = (m_chunk - m_fill) < count ? (m_chunk - m_fill) : count;
      std::memcpy(b.data.data() + m_fill * 2, frames, take * 4);
      m_fill += take; frames += take * 2; count -= take;
      if (m_fill == m_chunk) {
        // The device played everything it had: hold it until the queue is refilled.
        if (!m_paused && queued() == 0) { waveOutPause(m_out); m_paused = true; ++m_underruns; }
        b.header.lpData = reinterpret_cast<LPSTR>(b.data.data());
        b.header.dwBufferLength = (DWORD)(m_chunk * 4);
        if (waveOutPrepareHeader(m_out, &b.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) { b.header = {}; fail(); return; }
        if (waveOutWrite(m_out, &b.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
          waveOutUnprepareHeader(m_out, &b.header, sizeof(WAVEHDR)); b.header = {}; fail(); return;
        }
        m_next = (m_next + 1) % kBuffers; m_fill = 0;
        if (m_paused && queued() >= kPreroll) { waveOutRestart(m_out); m_paused = false; }
      }
    }
  }
  // Holds what is queued while the game is paused; writing again resumes after the pre-roll.
  void hold() { if (m_out && !m_paused) { waveOutPause(m_out); m_paused = true; } }
  bool failed() const { return m_failed; }
  // Buffers the device has not finished playing yet.
  unsigned queued() const
  {
    unsigned n = 0;
    if (!m_out) return 0;
    for (auto &b : m_buffers) if ((b.header.dwFlags & WHDR_PREPARED) && !(b.header.dwFlags & WHDR_DONE)) ++n;
    return n;
  }
  unsigned rate() const { return m_rate; }
  unsigned underruns() const { return m_underruns; }
  unsigned dropped() const { return m_dropped; }
  // The emulator waits while more than this many buffers are queued (about 80 ms of audio).
  static const unsigned kTarget = 4;

private:
  static const unsigned kBuffers = 10;   // about 200 ms of audio at most
  static const unsigned kPreroll = 3;    // 60 ms queued before playback starts or resumes
  struct Buffer { std::vector<std::int16_t> data; WAVEHDR header; };
  HWAVEOUT m_out = nullptr;
  Buffer m_buffers[kBuffers];
  unsigned m_next = 0, m_rate = 0, m_underruns = 0, m_dropped = 0;
  bool m_paused = false, m_failed = false;
  unsigned m_stalls = 0;
  void fail() { m_failed = true; close(); }
  std::size_t m_chunk = 0, m_fill = 0;
};
