#include "vr_gameplay.h"

#include "../client/client.h"
#include "vr_clientinfo.h"

void VR_Gameplay_OpenMenuAndPauseIfPossible(void)
{
	if ( clc.state == CA_ACTIVE && !clc.demoplaying && !Cvar_VariableValue ("cl_paused") )
	{
		Cbuf_ExecuteText(EXEC_APPEND, "togglemenu\n");
	}
}

qboolean VR_IsFollowingInFirstPerson( void )
{
	extern vr_clientinfo_t vr;

	return (
		(((cl.snap.ps.pm_flags & PMF_FOLLOW) || clc.demoplaying)) &&
		(vr.follow_mode == VRFM_FIRSTPERSON)
	);
}

qboolean VR_IsInMenu( void )
{
	int keyCatcher = Key_GetCatcher();
	return (keyCatcher & (KEYCATCH_UI | KEYCATCH_CONSOLE)) != 0;
}

qboolean VR_Gameplay_ShouldRenderInVirtualScreen( void )
{
	// Use screen layer for UI/console, EXCEPT during single-player intermission
	// where we want the in-world podium view even with the postgame menu active
	qboolean isSPIntermission = (cl.snap.ps.pm_type == PM_INTERMISSION) &&
	                            (Cvar_VariableValue("g_gametype") == GT_SINGLE_PLAYER);
	if ( VR_IsInMenu() && !isSPIntermission )
	{
		return qtrue;
	}

	// Intermission without menu is rendered in-world
	if ( cl.snap.ps.pm_type == PM_INTERMISSION )
	{
		return qfalse;
	}

	return (
		clc.state == CA_CINEMATIC ||
		clc.state != CA_ACTIVE ||
		VR_IsFollowingInFirstPerson()
	);
}
