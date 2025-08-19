#ifndef __VR_INSTANCE
#define __VR_INSTANCE

#include "vr_types.h"
#include "vr_safe_types.h"

XrResult VR_CreateInstance(const char* app_name, XrVersion api_version, uint32_t extensionsCount, const char* const* extensions, XrInstance* instance);
XrResult VR_GetHMDSystem(XrInstance instance, XrSystemId* systemId);
XrResult VR_GetSystemProperties(XrInstance instance, XrSystemId systemId, VR_SystemProperties* systemProperties);

#endif
