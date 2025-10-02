#include "vr_session.h"

#include <string.h>
#ifdef USE_INTERNAL_SDL_HEADERS
#include "SDL_syswm.h"
#else
#include <SDL_syswm.h>
#endif
#ifdef XR_USE_PLATFORM_XLIB
#include <GL/glx.h>
#endif

#include "vr_macros.h"

XrResult VR_CreateSession(XrInstance instance, XrSystemId systemId, XrSession* session)
{
#ifdef XR_USE_PLATFORM_WIN32
	XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {};
	graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
	graphicsBinding.next = NULL;
	graphicsBinding.hDC = wglGetCurrentDC();
	graphicsBinding.hGLRC = wglGetCurrentContext();

	CHECK(graphicsBinding.hDC != NULL, "[OpenXR] Failed to obtain current Device Context");
	CHECK(graphicsBinding.hGLRC != NULL, "[OpenXR] Failed to obtain current OpenGL RC");
#elif defined(XR_USE_PLATFORM_XLIB)
	extern SDL_Window *SDL_window;
	SDL_SysWMinfo wminfo;
	SDL_VERSION(&wminfo.version);
	SDL_GetWindowWMInfo(SDL_window, &wminfo);

	XrGraphicsBindingOpenGLXlibKHR graphicsBinding;
	memset(&graphicsBinding, 0, sizeof(graphicsBinding));
	graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
	graphicsBinding.next = NULL;
	graphicsBinding.xDisplay = wminfo.info.x11.display;
	graphicsBinding.glxContext = glXGetCurrentContext();
	graphicsBinding.glxDrawable = glXGetCurrentDrawable();
	// graphicsBinding.visualid = wminfo.info.x11.visualid;
#else
#	error "Unsupported platform"
#endif
	XrSessionCreateInfo sessionCreateInfo = {};
	memset(&sessionCreateInfo, 0, sizeof(sessionCreateInfo));
	sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
	sessionCreateInfo.next = &graphicsBinding;
	sessionCreateInfo.createFlags = 0;
	sessionCreateInfo.systemId = systemId;

	return xrCreateSession(instance, &sessionCreateInfo, session);
}

XrResult VR_BeginSession(XrSession session)
{
	XrSessionBeginInfo sessionBeginInfo;
	memset(&sessionBeginInfo, 0, sizeof(sessionBeginInfo));
	sessionBeginInfo.type = XR_TYPE_SESSION_BEGIN_INFO;
	sessionBeginInfo.next = NULL;
	sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

	return xrBeginSession(session, &sessionBeginInfo);
}

XrResult VR_EndSession(XrSession session)
{
	return xrEndSession(session);
}
