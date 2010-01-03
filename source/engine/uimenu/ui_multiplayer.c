/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


// THIS IS VERY TEMP STUFF WHILE WE WORK ON IT!!!

#include "common.h"
#include "ui_local.h"
#include "client.h"
#include "const.h"
#include "input.h"

#define ART_BANNER			"gfx/shell/head_multi"

#define ID_BACKGROUND		0
#define ID_BANNER			1

#define ID_INTERNETGAMES		2
#define ID_SPECTATEGAMES		3
#define ID_LANGAME			4
#define ID_CUSTOMIZE		5
#define ID_CONTROLS			6
#define ID_DONE			7

#define ID_ADDRESSBOOK		3
#define ID_REFRESH			4
#define ID_SERVERS			5

#define ID_FALLINGDAMAGE		3
#define ID_WEAPONSSTAY		4
#define ID_INSTANTPOWERUPS		5
#define ID_ALLOWPOWERUPS		6
#define ID_ALLOWHEALTH		7
#define ID_ALLOWARMOR		8
#define ID_SPAWNFARTHEST		9
#define ID_SAMEMAP			10
#define ID_FORCERESPAWN		11
#define ID_TEAMPLAY			12
#define ID_ALLOWEXIT		13
#define ID_INFINITEAMMO		14
#define ID_FIXEDFOV			15
#define ID_QUADDROP			16
#define ID_FRIENDLYFIRE		17

#define ID_MAPLIST			3
#define ID_RULES			4
#define ID_TIMELIMIT		5
#define ID_FRAGLIMIT		6
#define ID_MAXCLIENTS		7
#define ID_HOSTNAME			8
#define ID_DMOPTIONS		9
#define ID_BEGIN			10

static const char *uiDMOptionsYesNo[] = { "False", "True" };

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuBitmap_s	back;
	menuField_s	servers[16];
} uiAddressBook_t;

static uiAddressBook_t	uiAddressBook;

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;
	menuBitmap_s	back;

	menuAction_s	addressBook;
	menuAction_s	refresh;
	menuAction_s	servers[10];
} uiJoinServer_t;

static uiJoinServer_t	uiJoinServer;

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuBitmap_s	back;

	menuSpinControl_s	fallingDamage;
	menuSpinControl_s	weaponsStay;
	menuSpinControl_s	instantPowerups;
	menuSpinControl_s	allowPowerups;
	menuSpinControl_s	allowHealth;
	menuSpinControl_s	allowArmor;
	menuSpinControl_s	spawnFarthest;
	menuSpinControl_s	sameMap;
	menuSpinControl_s	forceRespawn;
	menuSpinControl_s	teamPlay;
	menuSpinControl_s	allowExit;
	menuSpinControl_s	infiniteAmmo;
	menuSpinControl_s	fixedFOV;
	menuSpinControl_s	quadDrop;
	menuSpinControl_s	friendlyFire;
} uiDMOptions_t;

static uiDMOptions_t	uiDMOptions;

typedef struct
{
	char		names[256][80];
	char		maps[256][80];
	int		numMaps;

	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuBitmap_s	back;

	menuSpinControl_s	mapList;
	menuSpinControl_s	rules;
	menuField_s	timeLimit;
	menuField_s	fragLimit;
	menuField_s	maxClients;
	menuField_s	hostName;
	menuAction_s	dmOptions;
	menuAction_s	begin;
} uiStartServer_t;

static uiStartServer_t	uiStartServer;

typedef struct
{
	menuFramework_s	menu;

	menuBitmap_s	background;
	menuBitmap_s	banner;

	menuAction_s	internetGames;
	menuAction_s	spectateGames;
	menuAction_s	LANGame;
	menuAction_s	Customize;	// playersetup
	menuAction_s	Controls;
	menuAction_s	done;
} uiMultiPlayer_t;

static uiMultiPlayer_t	uiMultiPlayer;


/*
=================
UI_AddressBook_SaveServers
=================
*/
static void UI_AddressBook_SaveServers( void )
{
	int	i;
	char	name[32];

	for( i = 0; i < 16; i++ )
	{
		com.snprintf( name, sizeof( name ), "server%i", i+1 );
		Cvar_Set( name, uiAddressBook.servers[i].buffer );
	}
}

/*
=================
UI_AddressBook_KeyFunc
=================
*/
static const char *UI_AddressBook_KeyFunc( int key, bool down )
{
	if( down && ( key == K_ESCAPE || key == K_MOUSE2 ))
		UI_AddressBook_SaveServers();
	return UI_DefaultKey( &uiAddressBook.menu, key, down );
}

/*
=================
UI_AddressBook_Callback
=================
*/
static void UI_AddressBook_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_DONE:
		UI_AddressBook_SaveServers();
		UI_PopMenu();
		break;
	}
}

/*
=================
UI_AddressBook_Ownerdraw
=================
*/
static void UI_AddressBook_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( uiAddressBook.menu.items[uiAddressBook.menu.cursor] == self )
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
	else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

	UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
}

/*
=================
UI_AddressBook_Init
=================
*/
static void UI_AddressBook_Init( void )
{
	int	i, y;

	Mem_Set( &uiAddressBook, 0, sizeof( uiAddressBook_t ));

	uiAddressBook.menu.keyFunc = UI_AddressBook_KeyFunc;

	uiAddressBook.background.generic.id = ID_BACKGROUND;
	uiAddressBook.background.generic.type = QMTYPE_BITMAP;
	uiAddressBook.background.generic.flags = QMF_INACTIVE;
	uiAddressBook.background.generic.x = 0;
	uiAddressBook.background.generic.y = 0;
	uiAddressBook.background.generic.width = 1024;
	uiAddressBook.background.generic.height = 768;
	uiAddressBook.background.pic = ART_BACKGROUND;

	uiAddressBook.banner.generic.id = ID_BANNER;
	uiAddressBook.banner.generic.type = QMTYPE_BITMAP;
	uiAddressBook.banner.generic.flags = QMF_INACTIVE;
	uiAddressBook.banner.generic.x = 0;
	uiAddressBook.banner.generic.y = 66;
	uiAddressBook.banner.generic.width = 1024;
	uiAddressBook.banner.generic.height = 46;
	uiAddressBook.banner.pic = ART_BANNER;

	uiAddressBook.back.generic.id = ID_DONE;
	uiAddressBook.back.generic.type = QMTYPE_BITMAP;
	uiAddressBook.back.generic.x = 413;
	uiAddressBook.back.generic.y = 656;
	uiAddressBook.back.generic.width = 198;
	uiAddressBook.back.generic.height = 38;
	uiAddressBook.back.generic.callback = UI_AddressBook_Callback;
	uiAddressBook.back.generic.ownerdraw = UI_AddressBook_Ownerdraw;
	uiAddressBook.back.pic = UI_BACKBUTTON;

	for( i = 0, y = 128; i < 16; i++, y += 32 )
	{
		uiAddressBook.servers[i].generic.id = ID_SERVERS+i;
		uiAddressBook.servers[i].generic.type = QMTYPE_FIELD;
		uiAddressBook.servers[i].generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
		uiAddressBook.servers[i].generic.x = 413;
		uiAddressBook.servers[i].generic.y = y;
		uiAddressBook.servers[i].generic.width = 198;
		uiAddressBook.servers[i].generic.height = 30;
		com.strncpy( uiAddressBook.servers[i].buffer, Cvar_VariableString( va( "server%i", i+1 )), sizeof( uiAddressBook.servers[i].buffer ));
	}

	UI_AddItem( &uiAddressBook.menu, (void *)&uiAddressBook.background );
	UI_AddItem( &uiAddressBook.menu, (void *)&uiAddressBook.banner );
	UI_AddItem( &uiAddressBook.menu, (void *)&uiAddressBook.back );

	for( i = 0; i < 16; i++ )
		UI_AddItem( &uiAddressBook.menu, (void *)&uiAddressBook.servers[i] );
	UI_PushMenu( &uiAddressBook.menu );
}

/*
=================
UI_JoinServer_Callback
=================
*/
static void UI_JoinServer_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_DONE:
		UI_PopMenu();
		break;
	case ID_ADDRESSBOOK:
		UI_AddressBook_Init();
		break;
	case ID_REFRESH:
		UI_RefreshServerList();
		break;
	default:
		if( com.stricmp( uiStatic.serverNames[item->id - ID_SERVERS], "<no server>" ))
		{
			char	text[128];

			com.snprintf( text, sizeof(text), "connect %s\n", NET_AdrToString( uiStatic.serverAddresses[item->id - ID_SERVERS] ));
			Cbuf_ExecuteText( EXEC_APPEND, text );
		}
		break;
	}
}

/*
=================
UI_JoinServer_Ownerdraw
=================
*/
static void UI_JoinServer_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( uiJoinServer.menu.items[uiJoinServer.menu.cursor] == self )
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
	else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

	UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
}

/*
=================
UI_JoinServer_Init
=================
*/
static void UI_JoinServer_Init( void )
{
	int	i, y;

	Mem_Set( &uiJoinServer, 0, sizeof( uiJoinServer_t ));

	uiJoinServer.background.generic.id = ID_BACKGROUND;
	uiJoinServer.background.generic.type = QMTYPE_BITMAP;
	uiJoinServer.background.generic.flags = QMF_INACTIVE;
	uiJoinServer.background.generic.x = 0;
	uiJoinServer.background.generic.y = 0;
	uiJoinServer.background.generic.width = 1024;
	uiJoinServer.background.generic.height = 768;
	uiJoinServer.background.pic = ART_BACKGROUND;

	uiJoinServer.banner.generic.id = ID_BANNER;
	uiJoinServer.banner.generic.type = QMTYPE_BITMAP;
	uiJoinServer.banner.generic.flags = QMF_INACTIVE;
	uiJoinServer.banner.generic.x = 0;
	uiJoinServer.banner.generic.y	= 66;
	uiJoinServer.banner.generic.width = 1024;
	uiJoinServer.banner.generic.height = 46;
	uiJoinServer.banner.pic = ART_BANNER;

	uiJoinServer.back.generic.id = ID_DONE;
	uiJoinServer.back.generic.type = QMTYPE_BITMAP;
	uiJoinServer.back.generic.x = 413;
	uiJoinServer.back.generic.y = 656;
	uiJoinServer.back.generic.width = 198;
	uiJoinServer.back.generic.height = 38;
	uiJoinServer.back.generic.callback = UI_JoinServer_Callback;
	uiJoinServer.back.generic.ownerdraw = UI_JoinServer_Ownerdraw;
	uiJoinServer.back.pic = UI_BACKBUTTON;

	uiJoinServer.addressBook.generic.id = ID_ADDRESSBOOK;
	uiJoinServer.addressBook.generic.name = "Address Book";
	uiJoinServer.addressBook.generic.type = QMTYPE_ACTION;
	uiJoinServer.addressBook.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiJoinServer.addressBook.generic.x = 413;
	uiJoinServer.addressBook.generic.y = 160;
	uiJoinServer.addressBook.generic.width = 198;
	uiJoinServer.addressBook.generic.height = 30;
	uiJoinServer.addressBook.generic.callback = UI_JoinServer_Callback;
	uiJoinServer.addressBook.background = "";

	uiJoinServer.refresh.generic.id = ID_REFRESH;
	uiJoinServer.refresh.generic.name = "Refresh Server List";
	uiJoinServer.refresh.generic.type = QMTYPE_ACTION;
	uiJoinServer.refresh.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiJoinServer.refresh.generic.x = 413;
	uiJoinServer.refresh.generic.y = 192;
	uiJoinServer.refresh.generic.width = 198;
	uiJoinServer.refresh.generic.height = 30;
	uiJoinServer.refresh.generic.callback = UI_JoinServer_Callback;
	uiJoinServer.refresh.background = "";

	for( i = 0, y = 256; i < 10; i++, y += 32 )
	{
		uiJoinServer.servers[i].generic.id = ID_SERVERS+i;
		uiJoinServer.servers[i].generic.name = uiStatic.serverNames[i];
		uiJoinServer.servers[i].generic.type = QMTYPE_ACTION;
		uiJoinServer.servers[i].generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
		uiJoinServer.servers[i].generic.x = 312;
		uiJoinServer.servers[i].generic.y = y;
		uiJoinServer.servers[i].generic.width = 400;
		uiJoinServer.servers[i].generic.height = 30;
		uiJoinServer.servers[i].generic.callback = UI_JoinServer_Callback;
		uiJoinServer.servers[i].generic.statusText = "Press ENTER or click to connect";
		uiJoinServer.servers[i].background = "";
	}

	UI_AddItem( &uiJoinServer.menu, (void *)&uiJoinServer.background );
	UI_AddItem( &uiJoinServer.menu, (void *)&uiJoinServer.banner );
	UI_AddItem( &uiJoinServer.menu, (void *)&uiJoinServer.back );
	UI_AddItem( &uiJoinServer.menu, (void *)&uiJoinServer.addressBook );
	UI_AddItem( &uiJoinServer.menu, (void *)&uiJoinServer.refresh );

	for( i = 0; i < 10; i++ )
		UI_AddItem( &uiJoinServer.menu, (void *)&uiJoinServer.servers[i] );

	UI_RefreshServerList();

	UI_PushMenu( &uiJoinServer.menu );
}

/*
=================
UI_DMOptions_Update
=================
*/
static void UI_DMOptions_Update( void )
{
	uiDMOptions.fallingDamage.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.fallingDamage.curValue];
	uiDMOptions.weaponsStay.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.weaponsStay.curValue];
	uiDMOptions.instantPowerups.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.instantPowerups.curValue];
	uiDMOptions.allowPowerups.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.allowPowerups.curValue];
	uiDMOptions.allowHealth.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.allowHealth.curValue];
	uiDMOptions.allowArmor.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.allowArmor.curValue];
	uiDMOptions.spawnFarthest.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.spawnFarthest.curValue];
	uiDMOptions.sameMap.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.sameMap.curValue];
	uiDMOptions.forceRespawn.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.forceRespawn.curValue];

	if( uiDMOptions.teamPlay.curValue == 0 )
		uiDMOptions.teamPlay.generic.name = "Disabled";
	else if( uiDMOptions.teamPlay.curValue == 1 )
		uiDMOptions.teamPlay.generic.name = "By Skin";
	else uiDMOptions.teamPlay.generic.name = "By Model";

	uiDMOptions.allowExit.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.allowExit.curValue];
	uiDMOptions.infiniteAmmo.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.infiniteAmmo.curValue];
	uiDMOptions.fixedFOV.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.fixedFOV.curValue];
	uiDMOptions.quadDrop.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.quadDrop.curValue];
	uiDMOptions.friendlyFire.generic.name = uiDMOptionsYesNo[(int)uiDMOptions.friendlyFire.curValue];
}

/*
=================
UI_DMOptions_Callback
=================
*/
static void UI_DMOptions_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;
	menuSpinControl_s	*sc = (menuSpinControl_s *)self;
	int		flags = Cvar_VariableInteger( "dmflags" );
	int		bit = 0;

	if( event == QM_ACTIVATED )
	{
		if( item->id == ID_DONE )
		{
			UI_PopMenu();
			return;
		}
	}

	if( item->id == ID_FRIENDLYFIRE )
	{
		if( sc->curValue ) flags &= ~DF_NO_FRIENDLY_FIRE;
		else flags |= DF_NO_FRIENDLY_FIRE;
		
		goto setvalue;
	}
	else if( item->id == ID_FALLINGDAMAGE )
	{
		if( sc->curValue ) flags &= ~DF_NO_FALLING;
		else flags |= DF_NO_FALLING;
		
		goto setvalue;
	}
	else if( item->id == ID_WEAPONSSTAY )
		bit = DF_WEAPONS_STAY;
	else if( item->id == ID_INSTANTPOWERUPS )
		bit = DF_INSTANT_ITEMS;
	else if( item->id == ID_ALLOWEXIT )
		bit = DF_ALLOW_EXIT;
	else if (item->id == ID_ALLOWPOWERUPS )
	{
		if( sc->curValue )
			flags &= ~DF_NO_ITEMS;
		else flags |= DF_NO_ITEMS;
		
		goto setvalue;
	}
	else if( item->id == ID_ALLOWHEALTH )
	{
		if( sc->curValue )
			flags &= ~DF_NO_HEALTH;
		else flags |= DF_NO_HEALTH;
		
		goto setvalue;
	}
	else if( item->id == ID_SPAWNFARTHEST )
		bit = DF_SPAWN_FARTHEST;
	else if( item->id == ID_TEAMPLAY )
	{
		if( sc->curValue == 1 )
		{
			flags |=  DF_SKINTEAMS;
			flags &= ~DF_MODELTEAMS;
		}
		else if( sc->curValue == 2 )
		{
			flags |=  DF_MODELTEAMS;
			flags &= ~DF_SKINTEAMS;
		}
		else flags &= ~(DF_MODELTEAMS|DF_SKINTEAMS);

		goto setvalue;
	}
	else if( item->id == ID_SAMEMAP )
		bit = DF_SAME_LEVEL;
	else if( item->id == ID_FORCERESPAWN )
		bit = DF_FORCE_RESPAWN;
	else if( item->id == ID_ALLOWARMOR )
	{
		if( sc->curValue )
			flags &= ~DF_NO_ARMOR;
		else flags |= DF_NO_ARMOR;
		
		goto setvalue;
	}
	else if( item->id == ID_INFINITEAMMO )
		bit = DF_INFINITE_AMMO;
	else if( item->id == ID_FIXEDFOV )
		bit = DF_FIXED_FOV;
	else if( item->id == ID_QUADDROP )
		bit = DF_QUAD_DROP;

	if( sc->curValue == 0 )
		flags &= ~bit;
	else flags |= bit;
	
setvalue:
	Cvar_SetValue( "dmflags", flags );
	UI_DMOptions_Update();
}

/*
=================
UI_DMOptions_Ownerdraw
=================
*/
static void UI_DMOptions_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;
	int		x, y, w, h, cw, ch, gap;

	if( item->id == ID_BACKGROUND )
	{
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ART_BACKGROUND );

		x = 246;
		y = 160;
		w = 198;
		h = 30;
		UI_ScaleCoords( &x, &y, &w, &h );

		cw = UI_SMALL_CHAR_WIDTH;
		ch = UI_SMALL_CHAR_HEIGHT;
		UI_ScaleCoords( NULL, NULL, &cw, &ch );

		gap = 32;
		UI_ScaleCoords( NULL, &gap, NULL, NULL );

		UI_DrawString( x, y+(gap*0), w, h, "Falling Damage", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*1), w, h, "Weapons Stay", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*2), w, h, "Instant Powerups", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*3), w, h, "Allow Powerups", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*4), w, h, "Allow Health", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*5), w, h, "Allow Armor", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*6), w, h, "Spawn Farthest", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*7), w, h, "Same Map", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*8), w, h, "Force Respawn", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*9), w, h, "Team Play", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*10), w, h, "Allow Exit", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*11), w, h, "Infinite Ammo", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*12), w, h, "Fixed FOV", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*13), w, h, "Quad Drop", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*14), w, h, "Friendly Fire", uiColorWhite, true, cw, ch, 2, true );
	}
	else
	{
		if( uiDMOptions.menu.items[uiDMOptions.menu.cursor] == self )
			UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
		else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );

		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
	}
}

/*
=================
UI_DMOptions_Init
=================
*/
static void UI_DMOptions_Init( void )
{
	int	flags = Cvar_VariableInteger( "dmflags" );

	Mem_Set( &uiDMOptions, 0, sizeof( uiDMOptions_t ));

	uiDMOptions.background.generic.id = ID_BACKGROUND;
	uiDMOptions.background.generic.type = QMTYPE_BITMAP;
	uiDMOptions.background.generic.flags = QMF_INACTIVE;
	uiDMOptions.background.generic.x = 0;
	uiDMOptions.background.generic.y = 0;
	uiDMOptions.background.generic.width = 1024;
	uiDMOptions.background.generic.height = 768;
	uiDMOptions.background.generic.ownerdraw = UI_DMOptions_Ownerdraw;
	uiDMOptions.background.pic = ART_BACKGROUND;

	uiDMOptions.banner.generic.id	= ID_BANNER;
	uiDMOptions.banner.generic.type = QMTYPE_BITMAP;
	uiDMOptions.banner.generic.flags = QMF_INACTIVE;
	uiDMOptions.banner.generic.x = 0;
	uiDMOptions.banner.generic.y = 66;
	uiDMOptions.banner.generic.width = 1024;
	uiDMOptions.banner.generic.height = 46;
	uiDMOptions.banner.pic = ART_BANNER;

	uiDMOptions.back.generic.id = ID_DONE;
	uiDMOptions.back.generic.type	= QMTYPE_BITMAP;
	uiDMOptions.back.generic.x = 413;
	uiDMOptions.back.generic.y = 656;
	uiDMOptions.back.generic.width = 198;
	uiDMOptions.back.generic.height = 38;
	uiDMOptions.back.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.back.generic.ownerdraw = UI_DMOptions_Ownerdraw;
	uiDMOptions.back.pic = UI_BACKBUTTON;

	uiDMOptions.fallingDamage.generic.id = ID_FALLINGDAMAGE;
	uiDMOptions.fallingDamage.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.fallingDamage.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.fallingDamage.generic.x = 580;
	uiDMOptions.fallingDamage.generic.y = 160;
	uiDMOptions.fallingDamage.generic.width = 198;
	uiDMOptions.fallingDamage.generic.height = 30;
	uiDMOptions.fallingDamage.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.fallingDamage.minValue = 0;
	uiDMOptions.fallingDamage.maxValue = 1;
	uiDMOptions.fallingDamage.range = 1;
	uiDMOptions.fallingDamage.curValue = (flags & DF_NO_FALLING) == 0;

	uiDMOptions.weaponsStay.generic.id = ID_WEAPONSSTAY;
	uiDMOptions.weaponsStay.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.weaponsStay.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.weaponsStay.generic.x = 580;
	uiDMOptions.weaponsStay.generic.y = 192;
	uiDMOptions.weaponsStay.generic.width = 198;
	uiDMOptions.weaponsStay.generic.height = 30;
	uiDMOptions.weaponsStay.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.weaponsStay.minValue = 0;
	uiDMOptions.weaponsStay.maxValue = 1;
	uiDMOptions.weaponsStay.range	= 1;
	uiDMOptions.weaponsStay.curValue = (flags & DF_WEAPONS_STAY) != 0;

	uiDMOptions.instantPowerups.generic.id = ID_INSTANTPOWERUPS;
	uiDMOptions.instantPowerups.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.instantPowerups.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.instantPowerups.generic.x = 580;
	uiDMOptions.instantPowerups.generic.y = 224;
	uiDMOptions.instantPowerups.generic.width = 198;
	uiDMOptions.instantPowerups.generic.height = 30;
	uiDMOptions.instantPowerups.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.instantPowerups.minValue = 0;
	uiDMOptions.instantPowerups.maxValue = 1;
	uiDMOptions.instantPowerups.range = 1;
	uiDMOptions.instantPowerups.curValue = (flags & DF_INSTANT_ITEMS) != 0;

	uiDMOptions.allowPowerups.generic.id = ID_ALLOWPOWERUPS;
	uiDMOptions.allowPowerups.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.allowPowerups.generic.flags	= QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.allowPowerups.generic.x = 580;
	uiDMOptions.allowPowerups.generic.y = 256;
	uiDMOptions.allowPowerups.generic.width	= 198;
	uiDMOptions.allowPowerups.generic.height = 30;
	uiDMOptions.allowPowerups.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.allowPowerups.minValue = 0;
	uiDMOptions.allowPowerups.maxValue = 1;
	uiDMOptions.allowPowerups.range = 1;
	uiDMOptions.allowPowerups.curValue = (flags & DF_NO_ITEMS) == 0;

	uiDMOptions.allowHealth.generic.id = ID_ALLOWHEALTH;
	uiDMOptions.allowHealth.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.allowHealth.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.allowHealth.generic.x = 580;
	uiDMOptions.allowHealth.generic.y = 288;
	uiDMOptions.allowHealth.generic.width = 198;
	uiDMOptions.allowHealth.generic.height = 30;
	uiDMOptions.allowHealth.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.allowHealth.minValue = 0;
	uiDMOptions.allowHealth.maxValue = 1;
	uiDMOptions.allowHealth.range	= 1;
	uiDMOptions.allowHealth.curValue = (flags & DF_NO_HEALTH) == 0;

	uiDMOptions.allowArmor.generic.id = ID_ALLOWARMOR;
	uiDMOptions.allowArmor.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.allowArmor.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.allowArmor.generic.x = 580;
	uiDMOptions.allowArmor.generic.y = 320;
	uiDMOptions.allowArmor.generic.width = 198;
	uiDMOptions.allowArmor.generic.height = 30;
	uiDMOptions.allowArmor.generic.callback	= UI_DMOptions_Callback;
	uiDMOptions.allowArmor.minValue = 0;
	uiDMOptions.allowArmor.maxValue = 1;
	uiDMOptions.allowArmor.range = 1;
	uiDMOptions.allowArmor.curValue = (flags & DF_NO_ARMOR) == 0;

	uiDMOptions.spawnFarthest.generic.id = ID_SPAWNFARTHEST;
	uiDMOptions.spawnFarthest.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.spawnFarthest.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.spawnFarthest.generic.x = 580;
	uiDMOptions.spawnFarthest.generic.y = 352;
	uiDMOptions.spawnFarthest.generic.width	= 198;
	uiDMOptions.spawnFarthest.generic.height = 30;
	uiDMOptions.spawnFarthest.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.spawnFarthest.minValue = 0;
	uiDMOptions.spawnFarthest.maxValue = 1;
	uiDMOptions.spawnFarthest.range = 1;
	uiDMOptions.spawnFarthest.curValue = (flags & DF_SPAWN_FARTHEST) != 0;

	uiDMOptions.sameMap.generic.id = ID_SAMEMAP;
	uiDMOptions.sameMap.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.sameMap.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.sameMap.generic.x	= 580;
	uiDMOptions.sameMap.generic.y = 384;
	uiDMOptions.sameMap.generic.width = 198;
	uiDMOptions.sameMap.generic.height = 30;
	uiDMOptions.sameMap.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.sameMap.minValue = 0;
	uiDMOptions.sameMap.maxValue = 1;
	uiDMOptions.sameMap.range = 1;
	uiDMOptions.sameMap.curValue = (flags & DF_SAME_LEVEL) != 0;

	uiDMOptions.forceRespawn.generic.id = ID_FORCERESPAWN;
	uiDMOptions.forceRespawn.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.forceRespawn.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.forceRespawn.generic.x = 580;
	uiDMOptions.forceRespawn.generic.y = 416;
	uiDMOptions.forceRespawn.generic.width = 198;
	uiDMOptions.forceRespawn.generic.height = 30;
	uiDMOptions.forceRespawn.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.forceRespawn.minValue = 0;
	uiDMOptions.forceRespawn.maxValue = 1;
	uiDMOptions.forceRespawn.range = 1;
	uiDMOptions.forceRespawn.curValue = (flags & DF_FORCE_RESPAWN) != 0;

	uiDMOptions.teamPlay.generic.id = ID_TEAMPLAY;
	uiDMOptions.teamPlay.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.teamPlay.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.teamPlay.generic.x = 580;
	uiDMOptions.teamPlay.generic.y = 448;
	uiDMOptions.teamPlay.generic.width = 198;
	uiDMOptions.teamPlay.generic.height = 30;
	uiDMOptions.teamPlay.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.teamPlay.minValue = 0;
	uiDMOptions.teamPlay.maxValue = 2;
	uiDMOptions.teamPlay.range = 1;

	uiDMOptions.allowExit.generic.id = ID_ALLOWEXIT;
	uiDMOptions.allowExit.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.allowExit.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.allowExit.generic.x = 580;
	uiDMOptions.allowExit.generic.y = 480;
	uiDMOptions.allowExit.generic.width = 198;
	uiDMOptions.allowExit.generic.height = 30;
	uiDMOptions.allowExit.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.allowExit.minValue = 0;
	uiDMOptions.allowExit.maxValue = 1;
	uiDMOptions.allowExit.range = 1;
	uiDMOptions.allowExit.curValue = (flags & DF_ALLOW_EXIT) != 0;

	uiDMOptions.infiniteAmmo.generic.id = ID_INFINITEAMMO;
	uiDMOptions.infiniteAmmo.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.infiniteAmmo.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.infiniteAmmo.generic.x = 580;
	uiDMOptions.infiniteAmmo.generic.y = 512;
	uiDMOptions.infiniteAmmo.generic.width = 198;
	uiDMOptions.infiniteAmmo.generic.height = 30;
	uiDMOptions.infiniteAmmo.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.infiniteAmmo.minValue = 0;
	uiDMOptions.infiniteAmmo.maxValue = 1;
	uiDMOptions.infiniteAmmo.range = 1;
	uiDMOptions.infiniteAmmo.curValue = (flags & DF_INFINITE_AMMO) != 0;

	uiDMOptions.fixedFOV.generic.id = ID_FIXEDFOV;
	uiDMOptions.fixedFOV.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.fixedFOV.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.fixedFOV.generic.x = 580;
	uiDMOptions.fixedFOV.generic.y = 544;
	uiDMOptions.fixedFOV.generic.width = 198;
	uiDMOptions.fixedFOV.generic.height = 30;
	uiDMOptions.fixedFOV.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.fixedFOV.minValue = 0;
	uiDMOptions.fixedFOV.maxValue = 1;
	uiDMOptions.fixedFOV.range = 1;
	uiDMOptions.fixedFOV.curValue	= (flags & DF_FIXED_FOV) != 0;

	uiDMOptions.quadDrop.generic.id = ID_QUADDROP;
	uiDMOptions.quadDrop.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.quadDrop.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.quadDrop.generic.x = 580;
	uiDMOptions.quadDrop.generic.y = 576;
	uiDMOptions.quadDrop.generic.width = 198;
	uiDMOptions.quadDrop.generic.height = 30;
	uiDMOptions.quadDrop.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.quadDrop.minValue	= 0;
	uiDMOptions.quadDrop.maxValue	= 1;
	uiDMOptions.quadDrop.range = 1;
	uiDMOptions.quadDrop.curValue	= (flags & DF_QUAD_DROP) != 0;

	uiDMOptions.friendlyFire.generic.id = ID_FRIENDLYFIRE;
	uiDMOptions.friendlyFire.generic.type = QMTYPE_SPINCONTROL;
	uiDMOptions.friendlyFire.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiDMOptions.friendlyFire.generic.x = 580;
	uiDMOptions.friendlyFire.generic.y = 608;
	uiDMOptions.friendlyFire.generic.width = 198;
	uiDMOptions.friendlyFire.generic.height = 30;
	uiDMOptions.friendlyFire.generic.callback = UI_DMOptions_Callback;
	uiDMOptions.friendlyFire.minValue = 0;
	uiDMOptions.friendlyFire.maxValue = 1;
	uiDMOptions.friendlyFire.range = 1;
	uiDMOptions.friendlyFire.curValue = (flags & DF_NO_FRIENDLY_FIRE) == 0;

	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.background );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.banner );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.back );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.fallingDamage );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.weaponsStay );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.instantPowerups );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.allowPowerups );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.allowHealth );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.allowArmor );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.spawnFarthest );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.sameMap );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.forceRespawn );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.teamPlay );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.allowExit );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.infiniteAmmo );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.fixedFOV );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.quadDrop );
	UI_AddItem( &uiDMOptions.menu, (void *)&uiDMOptions.friendlyFire );

	UI_DMOptions_Update();
	UI_PushMenu( &uiDMOptions.menu );
}

/*
=================
UI_StartServer_GetMapList
=================
*/
static void UI_StartServer_GetMapList( void )
{
	string	token;
	script_t	*script;

	// create new maplist if not exist
	if( !Cmd_CheckMapsList() || (script = Com_OpenScript( "scripts/maps.lst", NULL, 0 )) == NULL )
	{
		MsgDev( D_ERROR, "Cmd_GetMapsList: maps.lst not found or not created\n" );
		uiStartServer.mapList.generic.flags |= QMF_GRAYED;
		uiStartServer.begin.generic.flags |= QMF_GRAYED;
		return;
	}

	while( Com_ReadString( script, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, token ))
	{

		com.strncpy( uiStartServer.maps[uiStartServer.numMaps], token, sizeof( uiStartServer.maps[uiStartServer.numMaps] ));
		if( !Com_ReadString( script, SC_PARSE_GENERIC, token )) break;
		com.strncpy( uiStartServer.names[uiStartServer.numMaps], token, sizeof( uiStartServer.names[uiStartServer.numMaps] ));
		uiStartServer.numMaps++;
	}
	Com_CloseScript( script );

	uiStartServer.mapList.maxValue = uiStartServer.numMaps - 1;
}

/*
=================
UI_StartServer_Update
=================
*/
static void UI_StartServer_Update( void )
{
	uiStartServer.mapList.generic.name = uiStartServer.names[(int)uiStartServer.mapList.curValue];

	if( uiStartServer.rules.curValue )
		uiStartServer.rules.generic.name = "Coop";
	else uiStartServer.rules.generic.name = "Deathmatch";
}

/*
=================
UI_StartServer_Start
=================
*/
static void UI_StartServer_Start( void )
{
	if( Host_ServerState())
		Cbuf_ExecuteText( EXEC_APPEND, "disconnect\n" );

	Cvar_SetValue( "deathmatch", !((int)uiStartServer.rules.curValue));
	Cvar_SetValue( "coop", ((int)uiStartServer.rules.curValue));
	Cvar_SetValue( "timelimit", atoi(uiStartServer.timeLimit.buffer));
	Cvar_SetValue( "fraglimit", atoi(uiStartServer.fragLimit.buffer));
	Cvar_SetValue( "maxclients", atoi(uiStartServer.maxClients.buffer));
	Cvar_Set( "sv_hostname", uiStartServer.hostName.buffer );

	Cbuf_ExecuteText( EXEC_APPEND, va( "map %s\n", uiStartServer.maps[(int)uiStartServer.mapList.curValue] ));
}

/*
=================
UI_StartServer_Callback
=================
*/
static void UI_StartServer_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event == QM_CHANGED )
	{
		switch( item->id )
		{
		case ID_MAPLIST:
			UI_StartServer_Update();
			break;
		case ID_RULES:
			if( uiStartServer.rules.curValue )
			{
				if( com.atoi( uiStartServer.maxClients.buffer) > 8 )
					com.snprintf( uiStartServer.maxClients.buffer, sizeof( uiStartServer.maxClients.buffer ), "4" );
			}
			else {
				if( com.atoi( uiStartServer.maxClients.buffer) > 256 )
					com.snprintf( uiStartServer.maxClients.buffer, sizeof(uiStartServer.maxClients.buffer ), "256" );
			}
			UI_StartServer_Update();
			break;
		case ID_HOSTNAME:
			Cvar_Set( "sv_hostname", uiStartServer.hostName.buffer );
			break;
		}
		return;
	}

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_DONE:
		UI_PopMenu();
		break;
	case ID_DMOPTIONS:
		UI_DMOptions_Init();
		break;
	case ID_BEGIN:
		UI_StartServer_Start();
		break;
	}
}

/*
=================
UI_StartServer_Ownerdraw
=================
*/
static void UI_StartServer_Ownerdraw( void *self )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( item->id == ID_BACKGROUND )
	{
		int	x, y, w, h, cw, ch, gap;

		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ART_BACKGROUND );

		x = 246;
		y = 256;
		w = 198;
		h = 30;
		UI_ScaleCoords( &x, &y, &w, &h );

		cw = UI_SMALL_CHAR_WIDTH;
		ch = UI_SMALL_CHAR_HEIGHT;
		UI_ScaleCoords( NULL, NULL, &cw, &ch );

		gap = 32;
		UI_ScaleCoords( NULL, &gap, NULL, NULL );

		UI_DrawString( x, y+(gap*0), w, h, "Initial Map", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*2), w, h, "Rules", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*3), w, h, "Time Limit", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*4), w, h, "Frag Limit", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*5), w, h, "Max Clients", uiColorWhite, true, cw, ch, 2, true );
		UI_DrawString( x, y+(gap*6), w, h, "Host Name", uiColorWhite, true, cw, ch, 2, true );

		if( uiStartServer.numMaps )
		{
			x = 580;
			y = 288;
			w = 198;
			h = 30;

			UI_ScaleCoords( &x, &y, &w, &h );
			UI_DrawString( x, y, w, h, uiStartServer.maps[(int)uiStartServer.mapList.curValue], uiColorWhite, true, cw, ch, 1, true );
		}
	}
	else
	{
		if( uiStartServer.menu.items[uiStartServer.menu.cursor] == self )
			UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOXFOCUS );
		else UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, UI_MOVEBOX );
		UI_DrawPic( item->x, item->y, item->width, item->height, uiColorWhite, ((menuBitmap_s *)self)->pic );
	}
}

/*
=================
UI_StartServer_Init
=================
*/
static void UI_StartServer_Init( void )
{
	Mem_Set( &uiStartServer, 0, sizeof( uiStartServer_t ));

	uiStartServer.background.generic.id = ID_BACKGROUND;
	uiStartServer.background.generic.type = QMTYPE_BITMAP;
	uiStartServer.background.generic.flags = QMF_INACTIVE;
	uiStartServer.background.generic.x = 0;
	uiStartServer.background.generic.y = 0;
	uiStartServer.background.generic.width = 1024;
	uiStartServer.background.generic.height = 768;
	uiStartServer.background.generic.ownerdraw = UI_StartServer_Ownerdraw;

	uiStartServer.banner.generic.id = ID_BANNER;
	uiStartServer.banner.generic.type = QMTYPE_BITMAP;
	uiStartServer.banner.generic.flags = QMF_INACTIVE;
	uiStartServer.banner.generic.x = 0;
	uiStartServer.banner.generic.y = 66;
	uiStartServer.banner.generic.width = 1024;
	uiStartServer.banner.generic.height = 46;
	uiStartServer.banner.pic = ART_BANNER;

	uiStartServer.back.generic.id = ID_DONE;
	uiStartServer.back.generic.type = QMTYPE_BITMAP;
	uiStartServer.back.generic.x = 413;
	uiStartServer.back.generic.y = 656;
	uiStartServer.back.generic.width = 198;
	uiStartServer.back.generic.height = 38;
	uiStartServer.back.generic.callback = UI_StartServer_Callback;
	uiStartServer.back.generic.ownerdraw = UI_StartServer_Ownerdraw;
	uiStartServer.back.pic = UI_BACKBUTTON;

	uiStartServer.mapList.generic.id = ID_MAPLIST;
	uiStartServer.mapList.generic.type = QMTYPE_SPINCONTROL;
	uiStartServer.mapList.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiStartServer.mapList.generic.x = 580;
	uiStartServer.mapList.generic.y = 256;
	uiStartServer.mapList.generic.width = 198;
	uiStartServer.mapList.generic.height = 30;
	uiStartServer.mapList.generic.callback = UI_StartServer_Callback;
	uiStartServer.mapList.minValue = 0;
	uiStartServer.mapList.range = 1;
	
	uiStartServer.rules.generic.id = ID_RULES;
	uiStartServer.rules.generic.type = QMTYPE_SPINCONTROL;
	uiStartServer.rules.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiStartServer.rules.generic.x = 580;
	uiStartServer.rules.generic.y = 320;
	uiStartServer.rules.generic.width = 198;
	uiStartServer.rules.generic.height = 30;
	uiStartServer.rules.generic.callback = UI_StartServer_Callback;
	uiStartServer.rules.minValue = 0;
	uiStartServer.rules.maxValue = 1;
	uiStartServer.rules.range = 1;

	if( Cvar_VariableInteger( "coop" )) uiStartServer.rules.curValue = 1;

	uiStartServer.timeLimit.generic.id = ID_TIMELIMIT;
	uiStartServer.timeLimit.generic.type = QMTYPE_FIELD;
	uiStartServer.timeLimit.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW | QMF_NUMBERSONLY;
	uiStartServer.timeLimit.generic.x = 580;
	uiStartServer.timeLimit.generic.y = 352;
	uiStartServer.timeLimit.generic.width = 198;
	uiStartServer.timeLimit.generic.height = 30;
	uiStartServer.timeLimit.generic.statusText = "0 = No limit";
	uiStartServer.timeLimit.maxLenght = 3;
	com.snprintf( uiStartServer.timeLimit.buffer, sizeof( uiStartServer.timeLimit.buffer), "%i", Cvar_VariableInteger( "timelimit" ));

	uiStartServer.fragLimit.generic.id = ID_FRAGLIMIT;
	uiStartServer.fragLimit.generic.type = QMTYPE_FIELD;
	uiStartServer.fragLimit.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW | QMF_NUMBERSONLY;
	uiStartServer.fragLimit.generic.x = 580;
	uiStartServer.fragLimit.generic.y = 384;
	uiStartServer.fragLimit.generic.width = 198;
	uiStartServer.fragLimit.generic.height = 30;
	uiStartServer.fragLimit.generic.statusText = "0 = No limit";
	uiStartServer.fragLimit.maxLenght = 3;
	com.snprintf( uiStartServer.fragLimit.buffer, sizeof( uiStartServer.fragLimit.buffer ), "%i", Cvar_VariableInteger( "fraglimit" ));

	uiStartServer.maxClients.generic.id = ID_MAXCLIENTS;
	uiStartServer.maxClients.generic.type = QMTYPE_FIELD;
	uiStartServer.maxClients.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW | QMF_NUMBERSONLY;
	uiStartServer.maxClients.generic.x = 580;
	uiStartServer.maxClients.generic.y = 416;
	uiStartServer.maxClients.generic.width = 198;
	uiStartServer.maxClients.generic.height = 30;
	uiStartServer.maxClients.maxLenght = 3;
	if( Cvar_VariableInteger( "maxclients" ) <= 1 )
		com.snprintf( uiStartServer.maxClients.buffer, sizeof( uiStartServer.maxClients.buffer ), "8" );
	else com.snprintf( uiStartServer.maxClients.buffer, sizeof( uiStartServer.maxClients.buffer ), "%i", Cvar_VariableInteger( "maxclients" ));

	uiStartServer.hostName.generic.id = ID_HOSTNAME;
	uiStartServer.hostName.generic.type = QMTYPE_FIELD;
	uiStartServer.hostName.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiStartServer.hostName.generic.x = 580;
	uiStartServer.hostName.generic.y = 448;
	uiStartServer.hostName.generic.width = 198;
	uiStartServer.hostName.generic.height = 30;
	uiStartServer.hostName.generic.callback = UI_StartServer_Callback;
	uiStartServer.hostName.maxLenght = 16;
	com.strncpy( uiStartServer.hostName.buffer, Cvar_VariableString( "sv_hostname" ), sizeof( uiStartServer.hostName.buffer ));

	uiStartServer.dmOptions.generic.id = ID_DMOPTIONS;
	uiStartServer.dmOptions.generic.name = "DM Options";
	uiStartServer.dmOptions.generic.type = QMTYPE_ACTION;
	uiStartServer.dmOptions.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiStartServer.dmOptions.generic.x = 580;
	uiStartServer.dmOptions.generic.y = 480;
	uiStartServer.dmOptions.generic.width = 198;
	uiStartServer.dmOptions.generic.height = 30;
	uiStartServer.dmOptions.generic.callback = UI_StartServer_Callback;
	uiStartServer.dmOptions.background = "";

	uiStartServer.begin.generic.id = ID_BEGIN;
	uiStartServer.begin.generic.name = "Begin";
	uiStartServer.begin.generic.type = QMTYPE_ACTION;
	uiStartServer.begin.generic.flags = QMF_CENTER_JUSTIFY | QMF_PULSEIFFOCUS | QMF_DROPSHADOW;
	uiStartServer.begin.generic.x = 580;
	uiStartServer.begin.generic.y = 512;
	uiStartServer.begin.generic.width = 198;
	uiStartServer.begin.generic.height = 30;
	uiStartServer.begin.generic.callback = UI_StartServer_Callback;
	uiStartServer.begin.background = "";

	UI_StartServer_GetMapList();

	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.background );
	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.banner );
	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.back );
	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.mapList );
	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.rules );
	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.timeLimit );
	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.fragLimit );
	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.maxClients );
	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.hostName );
	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.dmOptions );
	UI_AddItem( &uiStartServer.menu, (void *)&uiStartServer.begin );

	UI_StartServer_Update();
	UI_PushMenu( &uiStartServer.menu );
}

/*
=================
UI_MultiPlayer_Callback
=================
*/
static void UI_MultiPlayer_Callback( void *self, int event )
{
	menuCommon_s	*item = (menuCommon_s *)self;

	if( event != QM_ACTIVATED )
		return;

	switch( item->id )
	{
	case ID_INTERNETGAMES:
	case ID_SPECTATEGAMES:
		UI_JoinServer_Init();
		break;
	case ID_LANGAME:
		UI_StartServer_Init();
		break;
	case ID_CUSTOMIZE:
		UI_PlayerSetup_Menu();
		break;
	case ID_CONTROLS:
		UI_Controls_Menu();
		break;
	case ID_DONE:
		UI_PopMenu();
		break;
	}
}

/*
=================
UI_MultiPlayer_Init
=================
*/
static void UI_MultiPlayer_Init( void )
{
	Mem_Set( &uiMultiPlayer, 0, sizeof( uiMultiPlayer_t ));

	uiMultiPlayer.background.generic.id = ID_BACKGROUND;
	uiMultiPlayer.background.generic.type = QMTYPE_BITMAP;
	uiMultiPlayer.background.generic.flags = QMF_INACTIVE;
	uiMultiPlayer.background.generic.x = 0;
	uiMultiPlayer.background.generic.y = 0;
	uiMultiPlayer.background.generic.width = 1024;
	uiMultiPlayer.background.generic.height = 768;
	uiMultiPlayer.background.pic = ART_BACKGROUND;

	uiMultiPlayer.banner.generic.id = ID_BANNER;
	uiMultiPlayer.banner.generic.type = QMTYPE_BITMAP;
	uiMultiPlayer.banner.generic.flags = QMF_INACTIVE;
	uiMultiPlayer.banner.generic.x = UI_BANNER_POSX;
	uiMultiPlayer.banner.generic.y = UI_BANNER_POSY;
	uiMultiPlayer.banner.generic.width = UI_BANNER_WIDTH;
	uiMultiPlayer.banner.generic.height = UI_BANNER_HEIGHT;
	uiMultiPlayer.banner.pic = ART_BANNER;

	uiMultiPlayer.internetGames.generic.id = ID_INTERNETGAMES;
	uiMultiPlayer.internetGames.generic.type = QMTYPE_ACTION;
	uiMultiPlayer.internetGames.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY|QMF_GRAYED;
	uiMultiPlayer.internetGames.generic.x = 72;
	uiMultiPlayer.internetGames.generic.y = 230;
	uiMultiPlayer.internetGames.generic.name = "Internet games";
	uiMultiPlayer.internetGames.generic.statusText = "View list of a game internet servers and join the one of your choise";
	uiMultiPlayer.internetGames.generic.callback = UI_MultiPlayer_Callback;

	uiMultiPlayer.spectateGames.generic.id = ID_SPECTATEGAMES;
	uiMultiPlayer.spectateGames.generic.type = QMTYPE_ACTION;
	uiMultiPlayer.spectateGames.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY|QMF_GRAYED;
	uiMultiPlayer.spectateGames.generic.x = 72;
	uiMultiPlayer.spectateGames.generic.y = 280;
	uiMultiPlayer.spectateGames.generic.name = "Spectate games";
	uiMultiPlayer.spectateGames.generic.statusText = "Spectate internet games";
	uiMultiPlayer.spectateGames.generic.callback = UI_MultiPlayer_Callback;

	uiMultiPlayer.LANGame.generic.id = ID_LANGAME;
	uiMultiPlayer.LANGame.generic.type = QMTYPE_ACTION;
	uiMultiPlayer.LANGame.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY|QMF_GRAYED;
	uiMultiPlayer.LANGame.generic.x = 72;
	uiMultiPlayer.LANGame.generic.y = 330;
	uiMultiPlayer.LANGame.generic.name = "LAN game";
	uiMultiPlayer.LANGame.generic.statusText = "Set up the game on the local area network";
	uiMultiPlayer.LANGame.generic.callback = UI_MultiPlayer_Callback;

	uiMultiPlayer.Customize.generic.id = ID_CUSTOMIZE;
	uiMultiPlayer.Customize.generic.type = QMTYPE_ACTION;
	uiMultiPlayer.Customize.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMultiPlayer.Customize.generic.x = 72;
	uiMultiPlayer.Customize.generic.y = 380;
	uiMultiPlayer.Customize.generic.name = "Customize";
	uiMultiPlayer.Customize.generic.statusText = "Choose your player name, and select visual options for your character";
	uiMultiPlayer.Customize.generic.callback = UI_MultiPlayer_Callback;

	uiMultiPlayer.Controls.generic.id = ID_CONTROLS;
	uiMultiPlayer.Controls.generic.type = QMTYPE_ACTION;
	uiMultiPlayer.Controls.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMultiPlayer.Controls.generic.x = 72;
	uiMultiPlayer.Controls.generic.y = 430;
	uiMultiPlayer.Controls.generic.name = "Controls";
	uiMultiPlayer.Controls.generic.statusText = "Change keyboard and mouse settings";
	uiMultiPlayer.Controls.generic.callback = UI_MultiPlayer_Callback;

	uiMultiPlayer.done.generic.id = ID_DONE;
	uiMultiPlayer.done.generic.type = QMTYPE_ACTION;
	uiMultiPlayer.done.generic.flags = QMF_HIGHLIGHTIFFOCUS|QMF_DROPSHADOW|QMF_NOTIFY;
	uiMultiPlayer.done.generic.x = 72;
	uiMultiPlayer.done.generic.y = 480;
	uiMultiPlayer.done.generic.name = "Done";
	uiMultiPlayer.done.generic.statusText = "Go back to the Main Menu";
	uiMultiPlayer.done.generic.callback = UI_MultiPlayer_Callback;

	UI_AddItem( &uiMultiPlayer.menu, (void *)&uiMultiPlayer.background );
	UI_AddItem( &uiMultiPlayer.menu, (void *)&uiMultiPlayer.banner );
	UI_AddItem( &uiMultiPlayer.menu, (void *)&uiMultiPlayer.internetGames );
	UI_AddItem( &uiMultiPlayer.menu, (void *)&uiMultiPlayer.spectateGames );
	UI_AddItem( &uiMultiPlayer.menu, (void *)&uiMultiPlayer.LANGame );
	UI_AddItem( &uiMultiPlayer.menu, (void *)&uiMultiPlayer.Customize );
	UI_AddItem( &uiMultiPlayer.menu, (void *)&uiMultiPlayer.Controls );
	UI_AddItem( &uiMultiPlayer.menu, (void *)&uiMultiPlayer.done );
}

/*
=================
UI_MultiPlayer_Precache
=================
*/
void UI_MultiPlayer_Precache( void )
{
	if( !re ) return;

	re->RegisterShader( ART_BACKGROUND, SHADER_NOMIP );
	re->RegisterShader( ART_BANNER, SHADER_NOMIP );
}

/*
=================
UI_MultiPlayer_Menu
=================
*/
void UI_MultiPlayer_Menu( void )
{
	UI_MultiPlayer_Precache();
	UI_MultiPlayer_Init();

	UI_PushMenu( &uiMultiPlayer.menu );
}