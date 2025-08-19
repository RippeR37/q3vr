#ifndef __VR_INPUT_H
#define __VR_INPUT_H

#include "../qcommon/q_shared.h"
#include "vr_types.h"

// Init
void VR_InitInstanceInput( VR_Engine* engine );
void VR_InitSessionInput( VR_Engine* engine );
void VR_DestroySessionInput( VR_Engine* engine );

// Render loop
void VR_ProcessInputActions( void );
void IN_VRUpdateHMD( XrView* views, uint32_t viewCount, XrFovf* fov );
void IN_VRSyncActions( VR_Engine* engine );
void IN_VRUpdateControllers( VR_Engine* engine, XrTime predictedDisplayTime );

void VR_HapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight );

void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out);

#endif
