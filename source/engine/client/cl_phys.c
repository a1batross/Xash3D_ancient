//=======================================================================
//			Copyright XashXT Group 2008 ©
//		   cl_physics.c - client physic and prediction
//=======================================================================

#include "common.h"
#include "client.h"
#include "matrix_lib.h"
#include "const.h"
#include "pm_defs.h"

/*
================
CL_CheckVelocity
================
*/
void CL_CheckVelocity( edict_t *ent )
{
	int	i;
	float	maxvel;

	maxvel = fabs( clgame.movevars.maxvelocity );

	// bound velocity
	for( i = 0; i < 3; i++ )
	{
		if( IS_NAN( ent->v.velocity[i] ))
		{
			MsgDev( D_INFO, "Got a NaN velocity on %s\n", STRING( ent->v.classname ));
			ent->v.velocity[i] = 0;
		}
		if( IS_NAN( ent->v.origin[i] ))
		{
			MsgDev( D_INFO, "Got a NaN origin on %s\n", STRING( ent->v.classname ));
			ent->v.origin[i] = 0;
		}
	}

	if( VectorLength( ent->v.velocity ) > maxvel )
		VectorScale( ent->v.velocity, maxvel / VectorLength( ent->v.velocity ), ent->v.velocity );
}

/*
=============
CL_CheckWater
=============
*/
bool CL_CheckWater( edict_t *ent )
{
	int	cont;
	vec3_t	point;

	point[0] = ent->v.origin[0];
	point[1] = ent->v.origin[1];
	point[2] = ent->v.origin[2] + ent->v.mins[2] + 1;

	ent->v.waterlevel = 0;
	ent->v.watertype = CONTENTS_EMPTY;
	cont = CL_PointContents( point );

	if( cont <= CONTENTS_WATER )
	{
		ent->v.watertype = cont;
		ent->v.waterlevel = 1;
		point[2] = ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5f;

		if( CL_PointContents( point ) <= CONTENTS_WATER )
		{
			ent->v.waterlevel = 2;
			point[2] = ent->v.origin[2] + ent->v.view_ofs[2];

			if( CL_PointContents( point ) <= CONTENTS_WATER )
				ent->v.waterlevel = 3;
		}
		if( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		{
			static vec3_t current_table[] =
			{
				{ 1,  0, 0 },
				{ 0,  1, 0 },
				{-1,  0, 0 },
				{ 0, -1, 0 },
				{ 0,  0, 1 },
				{ 0,  0, -1}
			};
			float	speed = 150.0f * ent->v.waterlevel / 3.0f;
			float	*dir = current_table[CONTENTS_CURRENT_0 - cont];
			VectorMA( ent->v.basevelocity, speed, dir, ent->v.basevelocity );
		}
	}
	return ent->v.waterlevel > 1;
}