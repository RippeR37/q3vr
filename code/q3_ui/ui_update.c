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

UPDATE MENU

=======================================================================
*/


#include "ui_local.h"

#define ART_BACK0		"menu/art/back_0"
#define ART_BACK1		"menu/art/back_1"	
#define ART_FRAMEL		"menu/art/frame2_l"
#define ART_FRAMER		"menu/art/frame1_r"

#define VR_X_POS		330

#define ID_VER_CURRENT	10
#define ID_VER_NEW			11
#define ID_VISIT				12
#define ID_BACK					20


typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	framel;
	menubitmap_s	framer;

	menutext_s		descriptiont;
	menutext_s		descriptionb;
	menulist_s	current;
	menulist_s	new;
	menutext_s		visit;
	menulist_s		visit_manual;

	menubitmap_s	back;
} updateMenuInfo_t;

static updateMenuInfo_t	updateMenuInfo;


/*
===============
UI_UpdateMenu_Event
===============
*/
static void UI_UpdateMenu_Event( void *ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_VISIT:
#if defined(_WIN32)
    const char command[] = "start https://ripper37.github.io/q3vr/";
    system(command);
		trap_Cmd_ExecuteText( EXEC_APPEND, "quit\n" );
#endif
		UI_PopMenu();
		break;
	case ID_BACK:
		UI_PopMenu();
		break;
	}
}


/*
===============
UI_UpdateMenu_Init
===============
*/
static void UI_UpdateMenu_Init( void ) {
	int				y;

	UI_UpdateMenu_Cache();

	memset( &updateMenuInfo, 0, sizeof(updateMenuInfo) );
	updateMenuInfo.menu.wrapAround = qtrue;
	updateMenuInfo.menu.fullscreen = qtrue;

	updateMenuInfo.banner.generic.type				= MTYPE_BTEXT;
	updateMenuInfo.banner.generic.x					= 320;
	updateMenuInfo.banner.generic.y					= 16;
	updateMenuInfo.banner.string						= "UPDATE";
	updateMenuInfo.banner.color						= color_white;
	updateMenuInfo.banner.style						= UI_CENTER;

	updateMenuInfo.framel.generic.type				= MTYPE_BITMAP;
	updateMenuInfo.framel.generic.name				= ART_FRAMEL;
	updateMenuInfo.framel.generic.flags				= QMF_INACTIVE;
	updateMenuInfo.framel.generic.x					= 0;
	updateMenuInfo.framel.generic.y					= 78;
	updateMenuInfo.framel.width  					= 256;
	updateMenuInfo.framel.height  					= 329;

	updateMenuInfo.framer.generic.type				= MTYPE_BITMAP;
	updateMenuInfo.framer.generic.name				= ART_FRAMER;
	updateMenuInfo.framer.generic.flags				= QMF_INACTIVE;
	updateMenuInfo.framer.generic.x					= 376;
	updateMenuInfo.framer.generic.y					= 76;
	updateMenuInfo.framer.width  					= 256;
	updateMenuInfo.framer.height  					= 334;

	updateMenuInfo.descriptiont.generic.type				= MTYPE_TEXT;
	updateMenuInfo.descriptiont.generic.x					= 320;
	updateMenuInfo.descriptiont.generic.y					= 150;
	updateMenuInfo.descriptiont.string						= "New version of the game";
	updateMenuInfo.descriptiont.color						= color_white;
	updateMenuInfo.descriptiont.style						= UI_CENTER;

	updateMenuInfo.descriptionb.generic.type				= MTYPE_TEXT;
	updateMenuInfo.descriptionb.generic.x					= 320;
	updateMenuInfo.descriptionb.generic.y					= 150 + BIGCHAR_HEIGHT+6;
	updateMenuInfo.descriptionb.string						= "is available";
	updateMenuInfo.descriptionb.color						= color_white;
	updateMenuInfo.descriptionb.style						= UI_CENTER;

	y = 200;

	static char current_version_buffer[20] = { 0 };
	static char latest_version_buffer[20] = { 0 };
	Com_sprintf(current_version_buffer, 20, "%d.%d.%d",
			Q3VR_VERSION_MAJOR,
			Q3VR_VERSION_MINOR,
			Q3VR_VERSION_PATCH);
	Com_sprintf(latest_version_buffer, 20, "%d.%d.%d",
			(int)trap_Cvar_VariableValue("q3vr_update_version_major"),
			(int)trap_Cvar_VariableValue("q3vr_update_version_minor"),
			(int)trap_Cvar_VariableValue("q3vr_update_version_patch"));

	static const char* current_version[] = { current_version_buffer, NULL };
	static const char* new_version[] = { latest_version_buffer, NULL };

	y += BIGCHAR_HEIGHT+2;
	updateMenuInfo.current.generic.type		= MTYPE_SPINCONTROL;
	updateMenuInfo.current.generic.name		= "Current:";
	updateMenuInfo.current.generic.flags		= QMF_INACTIVE;
	updateMenuInfo.current.generic.callback	= UI_UpdateMenu_Event;
	updateMenuInfo.current.generic.id			= ID_VER_CURRENT;
	updateMenuInfo.current.generic.x			= VR_X_POS;
	updateMenuInfo.current.generic.y			= y;
	updateMenuInfo.current.itemnames			= current_version;
	updateMenuInfo.current.numitems				= 1;

	y += BIGCHAR_HEIGHT+2;
	updateMenuInfo.new.generic.type		= MTYPE_SPINCONTROL;
	updateMenuInfo.new.generic.name		= "Latest:";
	updateMenuInfo.new.generic.flags		= QMF_INACTIVE;
	updateMenuInfo.new.generic.callback	= UI_UpdateMenu_Event;
	updateMenuInfo.new.generic.id			= ID_VER_NEW;
	updateMenuInfo.new.generic.x			= VR_X_POS;
	updateMenuInfo.new.generic.y			= y;
	updateMenuInfo.new.itemnames			= new_version;
	updateMenuInfo.new.numitems				= 1;

	updateMenuInfo.visit.generic.type			= MTYPE_PTEXT;
	updateMenuInfo.visit.generic.flags		= QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	updateMenuInfo.visit.generic.x				= 320;
	updateMenuInfo.visit.generic.y				= 310;
	updateMenuInfo.visit.generic.id				= ID_VISIT;
	updateMenuInfo.visit.generic.callback	= UI_UpdateMenu_Event;
	updateMenuInfo.visit.string						= "Visit Home Page";
	updateMenuInfo.visit.color						= color_red;
	updateMenuInfo.visit.style						= UI_CENTER;

	static const char* homePageUrl[] = { "https://ripper37.github.io/q3vr", NULL };
	updateMenuInfo.visit_manual.generic.type		= MTYPE_SPINCONTROL;
	updateMenuInfo.visit_manual.generic.name		= "Visit";
	updateMenuInfo.visit_manual.generic.flags		= QMF_INACTIVE;
	updateMenuInfo.visit_manual.generic.callback	= UI_UpdateMenu_Event;
	updateMenuInfo.visit_manual.generic.id			= ID_VER_NEW;
	updateMenuInfo.visit_manual.generic.x			= VR_X_POS - 110;
	updateMenuInfo.visit_manual.generic.y			= 310;
	updateMenuInfo.visit_manual.itemnames			= homePageUrl;
	updateMenuInfo.visit_manual.numitems				= 1;

	updateMenuInfo.back.generic.type					= MTYPE_BITMAP;
	updateMenuInfo.back.generic.name					= ART_BACK0;
	updateMenuInfo.back.generic.flags				= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	updateMenuInfo.back.generic.id					= ID_BACK;
	updateMenuInfo.back.generic.callback				= UI_UpdateMenu_Event;
	updateMenuInfo.back.generic.x					= 0;
	updateMenuInfo.back.generic.y					= 480-64;
	updateMenuInfo.back.width						= 128;
	updateMenuInfo.back.height						= 64;
	updateMenuInfo.back.focuspic						= ART_BACK1;

	Menu_AddItem( &updateMenuInfo.menu, &updateMenuInfo.banner );
	Menu_AddItem( &updateMenuInfo.menu, &updateMenuInfo.framel );
	Menu_AddItem( &updateMenuInfo.menu, &updateMenuInfo.framer );
	Menu_AddItem( &updateMenuInfo.menu, &updateMenuInfo.descriptiont );
	Menu_AddItem( &updateMenuInfo.menu, &updateMenuInfo.descriptionb );
	Menu_AddItem( &updateMenuInfo.menu, &updateMenuInfo.current );
	Menu_AddItem( &updateMenuInfo.menu, &updateMenuInfo.new );
#if defined(_WIN32)
	Menu_AddItem( &updateMenuInfo.menu, &updateMenuInfo.visit );
#else
	Menu_AddItem( &updateMenuInfo.menu, &updateMenuInfo.visit_manual );
#endif
	Menu_AddItem( &updateMenuInfo.menu, &updateMenuInfo.back );
}


/*
=================
UI_UpdateMenu_Cache
=================
*/
void UI_UpdateMenu_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
	trap_R_RegisterShaderNoMip( ART_FRAMEL );
	trap_R_RegisterShaderNoMip( ART_FRAMER );
}


/*
===============
UI_UpdateMenu
===============
*/
void UI_UpdateMenu( void ) {
	UI_UpdateMenu_Init();
	UI_PushMenu( &updateMenuInfo.menu );
}
