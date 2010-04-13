//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       ev_common.cpp - events common code
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "r_beams.h"
#include "r_particle.h"
#include "r_tempents.h"
#include "effects_api.h"
#include "pm_materials.h"
#include "pm_movevars.h"
#include "ev_hldm.h"
#include "bullets.h"			// server\client shared enum
#include "hud.h"

Vector previousorigin;	// egon use this
static int tracerCount[32];
extern char PM_FindTextureType( const char *name );
void V_PunchAxis( int axis, float punch );
void VectorAngles( const float *forward, float *angles );

extern "C"
{
	void EV_FireNull( event_args_t *args );
	void EV_EjectBrass( event_args_t *args );
	void EV_FireCrowbar( event_args_t *args );
	void EV_FireGlock1( event_args_t *args );
	void EV_FireMP5( event_args_t *args  );
	void EV_FirePython( event_args_t *args );
	void EV_FireGauss( event_args_t *args );
	void EV_SpinGauss( event_args_t *args );
	void EV_EgonFire( event_args_t *args );
	void EV_EgonStop( event_args_t *args );
	void EV_FireShotGunSingle( event_args_t *args );
	void EV_FireShotGunDouble( event_args_t *args );
	void EV_SnarkFire( event_args_t *args );
	void EV_TrainPitchAdjust( event_args_t *args );
	void EV_PlayEmptySound( event_args_t *args );
	void EV_Decals( event_args_t *args );
}

#define VECTOR_CONE_1DEGREES	Vector( 0.00873, 0.00873, 0.00873 )
#define VECTOR_CONE_2DEGREES	Vector( 0.01745, 0.01745, 0.01745 )
#define VECTOR_CONE_3DEGREES	Vector( 0.02618, 0.02618, 0.02618 )
#define VECTOR_CONE_4DEGREES	Vector( 0.03490, 0.03490, 0.03490 )
#define VECTOR_CONE_5DEGREES	Vector( 0.04362, 0.04362, 0.04362 )
#define VECTOR_CONE_6DEGREES	Vector( 0.05234, 0.05234, 0.05234 )
#define VECTOR_CONE_7DEGREES	Vector( 0.06105, 0.06105, 0.06105 )
#define VECTOR_CONE_8DEGREES	Vector( 0.06976, 0.06976, 0.06976 )
#define VECTOR_CONE_9DEGREES	Vector( 0.07846, 0.07846, 0.07846 )
#define VECTOR_CONE_10DEGREES	Vector( 0.08716, 0.08716, 0.08716 )
#define VECTOR_CONE_15DEGREES	Vector( 0.13053, 0.13053, 0.13053 )
#define VECTOR_CONE_20DEGREES	Vector( 0.17365, 0.17365, 0.17365 )

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play
float EV_HLDM_PlayTextureSound( int idx, TraceResult *ptr, float *vecSrc, float *vecEnd, int iBulletType )
{
	// hit the world, try to play sound based on texture material type
	char chTextureType = CHAR_TEX_CONCRETE;
	float fvol;
	float fvolbar;
	char *rgsz[4];
	int cnt;
	float fattn = ATTN_NORM;
	int entity = NULLENT_INDEX;
	char *pTextureName;
	char texname[64];
	char szbuffer[64];

	if( ptr->pHit ) entity = ptr->pHit->serialnumber;

	// check if playtexture sounds movevar is set
	if( gpGlobals->maxClients != 1 && gpMovevars->footsteps == 0 )
		return 0.0f;

	chTextureType = 0;

	// Player
	if ( entity >= 1 && entity <= gpGlobals->maxClients )
	{
		// hit body
		chTextureType = CHAR_TEX_FLESH;
	}
	else if ( entity == 0 )
	{
		// get texture from entity or world (world is ent(0))
		pTextureName = (char *)g_engfuncs.pfnTraceTexture( ptr->pHit, vecSrc, vecEnd );
		
		if( pTextureName )
		{
			strcpy( texname, pTextureName );
			pTextureName = texname;

			// strip leading '-0' or '+0~' or '{' or '!'
			if ( *pTextureName == '-' || *pTextureName == '+' )
			{
				pTextureName += 2;
			}

			if ( *pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ' )
			{
				pTextureName++;
			}
			
			// '}}'
			strcpy( szbuffer, pTextureName );
			szbuffer[CBTEXTURENAMEMAX - 1] = 0;
				
			// get texture type
			chTextureType = PM_FindTextureType( szbuffer );	
		}
	}
	
	switch ( chTextureType )
	{
	case CHAR_TEX_CONCRETE:
	default:	fvol = 0.9; fvolbar = 0.6;
		rgsz[0] = "player/pl_step1.wav";
		rgsz[1] = "player/pl_step2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_METAL: fvol = 0.9; fvolbar = 0.3;
		rgsz[0] = "player/pl_metal1.wav";
		rgsz[1] = "player/pl_metal2.wav";
		cnt = 2;
		break;
	case CHAR_TEX_DIRT:	fvol = 0.9; fvolbar = 0.1;
		rgsz[0] = "player/pl_dirt1.wav";
		rgsz[1] = "player/pl_dirt2.wav";
		rgsz[2] = "player/pl_dirt3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_VENT:	fvol = 0.5; fvolbar = 0.3;
		rgsz[0] = "player/pl_duct1.wav";
		rgsz[1] = "player/pl_duct1.wav";
		cnt = 2;
		break;
	case CHAR_TEX_GRATE: fvol = 0.9; fvolbar = 0.5;
		rgsz[0] = "player/pl_grate1.wav";
		rgsz[1] = "player/pl_grate4.wav";
		cnt = 2;
		break;
	case CHAR_TEX_TILE:	fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "player/pl_tile1.wav";
		rgsz[1] = "player/pl_tile3.wav";
		rgsz[2] = "player/pl_tile2.wav";
		rgsz[3] = "player/pl_tile4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_SLOSH: fvol = 0.9; fvolbar = 0.0;
		rgsz[0] = "player/pl_slosh1.wav";
		rgsz[1] = "player/pl_slosh3.wav";
		rgsz[2] = "player/pl_slosh2.wav";
		rgsz[3] = "player/pl_slosh4.wav";
		cnt = 4;
		break;
	case CHAR_TEX_WOOD: fvol = 0.9; fvolbar = 0.2;
		rgsz[0] = "debris/wood1.wav";
		rgsz[1] = "debris/wood2.wav";
		rgsz[2] = "debris/wood3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_GLASS:
	case CHAR_TEX_COMPUTER:
		fvol = 0.8; fvolbar = 0.2;
		rgsz[0] = "debris/glass1.wav";
		rgsz[1] = "debris/glass2.wav";
		rgsz[2] = "debris/glass3.wav";
		cnt = 3;
		break;
	case CHAR_TEX_FLESH:
		if ( iBulletType == BULLET_CROWBAR )
			return 0.0f; // crowbar already makes this sound
		fvol = 1.0;	fvolbar = 0.2;
		rgsz[0] = "weapons/bullet_hit1.wav";
		rgsz[1] = "weapons/bullet_hit2.wav";
		fattn = 1.0;
		cnt = 2;
		break;
	}

	// play material hit sound
	g_engfuncs.pEventAPI->EV_PlaySound( 0, ptr->vecEndPos, CHAN_STATIC, rgsz[RANDOM_LONG(0, cnt-1)], fvol, fattn, 0, 96 + RANDOM_LONG( 0, 0xf ));
	return fvolbar;
}

//======================
//	START DECALS
//======================
void EV_HLDM_FindHullIntersection( int idx, Vector vecSrc, TraceResult pTrace, float *mins, float *maxs )
{
	int		i, j, k;
	float		distance;
	float		*minmaxs[2] = {mins, maxs};
	TraceResult 	tmpTrace;
	Vector		vecHullEnd = pTrace.vecEndPos;
	Vector		vecEnd;

	distance = 1e6f;

	vecHullEnd = vecSrc + ((vecHullEnd - vecSrc ) * 2);

	UTIL_TraceLine( vecSrc, vecHullEnd, dont_ignore_monsters, GetEntityByIndex( idx ), &tmpTrace );
	
	if ( tmpTrace.flFraction < 1.0 )
	{
		pTrace = tmpTrace;
		return;
	}

	for ( i = 0; i < 2; i++ )
	{
		for ( j = 0; j < 2; j++ )
		{
			for ( k = 0; k < 2; k++ )
			{
				vecEnd.x = vecHullEnd.x + minmaxs[i][0];
				vecEnd.y = vecHullEnd.y + minmaxs[j][1];
				vecEnd.z = vecHullEnd.z + minmaxs[k][2];

				UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, GetEntityByIndex( idx ), &tmpTrace );                                        
				
				if ( tmpTrace.flFraction < 1.0 )
				{
					float thisDistance = (tmpTrace.vecEndPos - vecSrc).Length();
					if ( thisDistance < distance )
					{
						pTrace = tmpTrace;
						distance = thisDistance;
					}
				}
			}
		}
	}
}

char *EV_HLDM_DamageDecal( edict_t *pEnt )
{
	static char decalname[32];
	int idx;

	if ( pEnt->v.rendermode == kRenderTransTexture )
	{
		idx = RANDOM_LONG( 0, 2 );
		sprintf( decalname, "{break%i", idx + 1 );
	}
	else if ( pEnt->v.rendermode != kRenderNormal )
	{
		sprintf( decalname, "{bproof1" );
	}
	else
	{
		idx = RANDOM_LONG( 0, 4 );
		sprintf( decalname, "{shot%i", idx + 1 );
	}
	return decalname;
}

void EV_HLDM_CrowbarDecalTrace( TraceResult *pTrace, char *decalName )
{
	edict_t	*pEnt;

	pEnt = pTrace->pHit;

	// only decal brush models such as the world etc.
	if( decalName && decalName[0] && pEnt && ( pEnt->v.solid == SOLID_BSP || pEnt->v.movetype == MOVETYPE_PUSHSTEP ) )
	{
		g_pTempEnts->PlaceDecal( pTrace->vecEndPos, 2.0f, decalName );
	}
}

void EV_HLDM_GunshotDecalTrace( TraceResult *pTrace, char *decalName )
{
	int	iRand;
	edict_t	*pEnt;

	iRand = RANDOM_LONG( 0, 0x7FFF );
	if ( iRand < ( 0x7fff / 2 )) // not every bullet makes a sound.
	{
		switch( iRand % 5 )
		{
		case 0:
			g_engfuncs.pEventAPI->EV_PlaySound( NULL, pTrace->vecEndPos, 0, "weapons/ric1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 1:
			g_engfuncs.pEventAPI->EV_PlaySound( NULL, pTrace->vecEndPos, 0, "weapons/ric2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 2:	
			g_engfuncs.pEventAPI->EV_PlaySound( NULL, pTrace->vecEndPos, 0, "weapons/ric3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 3:
			g_engfuncs.pEventAPI->EV_PlaySound( NULL, pTrace->vecEndPos, 0, "weapons/ric4.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		case 4:
			g_engfuncs.pEventAPI->EV_PlaySound( NULL, pTrace->vecEndPos, 0, "weapons/ric5.wav", 1.0, ATTN_NORM, 0, PITCH_NORM );
			break;
		}
	}

	pEnt = pTrace->pHit;

	// Only decal brush models such as the world etc.
	if( decalName && decalName[0] && pEnt && ( pEnt->v.solid == SOLID_BSP || pEnt->v.movetype == MOVETYPE_PUSHSTEP ))
	{
		g_pTempEnts->PlaceDecal( pTrace->vecEndPos, 5.0f, decalName );
		g_pParticles->BulletParticles( pTrace->vecEndPos, Vector( 0, 0, -1 ));
	}
}

void EV_HLDM_DecalGunshot( TraceResult *pTrace, int iBulletType )
{
	edict_t	*pe;

	pe = pTrace->pHit;

	if ( pe && pe->v.solid == SOLID_BSP )
	{
		switch( iBulletType )
		{
		case BULLET_CROWBAR:
			EV_HLDM_CrowbarDecalTrace( pTrace, EV_HLDM_DamageDecal( pe ) );
			break;
		case BULLET_9MM:
		case BULLET_MP5:
		case BULLET_357:
		case BULLET_BUCKSHOT:
		default:	// smoke and decal
			EV_HLDM_GunshotDecalTrace( pTrace, EV_HLDM_DamageDecal( pe ));
			break;
		}
	}
}

int EV_HLDM_CheckTracer( int idx, Vector vecSrc, Vector end, Vector forward, Vector right, int iBulletType, int iTracerFreq, int *tracerCount )
{
	int tracer = 0;
	BOOL player = (idx >= 1 && idx <= gpGlobals->maxClients) ? true : false;

	if ( iTracerFreq != 0 && ( ( *tracerCount )++ % iTracerFreq ) == 0 )
	{
		Vector vecTracerSrc;

		if ( player )
		{
			Vector offset( 0, 0, -4 );

			// adjust tracer position for player
			vecTracerSrc = vecSrc + offset + right * 2 + forward * 16;
		}
		else
		{
			vecTracerSrc = vecSrc;
		}
		
		if ( iTracerFreq != 1 )	// guns that always trace also always decal
			tracer = 1;

		switch( iBulletType )
		{
		case BULLET_MP5:
		case BULLET_9MM:
		case BULLET_12MM:
		default:
			EV_CreateTracer( vecTracerSrc, end );
			break;
		}
	}
	return tracer;
}

//======================
//	END DECALS
//======================

//======================
//	Play Empty Sound
//======================

void EV_PlayEmptySound( event_args_s *args )
{
	// play custom empty sound
	// added pitch tuning for final spirit release
	if( args->iparam2 == 0 ) args->iparam2 = PITCH_NORM;
	edict_t	*pEnt = GetEntityByIndex( args->entindex );

	switch ( args->iparam1 )
	{
	case 0:
		g_engfuncs.pEventAPI->EV_PlaySound( pEnt, args->origin, CHAN_AUTO, "weapons/cock1.wav", 0.8f, ATTN_NORM, 0, args->iparam2 );
		break;
	case 1:
		g_engfuncs.pEventAPI->EV_PlaySound( pEnt, args->origin, CHAN_AUTO, "buttons/button10.wav", 0.8f, ATTN_NORM, 0, args->iparam2 );
		break;
	case 2:
		g_engfuncs.pEventAPI->EV_PlaySound( pEnt, args->origin, CHAN_AUTO, "buttons/button11.wav", 0.8f, ATTN_NORM, 0, args->iparam2 );
		break;
	case 3:
		g_engfuncs.pEventAPI->EV_PlaySound( pEnt, args->origin, CHAN_AUTO, "buttons/lightswitch2.wav", 0.8f, ATTN_NORM, 0, args->iparam2 );
		break;
	case 4:
		g_engfuncs.pEventAPI->EV_PlaySound( pEnt, args->origin, CHAN_AUTO, "weapons/shotgun_empty.wav", 0.8f, ATTN_NORM, 0, args->iparam2 );
		break;
	}
}


/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.
================
*/
void EV_HLDM_FireBullets( int idx, Vector forward, Vector right, Vector up, int cShots, Vector vecSrc, Vector vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int *tracerCount, float flSpreadX, float flSpreadY )
{
	TraceResult tr;
	int iShot;
	int tracer;
	
	for ( iShot = 1; iShot <= cShots; iShot++ )	
	{
		Vector vecDir, vecEnd;
			
		float x, y, z;
		// we randomize for the Shotgun.
		if ( iBulletType == BULLET_BUCKSHOT )
		{
			do {
				x = RANDOM_FLOAT( -0.5f, 0.5f ) + RANDOM_FLOAT( -0.5f, 0.5f );
				y = RANDOM_FLOAT( -0.5f, 0.5f ) + RANDOM_FLOAT( -0.5f, 0.5f );
				z = x * x + y * y;
			} while ( z > 1 );

			vecDir = vecDirShooting + x * flSpreadX * right + y * flSpreadY * up;
			vecEnd = vecSrc + flDistance * vecDir;
		}
		else
		{
			// But other guns already have their spread randomized in the synched spread.
			vecDir = vecDirShooting + flSpreadX * right + flSpreadY * up;
			vecEnd = vecSrc + flDistance * vecDir;
		}

		UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, GetEntityByIndex( idx ), &tr );

		tracer = EV_HLDM_CheckTracer( idx, vecSrc, tr.vecEndPos, forward, right, iBulletType, iTracerFreq, tracerCount );

		// do damage, paint decals
		if ( tr.flFraction != 1.0f )
		{
			switch( iBulletType )
			{
			default:
			case BULLET_9MM:		
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case BULLET_MP5:		
				if ( !tracer )
				{
					EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
					EV_HLDM_DecalGunshot( &tr, iBulletType );
				}
				break;
			case BULLET_BUCKSHOT:
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			case BULLET_357:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				EV_HLDM_DecalGunshot( &tr, iBulletType );
				break;
			}
		}
	}
}

//======================
//      ShellEject START
//======================
void EV_EjectBrass( event_args_t *args )
{
	int idx;
	int bullet;
	int firemode;
	Vector ShellVelocity;
	Vector ShellOrigin;
	Vector origin;
	Vector angles;
	Vector velocity;
	Vector up, right, forward;

	idx = args->entindex;
	origin = args->origin;
	angles = args->angles;
	velocity = args->velocity;
	bullet = args->iparam1;
	firemode = args->bparam1;

	AngleVectors( angles, forward, right, up );
	if( EV_IsLocal( idx )) EV_MuzzleFlash();

	model_t	shell = 0;

	switch( Bullet( bullet ))
	{
	case BULLET_9MM: shell = g_engfuncs.pEventAPI->EV_FindModelIndex( "models/gibs/shell9mm.mdl" ); break; 
	case BULLET_MP5: shell = g_engfuncs.pEventAPI->EV_FindModelIndex( "models/gibs/shellMp5.mdl" ); break;
	case BULLET_357: shell = g_engfuncs.pEventAPI->EV_FindModelIndex( "models/gibs/shell357.mdl" ); break;
	case BULLET_12MM: shell = g_engfuncs.pEventAPI->EV_FindModelIndex( "models/gibs/shell762.mdl" ); break;
	case BULLET_556: shell = g_engfuncs.pEventAPI->EV_FindModelIndex( "models/gibs/shell556.mdl" ); break;
	case BULLET_762: shell = g_engfuncs.pEventAPI->EV_FindModelIndex( "models/gibs/shell762.mdl" ); break;
	case BULLET_BUCKSHOT: shell = g_engfuncs.pEventAPI->EV_FindModelIndex ("models/gibs/shellBuck.mdl"); break;
	}

	if( bullet == BULLET_BUCKSHOT )
		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6 );
	else EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL ); 
}
//======================
//        ShellEject END
//======================

//======================
//	   CROWBAR START
//======================
int g_iSwing;

void EV_FireCrowbar( event_args_t *args )
{
	int	idx = args->entindex;
	Vector	origin = args->origin;
	Vector	angles = args->angles;
	Vector	velocity;
          Vector	up, right, forward;
	edict_t	*pHit;
	TraceResult tr;
	
	Vector vecSrc, vecEnd;
          
          AngleVectors( angles, forward, right, up );
	
	EV_GetGunPosition( args, vecSrc, origin );
	vecEnd = vecSrc + forward * 32;	
          
	// make trace 
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, GetEntityByIndex( idx ), &tr );

	if ( tr.flFraction >= 1.0 )
	{
		UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, GetEntityByIndex( idx ), &tr );

		if ( tr.flFraction < 1.0 )
		{
			// Calculate the point of intersection of the line (or hull) and the object we hit
			// This is and approximation of the "best" intersection
			pHit = tr.pHit;

			if ( !pHit || pHit->v.solid == SOLID_BSP )
				EV_HLDM_FindHullIntersection( idx, vecSrc, tr, VEC_DUCK_HULL_MIN, VEC_DUCK_HULL_MAX );
			vecEnd = tr.vecEndPos; // This is the point on the actual surface (the hull could have hit space)
		}
	}
	if ( tr.flFraction >= 1.0 )
	{
		if( args->iparam2 ) // fFirst
		{
			if ( EV_IsLocal( idx ) )
			{
				// miss
				switch(( g_iSwing++ ) % 3 )
				{
				case 0:
					g_engfuncs.pEventAPI->EV_WeaponAnim( CROWBAR_ATTACK1MISS, args->iparam1, 1.0f );
					break;
				case 1:
					g_engfuncs.pEventAPI->EV_WeaponAnim( CROWBAR_ATTACK2MISS, args->iparam1, 1.0f );
					break;
				case 2:
					g_engfuncs.pEventAPI->EV_WeaponAnim( CROWBAR_ATTACK3MISS, args->iparam1, 1.0f );
					break;
				}
			}

			// play wiff or swish sound
			g_engfuncs.pEventAPI->EV_PlaySound( GetEntityByIndex( idx ), origin, CHAN_WEAPON, "weapons/cbar_miss1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 0xF ));
		}
	}
	else
	{
		if ( EV_IsLocal( idx ) )
		{
			switch ((( g_iSwing++ ) % 2) + 1 )
			{
			case 0:
				g_engfuncs.pEventAPI->EV_WeaponAnim ( CROWBAR_ATTACK1HIT, args->iparam1, 1.0f );
				break;
			case 1:
				g_engfuncs.pEventAPI->EV_WeaponAnim ( CROWBAR_ATTACK2HIT, args->iparam1, 1.0f );
				break;
			case 2:
				g_engfuncs.pEventAPI->EV_WeaponAnim ( CROWBAR_ATTACK3HIT, args->iparam1, 1.0f );
				break;
			}
		}

		// play thwack, smack, or dong sound
		float flVol = 1.0;
		int fHitWorld = TRUE;
  		pHit = tr.pHit;
  		
		if ( pHit )
		{
			if ( args->bparam1 )
			{
				edict_t	*pSoundEnt = GetEntityByIndex( idx );

				// play thwack or smack sound
				switch( RANDOM_LONG(0,2) )
				{
				case 0:
					g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/cbar_hitbod1.wav", 1, ATTN_NORM, 0, PITCH_NORM );
					break;
				case 1:
					g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/cbar_hitbod2.wav", 1, ATTN_NORM, 0, PITCH_NORM );
					break;
				case 2:
					g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/cbar_hitbod3.wav", 1, ATTN_NORM, 0, PITCH_NORM );
					break;
				}

				fHitWorld = FALSE;
				args->bparam1 = FALSE; // hit monster
			}
		}

		if ( fHitWorld )
		{
			float fvolbar = EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecSrc + (vecEnd - vecSrc) * 2, BULLET_CROWBAR );
			edict_t	*pSoundEnt = GetEntityByIndex( idx );
		
			// also play crowbar strike
			switch( RANDOM_LONG(0,1) )
			{
			case 0:
				g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/cbar_hit1.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ));
				break;
			case 1:
				g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/cbar_hit2.wav", fvolbar, ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ));
				break;
			}

			// delay the decal a bit
			EV_HLDM_DecalGunshot( &tr, BULLET_CROWBAR );
		}
	}
}
//======================
//	   CROWBAR END 
//======================
//======================
//	    GLOCK START
//======================
void EV_FireGlock1( event_args_t *args )
{
	int idx;
	Vector origin = args->origin;
	Vector angles = args->angles;
	Vector velocity = args->velocity;
	int empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;
	
	idx = args->entindex;

	empty = args->bparam1;
	AngleVectors( angles, forward, right, up );

	shell = g_engfuncs.pEventAPI->EV_FindModelIndex ( "models/shell.mdl" ); // brass shell

	if ( EV_IsLocal( idx ) )
	{
		edict_t *view = GetViewModel();
		g_engfuncs.pEventAPI->EV_WeaponAnim( empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, args->iparam1, 1.0f );

		EV_MuzzleFlash();							

		if( args->bparam2 )
		{
			g_pTempEnts->MuzzleFlash( view, 2, 17 ); // silence mode
		}
		else
		{
			g_pTempEnts->MuzzleFlash( view, 1, 11 ); // normal flash
		}

		V_PunchAxis( 0, -2.0 );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL ); 

	edict_t	*pSoundEnt = GetEntityByIndex( idx );

	if ( args->bparam2 )
	{
		switch( RANDOM_LONG( 0, 1 ) )
		{
			case 0:
				g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/pl_gun1.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 2 ) );
				break;
			case 1:
				g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/pl_gun2.wav", RANDOM_FLOAT(0.92, 1.0), ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 2 ) );
				break;
		}
	}
	else g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/pl_gun3.wav", RANDOM_FLOAT( 0.92f, 1.0f ), ATTN_NORM, 0, 98 + RANDOM_LONG( 0, 3 ) );

	EV_GetGunPosition( args, vecSrc, origin );
	
	vecAiming = forward;

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_9MM, 0, 0, args->fparam1, args->fparam2 );
}
//======================
//	   GLOCK END
//======================

//======================
//	    MP5 START
//======================

void EV_FireMP5( event_args_t *args )
{
	int idx;
	Vector origin = args->origin;
	Vector angles = args->angles;
	Vector velocity = args->velocity;
	int body, animbase;
	
	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	body = args->iparam1;
	animbase = args->iparam2;	// because spirit and bshift dlls has different enums

	AngleVectors( angles, forward, right, up );

	shell = g_engfuncs.pEventAPI->EV_FindModelIndex ( "models/shell.mdl" );// brass shell
	
	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		g_engfuncs.pEventAPI->EV_WeaponAnim( animbase + RANDOM_LONG( 0, 2 ), body, 1.0f );

		V_PunchAxis( 0, RANDOM_FLOAT( -2, 2 ) );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHELL ); 

	edict_t	*pSoundEnt = GetEntityByIndex( idx );

	switch( RANDOM_LONG( 0, 1 ) )
	{
	case 0:
		g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/hks1.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 0xf ) );
		break;
	case 1:
		g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/hks2.wav", 1, ATTN_NORM, 0, 94 + RANDOM_LONG( 0, 0xf ) );
		break;
	}

	EV_GetGunPosition( args, vecSrc, origin );
	vecAiming = forward;

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
}
//======================
//		 MP5 END
//======================

//======================
//	  SHOTGUN START
//======================
void EV_FireShotGunDouble( event_args_t *args )
{
	int idx;
	Vector origin = args->origin;
	Vector angles = args->angles;
	Vector velocity = args->velocity;

	int j;
	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector vecSpread;
	Vector up, right, forward;
	float flSpread = 0.01f;

	idx = args->entindex;

	AngleVectors( angles, forward, right, up );

	shell = g_engfuncs.pEventAPI->EV_FindModelIndex ( "models/shotgunshell.mdl" );// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		g_engfuncs.pEventAPI->EV_WeaponAnim( args->iparam2, args->iparam1, 1.0f );
		V_PunchAxis( 0, -10.0 );
	}

	for ( j = 0; j < 2; j++ )
	{
		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[YAW], shell, TE_BOUNCE_SHOTSHELL ); 
	}

	g_engfuncs.pEventAPI->EV_PlaySound( GetEntityByIndex( idx ), origin, CHAN_WEAPON, "weapons/dbarrel1.wav", RANDOM_FLOAT(0.98, 1.0), ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	vecAiming = forward;

	if ( gpGlobals->maxClients > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 8, vecSrc, vecAiming, 2048, BULLET_BUCKSHOT, 0, &tracerCount[idx-1], 0.17365, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 12, vecSrc, vecAiming, 2048, BULLET_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.08716 );
	}
}

void EV_FireShotGunSingle( event_args_t *args )
{
	int idx;
	Vector origin = args->origin;
	Vector angles = args->angles;
	Vector velocity = args->velocity;
	
	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector vecSpread;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;

	AngleVectors( angles, forward, right, up );

	shell = g_engfuncs.pEventAPI->EV_FindModelIndex ( "models/shotgunshell.mdl" );// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		g_engfuncs.pEventAPI->EV_WeaponAnim( args->iparam2, args->iparam1, 1.0f );

		V_PunchAxis( 0, -5.0 );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 6 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL ); 

	g_engfuncs.pEventAPI->EV_PlaySound(  GetEntityByIndex( idx ), origin, CHAN_WEAPON, "weapons/sbarrel1.wav", RANDOM_FLOAT( 0.95f, 1.0f ), ATTN_NORM, 0, 93 + RANDOM_LONG( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	vecAiming = forward;

	if ( gpGlobals->maxClients > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 4, vecSrc, vecAiming, 2048, BULLET_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 6, vecSrc, vecAiming, 2048, BULLET_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.08716 );
	}
}
//======================
//	   SHOTGUN END
//======================

//======================
//	   PHYTON START 
//	     ( .357 )
//======================
void EV_FirePython( event_args_t *args )
{
	int idx;
	Vector origin = args->origin;
	Vector angles = args->angles;
	Vector velocity = args->velocity;

	Vector vecSrc, vecAiming;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;

	AngleVectors( angles, forward, right, up );

	if ( EV_IsLocal( idx ) )
	{
		// Python uses different body in multiplayer versus single player
		int multiplayer = gpGlobals->maxClients == 1 ? 0 : 1;

		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		g_engfuncs.pEventAPI->EV_WeaponAnim( args->iparam2, args->iparam1, 1.0f );

		V_PunchAxis( 0, -10.0 );
	}

	edict_t	*pSoundEnt = GetEntityByIndex( idx );

	switch( RANDOM_LONG( 0, 1 ))
	{
	case 0:
		g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/357_shot1.wav", RANDOM_FLOAT( 0.8f, 0.9f ), ATTN_NORM, 0, PITCH_NORM );
		break;
	case 1:
		g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, "weapons/357_shot2.wav", RANDOM_FLOAT( 0.8f, 0.9f ), ATTN_NORM, 0, PITCH_NORM );
		break;
	}

	EV_GetGunPosition( args, vecSrc, origin );
	vecAiming = forward;

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_357, 0, 0, args->fparam1, args->fparam2 );
}
//======================
//	    PHYTON END 
//	     ( .357 )
//======================

//======================
//	   GAUSS START 
//======================
#define GAUSS_PRIMARY_CHARGE_VOLUME	256	// how loud gauss is while charging
#define GAUSS_PRIMARY_FIRE_VOLUME	450	// how loud gauss is when discharged

void EV_SpinGauss( event_args_t *args )
{
	int pitch = args->iparam1;
	int iSoundState = args->bparam1 ? SND_CHANGE_PITCH : 0;

	g_engfuncs.pEventAPI->EV_PlaySound( GetEntityByIndex( args->entindex ), args->origin, CHAN_WEAPON, "ambience/pulsemachine.wav", 1.0, ATTN_NORM, iSoundState, pitch );

	if( EV_IsLocal( args->entindex ))
	{
		edict_t *view = GetViewModel();

		// change framerate from 1.0 to 2.5
		view->v.framerate = (float)pitch / PITCH_NORM;
	}
}

/*
==============================
EV_StopPreviousGauss

==============================
*/
void EV_StopPreviousGauss( int idx )
{
	// Make sure we don't have a gauss spin event in the queue for this guy
	g_engfuncs.pEventAPI->EV_KillEvents( idx, "evGaussSpin" );
	g_engfuncs.pEventAPI->EV_StopSound( idx, CHAN_WEAPON, "ambience/pulsemachine.wav" );
}

void EV_FireGauss( event_args_t *args )
{
	int idx;
	float flDamage = args->fparam1;
	Vector origin = args->origin;
	Vector angles = args->angles;
	Vector velocity = args->velocity;

	int m_fPrimaryFire = args->bparam1;
	int m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
	Vector vecSrc, vecDest;
  
	edict_t *pentIgnore;
	TraceResult tr, beam_tr;
	float flMaxFrac = 1.0;
	int nTotal = 0;                             
	int fHasPunched = 0;
	int fFirstBeam = 1;
	int nMaxHits = 10;
	edict_t *pEntity;
	int m_iBeam, m_iGlow, m_iBalls;
	Vector forward;

	idx = args->entindex;

	if ( args->bparam2 )
	{
		EV_StopPreviousGauss( idx );
		return;
	}

//	ALERT( at_console, "firing gauss with %f\n", flDamage );
	EV_GetGunPosition( args, vecSrc, origin );

	m_iBeam = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/smoke.spr" );
	m_iBalls = m_iGlow = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/hotglow.spr" );
	
	AngleVectors( angles, forward, NULL, NULL );
          
	vecDest = vecSrc + forward * 8192;
	
	if ( EV_IsLocal( idx ) )
	{
		V_PunchAxis( 0, -2.0 );
		g_engfuncs.pEventAPI->EV_WeaponAnim( GAUSS_FIRE2, args->iparam1, 1.0f );
	}

	g_engfuncs.pEventAPI->EV_PlaySound( GetEntityByIndex( idx ), origin, CHAN_WEAPON, "weapons/gauss2.wav", 0.5 + flDamage * (1.0 / 400.0), ATTN_NORM, 0, 85 + RANDOM_LONG( 0, 0x1f ) );

	while ( flDamage > 10 && nMaxHits > 0 )
	{
		nMaxHits--;

		UTIL_TraceLine( vecSrc, vecDest, dont_ignore_monsters, GetEntityByIndex( idx ), &tr );
                    
		if ( tr.fAllSolid )
			break;

		if ( fFirstBeam )
		{
			if ( EV_IsLocal( idx ) )
			{
				// Add muzzle flash to current weapon model
				EV_MuzzleFlash();
			}
			fFirstBeam = 0;

			g_pViewRenderBeams->CreateBeamEntPoint( idx | 0x1000, tr.vecEndPos, m_iBeam, 0.1,
			m_fPrimaryFire ? 1.0 : 2.5,0.0,m_fPrimaryFire ? 128.0 : flDamage, 0, 0, 0,
			m_fPrimaryFire ? 255 : 255, m_fPrimaryFire ? 128 : 255, m_fPrimaryFire ? 0 : 255 );
		}
		else
		{
			g_pViewRenderBeams->CreateBeamPoints( vecSrc, tr.vecEndPos, m_iBeam, 0.1,
			m_fPrimaryFire ? 1.0 : 2.5, 0.0f, m_fPrimaryFire ? 128.0 : flDamage, 0, 0, 0,
			m_fPrimaryFire ? 255 : 255, m_fPrimaryFire ? 128 : 255, m_fPrimaryFire ? 0 : 255 );
		}

		pEntity = tr.pHit;

		if ( pEntity == NULL )
			break;
                    
		if ( pEntity->v.solid == SOLID_BSP )
		{
			float n;

			pentIgnore = NULL;

			n = -DotProduct( tr.vecPlaneNormal, forward );

			if ( n < 0.5f ) // 60 degrees	
			{
				// ALERT( at_console, "reflect %f\n", n );
				// reflect
				Vector r;
			
				r = forward + ( tr.vecPlaneNormal * (2.0f * n ));

				flMaxFrac = flMaxFrac - tr.flFraction;
				
				forward = r;

				vecSrc = tr.vecEndPos + forward * 8.0f;
				vecDest = vecSrc + forward * 8192.0f;

				g_pTempEnts->TempSprite( tr.vecEndPos, g_vecZero, 0.2, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage * n / 255.0, flDamage * n * 0.5 * 0.1, FTENT_FADEOUT );
                                        
				Vector fwd;
				fwd = tr.vecEndPos + tr.vecPlaneNormal;

				g_pTempEnts->Sprite_Trail( TE_SPRITETRAIL, tr.vecEndPos, fwd, m_iBalls, 3, 0.1, RANDOM_FLOAT( 10, 20 ) / 100.0, 100, 255, 100 );
                                        
				// lose energy
				if ( n == 0 )
				{
					n = 0.1f;
				}
				
				flDamage = flDamage * (1 - n);

			}
			else
			{
				// tunnel
				EV_HLDM_DecalGunshot( &tr, BULLET_12MM );

				g_pTempEnts->TempSprite( tr.vecEndPos, g_vecZero, 1.0, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage / 255.0, 6.0, FTENT_FADEOUT );

				// limit it to one hole punch
				if( fHasPunched ) break;
				fHasPunched = 1;
				
				// try punching through wall if secondary attack (primary is incapable of breaking through)
				if ( !m_fPrimaryFire )
				{
					Vector start;
					
					start = tr.vecEndPos + forward * 8.0f;
					
					UTIL_TraceLine( start, vecDest, dont_ignore_monsters, GetEntityByIndex( idx ), &beam_tr );

					if ( !beam_tr.fAllSolid )
					{
						Vector delta;
						float n;

						// trace backwards to find exit point

						UTIL_TraceLine( beam_tr.vecEndPos, tr.vecEndPos, dont_ignore_monsters, GetEntityByIndex( idx ), &beam_tr );

						delta = beam_tr.vecEndPos - tr.vecEndPos;
						
						n = delta.Length();

						if ( n < flDamage )
						{
							if ( n == 0 ) n = 1;
							flDamage -= n;

							// absorption balls
							{
								Vector fwd = tr.vecEndPos - forward;
								g_pTempEnts->Sprite_Trail( TE_SPRITETRAIL, tr.vecEndPos, fwd, m_iBalls, 3, 0.1, RANDOM_FLOAT( 10, 20 ) / 100.0, 100, 255, 100 );
							}

							EV_HLDM_DecalGunshot( &beam_tr, BULLET_12MM );
							
							g_pTempEnts->TempSprite( beam_tr.vecEndPos, g_vecZero, 0.1, m_iGlow, kRenderGlow, kRenderFxNoDissipation, flDamage / 255.0, 6.0, FTENT_FADEOUT );
			
							// balls
							{
								Vector fwd = beam_tr.vecEndPos - forward;
								g_pTempEnts->Sprite_Trail( TE_SPRITETRAIL, beam_tr.vecEndPos, fwd, m_iBalls, (int)(flDamage * 0.3), 0.1, RANDOM_FLOAT( 10, 20 ) / 100.0, 200, 255, 40 );
							}
							
							vecSrc = beam_tr.vecEndPos + forward;
						}
					}
					else
					{
						flDamage = 0;
					}
				}
				else
				{
					if ( m_fPrimaryFire )
					{
						// slug doesn't punch through ever with primary 
						// fire, so leave a little glowy bit and make some balls
						g_pTempEnts->TempSprite( tr.vecEndPos, g_vecZero, 0.2, m_iGlow, kRenderGlow, kRenderFxNoDissipation, 200.0 / 255.0, 0.3, FTENT_FADEOUT );
						
						// balls
						{
							Vector fwd;
							int num_balls = RANDOM_FLOAT( 10, 20 );
							fwd = tr.vecEndPos + tr.vecPlaneNormal;
							g_pTempEnts->Sprite_Trail( TE_SPRITETRAIL, tr.vecEndPos, fwd, m_iBalls, 8, 0.6, num_balls / 100.0, 100, 255, 200 );
						}
					}

					flDamage = 0;
				}
			}
		}
		else
		{
			vecSrc = tr.vecEndPos + forward;
		}
	}
}
//======================
//	   GAUSS END 
//======================

//======================
//	    EGON START 
//======================
enum EGON_FIRESTATE { FIRE_OFF = 0, FIRE_CHARGE };
enum EGON_FIREMODE { FIRE_NARROW = 0, FIRE_WIDE };

#define EGON_PULSE_INTERVAL		0.1
#define EGON_DISCHARGE_INTERVAL	0.1
#define EGON_PRIMARY_VOLUME		450
#define EGON_BEAM_SPRITE		"sprites/xbeam1.spr"
#define EGON_FLARE_SPRITE		"sprites/XSpark1.spr"
#define EGON_SOUND_OFF		"weapons/egon_off1.wav"
#define EGON_SOUND_RUN		"weapons/egon_run3.wav"
#define EGON_SOUND_STARTUP		"weapons/egon_windup2.wav"

Beam_t *m_pBeam = NULL;
Beam_t *m_pNoise = NULL;

TEMPENTITY *m_pEndFlare = NULL;

void EV_EgonFire( event_args_t *args )
{
	int idx, iFireState, iFireMode;
	Vector origin = args->origin;

	idx = args->entindex;
	iFireMode = args->iparam2;
	int iStartup = args->bparam1;
	iFireState = args->bparam2;
	int m_iFlare = g_engfuncs.pEventAPI->EV_FindModelIndex( EGON_FLARE_SPRITE );

	edict_t *pSoundEnt = GetEntityByIndex( idx );

	if ( iStartup )
	{
		if ( iFireMode == FIRE_WIDE )
			g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, EGON_SOUND_STARTUP, 0.98f, ATTN_NORM, 0, 125 );
		else g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_WEAPON, EGON_SOUND_STARTUP, 0.9f, ATTN_NORM, 0, 100 );
	}
	else
	{
		if ( iFireMode == FIRE_WIDE )
			g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_STATIC, EGON_SOUND_RUN, 0.98f, ATTN_NORM, 0, 125 );
		else g_engfuncs.pEventAPI->EV_PlaySound( pSoundEnt, origin, CHAN_STATIC, EGON_SOUND_RUN, 0.9f, ATTN_NORM, 0, 100 );
	}

	// Only play the weapon anims if I shot it.
	// if ( EV_IsLocal( idx ) ) g_engfuncs.pEventAPI->EV_WeaponAnim ( EGON_FIRECYCLE, args->iparam1, 1.0f );

	if ( iStartup == 1 && EV_IsLocal( idx ) && !m_pBeam && !m_pNoise && cl_lw->integer )
	{
		Vector vecSrc, vecEnd, origin, angles, forward;
		TraceResult tr;

		edict_t *pl = GetEntityByIndex( idx );

		if ( pl )
		{
			angles = gHUD.m_vecAngles;
			AngleVectors( angles, forward, NULL, NULL );
			EV_GetGunPosition( args, vecSrc, pl->v.origin );
		
			vecEnd = vecSrc + forward * 2048;
				
			UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, GetEntityByIndex( idx ), &tr );

			int iBeamModelIndex = g_engfuncs.pEventAPI->EV_FindModelIndex( EGON_BEAM_SPRITE );

			float r = 0.5f;
			float g = 0.5f;
			float b = 125.0f;
	
			if ( iFireMode == FIRE_WIDE )
			{
				m_pBeam = g_pViewRenderBeams->CreateBeamEntPoint(
					idx | 0x1000,	// end entity & attachment
					tr.vecEndPos,	// start pos (sic!)
					iBeamModelIndex,	// beamSprite
					99999,		// life
					4.0,		// width
					2.0,		// amplitude (noise)
					100,		// brightness
					55,		// scrollSpeed
					0,		// startframe
					0,		// framerate
					r, g, b );	// color

				if ( m_pBeam ) m_pBeam->SetFlags( FBEAM_SINENOISE );

				m_pNoise = g_pViewRenderBeams->CreateBeamEntPoint(
					idx | 0x1000,	// end entity & attachment
					tr.vecEndPos,	// start pos (sic!)
					iBeamModelIndex,	// beamSprite
					99999,		// life
					5.5,		// width
					0.08,		// amplitude (noise)
					100,		// brightness
					2.5,		// scrollSpeed
					0,		// startframe
					0,		// framerate
					50, 50, 255 );	// color
			}
			else
			{
				m_pBeam = g_pViewRenderBeams->CreateBeamEntPoint(
					idx | 0x1000,	// end entity & attachment
					tr.vecEndPos,	// start pos (sic!)
					iBeamModelIndex,	// beamSprite
					99999,		// life
					1.5,		// width
					0.5,		// amplitude (noise)
					100,		// brightness
					5.5,		// scrollSpeed
					0,		// startframe
					0,		// framerate
					r, g, b );	// color

				if ( m_pBeam ) m_pBeam->SetFlags( FBEAM_SINENOISE );

				m_pNoise = g_pViewRenderBeams->CreateBeamEntPoint(
					idx | 0x1000,	// end entity & attachment
					tr.vecEndPos,	// start pos (sic!)
					iBeamModelIndex,	// beamSprite
					99999,		// life
					5.5,		// width
					0.2,		// amplitude (noise)
					100,		// brightness
					2.5,		// scrollSpeed
					0,		// startframe
					0,		// framerate
					80, 120, 255 );	// color
			}

                     	m_pEndFlare = g_pTempEnts->TempSprite( tr.vecEndPos, g_vecZero, 1.0, m_iFlare, kRenderGlow, kRenderFxNoDissipation, 1.0, 9999, FTENT_SPRCYCLE );
		}
	}
}

void EV_EgonStop( event_args_t *args )
{
	int idx;
	Vector origin = args->origin;

	idx = args->entindex;

	g_engfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, EGON_SOUND_RUN );
	
	if ( args->bparam2 )
		 g_engfuncs.pEventAPI->EV_PlaySound( GetEntityByIndex( idx ), origin, CHAN_WEAPON, EGON_SOUND_OFF, 0.98, ATTN_NORM, 0, 100 );

	if ( EV_IsLocal( idx ) ) 
	{
		int iFireMode = args->iparam2;
	
		if ( m_pBeam )
		{
			m_pBeam->die = 0.0f;
			m_pBeam = NULL;
		}

		if ( m_pNoise )
		{
			m_pNoise->die = 0.0f;
			m_pNoise = NULL;
		}

		if ( m_pEndFlare )
		{
			if ( iFireMode == FIRE_WIDE )
			{
				// m_pSprite->Expand( 10, 500 );
				m_pEndFlare->flags = FTENT_SCALE|FTENT_FADEOUT;
				m_pEndFlare->fadeSpeed = 3.0f;
			}
			else
			{
				// UTIL_Remove( m_pSprite );
				m_pEndFlare->die = 0.0f;
			}
			m_pEndFlare = NULL;
		}

		g_engfuncs.pEventAPI->EV_WeaponAnim( EGON_FIRESTOP, args->iparam1, 1.0f );
	}
}

void EV_UpdateBeams ( void )
{
	if ( !m_pBeam && !m_pNoise ) return;
	
	Vector forward, vecSrc, vecEnd, origin, angles;
	TraceResult tr;
	float timedist;

	edict_t *pthisplayer = GetLocalPlayer();
	int idx = pthisplayer->serialnumber;
	
	// Get our exact viewangles from engine
	GetViewAngles( angles );

	origin = previousorigin;

	AngleVectors( angles, forward, NULL, NULL );

	vecSrc = origin;
	vecEnd = vecSrc + forward * 2048;
	
	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, GetEntityByIndex( idx ), &tr );

	// HACKHACK: extract firemode from beamWidth
	int iFireMode = ( m_pBeam->width == 4.0 ) ? FIRE_WIDE : FIRE_NARROW;

	// calc timedelta for beam effects
	switch( iFireMode )
	{
	case FIRE_NARROW:
		if ( m_pBeam->m_flDmgTime < gpGlobals->time )
		{
			m_pBeam->m_flDmgTime = gpGlobals->time + EGON_PULSE_INTERVAL;
		}
		timedist = ( m_pBeam->m_flDmgTime - gpGlobals->time ) / EGON_DISCHARGE_INTERVAL;
		break;
	case FIRE_WIDE:
		if ( m_pBeam->m_flDmgTime < gpGlobals->time )
		{
			m_pBeam->m_flDmgTime = gpGlobals->time + EGON_PULSE_INTERVAL;
		}
		timedist = ( m_pBeam->m_flDmgTime - gpGlobals->time ) / EGON_DISCHARGE_INTERVAL;
		break;
	}

	// clamp and inverse
	timedist = bound( 0.0f, timedist, 1.0f );
	timedist = 1.0f - timedist;

	m_pBeam->SetEndPos( tr.vecEndPos );
	m_pBeam->SetBrightness( 255 - ( timedist * 180 ));
	m_pBeam->SetWidth( 40 - ( timedist * 20 ));
	m_pBeam->die = gpGlobals->time + 0.1f; // We keep it alive just a little bit forward in the future, just in case.

	if ( iFireMode == FIRE_WIDE )
		m_pBeam->SetColor( 30 + (25 * timedist), 30  + (30 * timedist), 64 + 80 * fabs( sin( gpGlobals->time * 10 )));
	else
		m_pBeam->SetColor( 60 + (25 * timedist), 120 + (30 * timedist), 64 + 80 * fabs( sin( gpGlobals->time * 10 )));

	m_pNoise->SetEndPos( tr.vecEndPos );
	m_pNoise->die = gpGlobals->time + 0.1f; // We keep it alive just a little bit forward in the future, just in case.

	if( m_pEndFlare )
	{
		m_pEndFlare->origin = tr.vecEndPos;
		m_pEndFlare->die = gpGlobals->time + 0.1f;
	}
}
//======================
//	    EGON END 
//======================

//======================
//	   DECALS START
//======================
void EV_Decals( event_args_t *args )
{
	TraceResult tr;
	TraceResult *pTrace = &tr;

	// simulate trace result          
	memset( &tr, 0, sizeof( tr ));
	pTrace->vecEndPos = args->origin;
	pTrace->pHit = GetEntityByIndex( args->iparam1 ); 
	
	if( args->iparam2 == 0 )
	{
		// explode decals
		switch( RANDOM_LONG( 0, 2 ) )
		{
		case 0: EV_HLDM_CrowbarDecalTrace( pTrace, "{scorch1" ); break;
		case 1: EV_HLDM_CrowbarDecalTrace( pTrace, "{scorch2" ); break;
		case 2: EV_HLDM_CrowbarDecalTrace( pTrace, "{scorch3" ); break;
		}
	}

	if( args->iparam2 == 1 )
	{
		// red blood decals
		switch( RANDOM_LONG( 0, 7 ) )
		{
		case 0: EV_HLDM_CrowbarDecalTrace( pTrace, "{blood1" ); break;
		case 1: EV_HLDM_CrowbarDecalTrace( pTrace, "{blood2" ); break;
		case 2: EV_HLDM_CrowbarDecalTrace( pTrace, "{blood3" ); break;
		case 3: EV_HLDM_CrowbarDecalTrace( pTrace, "{blood4" ); break;
		case 4: EV_HLDM_CrowbarDecalTrace( pTrace, "{blood5" ); break;
		case 5: EV_HLDM_CrowbarDecalTrace( pTrace, "{blood6" ); break;
		case 6: EV_HLDM_CrowbarDecalTrace( pTrace, "{blood7" ); break;
		case 7: EV_HLDM_CrowbarDecalTrace( pTrace, "{blood8" ); break;
		}
	}

	if( args->iparam2 == 2 )
	{
		// yellow blood decals
		switch( RANDOM_LONG( 0, 5 ) )
		{
		case 0: EV_HLDM_CrowbarDecalTrace( pTrace, "{yblood1" ); break;
		case 1: EV_HLDM_CrowbarDecalTrace( pTrace, "{yblood2" ); break;
		case 2: EV_HLDM_CrowbarDecalTrace( pTrace, "{yblood3" ); break;
		case 3: EV_HLDM_CrowbarDecalTrace( pTrace, "{yblood4" ); break;
		case 4: EV_HLDM_CrowbarDecalTrace( pTrace, "{yblood5" ); break;
		case 5: EV_HLDM_CrowbarDecalTrace( pTrace, "{yblood6" ); break;
		}
	}

	if( args->iparam2 == 4 )
	{
		// bulsquid decals
		switch( RANDOM_LONG( 0, 1 ) )
		{
		case 0: EV_HLDM_CrowbarDecalTrace( pTrace, "{spit1" ); break;
		case 1: EV_HLDM_CrowbarDecalTrace( pTrace, "{spit2" ); break;
		}
	}

	if( args->iparam2 == 5 )
	{
		// garg flames
		switch( RANDOM_LONG( 0, 2 ) )
		{
		case 0: EV_HLDM_CrowbarDecalTrace( pTrace, "{smscorch1" ); break;
		case 1: EV_HLDM_CrowbarDecalTrace( pTrace, "{smscorch2" ); break;
		case 2: EV_HLDM_CrowbarDecalTrace( pTrace, "{smscorch3" ); break;
		}
	}

	if( args->iparam2 == 6 )
	{
		// monsters shoot
           	if ( pTrace->pHit && pTrace->pHit->v.solid == SOLID_BSP )
			EV_HLDM_GunshotDecalTrace( pTrace, EV_HLDM_DamageDecal( pTrace->pHit ));
	}
}
//======================
//	   DECALS END
//======================

//======================
//	   LASERSPOT START
//======================
TEMPENTITY *m_pLaserSpot = NULL;

void EV_UpdateLaserSpot( void )
{
	edict_t	*m_pPlayer = GetLocalPlayer();
	edict_t	*m_pWeapon = GetViewModel();

	if( !m_pPlayer ) return;

	if( m_pPlayer->v.effects & EF_LASERSPOT && !m_pLaserSpot && cl_lw->integer )
	{
		// create laserspot
		int m_iSpotModel = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/laserdot.spr" );
	
		m_pLaserSpot = g_pTempEnts->TempSprite( g_vecZero, g_vecZero, 1.0, m_iSpotModel, kRenderGlow, kRenderFxNoDissipation, 1.0, 9999, FTENT_SPRCYCLE );
		if( !m_pLaserSpot ) return;

		m_pLaserSpot->renderColor = Vector( 200, 12, 12 );
//		ALERT( at_console, "CLaserSpot::Create()\n" );
	}
	else if( !( m_pPlayer->v.effects & EF_LASERSPOT ) && m_pLaserSpot )
	{
		// destroy laserspot
//		ALERT( at_console, "CLaserSpot::Killed()\n" );
		m_pLaserSpot->die = 0.0f;
		m_pLaserSpot = NULL;
		return;
	}
	else if( !m_pLaserSpot )
	{
		// inactive
		return;		
	}

	ASSERT( m_pLaserSpot != NULL );

	Vector forward, vecSrc, vecEnd, origin, angles;
	TraceResult tr;

#if 1
	GetViewAngles( angles );	// viewmodel doesn't have attachment
	origin = m_pWeapon->v.origin;
#else
	// TEST: give viewmodel first attachment
	GET_ATTACHMENT( m_pWeapon, 1, origin, angles );
#endif
	AngleVectors( angles, forward, NULL, NULL );

	vecSrc = origin;
	vecEnd = vecSrc + forward * 8192;

	UTIL_TraceLine( vecSrc, vecEnd, dont_ignore_monsters, m_pPlayer, &tr );

	// update laserspot endpos
	m_pLaserSpot->origin = tr.vecEndPos;
	m_pLaserSpot->die = gpGlobals->time + 0.1f;
}

//======================
//	   LASERSPOT END
//======================

//======================
//           EFX END
//======================
//======================
//	   SQUEAK START
//======================
void EV_SnarkFire( event_args_t *args )
{
	int idx;
	Vector vecSrc, angles, view_ofs, forward;
	TraceResult tr;

	idx = args->entindex;
	vecSrc = args->origin;
	angles = args->angles;

	AngleVectors ( angles, forward, NULL, NULL );
		
	if ( !EV_IsLocal ( idx ) )
		return;
	
	if ( args->ducking )
		vecSrc = vecSrc - ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
	
	// store off the old count
	UTIL_TraceLine( vecSrc + forward * 20, vecSrc + forward * 64, dont_ignore_monsters, GetEntityByIndex( idx ), &tr );

	// find space to drop the thing.
	if ( tr.fAllSolid == 0 && tr.fStartSolid == 0 && tr.flFraction > 0.25f )
		 g_engfuncs.pEventAPI->EV_WeaponAnim ( SQUEAK_THROW, args->iparam1, 1.0f );
}
//======================
//	   SQUEAK END
//======================

//======================
//	   NULL START
//======================
void EV_FireNull( event_args_t *args )
{
	ALERT( at_console, "Called null event!\n" );
}
//======================
//	   NULL END
//======================
void EV_TrainPitchAdjust( event_args_t *args )
{
	int idx;
	Vector origin = args->origin;
	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	int stop;
	
	char sz[256];

	idx = args->entindex;
	
	us_params = (unsigned short)args->iparam1;
	stop = args->bparam1;

	m_flVolume = (float)(us_params & 0x003f) / 40.0;
	noise = (int)(((us_params) >> 12 ) & 0x0007);
	pitch = (int)( 10.0 * (float)( ( us_params >> 6 ) & 0x003f ) );

	switch ( noise )
	{
	case 1: strcpy( sz, "plats/ttrain1.wav"); break;
	case 2: strcpy( sz, "plats/ttrain2.wav"); break;
	case 3: strcpy( sz, "plats/ttrain3.wav"); break; 
	case 4: strcpy( sz, "plats/ttrain4.wav"); break;
	case 5: strcpy( sz, "plats/ttrain6.wav"); break;
	case 6: strcpy( sz, "plats/ttrain7.wav"); break;
	default:
		// no sound
		strcpy( sz, "" );
		return;
	}

	if ( stop )
	{
		g_engfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, sz );
	}
	else
	{
		g_engfuncs.pEventAPI->EV_PlaySound( GetEntityByIndex( idx ), origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch );
	}
}