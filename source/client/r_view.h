//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VIEW_H
#define VIEW_H 

void V_StartPitchDrift( void );
void V_StopPitchDrift( void );

//camera flags
#define	CAMERA_ON		1
#define	DRAW_HUD		2
#define	INVERSE_X		4
#define	MONSTER_VIEW	8

typedef struct pitchdrift_s
{
	float	pitchvel;
	int  	nodrift;
	float	driftmove;
	double	laststop;
}pitchdrift_t;
static pitchdrift_t pd;


#define ORIGIN_BACKUP 64
#define ORIGIN_MASK ( ORIGIN_BACKUP - 1 )

typedef struct 
{
	float Origins[ ORIGIN_BACKUP ][3];
	float OriginTime[ ORIGIN_BACKUP ];

	float Angles[ ORIGIN_BACKUP ][3];
	float AngleTime[ ORIGIN_BACKUP ];

	int CurrentOrigin;
	int CurrentAngle;
}viewinterp_t;

#endif // VIEW_H