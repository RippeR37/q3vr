#ifndef __VR_BHAPTICS
#define __VR_BHAPTICS

#include "../qcommon/q_shared.h"

void VR_Bhaptics_Init(void);
void VR_Bhaptics_Shutdown(void);
void VR_Bhaptics_UpdateEnabled(void);
void VR_Bhaptics_HandleEvent(const char* event, int position, int intensity, float yaw, float height);

#endif
