#include "vr_session.h"

#include "vr_macros.h"

XrResult VR_CreateSession(XrInstance instance, XrSystemId systemId, XrSession* session)
{
	XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {};
	graphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
	graphicsBinding.next = NULL;
	graphicsBinding.hDC = wglGetCurrentDC();
	graphicsBinding.hGLRC = wglGetCurrentContext();

	CHECK(graphicsBinding.hDC != NULL, "[OpenXR] Failed to obtain current Device Context");
	CHECK(graphicsBinding.hGLRC != NULL, "[OpenXR] Failed to obtain current OpenGL RC");

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
