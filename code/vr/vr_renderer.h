#ifndef __VR_RENDERER
#define __VR_RENDERER

#include "vr_types.h"

void VR_GetResolution( VR_Engine* engine, int *pWidth, int *pHeight );

void VR_InitRenderer( VR_Engine* engine );
void VR_DestroyRenderer( VR_Engine* engine );

void VR_DrawFrame( VR_Engine* engine );

#endif
