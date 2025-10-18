#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "vr_base.h"
#include "vr_clientinfo.h"
#include "vr_debug.h"

#include "vr_debug.h"
#include "vr_instance.h"
#include "vr_macros.h"
#include "vr_session.h"

#if __ANDROID__
#include <assert.h>
#include <unistd.h>
#endif

static VR_Engine vr_engine;
vr_clientinfo_t vr;

qboolean vr_initialized = qfalse;
qboolean vr_shutdown = qfalse;

const char* const requiredExtensionNames[] = {
		XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
		XR_EXT_DEBUG_UTILS_EXTENSION_NAME,
		XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME};
const uint32_t numRequiredExtensions =
		sizeof(requiredExtensionNames) / sizeof(requiredExtensionNames[0]);

// Part of init
void VR_InitInstanceInput( VR_Engine* );

VR_Engine* VR_Init( void )
{
	if (vr_initialized || vr_shutdown)
	{
		return &vr_engine;
	}

	memset(&vr_engine, 0, sizeof(vr_engine));
	memset(&vr, 0, sizeof(vr));

	vr.follow_mode = VRFM_THIRDPERSON_1;

	fprintf(stderr, "[OpenXR] Initilizing OpenXR instance and system...\n");

	const qboolean listApLayersExtensions = qfalse;
	if (listApLayersExtensions)
	{
		VR_ListAPILayers();
		VR_ListExtensions();
	}

	// Create the OpenXR instance.
	const char* appName = "Quake 3 Arena";
	const XrVersion apiVersion = XR_MAKE_VERSION(1, 0, 0);
	XR_CHECK(
		VR_CreateInstance(appName, apiVersion, numRequiredExtensions, requiredExtensionNames, &vr_engine.appState.Instance), 
		"Failed to create OpenXR instance");

	XrInstanceProperties instanceInfo;
	instanceInfo.type = XR_TYPE_INSTANCE_PROPERTIES;
	instanceInfo.next = NULL;
	XR_CHECK(xrGetInstanceProperties(vr_engine.appState.Instance, &instanceInfo), "Failed to query OpenXR instance properties");
	fprintf(stdout, "[OpenXR] Runtime: %s | Version: %u.%u.%u\n",
		instanceInfo.runtimeName,
		XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
		XR_VERSION_MINOR(instanceInfo.runtimeVersion),
		XR_VERSION_PATCH(instanceInfo.runtimeVersion));

	VR_CreateDebugUtilsMessenger(vr_engine.appState.Instance, &vr_engine.appState.DebugUtilsMessenger);

	XR_CHECK(
		VR_GetHMDSystem(vr_engine.appState.Instance, &vr_engine.appState.SystemId), 
		"Failed to get OpenXR system ID");


	VR_GetSystemProperties(vr_engine.appState.Instance, vr_engine.appState.SystemId, &vr_engine.systemProperties);

	// Print some debug info
	fprintf(stderr, "[OpenXR] OpenGL version requirements: [%u.%u, %u.%u]\n",
		XR_VERSION_MAJOR(vr_engine.systemProperties.GraphicsRequirements.minApiVersionSupported),
		XR_VERSION_MINOR(vr_engine.systemProperties.GraphicsRequirements.minApiVersionSupported),
		XR_VERSION_MAJOR(vr_engine.systemProperties.GraphicsRequirements.maxApiVersionSupported),
		XR_VERSION_MINOR(vr_engine.systemProperties.GraphicsRequirements.maxApiVersionSupported));
	fprintf(stderr,
		"[OpenXR] system properties:\n"
		"  System name: %s\n"
		"  Tracking: {position: %s, orientation: %s}\n"
		"  Graphics: {maxLayerCount: %d, maxSwapchainResolution: %dx%d}\n",
		vr_engine.systemProperties.SystemProperties.systemName,
		vr_engine.systemProperties.SystemProperties.trackingProperties.positionTracking ? "yes" : "no",
		vr_engine.systemProperties.SystemProperties.trackingProperties.orientationTracking ? "yes" : "no",
		vr_engine.systemProperties.SystemProperties.graphicsProperties.maxLayerCount,
		vr_engine.systemProperties.SystemProperties.graphicsProperties.maxSwapchainImageWidth,
		vr_engine.systemProperties.SystemProperties.graphicsProperties.maxSwapchainImageHeight);

	// We're done
	fprintf(stderr, 
		"[OpenXR] Instance and system succesfully initialized:\n"
		"  - Instance: %p\n"
		"  - System: %llu\n\n",
		vr_engine.appState.Instance,
		vr_engine.appState.SystemId);

	vr_initialized = qtrue;
	VR_InitInstanceInput(&vr_engine);

	return &vr_engine;
}

VR_Engine* VR_GetEngine( void )
{
	return &vr_engine;
}

void VR_Destroy( VR_Engine* engine )
{
	if (engine == &vr_engine)
	{
		VR_DestroyDebugUtilsMessenger(engine->appState.Instance, &engine->appState.DebugUtilsMessenger);
		xrDestroyInstance(engine->appState.Instance);
		memset(&vr_engine, 0, sizeof(vr_engine));
	}
	vr_initialized = qfalse;
}

void VR_PrepareForShutdown( void )
{
	vr_shutdown = qtrue;
}

void VR_EnterVR( VR_Engine* engine )
{
	if (engine->appState.Session)
	{
		fprintf(stderr, "VR_EnterVR called with existing session");
		return;
	}

	fprintf(stderr, "[OpenXR] Creating XR session and reference space\n");

	// Create the OpenXR Session.
	XR_CHECK(
		VR_CreateSession(engine->appState.Instance, engine->appState.SystemId, &engine->appState.Session),
		"Failed to create XR session");

	// Create a space to the first path
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	XR_CHECK(
		xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.HeadSpace),
		"Failed to create reference space (HEAD/VIEW)");

	fprintf(stderr, "[OpenXR] XR session and reference space created\n\n");
}

void VR_LeaveVR( VR_Engine* engine )
{
	if (engine->appState.Session) 
	{
		fprintf(stderr, "[OpenXR] Destroying XR session and reference spaces\n");

		XR_CHECK(
			xrDestroySpace(engine->appState.HeadSpace),
			"Failed to destroy reference space (HEAD/VIEW)");
		engine->appState.HeadSpace = XR_NULL_HANDLE;

		// StageSpace is optional.
		if (engine->appState.StageSpace != XR_NULL_HANDLE)
		{
			XR_CHECK(
				xrDestroySpace(engine->appState.StageSpace),
				"Failed to destroy reference space (STAGE)");
			engine->appState.StageSpace = XR_NULL_HANDLE;
		}
		XR_CHECK(
			xrDestroySpace(engine->appState.FakeStageSpace),
			"Failed to destroy reference space (FAKE STAGE)");
		engine->appState.FakeStageSpace = XR_NULL_HANDLE;
		engine->appState.CurrentSpace = XR_NULL_HANDLE;

		XR_CHECK(
			xrDestroySession(engine->appState.Session),
			"Failed to destroy XR session");
		engine->appState.Session = NULL;

		engine->appState.SessionActive = GL_FALSE;
		engine->appState.Visible = GL_FALSE;
		engine->appState.Focused = GL_FALSE;

		fprintf(stderr, "[OpenXR] XR session and reference spaces destroyed\n");
	}
}

//#endif
