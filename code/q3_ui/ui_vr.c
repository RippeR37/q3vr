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

VR OPTIONS MENU

=======================================================================
*/


#include "ui_local.h"


#define ART_FRAMEL				"menu/art/frame2_l"
#define ART_FRAMER				"menu/art/frame1_r"
#define ART_BACK0				"menu/art/back_0"
#define ART_BACK1				"menu/art/back_1"
#define ART_ACCEPT0				"menu/art/accept_0"
#define ART_ACCEPT1				"menu/art/accept_1"

#define VR_X_POS		360

#define ID_DESKTOPMIRROR         150
#define ID_DESKTOPMODE           151
#define ID_VIRTUALSCREENMODE     152
#define ID_VIRTUALSCREENSHAPE    153
#define ID_SHOWOFFHAND           154
#define ID_DRAWHUD			         155
#define ID_SELECTORWITHHUD		   139
#define ID_APPLY				         157
#define ID_BACK					         156


typedef struct {
	menuframework_s menu;

	menutext_s banner;
	menubitmap_s framel;
	menubitmap_s framer;

	menulist_s desktopmirror;
	menulist_s desktopmode;
  menulist_s virtualscreenmode;
  menulist_s virtualscreenshape;
  menuradiobutton_s showoffhand;
	menulist_s drawhud;
	menuradiobutton_s selectorwithhud;

	menubitmap_s apply;
	menubitmap_s back;
} vr_t;

static vr_t s_vr;

static int s_ivo_desktopmirror;
static int s_ivo_desktopmode;

static void VR_SetMenuItems( void ) {
	s_vr.desktopmirror.curvalue = trap_Cvar_VariableValue( "vr_desktopMirror" );
	s_ivo_desktopmirror = s_vr.desktopmirror.curvalue;
	s_vr.desktopmode.curvalue = trap_Cvar_VariableValue( "vr_desktopMode" );
	s_ivo_desktopmode = s_vr.desktopmode.curvalue;
	s_vr.virtualscreenmode.curvalue = trap_Cvar_VariableValue( "vr_virtualScreenMode" );
  s_vr.virtualscreenshape.curvalue = trap_Cvar_VariableValue( "vr_virtualScreenShape" );
  s_vr.showoffhand.curvalue = trap_Cvar_VariableValue( "vr_showOffhand" ) != 0;
	s_vr.drawhud.curvalue = trap_Cvar_VariableValue( "vr_hudDrawStatus" );
	s_vr.selectorwithhud.curvalue = trap_Cvar_VariableValue( "vr_weaponSelectorWithHud" ) != 0;
}

static void VR_UpdateMenuItems( void )
{
	s_vr.apply.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;

	if ( s_ivo_desktopmirror != s_vr.desktopmirror.curvalue )
	{
		s_vr.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}

	// Show apply button if switching between one eye and both eyes in windowed mirror mode
	// (switching between left and right eye doesn't need restart)
	qboolean wasBothEyes = (s_ivo_desktopmode == 2);
	qboolean isBothEyes = (s_vr.desktopmode.curvalue == 2);
	if ( wasBothEyes != isBothEyes && trap_Cvar_VariableValue( "vr_desktopMirror" ) == 1 )
	{
		s_vr.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
}

static void VR_ApplyChanges( void *unused, int notification )
{
	if (notification != QM_ACTIVATED)
		return;

	trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

static void VR_MenuEvent( void* ptr, int notification ) {
	if( notification != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
		case ID_DESKTOPMIRROR:
			trap_Cvar_SetValue( "vr_desktopMirror", s_vr.desktopmirror.curvalue );
			break;

		case ID_DESKTOPMODE:
			trap_Cvar_SetValue( "vr_desktopMode", s_vr.desktopmode.curvalue );
			break;

		case ID_VIRTUALSCREENMODE:
			trap_Cvar_SetValue( "vr_virtualScreenMode", s_vr.virtualscreenmode.curvalue );
			break;

    case ID_VIRTUALSCREENSHAPE:
			trap_Cvar_SetValue( "vr_virtualScreenShape", s_vr.virtualscreenshape.curvalue );
			break;

    case ID_SHOWOFFHAND:
			trap_Cvar_SetValue( "vr_showOffhand", s_vr.showoffhand.curvalue );
			break;

    case ID_DRAWHUD:
			trap_Cvar_SetValue( "vr_hudDrawStatus", s_vr.drawhud.curvalue );
			trap_Cvar_SetValue("cg_draw3dIcons", (s_vr.drawhud.curvalue == 2) ? 0 : 1);
			break;

	  case ID_SELECTORWITHHUD:
			trap_Cvar_SetValue( "vr_weaponSelectorWithHud", s_vr.selectorwithhud.curvalue);
			break;

		case ID_APPLY:
			VR_ApplyChanges( ptr, notification );
			break;

		case ID_BACK:
			UI_PopMenu();
			break;
	}

	VR_UpdateMenuItems();
}

static void VR_MenuInit( void ) {
	int y;

	static const char *s_desktopMirrorModes[] =
	{
		"Off",
		"Windowed",
		"Fullscreen",
		NULL,
	};

	static const char *s_desktopModes[] =
	{
		"Left eye",
		"Right eye",
		"Both eyes",
		NULL,
	};

	static const char *s_virtualScreenModes[] =
	{
		"Fixed",
		"Follow",
		NULL,
	};

	static const char *s_virtualScreenShapes[] =
	{
		"Curved",
		"Flat",
		NULL,
	};

	static const char *hud_names[] =
	{
		"Off",
		"Floating",
		"Fixed to View",
		NULL,
	};

	memset( &s_vr, 0 ,sizeof(vr_t) );

	VR_Cache();

	s_vr.menu.wrapAround = qtrue;
	s_vr.menu.fullscreen = qtrue;

	s_vr.banner.generic.type  = MTYPE_BTEXT;
	s_vr.banner.generic.x	   = 320;
	s_vr.banner.generic.y	   = 16;
	s_vr.banner.string		   = "VR OPTIONS";
	s_vr.banner.color         = color_white;
	s_vr.banner.style         = UI_CENTER;

	s_vr.framel.generic.type  = MTYPE_BITMAP;
	s_vr.framel.generic.name  = ART_FRAMEL;
	s_vr.framel.generic.flags = QMF_INACTIVE;
	s_vr.framel.generic.x	   = 0;
	s_vr.framel.generic.y	   = 78;
	s_vr.framel.width  	   = 256;
	s_vr.framel.height  	   = 329;

	s_vr.framer.generic.type  = MTYPE_BITMAP;
	s_vr.framer.generic.name  = ART_FRAMER;
	s_vr.framer.generic.flags = QMF_INACTIVE;
	s_vr.framer.generic.x	   = 376;
	s_vr.framer.generic.y	   = 76;
	s_vr.framer.width  	   = 256;
	s_vr.framer.height  	   = 334;

	y = 198;
	s_vr.desktopmirror.generic.type	     = MTYPE_SPINCONTROL;
	s_vr.desktopmirror.generic.x			   = VR_X_POS;
	s_vr.desktopmirror.generic.y			   = y;
	s_vr.desktopmirror.generic.flags	 	 = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_vr.desktopmirror.generic.name	     = "Desktop mirror:";
	s_vr.desktopmirror.generic.id 	     = ID_DESKTOPMIRROR;
	s_vr.desktopmirror.generic.callback  = VR_MenuEvent;
	s_vr.desktopmirror.itemnames		     = s_desktopMirrorModes;
	s_vr.desktopmirror.numitems		       = 3;

	y += BIGCHAR_HEIGHT+2;
	s_vr.desktopmode.generic.type	     = MTYPE_SPINCONTROL;
	s_vr.desktopmode.generic.x			   = VR_X_POS;
	s_vr.desktopmode.generic.y			   = y;
	s_vr.desktopmode.generic.flags	 	 = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_vr.desktopmode.generic.name	     = "Desktop mode:";
	s_vr.desktopmode.generic.id 	     = ID_DESKTOPMODE;
	s_vr.desktopmode.generic.callback  = VR_MenuEvent;
	s_vr.desktopmode.itemnames		     = s_desktopModes;
	s_vr.desktopmode.numitems		       = 3;

	y += BIGCHAR_HEIGHT+2;
	s_vr.virtualscreenmode.generic.type	     = MTYPE_SPINCONTROL;
	s_vr.virtualscreenmode.generic.x			   = VR_X_POS;
	s_vr.virtualscreenmode.generic.y			   = y;
	s_vr.virtualscreenmode.generic.flags	 	 = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_vr.virtualscreenmode.generic.name	     = "Virtual screen mode:";
	s_vr.virtualscreenmode.generic.id 	     = ID_VIRTUALSCREENMODE;
	s_vr.virtualscreenmode.generic.callback  = VR_MenuEvent;
	s_vr.virtualscreenmode.itemnames		     = s_virtualScreenModes;
	s_vr.virtualscreenmode.numitems		       = 2;

	y += BIGCHAR_HEIGHT+2;
	s_vr.virtualscreenshape.generic.type	   = MTYPE_SPINCONTROL;
	s_vr.virtualscreenshape.generic.x			   = VR_X_POS;
	s_vr.virtualscreenshape.generic.y			   = y;
	s_vr.virtualscreenshape.generic.flags	 	 = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_vr.virtualscreenshape.generic.name	   = "Virtual screen shape:";
	s_vr.virtualscreenshape.generic.id 	     = ID_VIRTUALSCREENSHAPE;
	s_vr.virtualscreenshape.generic.callback = VR_MenuEvent;
	s_vr.virtualscreenshape.itemnames		     = s_virtualScreenShapes;
	s_vr.virtualscreenshape.numitems		     = 2;

	y += BIGCHAR_HEIGHT+2;
	s_vr.showoffhand.generic.type	     = MTYPE_RADIOBUTTON;
	s_vr.showoffhand.generic.x			   = VR_X_POS;
	s_vr.showoffhand.generic.y			   = y;
	s_vr.showoffhand.generic.flags	 	 = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_vr.showoffhand.generic.name	     = "Show offhand:";
	s_vr.showoffhand.generic.id 	     = ID_SHOWOFFHAND;
	s_vr.showoffhand.generic.callback  = VR_MenuEvent;

	y += BIGCHAR_HEIGHT+2;
	s_vr.drawhud.generic.type        = MTYPE_SPINCONTROL;
	s_vr.drawhud.generic.name	       = "HUD Mode:";
	s_vr.drawhud.generic.flags	     = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_vr.drawhud.generic.callback    = VR_MenuEvent;
	s_vr.drawhud.generic.id          = ID_DRAWHUD;
	s_vr.drawhud.generic.x	         = VR_X_POS;
	s_vr.drawhud.generic.y	         = y;
	s_vr.drawhud.itemnames			     = hud_names;

	y += BIGCHAR_HEIGHT+2;
	s_vr.selectorwithhud.generic.type        = MTYPE_RADIOBUTTON;
	s_vr.selectorwithhud.generic.name	       = "Draw HUD On Weapon Wheel:";
	s_vr.selectorwithhud.generic.flags	     = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_vr.selectorwithhud.generic.callback    = VR_MenuEvent;
	s_vr.selectorwithhud.generic.id          = ID_SELECTORWITHHUD;
	s_vr.selectorwithhud.generic.x	         = VR_X_POS;
	s_vr.selectorwithhud.generic.y	         = y;

	s_vr.apply.generic.type     = MTYPE_BITMAP;
	s_vr.apply.generic.name     = ART_ACCEPT0;
	s_vr.apply.generic.flags    = QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_HIDDEN|QMF_INACTIVE;
	s_vr.apply.generic.callback = VR_ApplyChanges;
	s_vr.apply.generic.id       = ID_APPLY;
	s_vr.apply.generic.x        = 640;
	s_vr.apply.generic.y        = 480-64;
	s_vr.apply.width            = 128;
	s_vr.apply.height           = 64;
	s_vr.apply.focuspic         = ART_ACCEPT1;

	s_vr.back.generic.type	    = MTYPE_BITMAP;
	s_vr.back.generic.name      = ART_BACK0;
	s_vr.back.generic.flags     = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_vr.back.generic.callback  = VR_MenuEvent;
	s_vr.back.generic.id	      = ID_BACK;
	s_vr.back.generic.x		      = 0;
	s_vr.back.generic.y		      = 480-64;
	s_vr.back.width  		        = 128;
	s_vr.back.height  		      = 64;
	s_vr.back.focuspic          = ART_BACK1;

	Menu_AddItem( &s_vr.menu, &s_vr.banner );
	Menu_AddItem( &s_vr.menu, &s_vr.framel );
	Menu_AddItem( &s_vr.menu, &s_vr.framer );

	Menu_AddItem( &s_vr.menu, &s_vr.desktopmirror );
	Menu_AddItem( &s_vr.menu, &s_vr.desktopmode );
	Menu_AddItem( &s_vr.menu, &s_vr.virtualscreenmode );
	Menu_AddItem( &s_vr.menu, &s_vr.virtualscreenshape );
	Menu_AddItem( &s_vr.menu, &s_vr.showoffhand );
	Menu_AddItem( &s_vr.menu, &s_vr.drawhud );
	Menu_AddItem( &s_vr.menu, &s_vr.selectorwithhud );

	Menu_AddItem( &s_vr.menu, &s_vr.apply );
	Menu_AddItem( &s_vr.menu, &s_vr.back );

	VR_SetMenuItems();
}


/*
===============
VR_Cache
===============
*/
void VR_Cache( void ) {
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
void UI_VRMenu( void ) {
	VR_MenuInit();
	UI_PushMenu( &s_vr.menu );
}
