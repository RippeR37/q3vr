#ifndef __VR_CYLINDER_DRAW
#define __VR_CYLINDER_DRAW

#include <openxr/openxr.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "vr_macros.h"
#include "../renderergl2/tr_local.h"

void VR_VirtualScreen_Init(void);
void VR_VirtualScreen_Destroy(void);
void VR_VirtualScreen_ResetPosition(void);
void VR_VirtualScreen_Draw(XrView* views, uint32_t viewCount, GLuint virtualScreenImage);
float VR_VirtualScreen_GetCurrentYaw(void);

#endif
