//=======================================================================
//			Copyright XashXT Group 2008 ©
//		         r_beams.h - beam rendering code
//=======================================================================

#ifndef R_BEAMS_H
#define R_BEAMS_H

#include "beam_def.h"
#include "te_message.h"

#define NOISE_DIVISIONS		128
#define MAX_BEAM_ENTS		2	// start & end entity
#define MAX_BEAMS			128	// Max simultaneous beams
#define MAX_BEAMTRAILS		2048	// default max # of particles at one time
	
struct BeamTrail_t
{
	// NOTE:  Don't add user defined fields except after these four fields.
	BeamTrail_t	*next;
	float		die;
	Vector		org;
	Vector		vel;
};

struct BeamInfo_t
{
	int		m_nType;

	// entities
	edict_t		*m_pStartEnt;
	int		m_nStartAttachment;
	edict_t		*m_pEndEnt;
	int		m_nEndAttachment;

	// points
	Vector		m_vecStart;
	Vector		m_vecEnd;

	int		m_nModelIndex;
	const char	*m_pszModelName;

	float		m_flLife;
	float		m_flWidth;
	float		m_flEndWidth;
	float		m_flFadeLength;
	float		m_flAmplitude;

	float		m_flBrightness;
	float		m_flSpeed;
	
	int		m_nStartFrame;
	float		m_flFrameRate;

	float		m_flRed;
	float		m_flGreen;
	float		m_flBlue;

	int		m_nSegments;
	int		m_nFlags;

	// rings
	Vector		m_vecCenter;
	float		m_flStartRadius;
	float		m_flEndRadius;

	BeamInfo_t() 
	{ 
		m_nType = TE_BEAMPOINTS;
		m_nSegments = -1;
		m_pszModelName = NULL;
		m_nModelIndex = -1;
		m_nFlags = 0;
	}
};

//-----------------------------------------------------------------------------
// Purpose: Beams fill out this data structure
// This is also used for rendering
//-----------------------------------------------------------------------------

class Beam_t
{
public:
	Beam_t();

	// resets the beam state
	void		Reset();

	// Method to computing the bounding box
	void		ComputeBounds();

	const Vector&	GetRenderOrigin( void );

	// bounding box...
	Vector		m_Mins;
	Vector		m_Maxs;

	// data is below..

	// next beam in list
	Beam_t		*next;

	// type of beam
	int		type;
	int		flags;

	// control points for the beam
	int		numAttachments;
	Vector		attachment[MAX_BEAM_ENTS+1]; // last attachment used for DrawRing center
	Vector		delta;

	// 0 .. 1 over lifetime of beam
	float		t;		
	float		freq;

	// time when beam should die
	float		die;
	float		width;
	float		endWidth;
	float		fadeLength;
	float		amplitude;
	float		life;

	// color
	float		r, g, b;
	float		brightness;

	// speed
	float		speed;

	// animation
	float		frameRate;
	float		frame;
	int		segments;

	// attachment entities for the beam
	edict_t		*entity[MAX_BEAM_ENTS];
	int		attachmentIndex[MAX_BEAM_ENTS];

	// model info
	int		modelIndex;
	int		frameCount;

	float		rgNoise[NOISE_DIVISIONS+1];

	// Popcorn trail for beam follows to use
	BeamTrail_t	*trail;

	// for TE_BEAMRINGPOINT
	float		start_radius;
	float		end_radius;

	// for FBEAM_ONLYNOISEONCE
	bool		m_bCalculatedNoise;
};

// ---------------------------------------------------------------- //
// CBeamSegDraw is a simple interface to beam rendering.
// ---------------------------------------------------------------- //
class CBeamSeg
{
public:
	Vector		m_vPos;
	Vector		m_vColor;
	float		m_flTexCoord;	// Y texture coordinate
	float		m_flWidth;
	float		m_flAlpha;
};

class CBeamSegDraw
{
public:
	// pass null for pMaterial if you have already set the material you want.
	void		Start( int nSegs, HSPRITE m_hSprite = 0, kRenderMode_t nRenderMode = kRenderTransAdd );
	void		NextSeg( CBeamSeg *pSeg );
	void		End();
private:

	void		SpecifySeg( const Vector &vNextPos );
	void		ComputeNormal( const Vector &vStartPos, const Vector &vNextPos, Vector *pNormal );


private:
	Vector		m_vNormalLast;

	int		m_nTotalSegs;
	int		m_nSegsDrawn;
	
	CBeamSeg		m_Seg;
};

//-----------------------------------------------------------------------------
// Purpose: Implements beam rendering apis
//-----------------------------------------------------------------------------
class CViewRenderBeams
{
public:
			CViewRenderBeams( void );
	virtual		~CViewRenderBeams( void );

public:
	void		ClearBeams( void );

	// Updates the state of the temp ent beams
	void		UpdateTempEntBeams();

	void		DrawBeam( edict_t *pbeam );
	void		DrawBeam( Beam_t *pbeam );

	void		KillDeadBeams( edict_t *pDeadEntity );

	Beam_t		*CreateBeamEnts( BeamInfo_t &beamInfo );
	Beam_t		*CreateBeamEntPoint( BeamInfo_t &beamInfo );
	Beam_t		*CreateBeamPoints( BeamInfo_t &beamInfo );
	Beam_t		*CreateBeamRing( BeamInfo_t &beamInfo );
	Beam_t		*CreateBeamRingPoint( BeamInfo_t &beamInfo );
	Beam_t		*CreateBeamCirclePoints( BeamInfo_t &beamInfo );
	Beam_t		*CreateBeamFollow( BeamInfo_t &beamInfo );

	Beam_t		*CreateBeamEnts( int startEnt, int endEnt, int modelIndex, float life, float width,
			float endWidth, float fadeLength, float amplitude, float brightness, float speed,
			int startFrame, float framerate, float r, float g, float b, int type = -1 );
	Beam_t		*CreateBeamEnts( int startEnt, int endEnt, int modelIndex, float life, float width,
			float amplitude, float brightness, float speed, int startFrame, float framerate,
			float r, float g, float b );
	Beam_t		*CreateBeamEntPoint( int nStartEntity, const Vector *pStart, int nEndEntity,
			const Vector* pEnd, int modelIndex, float life, float width, float endWidth,
			float fadeLength, float amplitude, float brightness, float speed, int startFrame, 
			float framerate, float r, float g, float b );
	Beam_t		*CreateBeamEntPoint( int startEnt, Vector end, int modelIndex, float life, float width,
			float amplitude, float brightness, float speed, int startFrame, float framerate,
			float r, float g, float b );
	Beam_t		*CreateBeamPoints( Vector& start, Vector& end, int modelIndex, float life, float width,
			float endWidth, float fadeLength, float amplitude, float brightness, float speed,
			int startFrame, float framerate, float r, float g, float b );
	Beam_t		*CreateBeamPoints( Vector start, Vector end, int modelIndex, float life, float width,
			float amplitude, float brightness, float speed, int startFrame, float framerate,
			float r, float g, float b );
	void		CreateBeamRing( int startEnt, int endEnt, int modelIndex, float life, float width,
			float endWidth, float fadeLength, float amplitude, float brightness, float speed,
			int startFrame, float framerate, float r, float g, float b );
	void		CreateBeamRingPoint( const Vector& center, float start_radius, float end_radius,
			int modelIndex, float life, float width, float m_nEndWidth, float m_nFadeLength,
			float amplitude, float brightness, float speed, int startFrame, float framerate,
			float r, float g, float b );
	void		CreateBeamCirclePoints( int type, Vector& start, Vector& end, int modelIndex, float life,
			float width, float endWidth, float fadeLength, float amplitude, float brightness,
			float speed, int startFrame, float framerate, float r, float g, float b );
	void		CreateBeamFollow( int startEnt, int modelIndex, float life, float width, float endWidth,
			float fadeLength, float r, float g, float b, float brightness );

	void		FreeBeam( Beam_t *pBeam ) { BeamFree( pBeam ); }
	void		UpdateBeamInfo( Beam_t *pBeam, BeamInfo_t &beamInfo );
	void		AddServerBeam( edict_t *pEnvBeam );
	void		ClearServerBeams( void );

	void		UpdateBeams( int fTrans );	// main drawing func
private:
	void		FreeDeadTrails( BeamTrail_t **trail );
	void		UpdateBeam( Beam_t *pbeam, float frametime );
	void		DrawBeamFollow( int spriteIndex, Beam_t *pbeam, int frame, int rendermode,
			float frametime, const float* color );
	void		DrawLaser( Beam_t* pBeam, int frame, int rendermode, float* color, int spriteIndex );

	bool		RecomputeBeamEndpoints( Beam_t *pbeam );

	int		CullBeam( const Vector &start, const Vector &end, int pvsOnly );

	// Creation
	Beam_t		*CreateGenericBeam( BeamInfo_t &beamInfo );
	void		SetupBeam( Beam_t *pBeam, const BeamInfo_t &beamInfo );
	void		SetBeamAttributes( Beam_t *pBeam, const BeamInfo_t &beamInfo );
	
	// Memory Alloc/Free
	Beam_t*		BeamAlloc( void );
	void		BeamFree( Beam_t* pBeam );

// DATA
private:
	Beam_t		m_Beams[MAX_BEAMS];
	Beam_t		*m_pActiveBeams;
	Beam_t		*m_pFreeBeams;

	BeamTrail_t	*m_pBeamTrails;
	BeamTrail_t	*m_pActiveTrails;
	BeamTrail_t	*m_pFreeTrails;

	edict_t		*m_pServerBeams[MAX_BEAMS];	// to avoid check all of the ents
	int		m_nNumServerBeams;
};

void DrawSegs( int noise_divisions, float *prgNoise, int spriteIndex, float frame, int rendermode, const Vector& source,
		const Vector& delta, float startWidth, float endWidth, float scale, float freq, float speed,
		int segments, int flags, float* color, float fadeLength );
void DrawDisk( int noise_divisions, float *prgNoise, int spriteIndex, float frame, int rendermode, const Vector& source,
		const Vector& delta, float width, float scale, float freq, float speed, int segments, float* color);
void DrawCylinder( int noise_divisions, float *prgNoise, int spriteIndex, float frame, int rendermode,
		const Vector& source, const Vector&  delta, float width, float scale, float freq, float speed,
		int segments, float* color );
void DrawRing( int noise_divisions, float *prgNoise, void (*pfnNoise)( float *noise, int divs, float scale ),
		int spriteIndex, float frame, int rendermode, const Vector& source, const Vector& delta,
		float width, float amplitude, float freq, float speed, int segments, float* color );
void DrawBeamFollow( int spriteIndex, BeamTrail_t* pHead, int frame, int rendermode, Vector& delta, Vector& screen,
		Vector& screenLast, float die, const Vector& source, int flags, float width, float amplitude,
		float freq, float* color );


extern CViewRenderBeams	*g_pViewRenderBeams;

#endif//R_BEAMS_H