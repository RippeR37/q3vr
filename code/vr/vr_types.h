#ifndef __VR_TYPES
#define __VR_TYPES


//#ifdef USE_INTERNAL_SDL
#	include "SDL_opengl.h"
//#else
//#	include <SDL_opengl.h>
//#endif

//OpenXR
#define XR_USE_GRAPHICS_API_OPENGL
#if defined(WIN32)
#include "unknwn.h"
#define XR_USE_PLATFORM_WIN32
#elif defined(__ANDROID__)
#define XR_USE_PLATFORM_ANDROID
#else
#define XR_USE_PLATFORM_XLIB
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "vr_safe_types.h"

#define OXR(func) func;

typedef struct
{
	XrSwapchain swapchain;
	int64_t swapchainFormat;
	uint32_t imageCount;
	uint32_t* images;
	uint32_t virtualScreenImage;
	
	int width;
	int height;
} VR_SwapchainInfo;

typedef struct
{
	uint32_t viewCount;
	VR_SwapchainInfo color;
	VR_SwapchainInfo depth;
	GLuint* framebuffers;
	GLuint** eyeFramebuffers; // separate FBOs with bound only single eye image
	GLuint virtualScreenFramebuffer;
} VR_SwapchainInfos;

typedef struct
{
	VR_SwapchainInfos Swapchains;
	float RefreshRate;
} VR_Renderer;

typedef struct
{
	GLboolean Active;
	XrPosef Pose;
} VR_TrackedController;

typedef struct
{
	XrInstance Instance;
	XrSystemId SystemId;
	XrSession Session;

	GLboolean SessionActive;
	GLboolean Focused;
	GLboolean Visible;

	XrDebugUtilsMessengerEXT DebugUtilsMessenger;

	XrSpace HeadSpace;
	XrSpace StageSpace;
	XrSpace FakeStageSpace;
	XrSpace CurrentSpace;

	VR_Renderer Renderer;
	VR_TrackedController TrackedController[2];
} VR_App;

typedef struct
{
	XrSystemProperties SystemProperties;
	XrGraphicsRequirementsOpenGLKHR GraphicsRequirements;
} VR_SystemProperties;

typedef struct
{
	int width;
	int height;
} VR_Window;

typedef struct
{
	VR_Window window;
	VR_SystemProperties systemProperties;
	VR_App appState;
} VR_Engine;

#endif
