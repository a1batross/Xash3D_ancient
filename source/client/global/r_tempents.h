//=======================================================================
//			Copyright XashXT Group 2010 ©
//		      r_tempents.h - tempentities management
//=======================================================================

#ifndef R_TEMPENTS_H
#define R_TEMPENTS_H

//-----------------------------------------------------------------------------
// Purpose: implementation for temp entities
//-----------------------------------------------------------------------------
#include "te_shared.h"
#include "tmpent_def.h"
#include "effects_api.h"

#define MAX_TEMP_ENTITIES		500
#define TENT_WIND_ACCEL		50
#define MAX_MUZZLEFLASH		4
#define SHARD_VOLUME		12.0	// on shard ever n^3 units

class CTempEnts
{
public:
		CTempEnts( void );
	virtual	~CTempEnts( void );

	void	Update( void );
	void	Clear( void );

	void	TE_Prepare( TEMPENTITY *pTemp, model_t modelIndex );
	int	TE_Active( TEMPENTITY *pTemp );
	int	TE_Update( TEMPENTITY *pTemp );	// return false for instantly die

	void	BloodSprite( const Vector &org, int colorIndex, int modelIndex, int modelIndex2, float size );
	void	RicochetSprite( const Vector &pos, int modelIndex, float duration, float scale );
	void	MuzzleFlash( edict_t *pEnt, int iAttachment, int type );
	void	TempModel( const Vector &pos, const Vector &dir, const Vector &ang, float life, int modelIndex, int soundtype );
	void	BreakModel( const Vector &pos, const Vector &size, const Vector &dir, float random, float life, int count, int modelIndex, char flags );
	void	Bubbles( const Vector &mins, const Vector &maxs, float height, int modelIndex, int count, float speed );
	void	BubbleTrail( const Vector &start, const Vector &end, float height, int modelIndex, int count, float speed );
	void	Sprite_Explode( TEMPENTITY *pTemp, float scale, int flags );
	void	FizzEffect( edict_t *pent, int modelIndex, int density );
	TEMPENTITY *DefaultSprite( const Vector &pos, int spriteIndex, float framerate );
	void	Sprite_Smoke( TEMPENTITY *pTemp, float scale );
	TEMPENTITY *TempSprite( const Vector &pos, const Vector &dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags );
	void	AttachTentToPlayer( int client, int modelIndex, float zoffset, float life );
	void	KillAttachedTents( int client );
	void	Sprite_Spray( const Vector &pos, const Vector &dir, int modelIndex, int count, int speed, int iRand );
	void	Sprite_Trail( int type, const Vector &vecStart, const Vector &vecEnd, int modelIndex, int nCount, float flLife, float flSize, float flAmplitude, int nRenderamt, float flSpeed );
	void	RocketFlare( const Vector& pos );
	void	PlaySound( TEMPENTITY *pTemp, float damp );
	void	TracerEffect( const Vector &start, const Vector &end );
	void	WeaponFlash( edict_t *pEnt, int iAttachment );
	void	PlaceDecal( Vector pos, float scale, int decalIndex );
	void	PlaceDecal( Vector pos, float scale, const char *decalname );
	void	AllocDLight( Vector pos, float r, float g, float b, float radius, float time, int flags );
	void	AllocDLight( Vector pos, float radius, float time, int flags );
	void	RocketTrail( Vector start, Vector end, int type );
// Data
private:
	int		m_iTempEntFrame;	// used for keyed dlights only

	// Global temp entity pool
	TEMPENTITY	m_TempEnts[MAX_TEMP_ENTITIES];

	// Free and active temp entity lists
	TEMPENTITY	*m_pFreeTempEnts;
	TEMPENTITY	*m_pActiveTempEnts;

	// muzzle flash sprites
	model_t		m_iMuzzleFlash[MAX_MUZZLEFLASH];

	void		TempEntFree( TEMPENTITY *pTemp, TEMPENTITY *pPrev );
	bool		FreeLowPriorityTempEnt( void );
public:
	TEMPENTITY	*TempEntAllocNoModel( const Vector& org );
	TEMPENTITY	*TempEntAlloc( const Vector& org, model_t model );
	TEMPENTITY	*TempEntAllocHigh( const Vector& org, model_t model );
	TEMPENTITY	*TempEntAllocCustom( const Vector& org, int modelIndex, int high, ENTCALLBACK pfnCallback );

	// misc utility shaders
	HSPRITE		hSprGlowShell;	// glowshell shader
};

extern CTempEnts *g_pTempEnts;

#endif//R_TEMPENTS_H