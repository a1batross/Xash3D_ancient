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
// cl_view.c -- player rendering positioning

#include "common.h"
#include "client.h"

// refdef ents
#define MAX_ENTITIES		4096	// FIXME: wrote dynamic allocator

//=============
//
// development tools for weapons
//
model_t *gun_model;

//=============

cvar_t		*crosshair;
cvar_t		*cl_testparticles;
cvar_t		*cl_testentities;
cvar_t		*cl_testlights;
cvar_t		*cl_testblend;

cvar_t		*cl_stats;

int		r_numdlights;
dlight_t		r_dlights[MAX_DLIGHTS];

int		r_numparticles;
particle_t	r_particles[MAX_PARTICLES];

lightstyle_t	r_lightstyles[MAX_LIGHTSTYLES];

char cl_weaponmodels[MAX_CLIENTWEAPONMODELS][MAX_QPATH];
int num_cl_weaponmodels;

int entitycmpfnc( const entity_t *a, const entity_t *b )
{
	// all other models are sorted by model then skin
	return ((int)a->model - (int)b->model);
}

/*
====================
V_ClearScene

Specifies the model that will be used as the world
====================
*/
void V_ClearScene (void)
{
	r_numdlights = 0;
	cls.ref_numents = 0;
	r_numparticles = 0;
}

/*
=================
void V_CalcRect( void )

Sets scr_vrect, the coordinates of the rendered window
=================
*/
void V_CalcRect( void )
{
	scr_vrect.width = scr_width->integer;
	scr_vrect.width &= ~7;
	scr_vrect.height = scr_height->integer;
	scr_vrect.height &= ~1;
	scr_vrect.y = scr_vrect.x = 0;
}

/*
=====================
V_AddEntity

=====================
*/
void V_AddEntity (entity_t *ent)
{
	if( cls.ref_numents >= host.max_edicts )
		return;
	cls.ref_entities[cls.ref_numents++] = *ent;
}


/*
=====================
V_AddParticle

=====================
*/
void V_AddParticle (vec3_t org, int color, float alpha)
{
	particle_t	*p;

	if (r_numparticles >= MAX_PARTICLES)
		return;
	p = &r_particles[r_numparticles++];
	VectorCopy (org, p->origin);
	p->color = color;
	p->alpha = alpha;
}

/*
=====================
V_AddLight

=====================
*/
void V_AddLight (vec3_t org, float intensity, float r, float g, float b)
{
	dlight_t	*dl;

	if (r_numdlights >= MAX_DLIGHTS)
		return;
	dl = &r_dlights[r_numdlights++];
	VectorCopy (org, dl->origin);
	dl->intensity = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
}


/*
=====================
V_AddLightStyle

=====================
*/
void V_AddLightStyle( int style, float r, float g, float b )
{
	lightstyle_t	*ls;

	if (style < 0 || style > MAX_LIGHTSTYLES)
		Host_Error("V_AddLightStyle: invalid light style %i\n", style);
	ls = &r_lightstyles[style];

	ls->white = r+g+b;
	ls->rgb[0] = r;
	ls->rgb[1] = g;
	ls->rgb[2] = b;
}

/*
================
V_TestParticles

If cl_testparticles is set, create 4096 particles in the view
================
*/
void V_TestParticles (void)
{
	particle_t	*p;
	int			i, j;
	float		d, r, u;

	r_numparticles = MAX_PARTICLES;
	for (i=0 ; i<r_numparticles ; i++)
	{
		d = i*0.25;
		r = 4*((i&7)-3.5);
		u = 4*(((i>>3)&7)-3.5);
		p = &r_particles[i];

		for (j=0 ; j<3 ; j++)
			p->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*d +
			cl.v_right[j]*r + cl.v_up[j]*u;

		p->color = 8;
		p->alpha = cl_testparticles->value;
	}
}

/*
================
V_TestEntities

If cl_testentities is set, create 32 player models
================
*/
void V_TestEntities (void)
{
	int		i, j;
	float		f, r;
	entity_t		*ent;

	cls.ref_numents = 32;
	memset( cls.ref_entities, 0, sizeof(entity_t));

	for( i = 0; i < cls.ref_numents; i++ )
	{
		ent = &cls.ref_entities[i];

		r = 64 * ((i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for( j = 0; j < 3; j++ )
			ent->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f + cl.v_right[j] * r;
		ent->model = cl.baseclientinfo.model;
	}
}

/*
================
V_TestLights

If cl_testlights is set, create 32 lights models
================
*/
void V_TestLights (void)
{
	int			i, j;
	float		f, r;
	dlight_t	*dl;

	r_numdlights = 32;
	memset (r_dlights, 0, sizeof(r_dlights));

	for (i=0 ; i<r_numdlights ; i++)
	{
		dl = &r_dlights[i];

		r = 64 * ( (i%4) - 1.5 );
		f = 64 * (i/4) + 128;

		for (j=0 ; j<3 ; j++)
			dl->origin[j] = cl.refdef.vieworg[j] + cl.v_forward[j]*f +
			cl.v_right[j]*r;
		dl->color[0] = ((i%6)+1) & 1;
		dl->color[1] = (((i%6)+1) & 2)>>1;
		dl->color[2] = (((i%6)+1) & 4)>>2;
		dl->intensity = 200;
	}
}

//===================================================================

/*
=================
CL_PrepRefresh

Call before entering a new level, or after changing dlls
=================
*/
void CL_PrepRefresh( void )
{
	char		mapname[32];
	string		name;
	float		rotate;
	vec3_t		axis;
	int		i; 
	int		mdlcount = 0, imgcount = 0, cl_count = 0;

	if (!cl.configstrings[CS_MODELS+1][0])
		return; // no map loaded

	Msg("CL_PrepRefresh: %s\n", cl.configstrings[CS_NAME] );

	// let the render dll load the map
	FS_FileBase( cl.configstrings[CS_MODELS+1], mapname ); 
	SCR_UpdateScreen();
	re->BeginRegistration( mapname ); // load map
	SCR_UpdateScreen();

	num_cl_weaponmodels = 1;
	com.strcpy(cl_weaponmodels[0], "v_glock.mdl");

	for( i = 1; i < MAX_MODELS; i++ )
	{
		if(!cl.configstrings[CS_MODELS+i][0])
			break;
		mdlcount++; // total num models
	}
	for( i = 1; i < MAX_IMAGES; i++ )
	{
		if(!cl.configstrings[CS_IMAGES+i][0])
			break;
		imgcount++; // total num models
	}
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if(!cl.configstrings[CS_PLAYERSKINS+i][0]) 
			continue;
		cl_count++;
	}
	
	// create thread here ?
	for (i = 1; i < MAX_MODELS && cl.configstrings[CS_MODELS+i][0]; i++)
	{
		com.strncpy( name, cl.configstrings[CS_MODELS+i], MAX_STRING );
		SCR_UpdateScreen();
		Sys_SendKeyEvents(); // pump message loop

		cl.model_draw[i] = re->RegisterModel( name );
		cl.models[i] = pe->RegisterModel( name );
		Cvar_SetValue("scr_loading", scr_loading->value + 50.0f/mdlcount );
		SCR_UpdateScreen();
	}

	// create thread here ?
	SCR_UpdateScreen();
	for (i = 1; i < MAX_IMAGES && cl.configstrings[CS_IMAGES+i][0]; i++)
	{
		cl.image_precache[i] = re->RegisterPic (cl.configstrings[CS_IMAGES+i]);
		Sys_SendKeyEvents (); // pump message loop
		Cvar_SetValue("scr_loading", scr_loading->value + 3.0f/imgcount );
		SCR_UpdateScreen();
	}

	// create thread here ?	
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if(!cl.configstrings[CS_PLAYERSKINS+i][0]) continue;
		Cvar_SetValue("scr_loading", scr_loading->value + 2.0f/cl_count );
		SCR_UpdateScreen ();
		Sys_SendKeyEvents();	// pump message loop
		CL_ParseClientinfo(i);
	}

	CL_LoadClientinfo (&cl.baseclientinfo, "unnamed\\male/grunt");

	// set sky textures and speed
	SCR_UpdateScreen();
	rotate = com.atof(cl.configstrings[CS_SKYROTATE]);
	com.atov( axis, cl.configstrings[CS_SKYAXIS], 3 );
	re->SetSky( cl.configstrings[CS_SKY], rotate, axis);
          Cvar_SetValue("scr_loading", 100.0f ); // all done
	
	re->EndRegistration (); // the render can now free unneeded stuff
	Con_ClearNotify(); // clear any lines of console text
	SCR_UpdateScreen();
	cl.refresh_prepped = true;
	cl.force_refdef = true;
}

/*
====================
V_CalcFov
====================
*/
float V_CalcFov( float fov_x, float width, float height )
{
	float	fov_y, x, rad = 360.0f * M_PI;

	fov_x = bound( 1, fov_x, 179 );
	x = width / tan( fov_x / rad );
	fov_y = atan2( height, x );
	fov_y = (fov_y * rad);

	return fov_y;
}

//============================================================================

/*
==================
V_RenderView

==================
*/
void V_RenderView( void )
{
	if (cls.state != ca_active) return;
	if (!cl.refresh_prepped) return; // still loading

	// an invalid frame will just use the exact previous refdef
	// we can't use the old frame if the video mode has changed, though...
	if ( cl.frame.valid && (cl.force_refdef || !cl_paused->value) )
	{
		cl.force_refdef = false;

		V_ClearScene ();

		// build a refresh entity list and calc cl.sim*
		// this also calls CL_CalcViewValues which loads
		// v_forward, etc.
		CL_VM_Begin();
		CL_AddEntities ();

		if (cl_testparticles->value)
			V_TestParticles ();
		if (cl_testentities->value)
			V_TestEntities ();
		if (cl_testlights->value)
			V_TestLights ();
		if (cl_testblend->value)
		{
			cl.refdef.blend[0] = 1;
			cl.refdef.blend[1] = 0.5;
			cl.refdef.blend[2] = 0.25;
			cl.refdef.blend[3] = 0.5;
		}

		// never let it sit exactly on a node line, because a water plane can
		// dissapear when viewed with the eye exactly on it.
		// the server protocol only specifies to 1/16 pixel, so add 1/32 in each axis
		cl.refdef.vieworg[0] += 1.0 / 32;
		cl.refdef.vieworg[1] += 1.0 / 32;
		cl.refdef.vieworg[2] += 1.0 / 32;

		cl.refdef.x = scr_vrect.x;
		cl.refdef.y = scr_vrect.y;
		cl.refdef.width = scr_vrect.width;
		cl.refdef.height = scr_vrect.height;
		cl.refdef.fov_y = V_CalcFov (cl.refdef.fov_x, cl.refdef.width, cl.refdef.height);
		cl.refdef.time = cls.realtime * 0.001f; // render use realtime now

		cl.refdef.areabits = cl.frame.areabits;

		if( !cl_add_entities->value )
			cls.ref_numents = 0;
		if (!cl_add_particles->value)
			r_numparticles = 0;
		if (!cl_add_lights->value)
			r_numdlights = 0;
		if (!cl_add_blend->value)
		{
			VectorClear (cl.refdef.blend);
		}

		cl.refdef.num_entities = cls.ref_numents;
		cl.refdef.entities = cls.ref_entities;
		cl.refdef.num_particles = r_numparticles;
		cl.refdef.particles = r_particles;
		cl.refdef.num_dlights = r_numdlights;
		cl.refdef.dlights = r_dlights;
		cl.refdef.lightstyles = r_lightstyles;

		cl.refdef.rdflags = cl.frame.ps.effects;

		// sort entities for better cache locality
        		qsort( cl.refdef.entities, cl.refdef.num_entities, sizeof( cl.refdef.entities[0] ), (int(*)(const void *, const void *))entitycmpfnc );
	}
	re->RenderFrame (&cl.refdef);
	if( cl_stats->value ) Msg("ent:%i  lt:%i  part:%i\n", cls.ref_numents, r_numdlights, r_numparticles );
}

/*
==================
V_PreRender

==================
*/
bool V_PreRender( void )
{
	// too early
	if(!re) return false;
		
	re->BeginFrame();

	// clear screen
	SCR_FillRect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, g_color_table[0] );

	return true;
}

/*
==================
V_PostRender

==================
*/
void V_PostRender( void )
{
	UI_Draw();
	Con_DrawConsole();
	re->EndFrame();
}

/*
=============
V_Viewpos_f
=============
*/
void V_Viewpos_f( void )
{
	Msg("(%g %g %g) : %g\n", cl.refdef.vieworg[0], cl.refdef.vieworg[1], cl.refdef.vieworg[2], cl.refdef.viewangles[YAW]);
}

/*
=============
V_Init
=============
*/
void V_Init (void)
{
	Cmd_AddCommand ("viewpos", V_Viewpos_f, "prints current player origin" );

	crosshair = Cvar_Get ("crosshair", "0", CVAR_ARCHIVE, "crosshair style" );
	cl_testblend = Cvar_Get ("cl_testblend", "0", 0, "test blending" );
	cl_testparticles = Cvar_Get ("cl_testparticles", "0", 0, "test particle engine" );
	cl_testentities = Cvar_Get ("cl_testentities", "0", 0, "test client entities" );
	cl_testlights = Cvar_Get ("cl_testlights", "0", 0, "test dynamic lights" );
	cl_stats = Cvar_Get ("cl_stats", "0", 0, "enable client stats" );
	cls.mempool = Mem_AllocPool( "Client Static" );
	cls.ref_entities = Mem_Alloc( cls.mempool, sizeof(entity_t) * host.max_edicts );	
}

void V_Shutdown( void )
{
	Mem_FreePool( &cls.mempool );
}