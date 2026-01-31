#ifndef __VR_SWAPCHAINS
#define __VR_SWAPCHAINS

#include "../qcommon/q_shared.h"
#include "vr_types.h"

typedef enum
{
	LEFT_EYE = 1,
	RIGHT_EYE = 2,
	BOTH_EYES = 3,
} XrDesktopViewConfiguration;

float VR_GetSupersamplingFactor(void);
void VR_GetRecommendedResolution(XrInstance instance, XrSystemId systemId, int* width, int* height, int* maxWidth, int* maxHeight);
void VR_GetSupersampledResolution(XrInstance instance, XrSystemId systemId, int* width, int* height);

VR_SwapchainInfos VR_CreateSwapchains(XrInstance instance, XrSystemId systemId, XrSession session);
void VR_DestroySwapchains(VR_SwapchainInfos* swapchains);

void VR_Swapchains_BindFramebuffers(VR_SwapchainInfos* swapchains, uint32_t swapchainColorIndex);
void VR_Swapchains_BlitXRToMainFbo(VR_SwapchainInfos* swapchains, uint32_t swapchainImageIndex, XrDesktopViewConfiguration viewConfig, qboolean useVirtualScreen);
void VR_Swapchains_BlitXRToVirtualScreen(VR_SwapchainInfos* swapchains, uint32_t swapchainImageIndex);

void VR_Swapchains_Acquire(VR_SwapchainInfos* swapchains, uint32_t* colorIndex);
void VR_Swapchains_Release(VR_SwapchainInfos* swapchains);

// Screen overlay (quad layer) swapchain functions
void VR_Swapchains_AcquireOverlay(VR_SwapchainInfo* overlay, uint32_t* index);
void VR_Swapchains_ReleaseOverlay(VR_SwapchainInfo* overlay);
void VR_Swapchains_BindOverlayFramebuffer(VR_SwapchainInfos* swapchains, uint32_t index);
GLuint VR_Swapchains_GetOverlayFramebuffer(VR_SwapchainInfos* swapchains);

#endif
