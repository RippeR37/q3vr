#ifndef __VR_BASE
#define __VR_BASE

#include "vr_types.h"

VR_Engine* VR_Init( void );
VR_Engine* VR_GetEngine( void );
void VR_Destroy( VR_Engine* engine );
void VR_PrepareForShutdown( void );

void VR_EnterVR( VR_Engine* engine );
void VR_LeaveVR( VR_Engine* engine );

#endif
