#ifndef __VR_HAPTICS
#define __VR_HAPTICS

#include "../qcommon/q_shared.h"

qboolean VR_AreHapticsEnabled(void);
float VR_GetHapticsIntensity(void);

// chan = { 0x1 left, 0x2 right, 0x3 both }
void VR_Vibrate(int duration, int chan, float intensity);
void VR_ProcessHaptics(void);

#endif
