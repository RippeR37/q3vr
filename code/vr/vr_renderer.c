#include "vr_renderer.h"

#include <math.h>

#include "../client/client.h"

#include "common/xr_linear.h"
#include "vr_base.h"
#include "vr_clientinfo.h"
#include "vr_events.h"
#include "vr_gameplay.h"
#include "vr_input.h"
#include "vr_macros.h"
#include "vr_math.h"
#include "vr_render_loop.h"
#include "vr_spaces.h"
#include "vr_swapchains.h"
#include "vr_types.h"
#include "vr_virtual_screen.h"

#define DEFAULT_SUPER_SAMPLING  1.1f

extern vr_clientinfo_t vr;
extern cvar_t *vr_heightAdjust;
extern cvar_t *vr_refreshrate;
extern cvar_t *vr_desktopMode;
extern cvar_t *vr_desktopMirror;

const float hudScale = M_PI * 15.0f / 180.0f;

XrBool32 stageSupported = XR_FALSE;
XrTime lastPredictedDisplayTime = 0;
qboolean frameStarted = qfalse;
qboolean needRecenter = qtrue;

// Data per-frame data held between BeginFrame and EndFrame
XrFovf fov = { 0 };
XrView views[2];
uint32_t viewCount = 2;
uint32_t swapchainColorIndex = 0;
uint32_t swapchainDepthIndex = 0;

void VR_Renderer_BeginFrame(VR_Engine* engine, XrBool32 needsRecenter);
void VR_Renderer_EndFrame(VR_Engine* engine);
void VR_Recenter(VR_Engine* engine, XrTime predictedDisplayTime);
void VR_ClearFrameBuffer( int width, int height);
void VR_UpdatePerFrameState( void );
void VR_DrawVirtualScreen(VR_SwapchainInfos* swapchains, uint32_t swapchainImageIndex, XrFovf fov, XrView* views, uint32_t viewCount);
XrDesktopViewConfiguration VR_GetDesktopViewConfiguration( void );

void VR_GetResolution(VR_Engine* engine, int *pWidth, int *pHeight)
{
	float superSampling = 0.0f;

	float configuredSuperSampling = Cvar_VariableValue("vr_superSampling");
	if (vr.superSampling == 0.0f || configuredSuperSampling != vr.superSampling)
	{
		vr.superSampling = configuredSuperSampling;
		if (vr.superSampling != 0.0f)
		{
			// Cbuf_AddText( "vid_restart\n" );
		}
	}

	if (vr.superSampling == 0.0f)
	{
		superSampling = DEFAULT_SUPER_SAMPLING;
	}
	else
	{
		superSampling = vr.superSampling;
	}

	VR_GetRecommendedResolution(engine->appState.Instance, engine->appState.SystemId, pWidth, pHeight);
	// TODO(ripper37): apply supersampling
}

void VR_InitRenderer( VR_Engine* engine )
{
	VR_GL_RegisterDebugLogSinkIfEnabled();

	// Get the supported display refresh rates for the system.
	{
		PFN_xrGetDisplayRefreshRateFB xrGetDisplayRefreshRateFB;
		XR_CHECK(
			xrGetInstanceProcAddr(engine->appState.Instance, "xrGetDisplayRefreshRateFB", (PFN_xrVoidFunction*)(&xrGetDisplayRefreshRateFB)),
			"failed to get xrGetDisplayRefreshRateFB func proc");

		engine->appState.Renderer.RefreshRate = 0.0f;
		XR_CHECK(
				xrGetDisplayRefreshRateFB(engine->appState.Session, &engine->appState.Renderer.RefreshRate),
				"failed to get current display refresh rate");
		printf("Current System Display Refresh Rate: %f\n", engine->appState.Renderer.RefreshRate);

		Cvar_SetValue("vr_refreshrate", engine->appState.Renderer.RefreshRate);
	}

	stageSupported = VR_IsStageSpaceSupported(engine->appState.Session);

	if (engine->appState.CurrentSpace == XR_NULL_HANDLE)
	{
		XrTime nullTime = 0; // won't be used anyway
		VR_Recenter(engine, nullTime);
	}

	engine->appState.Renderer.Swapchains = VR_CreateSwapchains(
		engine->appState.Instance,
		engine->appState.SystemId,
		engine->appState.Session);

	VR_VirtualScreen_Init();
	VR_VirtualScreen_ResetPosition();
}

void VR_DestroyRenderer( VR_Engine* engine )
{
	VR_VirtualScreen_Destroy();
	VR_DestroySwapchains(&engine->appState.Renderer.Swapchains);
}

void VR_ProcessFrame( VR_Engine* engine )
{
	VR_UpdatePerFrameState();

	const XrBool32 needsRecenter = VR_ProcessXrEvents(&engine->appState);
	if (engine->appState.SessionActive == GL_FALSE)
	{
		// If we haven't called Com_Frame() then let's at least process input
		// (specifically SDL events) so that app won't appear as stuck/deadlocked
		IN_Frame();
		return;
	}

	VR_Renderer_BeginFrame(engine, needsRecenter);
	Com_Frame();
	VR_Renderer_EndFrame(engine);

	if (needRecenter)
	{
		VR_Recenter(engine, lastPredictedDisplayTime);
		needRecenter = qfalse;
	}
}

void VR_Renderer_RestoreState(VR_Engine* engine)
{
	if (!frameStarted)
	{
		// Frame hasn't started, no need to restore anything here
		return;
	}

	VR_UpdatePerFrameState();

	// If we need to re-start frame until `Com_Frame()` call, we need session to
	// be active to proceed
	XrBool32 needsRecenter = XR_FALSE;
	while (!engine->appState.SessionActive)
	{
		needsRecenter |= VR_ProcessXrEvents(&engine->appState);
	}

	VR_Renderer_BeginFrame(engine, needsRecenter);
}

void VR_Renderer_BeginFrame(VR_Engine* engine, XrBool32 needsRecenter)
{
	frameStarted = qtrue;
	lastPredictedDisplayTime = VR_WaitFrame(engine->appState.Session).predictedDisplayTime;

	if (needsRecenter)
	{
		VR_Recenter(engine, lastPredictedDisplayTime);
	}

	VR_BeginFrame(engine->appState.Session);

	const XrViewState viewState = VR_LocateViews(
		engine->appState.Session,
		lastPredictedDisplayTime,
		engine->appState.CurrentSpace,
		views,
		&viewCount);

	// Update HMD position/views
	IN_VRUpdateHMD(views, viewCount, &fov);

	// [Input] poll actions, update controller state, issue action commands
	IN_VRSyncActions(engine);
	IN_VRUpdateControllers(engine, lastPredictedDisplayTime);

	VR_SwapchainInfos* swapchains = &engine->appState.Renderer.Swapchains;

	VR_Swapchains_Acquire(swapchains, &swapchainColorIndex, &swapchainDepthIndex);
	VR_Swapchains_BindFramebuffers(swapchains, swapchainColorIndex, swapchainDepthIndex);
	VR_ClearFrameBuffer(swapchains->color.width, swapchains->color.height);

	// Set renderer params
	XrMatrix4x4f vrMatrixMono, vrMatrixProjection;
	const XrFovf monoFov = { -hudScale, hudScale, hudScale, -hudScale };
	const XrFovf projectionFov =
	{
		fov.angleLeft / vr.weapon_zoomLevel,
		fov.angleRight / vr.weapon_zoomLevel,
		fov.angleUp / vr.weapon_zoomLevel,
		fov.angleDown / vr.weapon_zoomLevel,
	};
	XrMatrix4x4f_CreateProjectionFov(&vrMatrixMono, GRAPHICS_OPENGL, monoFov, 1.0f, 0.0f);
	XrMatrix4x4f_CreateProjectionFov(&vrMatrixProjection, GRAPHICS_OPENGL, projectionFov, 1.0f, 0.0f);
	re.SetVRHeadsetParms(vrMatrixProjection.m, vrMatrixMono.m, swapchains->framebuffers[swapchainColorIndex]);
}

void VR_Renderer_EndFrame(VR_Engine* engine)
{
	VR_SwapchainInfos* swapchains = &engine->appState.Renderer.Swapchains;

	// Draw Virtual Screen if needed
	const int use_virtual_screen = VR_Gameplay_ShouldRenderInVirtualScreen();
	if (use_virtual_screen)
	{
		VR_DrawVirtualScreen(swapchains, swapchainColorIndex, fov, views, viewCount);
		vr.menuYaw = VR_VirtualScreen_GetCurrentYaw();
	}
	else
	{
		VR_VirtualScreen_ResetPosition();
		vr.menuYaw = vr.hmdorientation[YAW];
	}

	VR_Swapchains_Release(swapchains);
	VR_Swapchains_BindFramebuffers(NULL, 0, 0);

	// Blit to main FBO (desktop window) - use virtual screen if active, otherwise eye view
	VR_Swapchains_BlitXRToMainFbo(swapchains, swapchainColorIndex, VR_GetDesktopViewConfiguration(), use_virtual_screen);

	VR_EndFrame(
		engine->appState.Session,
		swapchains,
		views,
		viewCount,
		fov,
		engine->appState.CurrentSpace,
		lastPredictedDisplayTime);

	// Flip desktop window's buffer
	GLimp_EndFrame();

	frameStarted = qfalse;
}

void VR_Recenter(VR_Engine* engine, XrTime predictedDisplayTime)
{
	// Calculate recenter reference
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	if (engine->appState.CurrentSpace != XR_NULL_HANDLE)
	{
		vec3_t rotation = {0, 0, 0};
		XrSpaceLocation loc = {};
		loc.type = XR_TYPE_SPACE_LOCATION;
		XR_CHECK(
			xrLocateSpace(engine->appState.HeadSpace, engine->appState.CurrentSpace, predictedDisplayTime, &loc),
			"Failed to locate space");
		QuatToYawPitchRoll(loc.pose.orientation, rotation, vr.hmdorientation);

		vr.recenterYaw += DegreesToRadians(vr.hmdorientation[YAW]);
		spaceCreateInfo.poseInReferenceSpace.orientation.x = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.y = sin(vr.recenterYaw / 2);
		spaceCreateInfo.poseInReferenceSpace.orientation.z = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.w = cos(vr.recenterYaw / 2);
	}

	// Delete previous space instances
	if (engine->appState.StageSpace != XR_NULL_HANDLE)
	{
		XR_CHECK(
			xrDestroySpace(engine->appState.StageSpace),
			"Failed to destroy stage space");
	}
	if (engine->appState.FakeStageSpace != XR_NULL_HANDLE)
	{
		XR_CHECK(
			xrDestroySpace(engine->appState.FakeStageSpace),
			"Failed to destroy fake stage space");
	}

	// Create a default stage space to use if SPACE_TYPE_STAGE is not
	// supported, or calls to xrGetReferenceSpaceBoundsRect fail.
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	spaceCreateInfo.poseInReferenceSpace.position.y = -1.6750f;
	XR_CHECK(
		xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.FakeStageSpace),
		"Failed to create reference space (fake stage)");
	printf("Created fake stage space from local space with offset\n");
	engine->appState.CurrentSpace = engine->appState.FakeStageSpace;

	if (stageSupported)
	{
		spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		spaceCreateInfo.poseInReferenceSpace.position.y = 0.0f;
		XR_CHECK(
			xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.StageSpace),
			"Failed to create reference space (stage)");
		printf("Created stage space\n");
		engine->appState.CurrentSpace = engine->appState.StageSpace;
	}

	// Update menu orientation
	vr.menuYaw = 0;

	// Reset VirtualScreen's position
	VR_VirtualScreen_ResetPosition();
}

void VR_ClearFrameBuffer( int width, int height)
{
	glEnable( GL_SCISSOR_TEST );
	glViewport( 0, 0, width, height );

	if (Cvar_VariableIntegerValue("vr_thirdPersonSpectator"))
	{
		//Blood red.. ish
		glClearColor( 0.12f, 0.0f, 0.05f, 1.0f );
	}
	else
	{
		//Black
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	}

	glScissor( 0, 0, width, height );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	glScissor( 0, 0, 0, 0 );
	glDisable( GL_SCISSOR_TEST );
}

void VR_UpdatePerFrameState( void )
{
	if (vr.weapon_zoomed)
	{
		vr.weapon_zoomLevel += 0.05;
		if (vr.weapon_zoomLevel > 2.5f)
				vr.weapon_zoomLevel = 2.5f;
	}
	else
	{
		//Zoom back out quicker
		vr.weapon_zoomLevel -= 0.25f;
		if (vr.weapon_zoomLevel < 1.0f)
				vr.weapon_zoomLevel = 1.0f;
	}
}

void VR_DrawVirtualScreen(VR_SwapchainInfos* swapchains, uint32_t swapchainImageIndex, XrFovf fov, XrView* views, uint32_t viewCount)
{
	// Copy current image to Virtual Screen's texture
	VR_Swapchains_BlitXRToVirtualScreen(swapchains, swapchainImageIndex);

	// Clear everything
	glClearColor(0.0, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Render VS
	VR_VirtualScreen_Draw(fov, &views[0].pose, &views[viewCount - 1].pose, swapchains->color.virtualScreenImage);
}

XrDesktopViewConfiguration VR_GetDesktopViewConfiguration( void )
{
	switch (vr_desktopMode->integer)
	{
		case 0:
			return LEFT_EYE;
		case 1:
			return RIGHT_EYE;
		case 2:
			return BOTH_EYES;
	}
	return LEFT_EYE;
}

qboolean VR_Renderer_SubmitLoadingFrame(VR_Engine* engine)
{
	// Only submit frames during loading states when a frame has been started
	if ((clc.state != CA_LOADING && clc.state != CA_PRIMED) || !frameStarted)
	{
		return qfalse;
	}

	// End the current VR frame (this will blit to virtual screen and submit to XR)
	VR_Renderer_EndFrame(engine);

	// Start a new VR frame for the next screen update
	VR_Renderer_BeginFrame(engine, XR_FALSE);

	return qtrue;
}
