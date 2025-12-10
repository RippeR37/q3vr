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
qboolean overlayAcquiredThisFrame = qfalse;

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

	// Create VIEW reference space for head-locked quad layers
	{
		XrReferenceSpaceCreateInfo viewSpaceCI = {};
		viewSpaceCI.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
		viewSpaceCI.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
		viewSpaceCI.poseInReferenceSpace.orientation.w = 1.0f;  // Identity
		XR_CHECK(
			xrCreateReferenceSpace(engine->appState.Session, &viewSpaceCI, &engine->appState.ViewSpace),
			"Failed to create VIEW reference space for quad layer");
		printf("Created VIEW reference space for head-locked quad layers\n");
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

	// Destroy VIEW reference space
	if (engine->appState.ViewSpace != XR_NULL_HANDLE)
	{
		xrDestroySpace(engine->appState.ViewSpace);
		engine->appState.ViewSpace = XR_NULL_HANDLE;
	}
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

	// Acquire overlay swapchain for 2D screen overlays (vignette, damage, reticle, HUD mode 2)
	// Skip during loading states to avoid submitting uninitialized overlay content
	// Skip when in virtual screen mode - overlay would obscure the virtual screen
	overlayAcquiredThisFrame = qfalse;
	if (swapchains->screenOverlay.swapchain != XR_NULL_HANDLE && clc.state == CA_ACTIVE && !vr.virtual_screen)
	{
		uint32_t overlayIndex;
		VR_Swapchains_AcquireOverlay(&swapchains->screenOverlay, &overlayIndex);
		VR_Swapchains_BindOverlayFramebuffer(swapchains, overlayIndex);
		overlayAcquiredThisFrame = qtrue;

		// Tell renderer about the overlay buffer so it can bind it when drawing screen overlays
		// Also provide the main scene read buffer for mono blit when weapon is zoomed
		re.SetScreenOverlayBuffer(
			swapchains->screenOverlayFramebuffer,
			swapchains->screenOverlay.width,
			swapchains->screenOverlay.height,
			swapchains->eyeFramebuffers[0][swapchainColorIndex],
			swapchains->color.width,
			swapchains->color.height);
	}

	// Set renderer params
	// Near plane must be in Quake units to match our view matrices
	// Default r_znear is 4 Quake units. With worldscale=32, that's 4/32 = 0.125 meters
	// Using too small a near plane (like 1.0) makes things appear too close
	float nearPlane = 4.0f;  // Match r_znear default in Quake units

	XrMatrix4x4f vrMatrixMono, vrMatrixProjection;
	const XrFovf monoFov = { -hudScale, hudScale, hudScale, -hudScale };
	const XrFovf projectionFov =
	{
		fov.angleLeft / vr.weapon_zoomLevel,
		fov.angleRight / vr.weapon_zoomLevel,
		fov.angleUp / vr.weapon_zoomLevel,
		fov.angleDown / vr.weapon_zoomLevel,
	};
	XrMatrix4x4f_CreateProjectionFov(&vrMatrixMono, GRAPHICS_OPENGL, monoFov, nearPlane, 0.0f);
	XrMatrix4x4f_CreateProjectionFov(&vrMatrixProjection, GRAPHICS_OPENGL, projectionFov, nearPlane, 0.0f);

	// Create per-eye projection matrices from actual OpenXR FOVs
	// These asymmetric projections must be paired with matching per-eye view positions
	XrMatrix4x4f vrMatrixEye[2];
	for (int eye = 0; eye < 2 && eye < (int)viewCount; eye++)
	{
		XrFovf eyeFov = {
			views[eye].fov.angleLeft / vr.weapon_zoomLevel,
			views[eye].fov.angleRight / vr.weapon_zoomLevel,
			views[eye].fov.angleUp / vr.weapon_zoomLevel,
			views[eye].fov.angleDown / vr.weapon_zoomLevel,
		};
		XrMatrix4x4f_CreateProjectionFov(&vrMatrixEye[eye], GRAPHICS_OPENGL, eyeFov, nearPlane, 0.0f);
	}

	// Compute combined stereo horizontal FOV for culling (encompasses both eyes)
	// Use leftmost angle from left eye, rightmost from right eye
	// Vertical FOV doesn't need combining since eyes are horizontally separated
	float combinedAngleLeft = views[0].fov.angleLeft / vr.weapon_zoomLevel;
	float combinedAngleRight = views[1].fov.angleRight / vr.weapon_zoomLevel;

	// Convert to degrees for Q3's FOV system
	float combinedFovX = (fabsf(combinedAngleLeft) + fabsf(combinedAngleRight)) * 180.0f / M_PI;

	// Calculate half-IPD in meters for frustum plane offset
	float halfIpdMeters = 0.0f;
	if (viewCount >= 2) {
		float dx = views[1].pose.position.x - views[0].pose.position.x;
		float dy = views[1].pose.position.y - views[0].pose.position.y;
		float dz = views[1].pose.position.z - views[0].pose.position.z;
		halfIpdMeters = sqrtf(dx*dx + dy*dy + dz*dz) * 0.5f;
	}

	re.SetVRHeadsetParms(vrMatrixProjection.m, vrMatrixMono.m, swapchains->framebuffers[swapchainColorIndex],
						 vrMatrixEye[0].m, vrMatrixEye[1].m, combinedFovX, halfIpdMeters);
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

	// Release overlay swapchain only if it was acquired this frame
	if (overlayAcquiredThisFrame)
	{
		VR_Swapchains_ReleaseOverlay(&swapchains->screenOverlay);
	}

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
		engine->appState.ViewSpace,
		lastPredictedDisplayTime,
		overlayAcquiredThisFrame);

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

	// Reset viewport/scissor to full framebuffer - game rendering may have changed these
	glViewport(0, 0, swapchains->color.width, swapchains->color.height);
	glScissor(0, 0, swapchains->color.width, swapchains->color.height);
	glDisable(GL_SCISSOR_TEST);

	// Clear everything
	glClearColor(0.0, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	VR_VirtualScreen_Draw(views, viewCount, swapchains->color.virtualScreenImage);
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
