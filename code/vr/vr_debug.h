#ifndef __VR_DEBUG
#define __VR_DEBUG

#include "vr_types.h"

#define VR_ENABLE_GL_DEBUG 0
#if VR_ENABLE_GL_DEBUG
#define VR_ENABLE_GL_DEBUG_VERBOSE 0
#else
#define VR_ENABLE_GL_DEBUG_VERBOSE 0
#endif

//
// VR
//
void VR_ListAPILayers(void);
void VR_ListExtensions(void);

const char* GetXRErrorString(XrResult result);

void VR_CreateDebugUtilsMessenger(XrInstance instance, XrDebugUtilsMessengerEXT* messenger);
void VR_DestroyDebugUtilsMessenger(XrInstance instance, XrDebugUtilsMessengerEXT* messenger);

//
// GL
//
void VR_GL_RegisterDebugLogSinkIfEnabled(void);

#endif
