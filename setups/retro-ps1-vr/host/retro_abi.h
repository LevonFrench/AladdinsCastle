// SPDX-License-Identifier: GPL-3.0-or-later
// The subset of the libretro core ABI this host uses, written from the public
// API description. Only declarations: no libretro code is included.
#pragma once
#include <cstddef>
#include <cstdint>

#define RETRO_API_VERSION 1

#define RETRO_DEVICE_TYPE_SHIFT 8
#define RETRO_DEVICE_MASK ((1 << RETRO_DEVICE_TYPE_SHIFT) - 1)
#define RETRO_DEVICE_SUBCLASS(base, id) (((id + 1) << RETRO_DEVICE_TYPE_SHIFT) | base)

#define RETRO_DEVICE_NONE     0
#define RETRO_DEVICE_JOYPAD   1
#define RETRO_DEVICE_MOUSE    2
#define RETRO_DEVICE_KEYBOARD 3
#define RETRO_DEVICE_LIGHTGUN 4
#define RETRO_DEVICE_ANALOG   5
#define RETRO_DEVICE_POINTER  6

#define RETRO_DEVICE_ID_JOYPAD_B      0
#define RETRO_DEVICE_ID_JOYPAD_Y      1
#define RETRO_DEVICE_ID_JOYPAD_SELECT 2
#define RETRO_DEVICE_ID_JOYPAD_START  3
#define RETRO_DEVICE_ID_JOYPAD_UP     4
#define RETRO_DEVICE_ID_JOYPAD_DOWN   5
#define RETRO_DEVICE_ID_JOYPAD_LEFT   6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT  7
#define RETRO_DEVICE_ID_JOYPAD_A      8
#define RETRO_DEVICE_ID_JOYPAD_X      9
#define RETRO_DEVICE_ID_JOYPAD_L      10
#define RETRO_DEVICE_ID_JOYPAD_R      11
#define RETRO_DEVICE_ID_JOYPAD_L2     12
#define RETRO_DEVICE_ID_JOYPAD_R2     13
#define RETRO_DEVICE_ID_JOYPAD_L3     14
#define RETRO_DEVICE_ID_JOYPAD_R3     15
#define RETRO_DEVICE_ID_JOYPAD_MASK   256

#define RETRO_DEVICE_ID_LIGHTGUN_TRIGGER      2
#define RETRO_DEVICE_ID_LIGHTGUN_AUX_A        3
#define RETRO_DEVICE_ID_LIGHTGUN_AUX_B        4
#define RETRO_DEVICE_ID_LIGHTGUN_START        6
#define RETRO_DEVICE_ID_LIGHTGUN_SELECT       7
#define RETRO_DEVICE_ID_LIGHTGUN_AUX_C        8
#define RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X     13
#define RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y     14
#define RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN 15
#define RETRO_DEVICE_ID_LIGHTGUN_RELOAD       16

#define RETRO_DEVICE_INDEX_ANALOG_LEFT   0
#define RETRO_DEVICE_INDEX_ANALOG_RIGHT  1
#define RETRO_DEVICE_INDEX_ANALOG_BUTTON 2
#define RETRO_DEVICE_ID_ANALOG_X         0
#define RETRO_DEVICE_ID_ANALOG_Y         1

#define RETRO_DEVICE_ID_POINTER_X       0
#define RETRO_DEVICE_ID_POINTER_Y       1
#define RETRO_DEVICE_ID_POINTER_PRESSED 2

#define RETRO_MEMORY_SAVE_RAM 0
#define RETRO_MEMORY_SYSTEM_RAM 2

#define RETRO_ENVIRONMENT_EXPERIMENTAL 0x10000
#define RETRO_ENVIRONMENT_GET_OVERSCAN               2
#define RETRO_ENVIRONMENT_GET_CAN_DUPE               3
#define RETRO_ENVIRONMENT_SET_MESSAGE                6
#define RETRO_ENVIRONMENT_SHUTDOWN                   7
#define RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL      8
#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY       9
#define RETRO_ENVIRONMENT_SET_PIXEL_FORMAT           10
#define RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS      11
#define RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE 13
#define RETRO_ENVIRONMENT_SET_HW_RENDER              14
#define RETRO_ENVIRONMENT_GET_VARIABLE               15
#define RETRO_ENVIRONMENT_SET_VARIABLES              16
#define RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE        17
#define RETRO_ENVIRONMENT_GET_LIBRETRO_PATH          19
#define RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE       23
#define RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES 24
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE          27
#define RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY  30
#define RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY         31
#define RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO         32
#define RETRO_ENVIRONMENT_SET_CONTROLLER_INFO        35
#define RETRO_ENVIRONMENT_SET_GEOMETRY               37
#define RETRO_ENVIRONMENT_GET_USERNAME               38
#define RETRO_ENVIRONMENT_GET_LANGUAGE               39
#define RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE     (47 | RETRO_ENVIRONMENT_EXPERIMENTAL)
#define RETRO_ENVIRONMENT_GET_FASTFORWARDING         (49 | RETRO_ENVIRONMENT_EXPERIMENTAL)
#define RETRO_ENVIRONMENT_GET_INPUT_BITMASKS         (51 | RETRO_ENVIRONMENT_EXPERIMENTAL)
#define RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION   52
#define RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION 59
#define RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS        61
#define RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER    56

enum retro_hw_context_type { RETRO_HW_CONTEXT_NONE = 0, RETRO_HW_CONTEXT_OPENGL = 1, RETRO_HW_CONTEXT_OPENGLES2 = 2,
  RETRO_HW_CONTEXT_OPENGL_CORE = 3, RETRO_HW_CONTEXT_OPENGLES3 = 4, RETRO_HW_CONTEXT_OPENGLES_VERSION = 5, RETRO_HW_CONTEXT_VULKAN = 6 };
#define RETRO_HW_FRAME_BUFFER_VALID ((void *)-1)
typedef void (*retro_hw_context_reset_t)(void);
typedef std::uintptr_t (*retro_hw_get_current_framebuffer_t)(void);
typedef void (*retro_proc_address_t)(void);
typedef retro_proc_address_t (*retro_hw_get_proc_address_t)(const char *sym);
struct retro_hw_render_callback {
  retro_hw_context_type context_type; retro_hw_context_reset_t context_reset;
  retro_hw_get_current_framebuffer_t get_current_framebuffer; retro_hw_get_proc_address_t get_proc_address;
  bool depth, stencil, bottom_left_origin; unsigned version_major, version_minor; bool cache_context;
  retro_hw_context_reset_t context_destroy; bool debug_context;
};

enum retro_pixel_format { RETRO_PIXEL_FORMAT_0RGB1555 = 0, RETRO_PIXEL_FORMAT_XRGB8888 = 1, RETRO_PIXEL_FORMAT_RGB565 = 2 };
enum retro_log_level { RETRO_LOG_DEBUG = 0, RETRO_LOG_INFO, RETRO_LOG_WARN, RETRO_LOG_ERROR };
enum retro_rumble_effect { RETRO_RUMBLE_STRONG = 0, RETRO_RUMBLE_WEAK = 1 };

struct retro_message { const char *msg; unsigned frames; };
struct retro_variable { const char *key; const char *value; };
struct retro_game_info { const char *path; const void *data; std::size_t size; const char *meta; };
struct retro_system_info { const char *library_name, *library_version, *valid_extensions; bool need_fullpath, block_extract; };
struct retro_game_geometry { unsigned base_width, base_height, max_width, max_height; float aspect_ratio; };
struct retro_system_timing { double fps, sample_rate; };
struct retro_system_av_info { retro_game_geometry geometry; retro_system_timing timing; };
struct retro_controller_description { const char *desc; unsigned id; };
struct retro_input_descriptor { unsigned port, device, index, id; const char *description; };
struct retro_controller_info { const retro_controller_description *types; unsigned num_types; };

typedef void (*retro_log_printf_t)(retro_log_level level, const char *fmt, ...);
struct retro_log_callback { retro_log_printf_t log; };
typedef bool (*retro_set_rumble_state_t)(unsigned port, retro_rumble_effect effect, std::uint16_t strength);
struct retro_rumble_interface { retro_set_rumble_state_t set_rumble_state; };

typedef bool (*retro_environment_t)(unsigned cmd, void *data);
typedef void (*retro_video_refresh_t)(const void *data, unsigned width, unsigned height, std::size_t pitch);
typedef void (*retro_audio_sample_t)(std::int16_t left, std::int16_t right);
typedef std::size_t (*retro_audio_sample_batch_t)(const std::int16_t *data, std::size_t frames);
typedef void (*retro_input_poll_t)(void);
typedef std::int16_t (*retro_input_state_t)(unsigned port, unsigned device, unsigned index, unsigned id);
