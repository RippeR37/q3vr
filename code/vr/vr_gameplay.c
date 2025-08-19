#include "vr_gameplay.h"

#include "../client/client.h"

void VR_Gameplay_OpenMenuAndPauseIfPossible(void)
{
	if ( clc.state == CA_ACTIVE && !clc.demoplaying && !Cvar_VariableValue ("cl_paused") )
	{
		Cbuf_ExecuteText(EXEC_APPEND, "togglemenu\n");
	}
}

qboolean VR_Gameplay_ShouldRenderInVirtualScreen( void )
{
	// intermission is never full screen
	if ( cl.snap.ps.pm_type == PM_INTERMISSION )
	{
		return qfalse;
	}

	int keyCatcher = Key_GetCatcher( );
	return ( 
		clc.state == CA_CINEMATIC || clc.state != CA_ACTIVE || ( keyCatcher & (KEYCATCH_UI | KEYCATCH_CONSOLE) )
	);
}
