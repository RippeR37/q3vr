#ifndef __VR_SWAPCHAINS
#define __VR_SWAPCHAINS

#include "vr_types.h"

typedef enum
{
	LEFT_EYE = 1,
	RIGHT_EYE = 2,
	BOTH_EYES = 3,
} XrDesktopViewConfiguration;

void VR_GetRecommendedResolution(XrInstance instance, XrSystemId systemId, int* width, int* height);

VR_SwapchainInfos VR_CreateSwapchains(XrInstance instance, XrSystemId systemId, XrSession session);
void VR_DestroySwapchains(VR_SwapchainInfos* swapchains);

void VR_Swapchains_BindFramebuffers(VR_SwapchainInfos* swapchains, uint32_t swapchainColorIndex, uint32_t swapchainDepthIndex);
void VR_Swapchains_BlitXRToMainFbo(VR_SwapchainInfos* swapchains, uint32_t swapchainImageIndex, XrDesktopViewConfiguration viewConfig);
void VR_Swapchains_BlitXRToVirtualScreen(VR_SwapchainInfos* swapchains, uint32_t swapchainImageIndex);

void VR_Swapchains_Acquire(VR_SwapchainInfos* swapchains, uint32_t* colorIndex, uint32_t* depthIndex);
void VR_Swapchains_Release(VR_SwapchainInfos* swapchains);

#endif
