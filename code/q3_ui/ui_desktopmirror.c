/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
/*
=======================================================================

DESKTOP MIRROR OPTIONS MENU

=======================================================================
*/


#include "ui_local.h"


#define ART_FRAMEL				"menu/art/frame2_l"
#define ART_FRAMER				"menu/art/frame1_r"
#define ART_BACK0					"menu/art/back_0"
#define ART_BACK1					"menu/art/back_1"
#define ART_ACCEPT0				"menu/art/accept_0"
#define ART_ACCEPT1				"menu/art/accept_1"

#define VR_X_POS		360

#define ID_DESKTOPMIRROR         150
#define ID_DESKTOPRESOLUTION     151
#define ID_DESKTOPCONTENT        152
#define ID_DESKTOPMODE           153
#define ID_DESKTOPMENUMODE       154
#define ID_APPLY                 155
#define ID_BACK                  156


typedef struct {
	menuframework_s menu;

	menutext_s banner;
	menubitmap_s framel;
	menubitmap_s framer;

	menulist_s desktopmirror;
	menulist_s desktopresolution;
	menulist_s desktopcontent;
	menulist_s desktopmode;
	menulist_s desktopmenumode;

	menubitmap_s apply;
	menubitmap_s back;
} desktopmirror_t;

static desktopmirror_t s_desktopmirror;

static int s_ivo_desktopmirror;
static int s_ivo_desktopresolution;

/*
=================
Resolutions
=================
*/
#define MAX_RESOLUTIONS	33
static char resbuf[ MAX_STRING_CHARS ];
static const char* detectedResolutions[ MAX_RESOLUTIONS ];
static const char** allResolutions = NULL;
static const char** listedResolutions = NULL;
static char currentResolution[ 20 ];
static char vrResolution[ 20 ];
static char vrHalvedResolution[ 20 ];
static const char** resolutions = NULL;
static qboolean resolutionsDetected = qfalse;

static void GraphicsOptions_SetPreviousResolutionOption( void )
{
	s_desktopmirror.desktopresolution.curvalue = s_ivo_desktopresolution = 0;
	resolutions = allResolutions;

	if (!resolutions)
	{
		return;
	}

	const int currentWidth = trap_Cvar_VariableValue("r_customdesktopwidth");
	const int currentHeight = trap_Cvar_VariableValue("r_customdesktopheight");
  if (currentWidth <= 0 || currentHeight <= 0)
	{
		return;
	}

	Com_sprintf(currentResolution, sizeof(currentResolution), "custom (%dx%d)", currentWidth, currentHeight);

	for (int idx = 0; listedResolutions[idx] != NULL; ++idx)
	{
		char w[ 16 ], h[ 16 ];
		Q_strncpyz( w, listedResolutions[idx], sizeof( w ) );
		*strchr( w, 'x' ) = 0;
		Q_strncpyz( h, strchr( listedResolutions[idx], 'x' ) + 1, sizeof( h ) );
		const int width = atoi(w), height = atoi(h);
		if (currentWidth == width && currentHeight == height) {
			s_desktopmirror.desktopresolution.curvalue = s_ivo_desktopresolution = idx;
			resolutions = listedResolutions;
			return;
		}
	}
}

/*
=================
GraphicsOptions_GetResolutions
=================
*/
static void GraphicsOptions_GetResolutions( void )
{
	trap_Cvar_VariableStringBuffer("r_availableModes", resbuf, sizeof(resbuf));
	if(*resbuf)
	{
		char* s = resbuf;
		unsigned int i = 0;

		// Set first resolution as `custom`, this option will be visible if the user
		// sets something outside of this list via cvars manually
		// exact values will be added in SetPreviousResolutionOption()
		Com_sprintf(currentResolution, sizeof(currentResolution), "custom");
		detectedResolutions[i++] = currentResolution;
		detectedResolutions[i] = NULL;

		while( s && i < ARRAY_LEN(detectedResolutions)-1 )
		{
			detectedResolutions[i++] = s;
			s = strchr(s, ' ');
			if( s )
				*s++ = '\0';
		}
		detectedResolutions[ i ] = NULL;

		// add custom swapchain resolution and halved if not in mode list
		if ( i < ARRAY_LEN(detectedResolutions)-2 )
		{
			Com_sprintf( vrResolution, sizeof ( vrResolution ), "%dx%d", uis.glconfig.vidWidth, uis.glconfig.vidHeight );

			for( i = 0; detectedResolutions[ i ]; i++ )
			{
				if ( strcmp( detectedResolutions[ i ], vrResolution ) == 0 )
					break;
			}

			if ( detectedResolutions[ i ] == NULL )
			{
				detectedResolutions[ i++ ] = vrResolution;
				detectedResolutions[ i ] = NULL;

				if ( i < ARRAY_LEN(detectedResolutions)-2 )
				{
					Com_sprintf( vrHalvedResolution, sizeof ( vrHalvedResolution ), "%dx%d", uis.glconfig.vidWidth / 2, uis.glconfig.vidHeight / 2 );
					detectedResolutions[ i++ ] = vrHalvedResolution;
					detectedResolutions[ i ] = NULL;
				}
			}
		}

		allResolutions = detectedResolutions;
		listedResolutions = detectedResolutions + 1;

		resolutions = allResolutions;
		resolutionsDetected = qtrue;
	}

	GraphicsOptions_SetPreviousResolutionOption();
}

static void DesktopMirror_SetMenuItems( void ) {
	// Desktop mirror UI: 0=off, 1=windowed, 2=fullscreen
	// Reads vr_desktopMirror (0=off, 1=on) and r_fullscreen (0=windowed, 1=fullscreen)
	int mirror = trap_Cvar_VariableValue( "vr_desktopMirror" );
	int fullscreen = trap_Cvar_VariableValue( "r_fullscreen" );
	if (mirror == 0) {
		s_desktopmirror.desktopmirror.curvalue = 0; // Off
	} else if (fullscreen == 0) {
		s_desktopmirror.desktopmirror.curvalue = 1; // Windowed (mirror=1, fullscreen=0)
	} else {
		s_desktopmirror.desktopmirror.curvalue = 2; // Fullscreen (mirror=1, fullscreen=1)
	}
	s_ivo_desktopmirror = s_desktopmirror.desktopmirror.curvalue;
	// s_desktopmirror.desktopresolution and s_ivo_desktopresolution are handled elsewhere
	s_desktopmirror.desktopcontent.curvalue = trap_Cvar_VariableValue( "vr_desktopContent" );
	s_desktopmirror.desktopmode.curvalue = trap_Cvar_VariableValue( "vr_desktopMode" );
	s_desktopmirror.desktopmenumode.curvalue = trap_Cvar_VariableValue( "vr_desktopMenuMode" );
}

static void DesktopMirror_UpdateMenuItems( void )
{
	s_desktopmirror.apply.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;

	if ( s_ivo_desktopmirror != s_desktopmirror.desktopmirror.curvalue )
	{
		s_desktopmirror.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}

	if ( s_ivo_desktopresolution != s_desktopmirror.desktopresolution.curvalue )
	{
		s_desktopmirror.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
}

static void DesktopMirror_ApplyChanges( void *unused, int notification )
{
	if (notification != QM_ACTIVATED)
		return;

	if (s_ivo_desktopmirror != s_desktopmirror.desktopmirror.curvalue)
	{
		// UI curvalue: 0=off, 1=windowed, 2=fullscreen
		// Set vr_desktopMirror (0=off, 1=on) and r_fullscreen (0=windowed, 1=fullscreen)
		if (s_desktopmirror.desktopmirror.curvalue == 0) {
			// Off
			trap_Cvar_SetValue( "vr_desktopMirror", 0 );
		} else if (s_desktopmirror.desktopmirror.curvalue == 1) {
			// Windowed
			trap_Cvar_SetValue( "vr_desktopMirror", 1 );
			trap_Cvar_SetValue( "r_fullscreen", 0 );
		} else {
			// Fullscreen
			trap_Cvar_SetValue( "vr_desktopMirror", 1 );
			trap_Cvar_SetValue( "r_fullscreen", 1 );
		}
	}

	if ( s_ivo_desktopresolution != s_desktopmirror.desktopresolution.curvalue )
	{
		if (resolutions)
		{
			char w[ 16 ], h[ 16 ];
			Q_strncpyz( w, resolutions[ s_desktopmirror.desktopresolution.curvalue ], sizeof( w ) );
			*strchr( w, 'x' ) = 0;
			Q_strncpyz( h, strchr( resolutions[ s_desktopmirror.desktopresolution.curvalue ], 'x' ) + 1, sizeof( h ) );
			trap_Cvar_Set( "r_customdesktopwidth", w );
			trap_Cvar_Set( "r_customdesktopheight", h );
		}
	}

	trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

static void DesktopMirror_MenuEvent( void* ptr, int notification ) {
	if( notification != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
		case ID_DESKTOPMIRROR:
			// Will be applied in DesktopMirror_ApplyChanges
			break;

		case ID_DESKTOPRESOLUTION:
			// Will be applied in DesktopMirror_ApplyChanges
			break;

		case ID_DESKTOPCONTENT:
			trap_Cvar_SetValue( "vr_desktopContent", s_desktopmirror.desktopcontent.curvalue );
			break;

		case ID_DESKTOPMODE:
			trap_Cvar_SetValue( "vr_desktopMode", s_desktopmirror.desktopmode.curvalue );
			break;

		case ID_DESKTOPMENUMODE:
			trap_Cvar_SetValue( "vr_desktopMenuMode", s_desktopmirror.desktopmenumode.curvalue );
			break;

		case ID_APPLY:
			DesktopMirror_ApplyChanges( ptr, notification );
			break;

		case ID_BACK:
			UI_PopMenu();
			break;
	}

	DesktopMirror_UpdateMenuItems();
}

static void DesktopMirror_MenuInit( void ) {
	int y;

	static const char *s_desktopMirrorModes[] =
	{
		"Off",
		"Windowed",
		"Fullscreen",
		NULL,
	};

	static const char *s_desktopContents[] =
	{
		"Fit",
		"Fill",
	};

	static const char *s_desktopModes[] =
	{
		"Left eye",
		"Right eye",
		"Both eyes",
		NULL,
	};

	static const char *s_desktopMenuModes[] =
	{
		"Desktop view",
		"VR view",
		NULL,
	};

	memset( &s_desktopmirror, 0 ,sizeof(desktopmirror_t) );

	GraphicsOptions_GetResolutions();
	DesktopMirror_Cache();

	s_desktopmirror.menu.wrapAround = qtrue;
	s_desktopmirror.menu.fullscreen = qtrue;

	s_desktopmirror.banner.generic.type  = MTYPE_BTEXT;
	s_desktopmirror.banner.generic.x     = 320;
	s_desktopmirror.banner.generic.y     = 16;
	s_desktopmirror.banner.string	       = "DESKTOP MIRROR";
	s_desktopmirror.banner.color         = color_white;
	s_desktopmirror.banner.style         = UI_CENTER;

	s_desktopmirror.framel.generic.type  = MTYPE_BITMAP;
	s_desktopmirror.framel.generic.name  = ART_FRAMEL;
	s_desktopmirror.framel.generic.flags = QMF_INACTIVE;
	s_desktopmirror.framel.generic.x	   = 0;
	s_desktopmirror.framel.generic.y	   = 78;
	s_desktopmirror.framel.width  	     = 256;
	s_desktopmirror.framel.height  	     = 329;

	s_desktopmirror.framer.generic.type  = MTYPE_BITMAP;
	s_desktopmirror.framer.generic.name  = ART_FRAMER;
	s_desktopmirror.framer.generic.flags = QMF_INACTIVE;
	s_desktopmirror.framer.generic.x     = 376;
	s_desktopmirror.framer.generic.y     = 76;
	s_desktopmirror.framer.width         = 256;
	s_desktopmirror.framer.height        = 334;

	y = 198;
	s_desktopmirror.desktopmirror.generic.type     = MTYPE_SPINCONTROL;
	s_desktopmirror.desktopmirror.generic.x        = VR_X_POS;
	s_desktopmirror.desktopmirror.generic.y        = y;
	s_desktopmirror.desktopmirror.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_desktopmirror.desktopmirror.generic.name     = "Desktop mirror:";
	s_desktopmirror.desktopmirror.generic.id       = ID_DESKTOPMIRROR;
	s_desktopmirror.desktopmirror.generic.callback = DesktopMirror_MenuEvent;
	s_desktopmirror.desktopmirror.itemnames        = s_desktopMirrorModes;
	s_desktopmirror.desktopmirror.numitems         = 3;

	y += BIGCHAR_HEIGHT+2;
	s_desktopmirror.desktopresolution.generic.type     = MTYPE_SPINCONTROL;
	s_desktopmirror.desktopresolution.generic.name     = "Desktop resolution:";
	s_desktopmirror.desktopresolution.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_desktopmirror.desktopresolution.generic.x        = VR_X_POS;
	s_desktopmirror.desktopresolution.generic.y        = y;
	s_desktopmirror.desktopresolution.itemnames        = resolutions;
	s_desktopmirror.desktopresolution.generic.callback = DesktopMirror_MenuEvent;
	s_desktopmirror.desktopresolution.generic.id       = ID_DESKTOPRESOLUTION;

	y += BIGCHAR_HEIGHT+2;
	s_desktopmirror.desktopcontent.generic.type      = MTYPE_SPINCONTROL;
	s_desktopmirror.desktopcontent.generic.x         = VR_X_POS;
	s_desktopmirror.desktopcontent.generic.y         = y;
	s_desktopmirror.desktopcontent.generic.flags     = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_desktopmirror.desktopcontent.generic.name      = "Desktop content:";
	s_desktopmirror.desktopcontent.generic.id        = ID_DESKTOPCONTENT;
	s_desktopmirror.desktopcontent.generic.callback  = DesktopMirror_MenuEvent;
	s_desktopmirror.desktopcontent.itemnames         = s_desktopContents;
	s_desktopmirror.desktopcontent.numitems          = 2;





	y += BIGCHAR_HEIGHT+2;
	s_desktopmirror.desktopmode.generic.type     = MTYPE_SPINCONTROL;
	s_desktopmirror.desktopmode.generic.x        = VR_X_POS;
	s_desktopmirror.desktopmode.generic.y        = y;
	s_desktopmirror.desktopmode.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_desktopmirror.desktopmode.generic.name     = "Desktop mode:";
	s_desktopmirror.desktopmode.generic.id       = ID_DESKTOPMODE;
	s_desktopmirror.desktopmode.generic.callback = DesktopMirror_MenuEvent;
	s_desktopmirror.desktopmode.itemnames        = s_desktopModes;
	s_desktopmirror.desktopmode.numitems         = 3;

	y += BIGCHAR_HEIGHT+2;
	s_desktopmirror.desktopmenumode.generic.type     = MTYPE_SPINCONTROL;
	s_desktopmirror.desktopmenumode.generic.x        = VR_X_POS;
	s_desktopmirror.desktopmenumode.generic.y        = y;
	s_desktopmirror.desktopmenumode.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_desktopmirror.desktopmenumode.generic.name     = "Desktop menu mode:";
	s_desktopmirror.desktopmenumode.generic.id       = ID_DESKTOPMENUMODE;
	s_desktopmirror.desktopmenumode.generic.callback = DesktopMirror_MenuEvent;
	s_desktopmirror.desktopmenumode.itemnames        = s_desktopMenuModes;
	s_desktopmirror.desktopmenumode.numitems         = 2;

	s_desktopmirror.apply.generic.type     = MTYPE_BITMAP;
	s_desktopmirror.apply.generic.name     = ART_ACCEPT0;
	s_desktopmirror.apply.generic.flags    = QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_HIDDEN|QMF_INACTIVE;
	s_desktopmirror.apply.generic.callback = DesktopMirror_ApplyChanges;
	s_desktopmirror.apply.generic.id       = ID_APPLY;
	s_desktopmirror.apply.generic.x        = 640;
	s_desktopmirror.apply.generic.y        = 480-64;
	s_desktopmirror.apply.width            = 128;
	s_desktopmirror.apply.height           = 64;
	s_desktopmirror.apply.focuspic         = ART_ACCEPT1;

	s_desktopmirror.back.generic.type      = MTYPE_BITMAP;
	s_desktopmirror.back.generic.name      = ART_BACK0;
	s_desktopmirror.back.generic.flags     = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_desktopmirror.back.generic.callback  = DesktopMirror_MenuEvent;
	s_desktopmirror.back.generic.id        = ID_BACK;
	s_desktopmirror.back.generic.x         = 0;
	s_desktopmirror.back.generic.y         = 480-64;
	s_desktopmirror.back.width             = 128;
	s_desktopmirror.back.height            = 64;
	s_desktopmirror.back.focuspic          = ART_BACK1;

	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.banner );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.framel );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.framer );

	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.desktopmirror );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.desktopresolution );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.desktopcontent );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.desktopmode );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.desktopmenumode );

	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.apply );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.back );

	DesktopMirror_SetMenuItems();
}


/*
===============
DesktopMirror_Cache
===============
*/
void DesktopMirror_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_FRAMEL );
	trap_R_RegisterShaderNoMip( ART_FRAMER );
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
	trap_R_RegisterShaderNoMip( ART_ACCEPT0 );
	trap_R_RegisterShaderNoMip( ART_ACCEPT1 );
}


/*
===============
UI_VRMenu
===============
*/
void UI_DesktopMirrorMenu( void ) {
	DesktopMirror_MenuInit();
	UI_PushMenu( &s_desktopmirror.menu );
}
