# Gun-room OpenXR format repair candidate

The bounded null-driver run failed during swapchain initialization because this runtime did not advertise linear RGBA8. It did advertise sRGB8-alpha8. The provider now selects RGBA8 when available, otherwise the renderer-supported sRGB8-alpha8, and passes the actual selected format to the backend and gun renderer. The room enables framebuffer sRGB conversion for its linear colours when that target is selected.

A subsequent run passed initialization but reached its fifteen-second deadline without an eye capture. It closed normally through the owned diagnostic cleanup. No image or gun acceptance is claimed. Session-transition logging is included to distinguish runtime visibility/focus from rendering failures on the next run.

Source review also found that the provider exposed a depth renderbuffer through `depth_image`, although the ABI and gun renderer require a texture name. The provider now allocates, attaches and deletes its own depth texture. Runtime-owned colour images remain borrowed and are never deleted by the provider.

The latest native Release build and all fifteen CPU CTests pass under Windows PowerShell 5.1. Format-selection regression covers the actual advertised runtime list, the existing linear preference, unsupported formats and empty input. Viewer independently reviewed the current source with no blocking finding. These checks do not prove actual depth allocation, colour conversion, session focus, gun visibility or both-eye pixels. Those remain the next booked runtime gates before integration.
