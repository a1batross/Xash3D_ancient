//=======================================================================
//			Copyright XashXT Group 2008 ©
//		        cl_progs.c - client.dat interface
//=======================================================================

#include "common.h"
#include "client.h"

/*
=========
PF_drawnet

void DrawNet( vector pos, string image )
=========
*/
static void PF_drawnet( void )
{
	float	*pos;
	shader_t	shader;

	if(!VM_ValidateArgs( "DrawNet", 2 ))
		return;
	if(cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < CMD_BACKUP-1)
		return;

	VM_ValidateString(PRVM_G_STRING(OFS_PARM1));
	pos = PRVM_G_VECTOR(OFS_PARM0);
	shader = re->RegisterShader( PRVM_G_STRING(OFS_PARM1), SHADER_NOMIP ); 

	SCR_DrawPic( pos[0], pos[1], -1, -1, shader );
}

/*
=========
PF_drawfps

void DrawFPS( vector pos )
=========
*/
static void PF_drawfps( void )
{
	float		calc;
	static double	nexttime = 0, lasttime = 0;
	static double	framerate = 0;
	static long	framecount = 0;
	double		newtime;
	bool		red = false; // fps too low
	char		fpsstring[32];
	float		*color, *pos;

	if(cls.state != ca_active) return; 
	if(!cl_showfps->integer) return;
	if(!VM_ValidateArgs( "drawfps", 1 ))
		return;
	
	newtime = Sys_DoubleTime();
	if( newtime >= nexttime )
	{
		framerate = framecount / (newtime - lasttime);
		lasttime = newtime;
		nexttime = max(nexttime + 1, lasttime - 1);
		framecount = 0;
	}
	framecount++;
	calc = framerate;
	pos = PRVM_G_VECTOR(OFS_PARM0);

	if ((red = (calc < 1.0f)))
	{
		com.snprintf(fpsstring, sizeof(fpsstring), "%4i spf", (int)(1.0f / calc + 0.5));
		color = g_color_table[1];
	}
	else
	{
		com.snprintf(fpsstring, sizeof(fpsstring), "%4i fps", (int)(calc + 0.5));
		color = g_color_table[3];
          }
	SCR_DrawBigStringColor(pos[0], pos[1], fpsstring, color );
}

/*
=========
PF_levelshot

float HUD_MakeLevelShot( void )
=========
*/
static void PF_levelshot( void )
{
	PRVM_G_FLOAT(OFS_RETURN) = 0;
	if(!VM_ValidateArgs( "HUD_MakeLevelShot", 0 ))
		return;
	
	if( cl.make_levelshot )
	{
		Con_ClearNotify();
		cl.make_levelshot = false;

		// make levelshot at nextframe()
		Cbuf_ExecuteText( EXEC_APPEND, "levelshot\n" );
		PRVM_G_FLOAT(OFS_RETURN) = 1;
	}
}

/*
=================
PF_findexplosionplane

vector CL_FindExplosionPlane( vector org, float radius )
=================
*/
static void PF_findexplosionplane( void )
{
	static vec3_t	planes[6] = {{0, 0, 1}, {0, 1, 0}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0}, {-1, 0, 0}};
	trace_t		trace;
	float		best = 1.0, radius;
	vec3_t		point, dir;
	const float	*org;
	int		i;

	if( !VM_ValidateArgs( "CL_FindExplosionPlane", 2 ))
		return;

	org = PRVM_G_VECTOR(OFS_PARM0);
	radius = PRVM_G_FLOAT(OFS_PARM1);
	VectorClear( dir );

	for( i = 0; i < 6; i++ )
	{
		VectorMA( org, radius, planes[i], point );

		trace = CL_Trace( org, vec3_origin, vec3_origin, point, MOVE_WORLDONLY, NULL, MASK_SOLID );
		if( trace.allsolid || trace.fraction == 1.0 )
			continue;

		if( trace.fraction < best )
		{
			best = trace.fraction;
			VectorCopy( trace.plane.normal, dir );
		}
	}
	VectorCopy( dir, PRVM_G_VECTOR( OFS_RETURN ));
}