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

#define ID_DESKTOPMODE           150
#define ID_DESKTOPRESOLUTION     151
#define ID_DESKTOPCONTENTTYPE    152
#define ID_DESKTOPCONTENTFIT     153
#define ID_DESKTOPMENUSTYLE      154
#define ID_APPLY                 155
#define ID_BACK                  156


typedef struct {
	menuframework_s menu;

	menutext_s banner;
	menubitmap_s framel;
	menubitmap_s framer;

	menulist_s mode;
	menulist_s resolution;
	menulist_s contenttype;
	menulist_s contentfit;
	menulist_s menustyle;

	menubitmap_s apply;
	menubitmap_s back;
} desktopmirror_t;

static desktopmirror_t s_desktopmirror;

static int s_ivo_desktopmode;
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
	s_desktopmirror.resolution.curvalue = s_ivo_desktopresolution = 0;
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
			s_desktopmirror.resolution.curvalue = s_ivo_desktopresolution = idx;
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
	// s_desktopmirror.resolution and s_ivo_resolution are handled elsewhere
	// Desktop mirror UI: 0=off, 1=windowed, 2=fullscreen
	// Reads vr_desktopMode (0=off, 1=on) and r_fullscreen (0=windowed, 1=fullscreen)
	int mirror = trap_Cvar_VariableValue( "vr_desktopMode" );
	int fullscreen = trap_Cvar_VariableValue( "r_fullscreen" );
	if (mirror == 0) {
		s_desktopmirror.mode.curvalue = 0; // Off
	} else if (fullscreen == 0) {
		s_desktopmirror.mode.curvalue = 1; // Windowed (mirror=1, fullscreen=0)
	} else {
		s_desktopmirror.mode.curvalue = 2; // Fullscreen (mirror=1, fullscreen=1)
	}
	s_ivo_desktopmode = s_desktopmirror.mode.curvalue;
	s_desktopmirror.contenttype.curvalue = trap_Cvar_VariableValue( "vr_desktopContentType" );
	s_desktopmirror.contentfit.curvalue = trap_Cvar_VariableValue( "vr_desktopContentFit" );
	s_desktopmirror.menustyle.curvalue = trap_Cvar_VariableValue( "vr_desktopMenuStyle" );
}

static void DesktopMirror_UpdateMenuItems( void )
{
	s_desktopmirror.apply.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;

	if ( s_ivo_desktopmode != s_desktopmirror.mode.curvalue )
	{
		s_desktopmirror.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}

	if ( s_ivo_desktopresolution != s_desktopmirror.resolution.curvalue )
	{
		s_desktopmirror.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
}

static void DesktopMirror_ApplyChanges( void *unused, int notification )
{
	if (notification != QM_ACTIVATED)
		return;

	if (s_ivo_desktopmode != s_desktopmirror.mode.curvalue)
	{
		// UI curvalue: 0=off, 1=windowed, 2=fullscreen
		// Set vr_desktopMode (0=off, 1=on) and r_fullscreen (0=windowed, 1=fullscreen)
		if (s_desktopmirror.mode.curvalue == 0) {
			// Off
			trap_Cvar_SetValue( "vr_desktopMode", 0 );
		} else if (s_desktopmirror.mode.curvalue == 1) {
			// Windowed
			trap_Cvar_SetValue( "vr_desktopMode", 1 );
			trap_Cvar_SetValue( "r_fullscreen", 0 );
		} else {
			// Fullscreen
			trap_Cvar_SetValue( "vr_desktopMode", 1 );
			trap_Cvar_SetValue( "r_fullscreen", 1 );
		}
	}

	if ( s_ivo_desktopresolution != s_desktopmirror.resolution.curvalue && resolutions )
	{
		char w[ 16 ], h[ 16 ];
		Q_strncpyz( w, resolutions[ s_desktopmirror.resolution.curvalue ], sizeof( w ) );
		*strchr( w, 'x' ) = 0;
		Q_strncpyz( h, strchr( resolutions[ s_desktopmirror.resolution.curvalue ], 'x' ) + 1, sizeof( h ) );
		trap_Cvar_Set( "r_customdesktopwidth", w );
		trap_Cvar_Set( "r_customdesktopheight", h );
	}

	trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

static void DesktopMirror_MenuEvent( void* ptr, int notification ) {
	if( notification != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
		case ID_DESKTOPMODE:
			// Will be applied in DesktopMirror_ApplyChanges
			break;

		case ID_DESKTOPRESOLUTION:
			// Will be applied in DesktopMirror_ApplyChanges
			break;

		case ID_DESKTOPCONTENTTYPE:
			trap_Cvar_SetValue( "vr_desktopContentType", s_desktopmirror.contenttype.curvalue );
			break;

		case ID_DESKTOPCONTENTFIT:
			trap_Cvar_SetValue( "vr_desktopContentFit", s_desktopmirror.contentfit.curvalue );
			break;

		case ID_DESKTOPMENUSTYLE:
			trap_Cvar_SetValue( "vr_desktopMenuStyle", s_desktopmirror.menustyle.curvalue );
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

	static const char *s_desktopModes[] =
	{
		"Off",
		"Windowed",
		"Fullscreen",
		NULL,
	};

	static const char *s_desktopContentTypes[] =
	{
		"Left eye",
		"Right eye",
		"Both eyes",
		NULL,
	};

	static const char *s_desktopContentFits[] =
	{
		"Fit / Contain",
		"Fill / Crop",
    NULL,
	};

	static const char *s_desktopMenuStyles[] =
	{
		"Desktop view",
		"VR view",
		NULL,
	};

	memset( &s_desktopmirror, 0, sizeof(desktopmirror_t) );

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
	s_desktopmirror.mode.generic.type     = MTYPE_SPINCONTROL;
	s_desktopmirror.mode.generic.x        = VR_X_POS;
	s_desktopmirror.mode.generic.y        = y;
	s_desktopmirror.mode.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_desktopmirror.mode.generic.name     = "Mode:";
	s_desktopmirror.mode.generic.id       = ID_DESKTOPMODE;
	s_desktopmirror.mode.generic.callback = DesktopMirror_MenuEvent;
	s_desktopmirror.mode.itemnames        = s_desktopModes;
	s_desktopmirror.mode.numitems         = 3;

	y += BIGCHAR_HEIGHT+2;
	s_desktopmirror.resolution.generic.type     = MTYPE_SPINCONTROL;
	s_desktopmirror.resolution.generic.name     = "Resolution:";
	s_desktopmirror.resolution.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_desktopmirror.resolution.generic.x        = VR_X_POS;
	s_desktopmirror.resolution.generic.y        = y;
	s_desktopmirror.resolution.itemnames        = resolutions;
	s_desktopmirror.resolution.generic.callback = DesktopMirror_MenuEvent;
	s_desktopmirror.resolution.generic.id       = ID_DESKTOPRESOLUTION;

	y += BIGCHAR_HEIGHT+2;
	s_desktopmirror.contenttype.generic.type     = MTYPE_SPINCONTROL;
	s_desktopmirror.contenttype.generic.x        = VR_X_POS;
	s_desktopmirror.contenttype.generic.y        = y;
	s_desktopmirror.contenttype.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_desktopmirror.contenttype.generic.name     = "Content type:";
	s_desktopmirror.contenttype.generic.id       = ID_DESKTOPCONTENTTYPE;
	s_desktopmirror.contenttype.generic.callback = DesktopMirror_MenuEvent;
	s_desktopmirror.contenttype.itemnames        = s_desktopContentTypes;
	s_desktopmirror.contenttype.numitems         = 3;

	y += BIGCHAR_HEIGHT+2;
	s_desktopmirror.contentfit.generic.type      = MTYPE_SPINCONTROL;
	s_desktopmirror.contentfit.generic.x         = VR_X_POS;
	s_desktopmirror.contentfit.generic.y         = y;
	s_desktopmirror.contentfit.generic.flags     = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_desktopmirror.contentfit.generic.name      = "Content fit:";
	s_desktopmirror.contentfit.generic.id        = ID_DESKTOPCONTENTFIT;
	s_desktopmirror.contentfit.generic.callback  = DesktopMirror_MenuEvent;
	s_desktopmirror.contentfit.itemnames         = s_desktopContentFits;
	s_desktopmirror.contentfit.numitems          = 2;

	y += BIGCHAR_HEIGHT+2;
	s_desktopmirror.menustyle.generic.type     = MTYPE_SPINCONTROL;
	s_desktopmirror.menustyle.generic.x        = VR_X_POS;
	s_desktopmirror.menustyle.generic.y        = y;
	s_desktopmirror.menustyle.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_desktopmirror.menustyle.generic.name     = "Menu style:";
	s_desktopmirror.menustyle.generic.id       = ID_DESKTOPMENUSTYLE;
	s_desktopmirror.menustyle.generic.callback = DesktopMirror_MenuEvent;
	s_desktopmirror.menustyle.itemnames        = s_desktopMenuStyles;
	s_desktopmirror.menustyle.numitems         = 2;

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

	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.mode  );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.resolution );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.contenttype );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.contentfit );
	Menu_AddItem( &s_desktopmirror.menu, &s_desktopmirror.menustyle );

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
