#ifndef __VR_BHAPTICS
#define __VR_BHAPTICS

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void VR_Bhaptics_Init(void);
void VR_Bhaptics_Shutdown(void);
void VR_Bhaptics_UpdateEnabled(void);
void VR_Bhaptics_HandleEvent(const char* event, int position, int intensity, float yaw, float height);

#ifdef __cplusplus
}
#endif

#if !defined(USE_BHAPTICS) && !defined(VR_BHAPTICS_IMPLEMENTATION)
#undef VR_Bhaptics_Init
#undef VR_Bhaptics_Shutdown
#undef VR_Bhaptics_UpdateEnabled
#undef VR_Bhaptics_HandleEvent
#define VR_Bhaptics_Init()
#define VR_Bhaptics_Shutdown()
#define VR_Bhaptics_UpdateEnabled()
#define VR_Bhaptics_HandleEvent(event, position, intensity, yaw, height)
#endif

#endif
