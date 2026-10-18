// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <cstdio>

inline PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
inline PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;
inline PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
inline PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
inline PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer = nullptr;
inline PFNGLFENCESYNCPROC glFenceSync=nullptr;
inline PFNGLWAITSYNCPROC glWaitSync=nullptr;
inline PFNGLDELETESYNCPROC glDeleteSync=nullptr;
inline bool Host_LoadGL()
{
#define LOAD(name) name = reinterpret_cast<decltype(name)>(wglGetProcAddress(#name)); if (!name) return false;
	LOAD(glGenFramebuffers) LOAD(glDeleteFramebuffers) LOAD(glBindFramebuffer)
	LOAD(glFramebufferTexture2D) LOAD(glBlitFramebuffer) LOAD(glFenceSync) LOAD(glWaitSync) LOAD(glDeleteSync)
#undef LOAD
	return true;
}
struct Host_Window { HWND window = nullptr; HDC dc = nullptr; HGLRC context = nullptr; };
using Host_GLContext = HGLRC;
using Uint8 = unsigned char;
#ifdef ACVR_VIEWER_TEST
inline bool (*Host_TestAllowCurrent)(HGLRC) = nullptr;
#endif
constexpr int Host_GL_SHARE_WITH_CURRENT_CONTEXT = 1, Host_WINDOWPOS_UNDEFINED = 0;
constexpr unsigned Host_WINDOW_OPENGL = 1, Host_WINDOW_HIDDEN = 2, Host_SCANCODE_HOME = 0;
inline HGLRC host_share = nullptr;
inline const char *Host_GetError() { static thread_local char error[64]; std::snprintf(error, sizeof(error), "Win32/WGL error %lu", GetLastError()); return error; }
inline int Host_GL_SetAttribute(int, int shared) { host_share = shared ? wglGetCurrentContext() : nullptr; return 0; }
inline HGLRC Host_GL_GetCurrentContext() { return wglGetCurrentContext(); }
inline Host_Window *Host_CreateWindow(const char *title, int, int, int w, int h, unsigned)
{
	WNDCLASSA cls = {}; cls.style = CS_OWNDC; cls.lpfnWndProc = DefWindowProcA;
	cls.hInstance = GetModuleHandleA(nullptr); cls.lpszClassName = "ACVRModel2HiddenGL";
	RegisterClassA(&cls);
	auto result = new Host_Window;
	result->window = CreateWindowExA(WS_EX_TOOLWINDOW, cls.lpszClassName, title, WS_POPUP,
		0, 0, w, h, nullptr, nullptr, cls.hInstance, nullptr);
	if (!result->window) { delete result; return nullptr; }
	result->dc = GetDC(result->window);
	PIXELFORMATDESCRIPTOR pfd = {}; pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cAlphaBits = 8;
	const int format = ChoosePixelFormat(result->dc, &pfd);
	if (!format || !SetPixelFormat(result->dc, format, &pfd)) { ReleaseDC(result->window, result->dc); DestroyWindow(result->window); delete result; return nullptr; }
	return result;
}
inline Host_GLContext Host_GL_CreateContext(Host_Window *window)
{
	window->context = wglCreateContext(window->dc);
	if (!window->context || (host_share && !wglShareLists(host_share, window->context))) return nullptr;
	if (!wglMakeCurrent(window->dc, window->context)) return nullptr;
	return window->context;
}
inline int Host_GL_MakeCurrent(Host_Window *window, Host_GLContext context)
{
#ifdef ACVR_VIEWER_TEST
	if (Host_TestAllowCurrent && !Host_TestAllowCurrent(context)) return -1;
#endif
	return wglMakeCurrent(context ? window->dc : nullptr, context) ? 0 : -1;
}
inline void Host_DestroyWindow(Host_Window *window)
{
	if (!window) return;
	if (wglGetCurrentContext() == window->context) wglMakeCurrent(nullptr, nullptr);
	if (window->context) wglDeleteContext(window->context);
	ReleaseDC(window->window, window->dc); DestroyWindow(window->window); delete window;
}
inline const Uint8 *Host_GetKeyboardState(int *) { static thread_local Uint8 keys[1]; keys[0] = !!(GetAsyncKeyState(VK_HOME) & 0x8000); return keys; }
inline void Host_Delay(unsigned ms) { Sleep(ms); }
