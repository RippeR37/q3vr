#ifndef __VR_RENDER_LOOP
#define __VR_RENDER_LOOP

#include "../qcommon/q_shared.h"
#include "vr_types.h"

XrFrameState VR_WaitFrame(XrSession session);
void VR_BeginFrame(XrSession session);
XrViewState VR_LocateViews(XrSession session, XrTime predictedDisplayTime, XrSpace space, XrView* views, uint32_t* viewCount);
void VR_EndFrame(XrSession session, VR_SwapchainInfos* swapchains, XrView* views, uint32_t viewCount, XrFovf fov, XrSpace worldSpace, XrSpace viewSpace, XrTime predictedDisplayTime, qboolean hasScreenOverlay);

#endif
