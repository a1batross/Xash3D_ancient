//=======================================================================
//			Copyright XashXT Group 2010 ©
//		 r_beams.cpp - rendering single beams and rings 
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "triangle_api.h"
#include "effects_api.h"
#include "ref_params.h"
#include "hud.h"
#include "r_beams.h"

extern ref_params_t		*gpViewParams;
CViewRenderBeams		*g_pViewRenderBeams = NULL;

//-----------------------------------------------------------------------------
// Global methods
//-----------------------------------------------------------------------------

// freq2 += step * 0.1;
// Fractal noise generator, power of 2 wavelength
static void FracNoise( float *noise, int divs, float scale )
{
	int div2;
	
	div2 = divs >> 1;

	if( divs < 2 ) return;

	// noise is normalized to +/- scale
	noise[div2] = (noise[0] + noise[divs]) * 0.5f + scale * RANDOM_FLOAT( -1.0f, 1.0f );

	if( div2 > 1 )
	{
		FracNoise( &noise[div2], div2, scale * 0.5f );
		FracNoise( noise, div2, scale * 0.5f );
	}
}

static void SineNoise( float *noise, int divs )
{
	float freq = 0;
	float step = M_PI / (float)divs;

	for ( int i = 0; i < divs; i++ )
	{
		noise[i] = sin( freq );
		freq += step;
	}
}

static bool ComputeBeamEntPosition( edict_t *pEnt, int nAttachment, Vector& pt )
{
	if( !pEnt )
	{
		pt = g_vecZero;
		return false;
	}

	GET_ATTACHMENT( pEnt, nAttachment, pt, NULL );

	return true;
}

//-----------------------------------------------------------------------------
// Compute vectors perpendicular to the beam
//-----------------------------------------------------------------------------
static void ComputeBeamPerpendicular( const Vector &vecBeamDelta, Vector *pPerp )
{
	// Direction in worldspace of the center of the beam
	Vector vecBeamCenter = vecBeamDelta;
	vecBeamCenter.Normalize();

	*pPerp = CrossProduct( gpViewParams->vieworg, vecBeamCenter ).Normalize();
}

// ------------------------------------------------------------------------------------------ //
// CBeamSegDraw implementation.
// ------------------------------------------------------------------------------------------ //
void CBeamSegDraw::Start( int nSegs, HSPRITE m_hSprite, kRenderMode_t nRenderMode )
{
	m_nSegsDrawn = 0;
	m_nTotalSegs = nSegs;

	ASSERT( nSegs >= 2 );

	g_engfuncs.pTriAPI->Enable( TRI_SHADER );
	g_engfuncs.pTriAPI->RenderMode( nRenderMode );
	g_engfuncs.pTriAPI->Bind( m_hSprite, 0 );	// GetSpriteTexture already set frame

	g_engfuncs.pTriAPI->Begin( TRI_TRIANGLE_STRIP );				
}

inline void CBeamSegDraw::ComputeNormal( const Vector &vStartPos, const Vector &vNextPos, Vector *pNormal )
{
	// vTangentY = line vector for beam
	Vector vTangentY;
	vTangentY = vStartPos - vNextPos;
	
	// vDirToBeam = vector from viewer origin to beam
	Vector vDirToBeam;
	vDirToBeam = vStartPos - gpViewParams->vieworg;

	// Get a vector that is perpendicular to us and perpendicular to the beam.
	// This is used to fatten the beam.
	*pNormal = CrossProduct( vTangentY, vDirToBeam );
	VectorNormalizeFast( *pNormal );
}

inline void CBeamSegDraw::SpecifySeg( const Vector &vNormal )
{
	// SUCKY: Need to do a fair amount more work to get the tangent owing to the averaged normal
	Vector vDirToBeam, vTangentY;

	vDirToBeam = m_Seg.m_vPos - gpViewParams->vieworg;
	vTangentY = CrossProduct( vDirToBeam, vNormal );
	VectorNormalizeFast( vTangentY );

	// Build the endpoints.
	Vector vPoint1, vPoint2;

	vPoint1 = m_Seg.m_vPos + ( m_Seg.m_flWidth * 0.5f) * vNormal;
	vPoint2 = m_Seg.m_vPos + (-m_Seg.m_flWidth * 0.5f) * vNormal;

	// Specify the points.
	g_engfuncs.pTriAPI->Color4f( m_Seg.m_vColor[0], m_Seg.m_vColor[1], m_Seg.m_vColor[2], m_Seg.m_flAlpha );
	g_engfuncs.pTriAPI->TexCoord2f( 0.0f, m_Seg.m_flTexCoord );
	g_engfuncs.pTriAPI->Normal3fv( vNormal );
//	g_engfuncs.pTriAPI->Tangent3fv( vTangentY );
	g_engfuncs.pTriAPI->Vertex3fv( vPoint1 );
	
	g_engfuncs.pTriAPI->Color4f( m_Seg.m_vColor[0], m_Seg.m_vColor[1], m_Seg.m_vColor[2], m_Seg.m_flAlpha );
	g_engfuncs.pTriAPI->TexCoord2f( 1.0f, m_Seg.m_flTexCoord );
	g_engfuncs.pTriAPI->Normal3fv( vNormal );
//	g_engfuncs.pTriAPI->Tangent3fv( vTangentY );
	g_engfuncs.pTriAPI->Vertex3fv( vPoint2 );

}


void CBeamSegDraw::NextSeg( CBeamSeg *pSeg )
{
 	if ( m_nSegsDrawn > 0 )
	{
		// Get a vector that is perpendicular to us and perpendicular to the beam.
		// This is used to fatten the beam.
		Vector vNormal, vAveNormal;
		ComputeNormal( m_Seg.m_vPos, pSeg->m_vPos, &vNormal );

		if ( m_nSegsDrawn > 1 )
		{
			// Average this with the previous normal
			vAveNormal = ( vNormal + m_vNormalLast ) * 0.5f;
			VectorNormalizeFast( vAveNormal );
		}
		else
		{
			vAveNormal = vNormal;
		}

		m_vNormalLast = vNormal;
		SpecifySeg( vAveNormal );
	}

	m_Seg = *pSeg;
	++m_nSegsDrawn;

 	if( m_nSegsDrawn == m_nTotalSegs )
	{
		SpecifySeg( m_vNormalLast );
	}
}

void CBeamSegDraw::End()
{
	g_engfuncs.pTriAPI->End();
	g_engfuncs.pTriAPI->Disable( TRI_SHADER );
}

//-----------------------------------------------------------------------------
//
// Methods of Beam_t
//
//-----------------------------------------------------------------------------
Beam_t::Beam_t( void )
{
	Reset();
}

void Beam_t::Reset( void )
{
	m_Mins.Init();
	m_Maxs.Init();
	type = 0;
	flags = 0;
	trail = 0;
	m_bCalculatedNoise = false;
}


const Vector& Beam_t::GetRenderOrigin( void )
{
	if (( type == TE_BEAMRING ) || ( type == TE_BEAMRINGPOINT ))
	{
		// return the center of the ring
		static Vector org;
		org = attachment[0] + delta * 0.5f;
		return org;
	}
	return attachment[0];
}

void Beam_t::ComputeBounds( void )
{
	switch( type )
	{
	case TE_BEAMDISK:
	case TE_BEAMCYLINDER:
		{
			// FIXME: This isn't quite right for the cylinder

			// Here, delta[2] is the radius
			int radius = delta[2];
			m_Mins.Init( -radius, -radius, -radius );
			m_Maxs.Init( radius, radius, radius );
		}
		break;
	case TE_BEAMRING:
	case TE_BEAMRINGPOINT:
		{
			int radius = delta.Length() * 0.5f;
			m_Mins.Init( -radius, -radius, -radius );
			m_Maxs.Init( radius, radius, radius );
		}
		break;
	case TE_BEAMPOINTS:
	default:
		{
			// just use the delta
			for( int i = 0; i < 3; i++ )
			{
				if( delta[i] > 0.0f )
				{
					m_Mins[i] = 0.0f;
					m_Maxs[i] = delta[i];
				}
				else
				{
					m_Mins[i] = delta[i];
					m_Maxs[i] = 0.0f;
				}
			}
		}
		break;
	}

	// deal with beam follow
	Vector org = GetRenderOrigin();
	Vector followDelta;
	BeamTrail_t* pFollow = trail;
	while ( pFollow )
	{
		followDelta = pFollow->org - org;

		m_Mins = m_Mins.Min( followDelta );
		m_Maxs = m_Maxs.Max( followDelta );

		pFollow = pFollow->next;
	}
}

//-----------------------------------------------------------------------------
// Constructor, destructor: 
//-----------------------------------------------------------------------------

CViewRenderBeams::CViewRenderBeams( void ) : m_pBeamTrails(0)
{
	m_pBeamTrails = (BeamTrail_t *)new BeamTrail_t[MAX_BEAMTRAILS];

	ASSERT( m_pBeamTrails != NULL );

	// Clear them out
	ClearBeams();
}

CViewRenderBeams::~CViewRenderBeams( void )
{
	if ( m_pBeamTrails )
		delete[] m_pBeamTrails;
}

//-----------------------------------------------------------------------------
// Purpose: Clear out all beams
//-----------------------------------------------------------------------------
void CViewRenderBeams::ClearBeams( void )
{
	int i;

	m_pFreeBeams = &m_Beams[ 0 ];
	m_pActiveBeams = NULL;

	for ( i = 0; i < MAX_BEAMS; i++ )
	{
		m_Beams[i].Reset();
		m_Beams[i].next = &m_Beams[i+1];
	}

	m_Beams[MAX_BEAMS-1].next = NULL;

	// Also clear any particles used by beams
	m_pFreeTrails = &m_pBeamTrails[0];
	m_pActiveTrails = NULL;

	for ( i = 0; i < MAX_BEAMTRAILS; i++ )
		m_pBeamTrails[i].next = &m_pBeamTrails[i+1];
	m_pBeamTrails[MAX_BEAMTRAILS-1].next = NULL;

	ClearServerBeams();
}

//-----------------------------------------------------------------------------
// Purpose: Try and allocate a free beam
// Output : Beam_t
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::BeamAlloc( void )
{
	if ( !m_pFreeBeams )
		return NULL;

	Beam_t *pBeam   = m_pFreeBeams;
	m_pFreeBeams = pBeam->next;
	pBeam->next = m_pActiveBeams;
	m_pActiveBeams = pBeam;

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Free the beam.
//-----------------------------------------------------------------------------
void CViewRenderBeams::BeamFree( Beam_t* pBeam )
{
	// Free particles that have died off.
	FreeDeadTrails( &pBeam->trail );

	// Clear us out
	pBeam->Reset();

	// Now link into free list;
	pBeam->next = m_pFreeBeams;
	m_pFreeBeams = pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Iterates through active list and kills beams associated with deadEntity
// Input  : deadEntity - 
//-----------------------------------------------------------------------------
void CViewRenderBeams::KillDeadBeams( edict_t *pDeadEntity )
{
	Beam_t *pbeam;
	Beam_t *pnewlist;
	Beam_t *pnext;
	BeamTrail_t *pHead; // Build a new list to replace m_pActiveBeams.

	pbeam    = m_pActiveBeams;  // Old list.
	pnewlist = NULL;           // New list.
	
	while (pbeam)
	{
		pnext = pbeam->next;
		if ( pbeam->entity[0] != pDeadEntity )   // Link into new list.
		{
			pbeam->next = pnewlist;
			pnewlist = pbeam;

			pbeam = pnext;
			continue;
		}

		pbeam->flags &= ~(FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		if ( pbeam->type != TE_BEAMFOLLOW )
		{
			// Die Die Die!
			pbeam->die = gpGlobals->time - 0.1f;  

			// Kill off particles
			pHead = pbeam->trail;
			while (pHead)
			{
				pHead->die = gpGlobals->time - 0.1f;
				pHead = pHead->next;
			}

			// Free the beam
			BeamFree( pbeam );
		}
		else
		{
			// Stay active
			pbeam->next = pnewlist;
			pnewlist = pbeam;
		}
		pbeam = pnext;
	}

	// We now have a new list with the bogus stuff released.
	m_pActiveBeams = pnewlist;
}

//-----------------------------------------------------------------------------
// Purpose: Fill in values to beam structure.
//   Input: pBeam -
//          beamInfo -
//-----------------------------------------------------------------------------
void CViewRenderBeams::SetupBeam( Beam_t *pBeam, const BeamInfo_t &beamInfo )
{
	if( GetModelType( beamInfo.m_nModelIndex ) != mod_sprite )
		return;

	pBeam->type		= ( beamInfo.m_nType < 0 ) ? TE_BEAMPOINTS : beamInfo.m_nType;
	pBeam->modelIndex		= beamInfo.m_nModelIndex;
	pBeam->frame		= 0;
	pBeam->frameRate		= 0;
	pBeam->frameCount		= GetModelFrames( beamInfo.m_nModelIndex );
	pBeam->freq		= gpGlobals->time * beamInfo.m_flSpeed;
	pBeam->die		= gpGlobals->time + beamInfo.m_flLife;
	pBeam->width		= beamInfo.m_flWidth;
	pBeam->endWidth		= beamInfo.m_flEndWidth;
	pBeam->fadeLength		= beamInfo.m_flFadeLength;
	pBeam->amplitude		= beamInfo.m_flAmplitude;
	pBeam->brightness		= beamInfo.m_flBrightness;
	pBeam->speed		= beamInfo.m_flSpeed;
	pBeam->life		= beamInfo.m_flLife;
	pBeam->flags		= 0;

	pBeam->attachment[0] = beamInfo.m_vecStart;
	pBeam->attachment[1] = beamInfo.m_vecEnd;

//	ASSERT( beamInfo.m_vecStart != g_vecZero );
//	ASSERT( beamInfo.m_vecEnd != g_vecZero );

	pBeam->delta = beamInfo.m_vecEnd - beamInfo.m_vecStart;

	ASSERT( pBeam->delta.IsValid() );

	if ( beamInfo.m_nSegments == -1 )
	{
		if ( pBeam->amplitude >= 0.50 )
		{
			pBeam->segments = pBeam->delta.Length() * 0.25 + 3; // one per 4 pixels
		}
		else
		{
			pBeam->segments = pBeam->delta.Length() * 0.075 + 3; // one per 16 pixels
		}
	}
	else
	{
		pBeam->segments = beamInfo.m_nSegments;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set beam color and frame data.
//	 Input: pBeam -
//          beamInfo -
//-----------------------------------------------------------------------------
void CViewRenderBeams::SetBeamAttributes( Beam_t *pBeam, const BeamInfo_t &beamInfo )
{
	pBeam->frame = ( float )beamInfo.m_nStartFrame;
	pBeam->frameRate = beamInfo.m_flFrameRate;
	pBeam->flags |= beamInfo.m_nFlags;

	pBeam->r = beamInfo.m_flRed;
	pBeam->g = beamInfo.m_flGreen;
	pBeam->b = beamInfo.m_flBlue;
}

//-----------------------------------------------------------------------------
// Purpose: Cull beam by bbox
// Input  : *start - 
//			*end - 
//			pvsOnly - 
// Output : int
//-----------------------------------------------------------------------------

int CViewRenderBeams::CullBeam( const Vector &start, const Vector &end, int pvsOnly )
{
	Vector mins, maxs;

	for ( int i = 0; i < 3; i++ )
	{
		if ( start[i] < end[i] )
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}
		
		// Don't let it be zero sized
		if ( mins[i] == maxs[i] )
		{
			maxs[i] += 1;
		}
	}

	// check bbox
	if( g_engfuncs.pEfxAPI->CL_IsBoxVisible( mins, maxs ))
	{
		if ( pvsOnly || !g_engfuncs.pEfxAPI->R_CullBox( mins, maxs ) )
		{
			// Beam is visible
			return 1;	
		}
	}

	// Beam is not visible
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Allocate and setup a generic beam.
//   Input: beamInfo -
//  Output: Beam_t
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateGenericBeam( BeamInfo_t &beamInfo )
{
	Beam_t *pBeam = BeamAlloc();
	if ( !pBeam )
		return NULL;

	// In case we fail.
	pBeam->die = gpGlobals->time;

	// Need a valid model.
	if ( GetModelType( beamInfo.m_nModelIndex ) == mod_bad )
		return NULL;

	// Set it up
	SetupBeam( pBeam, beamInfo );

	return pBeam;	
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam between two ents
// Input  : startEnt - 
//			endEnt - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			BEAMENT_ENTITY(startEnt - 
// Output : Beam_t
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamEnts( int startEnt, int endEnt, int modelIndex, float life,
	float width, float endWidth, float fadeLength, float amplitude, float brightness,
	float speed, int startFrame, float framerate, float r, float g, float b, int type )
{
	BeamInfo_t beamInfo;

	beamInfo.m_nType = type;
	beamInfo.m_pStartEnt = GetEntityByIndex( BEAMENT_ENTITY( startEnt ));
	beamInfo.m_nStartAttachment = BEAMENT_ATTACHMENT( startEnt );
	beamInfo.m_pEndEnt = GetEntityByIndex( BEAMENT_ENTITY( endEnt ));
	beamInfo.m_nEndAttachment = BEAMENT_ATTACHMENT( endEnt );
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;

	return CreateBeamEnts( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam between two entities.
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamEnts( BeamInfo_t &beamInfo )
{
	// Don't start temporary beams out of the PVS
	if ( beamInfo.m_flLife != 0 && ( !beamInfo.m_pStartEnt || beamInfo.m_pStartEnt->v.modelindex == 0 || 
		   !beamInfo.m_pEndEnt || beamInfo.m_pEndEnt->v.modelindex == 0 ))
	{
		return NULL;
	}

	beamInfo.m_vecStart = g_vecZero;
	beamInfo.m_vecEnd = g_vecZero;

	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return NULL;

	pBeam->type = ( beamInfo.m_nType < 0 ) ? TE_BEAMPOINTS : beamInfo.m_nType;
	pBeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;

	pBeam->entity[0] = beamInfo.m_pStartEnt;
	pBeam->attachmentIndex[0] = beamInfo.m_nStartAttachment;
	pBeam->entity[1] = beamInfo.m_pEndEnt;
	pBeam->attachmentIndex[1] = beamInfo.m_nEndAttachment;

	// Attributes.
	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	UpdateBeam( pBeam, 0 );

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a beam between an entity and a point
// Input  : startEnt - 
//			*end - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			r - 
//			g - 
//			b - 
// Output : Beam_t
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamEntPoint( int nStartEntity, const Vector *pStart, int nEndEntity,
	const Vector* pEnd, int modelIndex, float life,  float width, float endWidth, float fadeLength,
	float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	BeamInfo_t beamInfo;

	if ( nStartEntity <= 0 )
	{
		beamInfo.m_vecStart = pStart ? *pStart : g_vecZero;
		beamInfo.m_pStartEnt = NULL;
	}
	else
	{
		beamInfo.m_pStartEnt = GetEntityByIndex( BEAMENT_ENTITY( nStartEntity ) );
		beamInfo.m_nStartAttachment = BEAMENT_ATTACHMENT( nStartEntity );

		// don't start beams out of the PVS
		if ( !beamInfo.m_pStartEnt )
			return NULL;
	}

	if ( nEndEntity <= 0 )
	{
		beamInfo.m_vecEnd = pEnd ? *pEnd : g_vecZero;
		beamInfo.m_pEndEnt = NULL;
	}
	else
	{
		beamInfo.m_pEndEnt = GetEntityByIndex( BEAMENT_ENTITY( nEndEntity ) );
		beamInfo.m_nEndAttachment = BEAMENT_ATTACHMENT( nEndEntity );

		// Don't start beams out of the PVS
		if ( !beamInfo.m_pEndEnt )
			return NULL;
	}

	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;

	return CreateBeamEntPoint( beamInfo );
}

Beam_t *CViewRenderBeams::CreateBeamEntPoint( int startEnt, Vector end, int modelIndex, float life, float width,
			float amplitude, float brightness, float speed, int startFrame, float framerate,
			float r, float g, float b )
{
	return CreateBeamEntPoint( startEnt, NULL, 0, &end, modelIndex, life, width, width, 0.0f, amplitude,
	brightness, speed, startFrame, framerate, r, g, b );
}

Beam_t *CViewRenderBeams::CreateBeamEnts( int startEnt, int endEnt, int modelIndex, float life, float width,
	float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	return CreateBeamEnts( startEnt, endEnt, modelIndex, life, width, width, 0.0f, amplitude, brightness, speed,
	startFrame, framerate, r, g, b );
}

Beam_t *CViewRenderBeams::CreateBeamPoints( Vector start, Vector end, int modelIndex, float life, float width,
	float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b )
{
	return CreateBeamPoints( start, end, modelIndex, life, width, width, 0.0f, amplitude, brightness, speed,
	startFrame, framerate, r, g, b );
}
	
//-----------------------------------------------------------------------------
// Purpose: Creates a beam between an entity and a point.
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamEntPoint( BeamInfo_t &beamInfo )
{
	if ( beamInfo.m_flLife != 0 )
	{
		if ( beamInfo.m_pStartEnt && beamInfo.m_pStartEnt->v.modelindex == NULL )
			return NULL;

		if ( beamInfo.m_pEndEnt && beamInfo.m_pEndEnt->v.modelindex == NULL )
			return NULL;
	}

	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return NULL;

	pBeam->type = TE_BEAMPOINTS;
	pBeam->flags = 0;

	if ( beamInfo.m_pStartEnt )
	{
		pBeam->flags |= FBEAM_STARTENTITY;
		pBeam->entity[0] = beamInfo.m_pStartEnt;
		pBeam->attachmentIndex[0] = beamInfo.m_nStartAttachment;
		beamInfo.m_vecStart = g_vecZero;
	}
	if ( beamInfo.m_pEndEnt )
	{
		pBeam->flags |= FBEAM_ENDENTITY;
		pBeam->entity[1] = beamInfo.m_pEndEnt;
		pBeam->attachmentIndex[1] = beamInfo.m_nEndAttachment;
		beamInfo.m_vecEnd = g_vecZero;
	}

	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	UpdateBeam( pBeam, 0 );

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a beam between two points
// Input  : *start - 
//			*end - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			r - 
//			g - 
//			b - 
// Output : Beam_t
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamPoints( Vector& start, Vector& end, int modelIndex, float life, float width, 
		float endWidth, float fadeLength,float amplitude, float brightness, float speed, int startFrame, 
		float framerate, float r, float g, float b )
{
	BeamInfo_t beamInfo;
	
	beamInfo.m_vecStart = start;
	beamInfo.m_vecEnd = end;
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;

	return CreateBeamPoints( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Creates a beam between two points.
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamPoints( BeamInfo_t &beamInfo )
{
	// don't start temporary beams out of the PVS
	if ( beamInfo.m_flLife != 0 && !CullBeam( beamInfo.m_vecStart, beamInfo.m_vecEnd, 1 ))
		return NULL;

	// Model index.
	if (( beamInfo.m_pszModelName ) && ( beamInfo.m_nModelIndex == -1 ) )
	{
		beamInfo.m_nModelIndex = g_engfuncs.pEventAPI->EV_FindModelIndex( beamInfo.m_pszModelName );
	}

	// Create the new beam.
	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return NULL;

	// Set beam initial state.
	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a circular beam between two points
// Input  : type - 
//			*start - 
//			*end - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			r - 
//			g - 
//			b - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamCirclePoints( int type, Vector& start, Vector& end, int modelIndex, float life,
		float width, float endWidth, float fadeLength,float amplitude, float brightness, float speed,
		int startFrame, float framerate, float r, float g, float b )
{
	BeamInfo_t beamInfo;
	
	beamInfo.m_nType = type;
	beamInfo.m_vecStart = start;
	beamInfo.m_vecEnd = end;
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;

	CreateBeamCirclePoints( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Creates a circular beam between two points.
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamCirclePoints( BeamInfo_t &beamInfo )
{
	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return NULL;

	pBeam->type = beamInfo.m_nType;

	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam which follows an entity
// Input  : startEnt - 
//			modelIndex - 
//			life - 
//			width - 
//			r - 
//			g - 
//			b - 
//			brightness - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamFollow( int startEnt, int modelIndex, float life, float width, float endWidth, 
		float fadeLength, float r, float g, float b, float brightness )
{
	BeamInfo_t beamInfo;

	beamInfo.m_pStartEnt = GetEntityByIndex( BEAMENT_ENTITY( startEnt ));
	beamInfo.m_nStartAttachment = BEAMENT_ATTACHMENT( startEnt );
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;
	beamInfo.m_flAmplitude = life;

	CreateBeamFollow( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam which follows an entity.
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamFollow( BeamInfo_t &beamInfo )
{
	beamInfo.m_vecStart = g_vecZero;
	beamInfo.m_vecEnd = g_vecZero;
	beamInfo.m_flSpeed = 1.0f;
	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return NULL;

	pBeam->type = TE_BEAMFOLLOW;
	pBeam->flags = FBEAM_STARTENTITY;
	pBeam->entity[0] = beamInfo.m_pStartEnt;
	pBeam->attachmentIndex[0] = beamInfo.m_nStartAttachment;

	beamInfo.m_flFrameRate = 1.0f;
	beamInfo.m_nStartFrame = 0;

	SetBeamAttributes( pBeam, beamInfo );

	UpdateBeam( pBeam, 0 );

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam ring between two entities
// Input  : startEnt - 
//			endEnt - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			startEnt - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamRingPoint( const Vector& center, float start_radius, float end_radius, int modelIndex,
		float life, float width, float endWidth, float fadeLength, float amplitude, float brightness,
		float speed, int startFrame, float framerate, float r, float g, float b )
{
	BeamInfo_t beamInfo;
	
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;
	beamInfo.m_vecCenter = center;
	beamInfo.m_flStartRadius = start_radius;
	beamInfo.m_flEndRadius = end_radius;

	CreateBeamRingPoint( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam ring between two entities
//   Input: beamInfo -
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamRingPoint( BeamInfo_t &beamInfo )
{
	// ??
	Vector endpos = beamInfo.m_vecCenter;

	beamInfo.m_vecStart = beamInfo.m_vecCenter;
	beamInfo.m_vecEnd = beamInfo.m_vecCenter;

	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return NULL;

	pBeam->type = TE_BEAMRINGPOINT;
	pBeam->start_radius = beamInfo.m_flStartRadius;
	pBeam->end_radius = beamInfo.m_flEndRadius;
	pBeam->attachment[2] = beamInfo.m_vecCenter;

	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam ring between two entities
// Input  : startEnt - 
//			endEnt - 
//			modelIndex - 
//			life - 
//			width - 
//			amplitude - 
//			brightness - 
//			speed - 
//			startFrame - 
//			framerate - 
//			startEnt - 
// Output : Beam_t
//-----------------------------------------------------------------------------
void CViewRenderBeams::CreateBeamRing( int startEnt, int endEnt, int modelIndex, float life, float width,
		float endWidth, float fadeLength,  float amplitude, float brightness, float speed,
		int startFrame, float framerate, float r, float g, float b )
{
	BeamInfo_t beamInfo;

	beamInfo.m_pStartEnt = GetEntityByIndex( BEAMENT_ENTITY( startEnt ));
	beamInfo.m_nStartAttachment = BEAMENT_ATTACHMENT( startEnt );
	beamInfo.m_pEndEnt = GetEntityByIndex( BEAMENT_ENTITY( endEnt ));
	beamInfo.m_nEndAttachment = BEAMENT_ATTACHMENT( endEnt );
	beamInfo.m_nModelIndex = modelIndex;
	beamInfo.m_flLife = life;
	beamInfo.m_flWidth = width;
	beamInfo.m_flEndWidth = endWidth;
	beamInfo.m_flFadeLength = fadeLength;
	beamInfo.m_flAmplitude = amplitude;
	beamInfo.m_flBrightness = brightness;
	beamInfo.m_flSpeed = speed;
	beamInfo.m_nStartFrame = startFrame;
	beamInfo.m_flFrameRate = framerate;
	beamInfo.m_flRed = r;
	beamInfo.m_flGreen = g;
	beamInfo.m_flBlue = b;

	CreateBeamRing( beamInfo );
}

//-----------------------------------------------------------------------------
// Purpose: Create a beam ring between two entities.
//   Input: beamInfo -
//-----------------------------------------------------------------------------
Beam_t *CViewRenderBeams::CreateBeamRing( BeamInfo_t &beamInfo )
{
	// Don't start temporary beams out of the PVS
	if ( beamInfo.m_flLife != 0 && 
		 ( !beamInfo.m_pStartEnt || beamInfo.m_pStartEnt->v.modelindex == NULL || 
		   !beamInfo.m_pEndEnt || beamInfo.m_pEndEnt->v.modelindex == NULL ))
	{
		return NULL;
	}

	beamInfo.m_vecStart = g_vecZero;
	beamInfo.m_vecEnd = g_vecZero;
	Beam_t *pBeam = CreateGenericBeam( beamInfo );
	if ( !pBeam )
		return NULL;

	pBeam->type = TE_BEAMRING;
	pBeam->flags = FBEAM_STARTENTITY | FBEAM_ENDENTITY;
	pBeam->entity[0] = beamInfo.m_pStartEnt;
	pBeam->attachmentIndex[0] = beamInfo.m_nStartAttachment;
	pBeam->entity[1] = beamInfo.m_pEndEnt;
	pBeam->attachmentIndex[1] = beamInfo.m_nEndAttachment;

	SetBeamAttributes( pBeam, beamInfo );
	if ( beamInfo.m_flLife == 0 )
	{
		pBeam->flags |= FBEAM_FOREVER;
	}

	UpdateBeam( pBeam, 0 );

	return pBeam;
}

//-----------------------------------------------------------------------------
// Purpose: Free dead trails associated with beam
// Input  : **ppparticles - 
//-----------------------------------------------------------------------------
void CViewRenderBeams::FreeDeadTrails( BeamTrail_t **trail )
{
	BeamTrail_t *kill;
	BeamTrail_t *p;

	// kill all the ones hanging direcly off the base pointer
	while ( 1 ) 
	{
		kill = *trail;
		if ( kill && kill->die < gpGlobals->time )
		{
			*trail = kill->next;
			kill->next = m_pFreeTrails;
			m_pFreeTrails = kill;
			continue;
		}
		break;
	}

	// kill off all the others
	for ( p = *trail; p; p = p->next )
	{
		while ( 1 )
		{
			kill = p->next;
			if ( kill && kill->die < gpGlobals->time )
			{
				p->next = kill->next;
				kill->next = m_pFreeTrails;
				m_pFreeTrails = kill;
				continue;
			}
			break;
		}
	}
}



//-----------------------------------------------------------------------------
// Updates beam state
//-----------------------------------------------------------------------------
void CViewRenderBeams::UpdateBeam( Beam_t *pbeam, float frametime )
{
	if ( GetModelType( pbeam->modelIndex ) == mod_bad )
	{
		pbeam->die = gpGlobals->time;
		return;
	}

	// If FBEAM_ONLYNOISEONCE is set, we don't want to move once we've first calculated noise
	if ( !(pbeam->flags & FBEAM_ONLYNOISEONCE ) )
	{
		pbeam->freq += frametime;
	}
	else
	{
		pbeam->freq += frametime * RANDOM_FLOAT( 1.0f, 2.0f );
	}

	// OPTIMIZE: Do this every frame?
	// UNDONE: Do this differentially somehow?
	// Generate fractal noise
	pbeam->rgNoise[0] = 0;
	pbeam->rgNoise[NOISE_DIVISIONS] = 0;

	if ( pbeam->amplitude != 0 )
	{
		if ( !(pbeam->flags & FBEAM_ONLYNOISEONCE ) || !pbeam->m_bCalculatedNoise )
		{
			if ( pbeam->flags & FBEAM_SINENOISE )
			{
				SineNoise( pbeam->rgNoise, NOISE_DIVISIONS );
			}
			else
			{
				FracNoise( pbeam->rgNoise, NOISE_DIVISIONS, 1.0f );
			}

			pbeam->m_bCalculatedNoise = true;
		}
	}

	// update end points
	if ( pbeam->flags & ( FBEAM_STARTENTITY|FBEAM_ENDENTITY ))
	{
		// Makes sure attachment[0] + attachment[1] are valid
		if (!RecomputeBeamEndpoints( pbeam ))
			return;

		// Compute segments from the new endpoints
		pbeam->delta = pbeam->attachment[1] - pbeam->attachment[0];
		if ( pbeam->amplitude >= 0.50f )
			pbeam->segments = pbeam->delta.Length( ) * 0.25f + 3.0f; // one per 4 pixels
		else
			pbeam->segments = pbeam->delta.Length( ) * 0.075f + 3.0f; // one per 16 pixels
	}

	float	dr;

	// Get position data for spline beam
	switch ( pbeam->type )
	{
	case TE_BEAMRINGPOINT:
		dr = pbeam->end_radius - pbeam->start_radius;
		
		if ( dr != 0.0f )
		{
			float frac = 1.0f;
			// Go some portion of the way there based on life
			float remaining = pbeam->die - gpGlobals->time;
		
			if ( remaining < pbeam->life && pbeam->life > 0.0f )
			{
				frac = remaining / pbeam->life;
			}
			frac = min( 1.0f, frac );
			frac = max( 0.0f, frac );

			frac = 1.0f - frac;

			// Start pos
			Vector endpos = pbeam->attachment[2];
			endpos.x += ( pbeam->start_radius + frac * dr ) / 2.0f;
			Vector startpos = pbeam->attachment[2];
			startpos.x -= ( pbeam->start_radius + frac * dr ) / 2.0f;

			pbeam->attachment[0] = startpos;
			pbeam->attachment[1] = endpos;

			pbeam->delta = pbeam->attachment[1] - pbeam->attachment[0];
	
			if ( pbeam->amplitude >= 0.50f )
				pbeam->segments = pbeam->delta.Length( ) * 0.25f + 3.0f; // one per 4 pixels
			else pbeam->segments = pbeam->delta.Length( ) * 0.075f + 3.0f; // one per 16 pixels

		}
		break;
	case TE_BEAMPOINTS:
		// UNDONE: Build culling volumes for other types of beams
		if ( !CullBeam( pbeam->attachment[0], pbeam->attachment[1], 0 ))
			return;
		break;
	}

	// update life cycle
	pbeam->t = pbeam->freq + (pbeam->die - gpGlobals->time);
	if ( pbeam->t != 0.0f ) pbeam->t = 1.0 - pbeam->freq / pbeam->t;
	
	// ------------------------------------------
	// check for zero fadeLength (means no fade)
	// ------------------------------------------
	if ( pbeam->fadeLength == 0.0f )
	{
		ASSERT( pbeam->delta.IsValid() );
		pbeam->fadeLength = pbeam->delta.Length();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update beams created by temp entity system
//-----------------------------------------------------------------------------

void CViewRenderBeams::UpdateTempEntBeams( void )
{
	if ( !m_pActiveBeams )
		return;

	// Get frame time
	float frametime = gpGlobals->frametime;

	// Draw temporary entity beams
	Beam_t* pPrev = 0;
	Beam_t* pNext;
	for ( Beam_t* pBeam = m_pActiveBeams; pBeam ; pBeam = pNext )
	{
		// Need to store the next one since we may delete this one
		pNext = pBeam->next;

		// Retire old beams
		if (!( pBeam->flags & FBEAM_FOREVER ) && pBeam->die <= gpGlobals->time )
		{
			// Reset links
			if ( pPrev )
			{
				pPrev->next = pNext;
			}
			else
			{
				m_pActiveBeams = pNext;
			}

			// Free the beam
			BeamFree( pBeam );

			pBeam = NULL;
			continue;
		}

		// Update beam state
		UpdateBeam( pBeam, frametime );

		// Compute bounds for the beam
		pBeam->ComputeBounds();

		pPrev = pBeam;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draw helper for beam follow beams
// Input  : *pbeam - 
//			frametime - 
//			*color - 
//-----------------------------------------------------------------------------
void CViewRenderBeams::DrawBeamFollow( int spriteIndex, Beam_t *pbeam, int frame, int rendermode, float frametime,
	const float* color )
{
	BeamTrail_t	*particles;
	BeamTrail_t	*pnew;
	float		div;
	Vector		delta;
	
	Vector		screenLast;
	Vector		screen;

	FreeDeadTrails( &pbeam->trail );

	particles = pbeam->trail;
	pnew = NULL;

	div = 0;
	if ( pbeam->flags & FBEAM_STARTENTITY )
	{
		if ( particles )
		{
			delta = particles->org - pbeam->attachment[0];
			div = delta.Length( );

			if ( div >= 32 && m_pFreeTrails )
			{
				pnew = m_pFreeTrails;
				m_pFreeTrails = pnew->next;
			}
		}
		else if ( m_pFreeTrails )
		{
			pnew = m_pFreeTrails;
			m_pFreeTrails = pnew->next;
			div = 0;
		}
	}

	if ( pnew )
	{
		pnew->org = pbeam->attachment[0];
		pnew->die = gpGlobals->time + pbeam->amplitude;
		pnew->vel = g_vecZero;

		pnew->next = particles;
		pbeam->trail = pnew;
		particles = pnew;
	}

	if ( !particles )
	{
		return;
	}
	if ( !pnew && div != 0 )
	{
		delta = pbeam->attachment[0];
		g_engfuncs.pTriAPI->WorldToScreen( pbeam->attachment[0], screenLast );
		g_engfuncs.pTriAPI->WorldToScreen( particles->org, screen );
	}
	else if ( particles && particles->next )
	{
		delta = particles->org;
		g_engfuncs.pTriAPI->WorldToScreen( particles->org, screenLast );
		g_engfuncs.pTriAPI->WorldToScreen( particles->next->org, screen );
		particles = particles->next;
	}
	else
	{
		return;
	}

	// Draw it
	::DrawBeamFollow( spriteIndex, pbeam->trail, frame, rendermode, delta, screen, screenLast, 
		pbeam->die, pbeam->attachment[0], pbeam->flags, pbeam->width, 
		pbeam->amplitude, pbeam->freq, (float*)color );
	
	// Drift popcorn trail if there is a velocity
	particles = pbeam->trail;
	while ( particles )
	{
		particles->org = particles->org + (particles->vel * frametime);
		particles = particles->next;
	}
}


//------------------------------------------------------------------------------
// Purpose : Draw a beam based upon the viewpoint
//------------------------------------------------------------------------------
void CViewRenderBeams::DrawLaser( Beam_t *pbeam, int frame, int rendermode, float *color, int spriteIndex )
{
	float	color2[3];

	color2[0] = color[0];
	color2[1] = color[1];
	color2[2] = color[2];

	Vector vecForward;
	Vector beamDir = ( pbeam->attachment[1] - pbeam->attachment[0] ).Normalize();

	AngleVectors( gpViewParams->viewangles, vecForward, NULL, NULL );
	float flDot = DotProduct( beamDir, vecForward );

	// abort if the player's looking along it away from the source
	if ( flDot > 0 )
	{
		return;
	}
	else
	{
		// Fade the beam if the player's not looking at the source
		float flFade = pow( flDot, 10 );

		// Fade the beam based on the player's proximity to the beam
		Vector localDir = gpViewParams->vieworg - pbeam->attachment[0];
		flDot = DotProduct( beamDir, localDir );
		Vector vecProjection = flDot * beamDir;
		float flDistance = ( localDir - vecProjection ).Length();

		if ( flDistance > 30 )
		{
			flDistance = 1 - (( flDistance - 30 ) / 64);
			if ( flDistance <= 0 )
			{
				flFade = 0;
			}
			else
			{
				flFade *= pow( flDistance, 3 );
			}
		}

		if (flFade < (1.0f / 255.0f))
			return;

		color2[0] *= flFade;
		color2[1] *= flFade;
		color2[2] *= flFade;

		//ALERT( at_console, "Fade: %f", flFade );
		//ALERT( at_console, "Dist: %f", flDistance );
	}

	DrawSegs( NOISE_DIVISIONS, pbeam->rgNoise, spriteIndex, frame, rendermode, pbeam->attachment[0],
		pbeam->delta, pbeam->width, pbeam->endWidth, pbeam->amplitude, pbeam->freq, pbeam->speed,
		pbeam->segments, pbeam->flags, color2, pbeam->fadeLength );

}

//-----------------------------------------------------------------------------
// Purpose: Draw all beam entities
// Input  : *pbeam - 
//			frametime - 
//-----------------------------------------------------------------------------
void CViewRenderBeams::DrawBeam( Beam_t *pbeam )
{
	ASSERT( pbeam->delta.IsValid() );

	// Don't draw really short beams
	if ( pbeam->delta.Length() < 0.1f )
	{
		return;
	}

	if ( GetModelType( pbeam->modelIndex ) == mod_bad )
	{
		pbeam->die = gpGlobals->time;
		return;
	}

	int frame = ( ( int )( pbeam->frame + gpGlobals->time * pbeam->frameRate) % pbeam->frameCount );
	int rendermode = ( pbeam->flags & FBEAM_SOLID ) ? kRenderNormal : kRenderTransAdd;

	// set color
	float srcColor[3];
	float color[3];

	srcColor[0] = pbeam->r;
	srcColor[1] = pbeam->g;
	srcColor[2] = pbeam->b;

	if ( pbeam->flags & FBEAM_FADEIN )
	{
		color[0] = srcColor[0] * pbeam->t;
		color[1] = srcColor[1] * pbeam->t;
		color[2] = srcColor[2] * pbeam->t;
	}
	else if ( pbeam->flags & FBEAM_FADEOUT )
	{
		color[0] = srcColor[0] * ( 1.0f - pbeam->t );
		color[1] = srcColor[1] * ( 1.0f - pbeam->t );
		color[2] = srcColor[2] * ( 1.0f - pbeam->t );
	}
	else
	{
		color[0] = srcColor[0];
		color[1] = srcColor[1];
		color[2] = srcColor[2];
	}

	color[0] *= ((float)pbeam->brightness / 255.0);
	color[1] *= ((float)pbeam->brightness / 255.0);
	color[2] *= ((float)pbeam->brightness / 255.0);
	color[0] *= (1/255.0);
	color[1] *= (1/255.0);
	color[2] *= (1/255.0);

	switch( pbeam->type )
	{
	case TE_BEAMDISK:
		DrawDisk( NOISE_DIVISIONS, pbeam->rgNoise, pbeam->modelIndex, frame, rendermode, 
			pbeam->attachment[0], pbeam->delta, pbeam->width, pbeam->amplitude, 
			pbeam->freq, pbeam->speed, pbeam->segments, color );
		break;
	case TE_BEAMCYLINDER:
		DrawCylinder( NOISE_DIVISIONS, pbeam->rgNoise, pbeam->modelIndex, frame, rendermode, 
			pbeam->attachment[0], pbeam->delta, pbeam->width, pbeam->amplitude, 
			pbeam->freq, pbeam->speed, pbeam->segments, color );
		break;
	case TE_BEAMPOINTS:
		DrawSegs( NOISE_DIVISIONS, pbeam->rgNoise, pbeam->modelIndex, frame, rendermode, 
			pbeam->attachment[0], pbeam->delta, pbeam->width, pbeam->endWidth, 
			pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, 
			pbeam->flags, color, pbeam->fadeLength );
		break;
	case TE_BEAMFOLLOW:
		DrawBeamFollow( pbeam->modelIndex, pbeam, frame, rendermode, gpGlobals->frametime, color );
		break;
	case TE_BEAMRING:
	case TE_BEAMRINGPOINT:
		DrawRing( NOISE_DIVISIONS, pbeam->rgNoise, FracNoise, pbeam->modelIndex, frame, rendermode, 
			pbeam->attachment[0], pbeam->delta, pbeam->width, pbeam->amplitude, 
			pbeam->freq, pbeam->speed, pbeam->segments, color );
		break;
	case TE_BEAMLASER:
		DrawLaser( pbeam, frame, rendermode, color, pbeam->modelIndex );
		break;
	default:
		ALERT( at_warning, "CViewRenderBeams::DrawBeam:  Unknown beam type %i\n", pbeam->type );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Update the beam 
//-----------------------------------------------------------------------------
void CViewRenderBeams::UpdateBeamInfo( Beam_t *pBeam, BeamInfo_t &beamInfo )
{
	pBeam->attachment[0] = beamInfo.m_vecStart;
	pBeam->attachment[1] = beamInfo.m_vecEnd;
	pBeam->delta = beamInfo.m_vecEnd - beamInfo.m_vecStart;

	ASSERT( pBeam->delta.IsValid() );

	SetBeamAttributes( pBeam, beamInfo );
}


//-----------------------------------------------------------------------------
// Recomputes beam endpoints..
//-----------------------------------------------------------------------------
bool CViewRenderBeams::RecomputeBeamEndpoints( Beam_t *pbeam )
{
	if ( pbeam->flags & FBEAM_STARTENTITY )
	{
		if( ComputeBeamEntPosition( pbeam->entity[0], pbeam->attachmentIndex[0], pbeam->attachment[0] ))
		{
			pbeam->flags |= FBEAM_STARTVISIBLE;
		}
		else if (!( pbeam->flags & FBEAM_FOREVER ))
		{
			pbeam->flags &= ~(FBEAM_STARTENTITY);
		}
		else
		{
			// ALERT( at_warning, "can't find start entity\n" );
			// return false;
		}

		// If we've never seen the start entity, don't display
		if (!( pbeam->flags & FBEAM_STARTVISIBLE ))
			return false;
	}

	if ( pbeam->flags & FBEAM_ENDENTITY )
	{
		if ( ComputeBeamEntPosition( pbeam->entity[1], pbeam->attachmentIndex[1], pbeam->attachment[1] ))
		{
			pbeam->flags |= FBEAM_ENDVISIBLE;
		}
		else if (!( pbeam->flags & FBEAM_FOREVER ))
		{
			pbeam->flags &= ~(FBEAM_ENDENTITY);
			pbeam->die = gpGlobals->time;
			return false;
		}
		else
		{
			return false;
		}

		// If we've never seen the end entity, don't display
		if (!( pbeam->flags & FBEAM_ENDVISIBLE ))
			return false;
	}
	return true;
}

void CViewRenderBeams::ClearServerBeams( void )
{
	m_nNumServerBeams = 0;
}
	
void CViewRenderBeams::AddServerBeam( edict_t *pEnvBeam )
{
	if( m_nNumServerBeams >= MAX_BEAMS )
	{
		ALERT( at_error, "Too many static beams %d!\n", m_nNumServerBeams );
		return;
	}

	if( pEnvBeam && !( pEnvBeam->v.effects & EF_NODRAW ))
	{
		m_pServerBeams[m_nNumServerBeams] = pEnvBeam;
		m_nNumServerBeams++;
	}
}

void CViewRenderBeams::UpdateBeams( int fTrans )
{
	for( int i = 0; i < m_nNumServerBeams; i++ )
	{
		edict_t	*pBeam = m_pServerBeams[i];

		if( (fTrans && pBeam->v.renderfx & FBEAM_SOLID) || (!fTrans && !(pBeam->v.renderfx & FBEAM_SOLID)))
			continue;

		DrawBeam( pBeam );
	}

	if ( !m_pActiveBeams )
		return;

	// Draw temporary entity beams
	Beam_t *pNext, *pBeam;

	for ( pBeam = m_pActiveBeams; pBeam ; pBeam = pNext )
	{
		// Need to store the next one since we may delete this one
		pNext = pBeam->next;

		if( (fTrans && pBeam->flags & FBEAM_SOLID) || (!fTrans && !(pBeam->flags & FBEAM_SOLID)))
			continue;

		// Update beam state
		DrawBeam( pBeam );
	}
}
	
//-----------------------------------------------------------------------------
// Draws a single beam
//-----------------------------------------------------------------------------
void CViewRenderBeams::DrawBeam( edict_t *pbeam )
{
	Beam_t beam;

	// NOTE: beam settings stored in various entavrs_t fields
	// see server/basebeams.h for details

	// Set up the beam.
	int beamType = pbeam->v.rendermode;

	BeamInfo_t beamInfo;
	beamInfo.m_vecStart = pbeam->v.origin;
	beamInfo.m_vecEnd = pbeam->v.oldorigin;
	beamInfo.m_pStartEnt = beamInfo.m_pEndEnt = NULL;
	beamInfo.m_nModelIndex = pbeam->v.modelindex;
	beamInfo.m_flLife = 0;
	beamInfo.m_flWidth = pbeam->v.scale * 0.1f;
	beamInfo.m_flEndWidth = pbeam->v.scale * 0.1f;	// FIXME: make tuneable
	beamInfo.m_flFadeLength = 0.0f;		// FIXME: make tuneable
	beamInfo.m_flAmplitude = pbeam->v.body * 0.1f;
	beamInfo.m_flBrightness = pbeam->v.renderamt;
	beamInfo.m_flSpeed = pbeam->v.animtime * 0.1f;

	SetupBeam( &beam, beamInfo );

	beamInfo.m_nStartFrame = pbeam->v.frame;
	beamInfo.m_flFrameRate = pbeam->v.framerate;
	beamInfo.m_flRed = pbeam->v.rendercolor[0];
	beamInfo.m_flGreen = pbeam->v.rendercolor[1];
	beamInfo.m_flBlue = pbeam->v.rendercolor[2];

	SetBeamAttributes( &beam, beamInfo );

	// handle code from relinking.
	switch( beamType )
	{
	case BEAM_ENTS:
		beam.type			= TE_BEAMPOINTS;
		beam.flags		= FBEAM_STARTENTITY|FBEAM_ENDENTITY;
		beam.entity[0]		= pbeam->v.owner;
		beam.attachmentIndex[0]	= pbeam->v.colormap & 0xFF;
		beam.entity[1]		= pbeam->v.aiment;
		beam.attachmentIndex[1]	= ((pbeam->v.colormap & 0xFF00)>>8);
		beam.numAttachments		= (pbeam->v.owner) ? ((pbeam->v.aiment) ? 2 : 1) : 0;
		break;
	case BEAM_LASER:
		beam.type			= TE_BEAMLASER;
		beam.flags		= FBEAM_STARTENTITY|FBEAM_ENDENTITY;
		beam.entity[0]		= pbeam->v.owner;
		beam.attachmentIndex[0]	= pbeam->v.colormap & 0xFF;
		beam.entity[1]		= pbeam->v.aiment;
		beam.attachmentIndex[1]	= ((pbeam->v.colormap & 0xFF00)>>8);
		beam.numAttachments		= (pbeam->v.owner) ? ((pbeam->v.aiment) ? 2 : 1) : 0;
		break;
	case BEAM_ENTPOINT:
		beam.type			= TE_BEAMPOINTS;
		beam.flags 		= 0;
		beam.entity[0]		= pbeam->v.owner;
		beam.attachmentIndex[0]	= pbeam->v.colormap & 0xFF;
		beam.entity[1]		= pbeam->v.aiment;
		beam.attachmentIndex[1]	= ((pbeam->v.colormap & 0xFF00)>>8);
		beam.numAttachments		= 0;
		beam.flags 		= 0;
		if ( beam.entity[0] )
		{
			beam.flags |= FBEAM_STARTENTITY;
			beam.numAttachments++;
		}
		if ( beam.entity[1] )
		{
			beam.flags |= FBEAM_ENDENTITY;
			beam.numAttachments++;
		}
		break;
	case BEAM_POINTS:
		// already set up
		break;
	}

	beam.flags |= pbeam->v.renderfx & (FBEAM_SINENOISE|FBEAM_SOLID|FBEAM_SHADEIN|FBEAM_SHADEOUT);

	// draw it
	UpdateBeam( &beam, gpGlobals->frametime );
	DrawBeam( &beam );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : noise_divisions - 
//			*prgNoise - 
//			*spritemodel - 
//			frame - 
//			rendermode - 
//			source - 
//			delta - 
//			flags - 
//			*color - 
//			fadescale - 
//-----------------------------------------------------------------------------
void DrawSegs( int noise_divisions, float *prgNoise, int modelIndex, float frame, int rendermode,
	const Vector& source, const Vector& delta, float startWidth, float endWidth, float scale,
	float freq, float speed, int segments, int flags, float* color, float fadeLength )
{
	int	i, noiseIndex, noiseStep;
	float	div, length, fraction, factor, vLast, vStep, brightness;

	if( !CVAR_GET_FLOAT( "cl_draw_beams" ))
		return;
	
	ASSERT( fadeLength >= 0.0f );
	HSPRITE m_hSprite = g_engfuncs.pTriAPI->GetSpriteTexture( modelIndex, frame );

	if ( !m_hSprite )
		return;

	if ( segments < 2 )
		return;
	
	length = delta.Length( );
	float flMaxWidth = max( startWidth, endWidth ) * 0.5f;
	div = 1.0 / (segments - 1);

	if ( length*div < flMaxWidth * 1.414f )
	{
		// Here, we have too many segments; we could get overlap... so lets have less segments
		segments = (int)(length / ( flMaxWidth * 1.414f )) + 1;
		if ( segments < 2 ) segments = 2;
	}

	if ( segments > noise_divisions )		// UNDONE: Allow more segments?
	{
		segments = noise_divisions;
	}

	div = 1.0 / (segments-1);
	length *= 0.01;

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vStep = length * div;	// Texture length texels per space pixel

	// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod(freq * speed, 1 );

	if ( flags & FBEAM_SINENOISE )
	{
		if ( segments < 16 )
		{
			segments = 16;
			div = 1.0 / (segments - 1);
		}
		scale *= 100;
		length = segments * (1.0f / 10);
	}
	else
	{
		scale *= length;
	}

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)((float)(noise_divisions - 1) * div * 65536.0f);
	noiseIndex = 0;
	
	if ( flags & FBEAM_SINENOISE )
	{
		noiseIndex = 0;
	}

	brightness = 1.0;
	if ( flags & FBEAM_SHADEIN )
	{
		brightness = 0;
	}

	// What fraction of beam should be faded
	ASSERT( fadeLength >= 0.0f );
	float fadeFraction = fadeLength/ delta.Length();
	
	// BUGBUG: This code generates NANs when fadeFraction is zero! REVIST!
	fadeFraction = bound( 1e-6, fadeFraction, 1.0f );

	// Choose two vectors that are perpendicular to the beam
	Vector perp1;
	ComputeBeamPerpendicular( delta, &perp1 );

	// Specify all the segments.
	CBeamSegDraw segDraw;
	segDraw.Start( segments, m_hSprite, (kRenderMode_t)rendermode );

	for ( i = 0; i < segments; i++ )
	{
		ASSERT( noiseIndex < ( noise_divisions << 16 ));
		CBeamSeg curSeg;
		curSeg.m_flAlpha = 1;

		fraction = i * div;

		// Fade in our out beam to fadeLength

		if ( (flags & FBEAM_SHADEIN) && (flags & FBEAM_SHADEOUT) )
		{
			if (fraction < 0.5)
			{
				brightness = 2 * (fraction / fadeFraction);
			}
			else
			{
				brightness = 2 * (1.0f - (fraction / fadeFraction));
			}
		}
		else if ( flags & FBEAM_SHADEIN )
		{
			brightness = fraction / fadeFraction;
		}
		else if ( flags & FBEAM_SHADEOUT )
		{
			brightness = 1.0f - (fraction / fadeFraction);
		}

		// clamps
		if ( brightness < 0 )		
		{
			brightness = 0;
		}
		else if ( brightness > 1 )		
		{
			brightness = 1;
		}

		curSeg.m_vColor.x = color[0] * brightness;
		curSeg.m_vColor.y = color[1] * brightness;
		curSeg.m_vColor.z = color[2] * brightness;

		// UNDONE: Make this a spline instead of just a line?
		curSeg.m_vPos = source + ( delta * fraction );
 
		// Distort using noise
		if ( scale != 0 )
		{
			factor = prgNoise[noiseIndex>>16] * scale;
			if ( flags & FBEAM_SINENOISE )
			{
				float	s, c;
				s = sin( fraction * M_PI * length + freq );
				c = cos( fraction * M_PI * length + freq );
				
				curSeg.m_vPos = curSeg.m_vPos + (gpViewParams->up * (factor * s));
				// Rotate the noise along the perpendicluar axis a bit to keep the bolt
				// from looking diagonal
				curSeg.m_vPos = curSeg.m_vPos + (gpViewParams->right * (factor * c));
			}
			else
			{
				curSeg.m_vPos = curSeg.m_vPos + (perp1 * factor);
			}
		}

		// Specify the next segment.
		if( endWidth == startWidth )
		{
			curSeg.m_flWidth = startWidth * 2;
		}
		else
		{
			curSeg.m_flWidth = ((fraction*(endWidth-startWidth))+startWidth) * 2;
		}
		
		curSeg.m_flTexCoord = vLast;
		segDraw.NextSeg( &curSeg );


		vLast += vStep;	// Advance texture scroll (v axis only)
		noiseIndex += noiseStep;
	}

	segDraw.End();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : noise_divisions - 
//			*prgNoise - 
//			*spritemodel - 
//			frame - 
//			rendermode - 
//			source - 
//			delta - 
//			width - 
//			scale - 
//			freq - 
//			speed - 
//			segments - 
//			*color - 
//-----------------------------------------------------------------------------
void DrawDisk( int noise_divisions, float *prgNoise, int modelIndex, float frame, int rendermode,
	const Vector& source, const Vector& delta, float width, float scale, float freq, float speed,
	int segments, float* color )
{
	int	i;
	float	div, length, fraction, vLast, vStep;
	Vector	point;
	float	w;

	HSPRITE m_hSprite = g_engfuncs.pTriAPI->GetSpriteTexture( modelIndex, frame );

	if ( !m_hSprite )
		return;

	if ( segments < 2 )
		return;
	
	if ( segments > noise_divisions )		// UNDONE: Allow more segments?
		segments = noise_divisions;

	length = delta.Length( ) * 0.01f;
	if ( length < 0.5f ) length = 0.5f;		// Don't lose all of the noise/texture on short beams
		
	div = 1.0 / (segments-1);

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vStep = length * div;	// Texture length texels per space pixel
	
	// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod( freq * speed, 1 );
	scale = scale * length;

	w = freq * delta[2];

	g_engfuncs.pTriAPI->Enable( TRI_SHADER );
	g_engfuncs.pTriAPI->RenderMode( (kRenderMode_t)rendermode );
	g_engfuncs.pTriAPI->Bind( m_hSprite, 0 );	// GetSpriteTexture already set frame

	g_engfuncs.pTriAPI->Begin( TRI_TRIANGLE_STRIP );

	// NOTE: We must force the degenerate triangles to be on the edge
	for ( i = 0; i < segments; i++ )
	{
		float	s, c;
		fraction = i * div;

		point[0] = source[0];
		point[1] = source[1];
		point[2] = source[2];

		g_engfuncs.pTriAPI->Color4f( color[0], color[1], color[2], 1.0f );
		g_engfuncs.pTriAPI->TexCoord2f( 1.0f, vLast );
		g_engfuncs.pTriAPI->Vertex3fv( point );

		s = sin( fraction * 2 * M_PI );
		c = cos( fraction * 2 * M_PI );
		point[0] = s * w + source[0];
		point[1] = c * w + source[1];
		point[2] = source[2];

		g_engfuncs.pTriAPI->Color4f( color[0], color[1], color[2], 1.0f );
		g_engfuncs.pTriAPI->TexCoord2f( 0.0f, vLast );
		g_engfuncs.pTriAPI->Vertex3fv( point );

		vLast += vStep;	// Advance texture scroll (v axis only)
	}

	g_engfuncs.pTriAPI->End();
	g_engfuncs.pTriAPI->Disable( TRI_SHADER );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : noise_divisions - 
//			*prgNoise - 
//			*spritemodel - 
//			frame - 
//			rendermode - 
//			source - 
//			delta - 
//			width - 
//			scale - 
//			freq - 
//			speed - 
//			segments - 
//			*color - 
//-----------------------------------------------------------------------------
void DrawCylinder( int noise_divisions, float *prgNoise, int modelIndex, float frame, int rendermode,
	const Vector&  source, const Vector& delta, float width, float scale, float freq, float speed,
	int segments, float* color )
{
	int	i;
	float	div, length, fraction, vLast, vStep;
	Vector	point;

	HSPRITE m_hSprite = g_engfuncs.pTriAPI->GetSpriteTexture( modelIndex, frame );

	if ( !m_hSprite )
		return;

	if ( segments < 2 )
		return;
	
	if ( segments > noise_divisions )		// UNDONE: Allow more segments?
		segments = noise_divisions;

	length = delta.Length( ) * 0.01f;
	if ( length < 0.5f ) length = 0.5f;		// Don't lose all of the noise/texture on short beams
	
	div = 1.0f / (segments - 1);

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vStep = length * div;	// Texture length texels per space pixel
	
	// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod(freq * speed, 1.0f );
	scale = scale * length;
	
	g_engfuncs.pTriAPI->Enable( TRI_SHADER );
	g_engfuncs.pTriAPI->RenderMode( (kRenderMode_t)rendermode );
	g_engfuncs.pTriAPI->Bind( m_hSprite, 0 );	// GetSpriteTexture already set frame

	g_engfuncs.pTriAPI->Begin( TRI_TRIANGLE_STRIP );

	float radius = delta[2];
	for ( i = 0; i < segments; i++ )
	{
		float	s, c;
		fraction = i * div;
		s = sin( fraction * 2 * M_PI );
		c = cos( fraction * 2 * M_PI );

		point[0] = s * freq * radius + source[0];
		point[1] = c * freq * radius + source[1];
		point[2] = source[2] + width;

		g_engfuncs.pTriAPI->Color4f( 0.0f, 0.0f, 0.0f, 1.0f );
		g_engfuncs.pTriAPI->TexCoord2f( 1.0f, vLast );
		g_engfuncs.pTriAPI->Vertex3fv( point );

		point[0] = s * freq * (radius + width) + source[0];
		point[1] = c * freq * (radius + width) + source[1];
		point[2] = source[2] - width;

		g_engfuncs.pTriAPI->Color4f( color[0], color[1], color[2], 1.0f );
		g_engfuncs.pTriAPI->TexCoord2f( 0.0f, vLast );
		g_engfuncs.pTriAPI->Vertex3fv( point );

		vLast += vStep;	// Advance texture scroll (v axis only)
	}
	
	g_engfuncs.pTriAPI->End();
	g_engfuncs.pTriAPI->Disable( TRI_SHADER );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : noise_divisions - 
//			*prgNoise - 
//			(*pfnNoise - 
//-----------------------------------------------------------------------------
void DrawRing( int noise_divisions, float *prgNoise, void (*pfnNoise)( float *noise, int divs, float scale ), 
	int modelIndex, float frame, int rendermode, const Vector& source, const Vector& delta, float width, 
	float amplitude, float freq, float speed, int segments, float *color )
{
	int	i, j, noiseIndex, noiseStep;
	float	div, length, fraction, factor, vLast, vStep;
	Vector	last1, last2, point, screen, screenLast(0,0,0), tmp, normal;
	Vector	center, xaxis, yaxis, zaxis;
	float	radius, x, y, scale;
	Vector	d;

	HSPRITE m_hSprite = g_engfuncs.pTriAPI->GetSpriteTexture( modelIndex, frame );

	if ( !m_hSprite )
		return;

	d = delta;

	if ( segments < 2 )
		return;

	segments = segments * M_PI;
	
	if ( segments > noise_divisions * 8 )		// UNDONE: Allow more segments?
		segments = noise_divisions * 8;

	length = d.Length( ) * 0.01f * M_PI;
	if ( length < 0.5f ) length = 0.5f;		// Don't lose all of the noise/texture on short beams
		
	div = 1.0 / (segments - 1);

	// UNDONE: Expose texture length scale factor to control "fuzziness"
	vStep = length * div / 8.0f;			// Texture length texels per space pixel
	
	// UNDONE: Expose this paramter as well(3.5)?  Texture scroll rate along beam
	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod( freq * speed, 1.0f );
	scale = amplitude * length / 8.0f;

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)((noise_divisions - 1) * div * 65536.0) * 8;
	noiseIndex = 0;

	d *= 0.5f;
	center = source + d;
	zaxis.Init();

	xaxis = d;
	radius = xaxis.Length( );
	
	// cull beamring
	// --------------------------------
	// Compute box center +/- radius
	last1[0] = radius;
	last1[1] = radius;
	last1[2] = scale;
	tmp = center + last1;	// maxs
	screen = center - last1;	// mins

	// Is that box in PVS && frustum?
	if ( !g_engfuncs.pEfxAPI->CL_IsBoxVisible( screen, tmp ) || g_engfuncs.pEfxAPI->R_CullBox( screen, tmp ))	
	{
		return;
	}

	yaxis[0] = xaxis[1];
	yaxis[1] = -xaxis[0];
	yaxis[2] = 0;

	yaxis = yaxis.Normalize( );
	yaxis *= radius;

	j = segments / 8;

	g_engfuncs.pTriAPI->Enable( TRI_SHADER );
	g_engfuncs.pTriAPI->RenderMode( (kRenderMode_t)rendermode );
	g_engfuncs.pTriAPI->Bind( m_hSprite, 0 );	// GetSpriteTexture already set frame

	g_engfuncs.pTriAPI->Begin( TRI_TRIANGLE_STRIP );

	for ( i = 0; i < segments + 1; i++ )
	{
		fraction = i * div;
		x = sin( fraction * 2 * M_PI );
		y = cos( fraction * 2 * M_PI );

		point[0] = xaxis[0] * x + yaxis[0] * y + center[0];
		point[1] = xaxis[1] * x + yaxis[1] * y + center[1];
		point[2] = xaxis[2] * x + yaxis[2] * y + center[2];

		// Distort using noise
		factor = prgNoise[(noiseIndex>>16) & 0x7F] * scale;
		point = point + (gpViewParams->up * factor);

		// Rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
		factor = prgNoise[(noiseIndex>>16) & 0x7F] * scale * cos(fraction * M_PI * 3 * 8 + freq);
		point = point + (gpViewParams->right * factor);
		
		// Transform point into screen space
		g_engfuncs.pTriAPI->WorldToScreen( point, screen );

		if ( i != 0 )
		{
			// Build world-space normal to screen-space direction vector
			tmp = screen - screenLast;
			// We don't need Z, we're in screen space
			tmp[2] = 0;
			tmp = tmp.Normalize();
			normal = gpViewParams->up * tmp.x; // Build point along noraml line (normal is -y, x)
			normal = normal + (gpViewParams->right * -tmp.y);
			
			// make a wide line
			last1 = point + (normal * width );
			last2 = point + (normal * -width);

			vLast += vStep;	// Advance texture scroll (v axis only)
			g_engfuncs.pTriAPI->Color4f( color[0], color[1], color[2], 1.0f );
			g_engfuncs.pTriAPI->TexCoord2f( 1.0f, vLast );
			g_engfuncs.pTriAPI->Vertex3fv( last2 );

			g_engfuncs.pTriAPI->Color4f( color[0], color[1], color[2], 1.0f );
			g_engfuncs.pTriAPI->TexCoord2f( 0.0f, vLast );
			g_engfuncs.pTriAPI->Vertex3fv( last1 );
		}

		screenLast = screen;
		noiseIndex += noiseStep;

		j--;
		if ( j == 0 && amplitude != 0 )
		{
			j = segments / 8;
			(*pfnNoise)( prgNoise, noise_divisions, 1.0f );
		}
	}

	g_engfuncs.pTriAPI->End();
	g_engfuncs.pTriAPI->Disable( TRI_SHADER );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : spritemodel - 
//			*pHead - 
//			delta - 
//			*screen - 
//			*screenLast - 
//			die - 
//			source - 
//			flags - 
//			width - 
//			amplitude - 
//			freq - 
//			*color - 
//-----------------------------------------------------------------------------
void DrawBeamFollow( int modelIndex, BeamTrail_t* pHead, int frame, int rendermode, Vector& delta,
	Vector& screen, Vector& screenLast, float die, const Vector& source, int flags, float width,
	float amplitude, float freq, float* color )
{
	float	fraction;
	float	div;
	float	vLast = 0.0;
	float	vStep = 1.0;
	Vector	last1, last2, tmp, normal;
	float	scaledColor[3];

	HSPRITE m_hSprite = g_engfuncs.pTriAPI->GetSpriteTexture( modelIndex, frame );

	if ( !m_hSprite )
		return;

	// UNDONE: This won't work, screen and screenLast must be extrapolated here to fix the
	// first beam segment for this trail

	// Build world-space normal to screen-space direction vector
	tmp = screen - screenLast;
	// We don't need Z, we're in screen space
	tmp.z = 0;
	tmp = tmp.Normalize( );
	normal = gpViewParams->up * tmp.x;	// Build point along noraml line (normal is -y, x)
	normal = normal + (gpViewParams->right * -tmp.y );
	
	// Make a wide line
	last1 = delta + ( normal * width );
	last2 = delta + ( normal * -width );

	div = 1.0f / amplitude;
	fraction = ( die - gpGlobals->time ) * div;
	byte nColor[3];

	scaledColor[0] = color[0] * fraction;
	scaledColor[1] = color[1] * fraction;
	scaledColor[2] = color[2] * fraction;
	nColor[0] = (byte)bound( 0, (int)(scaledColor[0] * 255.0f), 255 );
	nColor[1] = (byte)bound( 0, (int)(scaledColor[1] * 255.0f), 255 );
	nColor[2] = (byte)bound( 0, (int)(scaledColor[2] * 255.0f), 255 );
	
	// need to count the segments
	int count = 0;
	BeamTrail_t* pTraverse = pHead;
	while ( pTraverse )
	{
		++count;
		pTraverse = pTraverse->next;
	}

	g_engfuncs.pTriAPI->Enable( TRI_SHADER );
	g_engfuncs.pTriAPI->RenderMode( (kRenderMode_t)rendermode );
	g_engfuncs.pTriAPI->Bind( m_hSprite, 0 );	// GetSpriteTexture already set frame

	g_engfuncs.pTriAPI->Begin( TRI_QUADS );

	while ( pHead )
	{
		// ALERT( at_console, "%.2f ", fraction );
		g_engfuncs.pTriAPI->Color4ub( nColor[0], nColor[1], nColor[2], 255 );
		g_engfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f );
		g_engfuncs.pTriAPI->Vertex3fv( last1 );

		g_engfuncs.pTriAPI->Color4ub( nColor[0], nColor[1], nColor[2], 255 );
		g_engfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f );
		g_engfuncs.pTriAPI->Vertex3fv( last2 );

		// Transform point into screen space
		g_engfuncs.pTriAPI->WorldToScreen( pHead->org, screen );

		// Build world-space normal to screen-space direction vector
		tmp = screen - screenLast;
		// We don't need Z, we're in screen space
		tmp.z = 0;
		tmp = tmp.Normalize();
		normal = gpViewParams->up * tmp.x;	// Build point along noraml line (normal is -y, x)
		normal = normal + (gpViewParams->right * -tmp.y );
		
		// Make a wide line
		last1 = pHead->org + (normal * width );
		last2 = pHead->org + (normal * -width );

		vLast += vStep;	// Advance texture scroll (v axis only)

		if ( pHead->next != NULL )
		{
			fraction = (pHead->die - gpGlobals->time) * div;
			scaledColor[0] = color[0] * fraction;
			scaledColor[1] = color[1] * fraction;
			scaledColor[2] = color[2] * fraction;
			nColor[0] = (byte)bound( 0, (int)(scaledColor[0] * 255.0f), 255 );
			nColor[1] = (byte)bound( 0, (int)(scaledColor[1] * 255.0f), 255 );
			nColor[2] = (byte)bound( 0, (int)(scaledColor[2] * 255.0f), 255 );
		}
		else
		{
			fraction = 0.0;
			nColor[0] = nColor[1] = nColor[2] = 0;
		}

		g_engfuncs.pTriAPI->Color4ub( nColor[0], nColor[1], nColor[2], 255 );
		g_engfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f );
		g_engfuncs.pTriAPI->Vertex3fv( last2 );

		g_engfuncs.pTriAPI->Color4ub( nColor[0], nColor[1], nColor[2], 255 );
		g_engfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f );
		g_engfuncs.pTriAPI->Vertex3fv( last1 );

		screenLast = screen;

		pHead = pHead->next;
	}

	g_engfuncs.pTriAPI->End();
	g_engfuncs.pTriAPI->Disable( TRI_SHADER );
}