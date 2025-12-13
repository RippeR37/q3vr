#include "vr_render_loop.h"

#include <string.h>
#include <math.h>

#include "vr_macros.h"
#include "vr_clientinfo.h"
#include "vr_gameplay.h"
#include "common/xr_linear.h"

XrFrameState VR_WaitFrame(XrSession session)
{
	XrFrameWaitInfo waitFrameInfo = {};
	waitFrameInfo.type = XR_TYPE_FRAME_WAIT_INFO;
	waitFrameInfo.next = NULL;

	XrFrameState frameState = {};
	frameState.type = XR_TYPE_FRAME_STATE;
	frameState.next = NULL;

	XR_CHECK(
		xrWaitFrame(session, &waitFrameInfo, &frameState),
		"Failed to wait for XR frame");

	return frameState;
}

void VR_BeginFrame(XrSession session)
{
	XrFrameBeginInfo beginFrameDesc = {};
	beginFrameDesc.type = XR_TYPE_FRAME_BEGIN_INFO;
	beginFrameDesc.next = NULL;
	XR_CHECK(
		xrBeginFrame(session, &beginFrameDesc),
		"Failed to begin XR frame");
}

XrViewState VR_LocateViews(XrSession session, XrTime predictedDisplayTime, XrSpace space, XrView* views, uint32_t* viewCount)
{
	XrViewLocateInfo projectionInfo = {};
	projectionInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	projectionInfo.next = NULL;
	projectionInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	projectionInfo.displayTime = predictedDisplayTime;
	projectionInfo.space = space;

	XrViewState viewState = {0};
	viewState.type = XR_TYPE_VIEW_STATE;
	viewState.next = NULL;

	views[0].type = views[1].type = XR_TYPE_VIEW;
	views[0].next = views[1].next = NULL;

	XR_CHECK(
		xrLocateViews(
			session,
			&projectionInfo,
			&viewState,
			*viewCount,
			viewCount,
			views),
		"Failed to locate XR views");
	
	return viewState;
}

void VR_EndFrame(XrSession session, VR_SwapchainInfos* swapchains, XrView* views, uint32_t viewCount, XrFovf fov, XrSpace worldSpace, XrSpace viewSpace, XrTime predictedDisplayTime, qboolean hasScreenOverlay)
{
	extern vr_clientinfo_t vr;

	XrCompositionLayerProjectionView projection_layer_elements[2] = {};

	for (uint32_t view = 0; view < viewCount; view++)
	{
		memset(&projection_layer_elements[view], 0, sizeof(XrCompositionLayerProjectionView));
		projection_layer_elements[view].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		projection_layer_elements[view].pose = views[view].pose;
		projection_layer_elements[view].fov = views[view].fov;

		memset(&projection_layer_elements[view].subImage, 0, sizeof(XrSwapchainSubImage));
		projection_layer_elements[view].subImage.swapchain = swapchains->color.swapchain;
		projection_layer_elements[view].subImage.imageRect.offset.x = 0;
		projection_layer_elements[view].subImage.imageRect.offset.y = 0;
		projection_layer_elements[view].subImage.imageRect.extent.width = swapchains->color.width;
		projection_layer_elements[view].subImage.imageRect.extent.height = swapchains->color.height;
		projection_layer_elements[view].subImage.imageArrayIndex = view;
	}

	XrCompositionLayerProjection projection_layer = {};
	projection_layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
	projection_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
	projection_layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
	projection_layer.space = worldSpace;
	projection_layer.viewCount = viewCount;
	projection_layer.views = projection_layer_elements;

	// Quad layer for screen overlays (vignette, damage effects, reticle, HUD mode 2)
	// OR for full scene when weapon is zoomed (mono rendering via quad layer)
	// Using XR_EYE_VISIBILITY_BOTH - both eyes see the same quad at the same position.
	XrCompositionLayerQuad quad_layer = {};
	qboolean useQuadForScene = vr.weapon_zoomed;  // When zoomed, quad layer IS the scene

	// SP intermission overlay anchoring - track state for world-fixed UI
	static XrPosef sp_intermission_anchor_pose;
	static qboolean sp_intermission_pose_captured = qfalse;
	qboolean isSPIntermission = VR_IsSPIntermission();

	// Detect SP intermission start/end
	if (isSPIntermission && !vr.sp_intermission_active)
	{
		// First frame of SP intermission - capture anchor position
		vr.sp_intermission_active = qtrue;
		sp_intermission_pose_captured = qfalse;
	}
	else if (!isSPIntermission && vr.sp_intermission_active)
	{
		// Exiting SP intermission - reset state
		vr.sp_intermission_active = qfalse;
		sp_intermission_pose_captured = qfalse;
	}

	if (hasScreenOverlay && swapchains->screenOverlay.swapchain != XR_NULL_HANDLE && viewCount >= 2)
	{
		float distance = 0.5f;

		// Calculate the edges at the given distance using the averaged FOV
		float leftEdge = tanf(fov.angleLeft) * distance;
		float rightEdge = tanf(fov.angleRight) * distance;
		float bottomEdge = tanf(fov.angleDown) * distance;
		float topEdge = tanf(fov.angleUp) * distance;

		// The FOV center (where screen center should appear) is the midpoint
		float fovCenterX = (leftEdge + rightEdge) / 2.0f;
		float fovCenterY = (bottomEdge + topEdge) / 2.0f;

		// Quad size - use texture aspect ratio to avoid stretching
		// The texture dimensions already account for the FOV via the projection matrix
		float fovWidth = (rightEdge - leftEdge) * 1.1f;
		float fovHeight = (topEdge - bottomEdge) * 1.1f;
		float textureAspect = (float)swapchains->screenOverlay.width / (float)swapchains->screenOverlay.height;
		float fovAspect = fovWidth / fovHeight;

		float totalWidth, totalHeight;
		if (textureAspect > fovAspect) {
			// Texture is wider - fit to FOV width
			totalWidth = fovWidth;
			totalHeight = fovWidth / textureAspect;
		} else {
			// Texture is taller - fit to FOV height
			totalHeight = fovHeight;
			totalWidth = fovHeight * textureAspect;
		}

		quad_layer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
		// When zoomed, quad contains the full scene - no alpha blending needed
		// When not zoomed, quad is overlay with alpha blending
		if (useQuadForScene)
		{
			quad_layer.layerFlags = 0;  // No alpha blending - quad IS the scene
		}
		else
		{
			quad_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
		}
		quad_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;

		// During SP intermission, anchor the overlay in world space
		// so UI elements stay fixed relative to the podium
		if (isSPIntermission)
		{
			quad_layer.space = worldSpace;

			// Get current head position in worldSpace using xrLocateSpace
			XrSpaceLocation viewInWorld = {XR_TYPE_SPACE_LOCATION};
			xrLocateSpace(viewSpace, worldSpace, predictedDisplayTime, &viewInWorld);
			XrVector3f currentHeadPos = viewInWorld.pose.position;

			// Capture initial state on first frame
			static XrVector3f initialHeadPos;
			static float capturedYaw = 0.0f;
			static float targetQuadY = 0.0f;
			if (!sp_intermission_pose_captured)
			{
				initialHeadPos = currentHeadPos;

				// Extract yaw from HMD orientation
				XrQuaternionf q = views[0].pose.orientation;
				float siny_cosp = 2.0f * (q.w * q.y + q.z * q.x);
				float cosy_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
				capturedYaw = atan2f(siny_cosp, cosy_cosp);

				// Store yaw for cursor calculation
				vr.sp_intermission_yaw = capturedYaw * 180.0f / M_PI;

				// Calculate target Y position (below initial head height)
				targetQuadY = initialHeadPos.y - 1.1f;

				sp_intermission_pose_captured = qtrue;
			}

			// Distance for the quad
			float worldDistance = 4.0f;

			// Calculate position in front of where player was initially looking
			// X and Z use initial position (worldSpace X/Z appear stable)
			sp_intermission_anchor_pose.position.x = initialHeadPos.x - sinf(capturedYaw) * worldDistance;
			sp_intermission_anchor_pose.position.z = initialHeadPos.z - cosf(capturedYaw) * worldDistance;

			// Y: Compensate for LOCAL space vertical tracking
			// The worldSpace Y origin moves with head, so we counteract by
			// adjusting based on how much the head has moved from initial position
			float headYDelta = currentHeadPos.y - initialHeadPos.y;
			sp_intermission_anchor_pose.position.y = targetQuadY - headYDelta;

			// Rotation: face back toward the initial position
			float halfYaw = capturedYaw * 0.5f;
			sp_intermission_anchor_pose.orientation.x = 0.0f;
			sp_intermission_anchor_pose.orientation.y = sinf(halfYaw);
			sp_intermission_anchor_pose.orientation.z = 0.0f;
			sp_intermission_anchor_pose.orientation.w = cosf(halfYaw);

			quad_layer.pose = sp_intermission_anchor_pose;

			// Scale size for distance
			quad_layer.size.width = totalWidth * 14.0f;
			quad_layer.size.height = totalHeight * 14.0f;
		}
		else
		{
			// Normal head-locked overlay
			quad_layer.space = viewSpace;

			quad_layer.pose.orientation.x = 0.0f;
			quad_layer.pose.orientation.y = 0.0f;
			quad_layer.pose.orientation.z = 0.0f;
			quad_layer.pose.orientation.w = 1.0f;

			// Position the quad at the FOV center (accounts for asymmetric FOV)
			quad_layer.pose.position.x = fovCenterX;
			quad_layer.pose.position.y = fovCenterY;
			quad_layer.pose.position.z = -distance;

			quad_layer.size.width = totalWidth;
			quad_layer.size.height = totalHeight;
		}

		quad_layer.subImage.swapchain = swapchains->screenOverlay.swapchain;
		quad_layer.subImage.imageRect.offset.x = 0;
		quad_layer.subImage.imageRect.offset.y = 0;
		quad_layer.subImage.imageRect.extent.width = swapchains->screenOverlay.width;
		quad_layer.subImage.imageRect.extent.height = swapchains->screenOverlay.height;
		quad_layer.subImage.imageArrayIndex = 0;
	}

	// Submit layers
	const XrCompositionLayerBaseHeader* layers[2];
	int layerCount = 0;

	// When weapon is zoomed, skip projection layer - use quad layer only for true mono
	if (viewCount > 0 && !useQuadForScene)
	{
		layers[layerCount++] = (const XrCompositionLayerBaseHeader*)&projection_layer;
	}
	if (hasScreenOverlay && swapchains->screenOverlay.swapchain != XR_NULL_HANDLE && viewCount >= 2)
	{
		layers[layerCount++] = (const XrCompositionLayerBaseHeader*)&quad_layer;
	}

	XrFrameEndInfo endFrameInfo = {};
	endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
	endFrameInfo.displayTime = predictedDisplayTime;
	endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endFrameInfo.layerCount = layerCount;
	endFrameInfo.layers = layerCount > 0 ? layers : NULL;

	XR_CHECK(
		xrEndFrame(session, &endFrameInfo),
		"Failed to end XR frame");
}
