#ifndef __VR_GAMEPLAY
#define __VR_GAMEPLAY

#include "../qcommon/q_shared.h"

void VR_Gameplay_OpenMenuAndPauseIfPossible( void );
qboolean VR_IsFollowingInFirstPerson( void );
qboolean VR_IsInMenu( void );
qboolean VR_Gameplay_ShouldRenderInVirtualScreen( void );
qboolean VR_ShouldDisableStereo( void );

#endif
