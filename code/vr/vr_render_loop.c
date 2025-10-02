#include "vr_render_loop.h"

#include <string.h>

#include "vr_macros.h"

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

void VR_EndFrame(XrSession session, VR_SwapchainInfos* swapchains, XrView* views, uint32_t viewCount, XrFovf fov, XrSpace space, XrTime predictedDisplayTime)
{
	XrCompositionLayerProjectionView projection_layer_elements[2] = {};

	for (uint32_t view = 0; view < viewCount; view++)
	{
		memset(&projection_layer_elements[view], 0, sizeof(XrCompositionLayerProjectionView));
		projection_layer_elements[view].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		projection_layer_elements[view].pose = views[view].pose;
		projection_layer_elements[view].fov = fov;

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
	projection_layer.space = space;
	projection_layer.viewCount = viewCount;
	projection_layer.views = projection_layer_elements;

	const XrCompositionLayerBaseHeader* layers[] = {
		(const XrCompositionLayerBaseHeader*)&projection_layer,
	};

	XrFrameEndInfo endFrameInfo = {};
	endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
	endFrameInfo.displayTime = predictedDisplayTime;
	endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	if (viewCount > 0)
	{
		endFrameInfo.layerCount = sizeof(layers) / sizeof(layers[0]);
		endFrameInfo.layers = layers;
	}
	else
	{
		endFrameInfo.layerCount = 0;
		endFrameInfo.layers = NULL;
	}

	XR_CHECK(
		xrEndFrame(session, &endFrameInfo),
		"Failed to end XR frame");
}
