#ifndef __VR_SESSION
#define __VR_SESSION

#include "vr_types.h"

XrResult VR_CreateSession(XrInstance instance, XrSystemId systemId, XrSession* session);

XrResult VR_BeginSession(XrSession session);
XrResult VR_EndSession(XrSession session);

#endif
