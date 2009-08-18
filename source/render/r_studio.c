//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_studio.c - render studio models
//=======================================================================

#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "const.h"

/*
=============================================================

  STUDIO MODELS

=============================================================
*/

#define STUDIO_API_VERSION		0.2

matrix4x4		m_pbonestransform[MAXSTUDIOBONES];
matrix4x4		m_plighttransform[MAXSTUDIOBONES];
matrix4x4		m_protationmatrix;
static mesh_t	studio_mesh;

vec3_t		m_plightcolor;		// ambient light color
vec3_t		m_plightvec;			// ambleint light vector
vec3_t		m_pshadevector;		// shadow vector
vec3_t		studio_mins, studio_maxs;
float		studio_radius;

// lighting stuff
vec3_t		*m_pxformverts;
vec3_t		*m_pvlightvalues;
vec3_t		m_blightvec [MAXSTUDIOBONES];
vec3_t		g_lightvalues[MAXSTUDIOVERTS];
vec3_t		g_xformverts[MAXSTUDIOMODELS][MAXSTUDIOVERTS];	// cache for current rendering model
bool		g_cachestate[MAXSTUDIOMODELS];		// true if vertices already transformed

// chrome stuff
float g_chrome[MAXSTUDIOVERTS][2];	// texture coords for surface normals
int g_chromeage[MAXSTUDIOBONES];	// last time chrome vectors were updated
vec3_t g_chromeup[MAXSTUDIOBONES];	// chrome vector "up" in bone reference frames
vec3_t g_chromeright[MAXSTUDIOBONES];	// chrome vector "right" in bone reference frames

int m_fDoInterp;			
int m_pStudioModelCount;
int m_nTopColor;			// palette substition for top and bottom of model			
int m_nBottomColor;
dstudiomodel_t *m_pSubModel;
dstudiohdr_t *m_pStudioHeader;
dstudiohdr_t *m_pTextureHeader;
dstudiobodyparts_t *m_pBodyPart;

int m_nCachedBones;			// number of bones in cache
char m_nCachedBoneNames[MAXSTUDIOBONES][32];
matrix4x4 m_rgCachedBonesTransform [MAXSTUDIOBONES];
matrix4x4 m_rgCachedLightTransform [MAXSTUDIOBONES];

// sprite model used for drawing studio model chrome
ref_model_t *m_pChromeSprite;
byte	*r_studiopool;

// player gait sequence stuff
int m_fGaitEstimation;
float m_flGaitMovement;

#define Studio_CopyString( str )	com.stralloc( r_studiopool, str, __FILE__, __LINE__ )		

/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	m_pBodyPart = NULL;
	m_pStudioHeader = NULL;
	m_flGaitMovement = 1;
	r_studiopool = Mem_AllocPool( "StudioModels Zone" );
}

void R_StudioShutdown( void )
{
	Mem_FreePool( &r_studiopool );
}

void R_StudioFreeBoneposes( void )
{
	Mem_EmptyPool( r_studiopool );
}

/*
====================
MISC STUDIO UTILS
====================
*/
// extract texture filename from modelname
char *R_ExtName( ref_model_t *mod )
{
	static string	texname;

	com.strncpy( texname, mod->name, MAX_STRING );
	FS_StripExtension( texname );
	com.strncat( texname, "T.mdl", MAX_STRING );
	return texname;
}

int R_StudioExtractBbox( dstudiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	dstudioseqdesc_t	*pseqdesc;
	pseqdesc = (dstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if( sequence == -1 ) return 0;
	
	VectorCopy( pseqdesc[sequence].bbmin, mins );
	VectorCopy( pseqdesc[sequence].bbmax, maxs );

	return 1;
}

void R_StudioModelBBox( ref_entity_t *e, vec3_t mins, vec3_t maxs )
{
	R_StudioExtractBbox( e->model->phdr, e->sequence, mins, maxs );
}

/*
====================
Studio model loader
====================
*/
void R_StudioSurfaceParm( dstudiotexture_t *tex )
{
	if( tex->flags & STUDIO_NF_TRANSPARENT )
		R_ShaderSetRenderMode( kRenderTransAlpha );
	else if( tex->flags & STUDIO_NF_ADDITIVE )
		R_ShaderSetRenderMode( kRenderTransAdd );
	else if( tex->flags & STUDIO_NF_BLENDED )
		R_ShaderSetRenderMode( kRenderTransTexture );
	else R_ShaderSetRenderMode( kRenderNormal );
}

dstudiohdr_t *R_StudioLoadHeader( ref_model_t *mod, const uint *buffer )
{
	int		i;
	byte		*pin;
	dstudiohdr_t	*phdr;
	dstudiotexture_t	*ptexture;
	string		shadername;
	
	pin = (byte *)buffer;
	phdr = (dstudiohdr_t *)pin;

	if( phdr->version != STUDIO_VERSION )
	{
		Msg("%s has wrong version number (%i should be %i)\n", phdr->name, phdr->version, STUDIO_VERSION);
		return NULL;
	}	

	ptexture = (dstudiotexture_t *)(pin + phdr->textureindex);
	if( phdr->textureindex > 0 && phdr->numtextures <= MAXSTUDIOSKINS )
	{
		mod->numshaders = phdr->numtextures;
		mod->shaders = Mem_Alloc( mod->mempool, sizeof( shader_t* ) * mod->numshaders );

		for( i = 0; i < phdr->numtextures; i++ )
		{
			R_StudioSurfaceParm( &ptexture[i] );
			com.snprintf( shadername, MAX_STRING, "%s/%s", mod->name, ptexture[i].name );
			FS_StripExtension( shadername ); // doesn't produce shaders with .ext
			mod->shaders[i] = R_LoadShader( shadername, SHADER_STUDIO, 0, 0, SHADER_INVALID );
			ptexture[i].shader = mod->shaders[i]->shadernum;
		}
	}
	return (dstudiohdr_t *)buffer;
}

void Mod_StudioLoadModel( ref_model_t *mod, ref_model_t *parent, const void *buffer )
{
	dstudiohdr_t	*phdr = R_StudioLoadHeader( mod, buffer );
	dstudiohdr_t	*thdr = NULL;
	void		*texbuf;
	
	if( !phdr ) return; // there were problems
	mod->phdr = (dstudiohdr_t *)Mem_Alloc(mod->mempool, LittleLong(phdr->length));
	Mem_Copy( mod->phdr, buffer, LittleLong( phdr->length ));
	
	if( phdr->numtextures == 0 )
	{
		texbuf = FS_LoadFile( R_ExtName( mod ), NULL ); // use buffer again
		if( texbuf ) thdr = R_StudioLoadHeader( mod, texbuf );
		else MsgDev( D_ERROR, "textures for %s not found!\n", mod->name ); 

		if( !thdr ) return; // there were problems
		mod->thdr = (dstudiohdr_t *)Mem_Alloc( mod->mempool, LittleLong( thdr->length ));
          	Mem_Copy( mod->thdr, texbuf, LittleLong( thdr->length ));
		if( texbuf ) Mem_Free( texbuf );
	}
	else mod->thdr = mod->phdr; // just make link

	R_StudioExtractBbox( phdr, 0, mod->mins, mod->maxs );
	mod->radius = RadiusFromBounds( mod->mins, mod->maxs );
	mod->touchFrame = tr.registration_sequence;
	mod->type = mod_studio;
}

// extract bbox from animation
int R_ExtractBbox( int sequence, float *mins, float *maxs )
{
	return R_StudioExtractBbox( m_pStudioHeader, sequence, mins, maxs );
}

void SetBodygroup( int iGroup, int iValue )
{
	int iCurrent;
	dstudiobodyparts_t *m_pBodyPart;

	if( iGroup > m_pStudioHeader->numbodyparts )
		return;

	m_pBodyPart = (dstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + iGroup;

	if (iValue >= m_pBodyPart->nummodels)
		return;

	iCurrent = (RI.currententity->body / m_pBodyPart->base) % m_pBodyPart->nummodels;
	RI.currententity->body = (RI.currententity->body - (iCurrent * m_pBodyPart->base) + (iValue * m_pBodyPart->base));
}

int R_StudioGetBodygroup( int iGroup )
{
	dstudiobodyparts_t *m_pBodyPart;
	
	if (iGroup > m_pStudioHeader->numbodyparts)
		return 0;

	m_pBodyPart = (dstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + iGroup;

	if (m_pBodyPart->nummodels <= 1)
		return 0;

	return (RI.currententity->body / m_pBodyPart->base) % m_pBodyPart->nummodels;
}

void R_StudioAddEntityToRadar( ref_entity_t *e )
{
	if( r_minimap->integer < 2 ) return; 

	if( numRadarEnts >= MAX_RADAR_ENTS ) return;
	if( e->ent_type == ED_VIEWMODEL ) return;

	if( e->ent_type == ED_MONSTER )
		Vector4Set( RadarEnts[numRadarEnts].color, 255, 0, 128, 255 ); 
	else Vector4Set( RadarEnts[numRadarEnts].color, 0, 255, 255, 128 ); 
	VectorCopy( e->origin, RadarEnts[numRadarEnts].origin );
	VectorCopy( e->angles, RadarEnts[numRadarEnts].angles );
	numRadarEnts++;
}

/*
====================
R_StudioGetSequenceInfo

used for client animation
====================
*/
float R_StudioSequenceDuration( dstudiohdr_t *hdr, ref_entity_t *ent )
{
	dstudioseqdesc_t	*pseqdesc;

	if( !hdr || ent->sequence >= hdr->numseq )
		return 0.0f;

	pseqdesc = (dstudioseqdesc_t *)((byte *)hdr + hdr->seqindex) + ent->sequence;
	return pseqdesc->numframes / pseqdesc->fps;
}

int R_StudioGetSequenceFlags( dstudiohdr_t *hdr, ref_entity_t *ent )
{
	dstudioseqdesc_t	*pseqdesc;

	if( !hdr || ent->sequence >= hdr->numseq )
		return 0;
	
	pseqdesc = (dstudioseqdesc_t *)((byte *)hdr + hdr->seqindex) + (int)ent->sequence;
	return pseqdesc->flags;
}

float R_StudioFrameAdvance( ref_entity_t *ent, float flInterval )
{
	if( flInterval == 0.0 )
	{
		flInterval = ( RI.refdef.time - ent->animtime );
		if( flInterval <= 0.001 )
		{
			ent->animtime = RI.refdef.time;
			return 0.0;
		}
	}
	if( !ent->animtime ) flInterval = 0.0;
	
	ent->frame += flInterval * ent->framerate;
	//ent->animtime = RI.refdef.time;

	if( ent->frame < 0.0 || ent->frame >= 256.0 ) 
	{
		if( ent->m_fSequenceLoops )
			ent->frame -= (int)(ent->frame / 256.0) * 256.0;
		else ent->frame = (ent->frame < 0.0) ? 0 : 255;
		ent->m_fSequenceFinished = true;
	}
	return flInterval;
}

void R_StudioResetSequenceInfo( ref_entity_t *ent, dstudiohdr_t *hdr )
{
	if( !ent || !hdr ) return;

	ent->m_fSequenceLoops = ((R_StudioGetSequenceFlags( hdr, ent ) & STUDIO_LOOPING) != 0 );

	// calc anim time
	if( !ent->animtime ) ent->animtime = RI.refdef.time;
	ent->prev.animtime = ent->animtime;
	ent->animtime = RI.refdef.time + R_StudioSequenceDuration( hdr, ent );
	ent->m_fSequenceFinished = FALSE;
}

int R_StudioGetEvent( ref_entity_t *e, dstudioevent_t *pcurrent, float flStart, float flEnd, int index )
{
	dstudioseqdesc_t	*pseqdesc;
	dstudioevent_t	*pevent;
	int		events = 0;

	pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->sequence;
	pevent = (dstudioevent_t *)((byte *)m_pStudioHeader + pseqdesc->eventindex);

	if( pseqdesc->numevents == 0 || index > pseqdesc->numevents )
		return 0;

	if( pseqdesc->numframes == 1 )
	{
		flStart = 0;
		flEnd = 1.0;
	}
	for( ; index < pseqdesc->numevents; index++ )
	{
		// don't send server-side events to the client effects
		if( pevent[index].event < EVENT_CLIENT )
			continue;

		if(( pevent[index].frame >= flStart && pevent[index].frame < flEnd )
			|| ((pseqdesc->flags & STUDIO_LOOPING) && flEnd >= pseqdesc->numframes - 1 && pevent[index].frame < flEnd - pseqdesc->numframes + 1 ))
		{
			Mem_Copy( pcurrent, &pevent[index], sizeof( *pcurrent ));
			return index + 1;
		}
	}
	return 0;
}

/*
====================
StudioCalcBoneAdj

====================
*/
void R_StudioCalcBoneAdj( float dadt, float *adj, const float *pcontroller1, const float *pcontroller2, byte mouthopen )
{
	int	i, j;
	float	value;
	dstudiobonecontroller_t *pbonecontroller;
	
	pbonecontroller = (dstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	for (j = 0; j < m_pStudioHeader->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;

		if( i == STUDIO_MOUTH )
		{
			// mouth hardcoded at controller 4
			value = mouthopen / 64.0;
			if (value > 1.0) value = 1.0;				
			value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			// Msg("%d %f\n", mouthopen, value );
		}
		else if( i <= MAXSTUDIOCONTROLLERS )
		{
			// check for 360% wrapping
			if( pbonecontroller[j].type & STUDIO_RLOOP )
			{
				if(abs(pcontroller1[i] - pcontroller2[i]) > 128)
				{
					float	a, b;
					a = fmod((pcontroller1[j] + 128), 256 );
					b = fmod((pcontroller2[j] + 128), 256 );
					value = ((a * dadt) + (b * (1 - dadt)) - 128) * (360.0/256.0) + pbonecontroller[j].start;
				}
				else 
				{
					value = ((pcontroller1[i] * dadt + (pcontroller2[i]) * (1.0 - dadt))) * (360.0/256.0) + pbonecontroller[j].start;
				}
			}
			else 
			{
				value = (pcontroller1[i] * dadt + pcontroller2[i] * (1.0 - dadt)) / 255.0;
				if (value < 0) value = 0;
				if (value > 1.0) value = 1.0;
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
			// Msg("%d %d %f : %f\n", RI.currententity->curstate.controller[j], RI.currententity->latched.prevcontroller[j], value, dadt );
		}

		switch( pbonecontroller[j].type & STUDIO_TYPES )
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = value * (M_PI / 180.0);
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}

/*
====================
StudioCalcBoneQuaterion

====================
*/
void R_StudioCalcBoneQuaterion( int frame, float s, dstudiobone_t *pbone, dstudioanim_t *panim, float *adj, float *q )
{
	int	j, k;
	vec4_t	q1, q2;
	vec3_t	angle1, angle2;
	dstudioanimvalue_t	*panimvalue;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j+3] == 0)
		{
			angle2[j] = angle1[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (dstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			k = frame;
			
			// debug
			if (panimvalue->num.total < panimvalue->num.valid) k = 0;
			
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// Bah, missing blend!
			if (panimvalue->num.valid > k)
			{
				angle1[j] = panimvalue[k+1].value;

				if (panimvalue->num.valid > k + 1)
				{
					angle2[j] = panimvalue[k+2].value;
				}
				else
				{
					if (panimvalue->num.total > k + 1)
						angle2[j] = angle1[j];
					else
						angle2[j] = panimvalue[panimvalue->num.valid+2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if (panimvalue->num.total > k + 1)
				{
					angle2[j] = angle1[j];
				}
				else
				{
					angle2[j] = panimvalue[panimvalue->num.valid + 2].value;
				}
			}
			angle1[j] = pbone->value[j+3] + angle1[j] * pbone->scale[j+3];
			angle2[j] = pbone->value[j+3] + angle2[j] * pbone->scale[j+3];
		}

		if (pbone->bonecontroller[j+3] != -1)
		{
			angle1[j] += adj[pbone->bonecontroller[j+3]];
			angle2[j] += adj[pbone->bonecontroller[j+3]];
		}
	}

	if (!VectorCompare( angle1, angle2 ))
	{
		AngleQuaternion( angle1, q1 );
		AngleQuaternion( angle2, q2 );
		QuaternionSlerp( q1, q2, s, q );
	}
	else
	{
		AngleQuaternion( angle1, q );
	}
}

/*
====================
StudioCalcBonePosition

====================
*/
void R_StudioCalcBonePosition( int frame, float s, dstudiobone_t *pbone, dstudioanim_t *panim, float *adj, float *pos )
{
	int j, k;
	dstudioanimvalue_t	*panimvalue;

	for (j = 0; j < 3; j++)
	{
		pos[j] = pbone->value[j]; // default;
		if (panim->offset[j] != 0)
		{
			panimvalue = (dstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			
			//if (j == 0) Msg("%d  %d:%d  %f\n", frame, panimvalue->num.valid, panimvalue->num.total, s );
			k = frame;

			// debug
			if (panimvalue->num.total < panimvalue->num.valid) k = 0;
			// find span of values that includes the frame we want
			while (panimvalue->num.total <= k)
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
  				// DEBUG
				if (panimvalue->num.total < panimvalue->num.valid)
					k = 0;
			}
			// if we're inside the span
			if (panimvalue->num.valid > k)
			{
				// and there's more data in the span
				if (panimvalue->num.valid > k + 1)
				{
					pos[j] += (panimvalue[k+1].value * (1.0 - s) + s * panimvalue[k+2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[k+1].value * pbone->scale[j];
				}
			}
			else
			{
				// are we at the end of the repeating values section and there's another section with data?
				if (panimvalue->num.total <= k + 1)
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if ( pbone->bonecontroller[j] != -1 && adj )
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
StudioSlerpBones

====================
*/
void R_StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s )
{
	int	i;
	vec4_t	q3;
	float	s1;

	if( s < 0 ) s = 0;
	else if( s > 1.0 ) s = 1.0;

	s1 = 1.0 - s;

	for( i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		QuaternionSlerp( q1[i], q2[i], s, q3 );
		q1[i][0] = q3[0];
		q1[i][1] = q3[1];
		q1[i][2] = q3[2];
		q1[i][3] = q3[3];
		pos1[i][0] = pos1[i][0] * s1 + pos2[i][0] * s;
		pos1[i][1] = pos1[i][1] * s1 + pos2[i][1] * s;
		pos1[i][2] = pos1[i][2] * s1 + pos2[i][2] * s;
	}
}

/*
====================
StudioGetAnim

====================
*/
dstudioanim_t *R_StudioGetAnim( ref_model_t *m_pSubModel, dstudioseqdesc_t *pseqdesc )
{
	dstudioseqgroup_t	*pseqgroup;
	byte		*paSequences;
          size_t		filesize;
          byte		*buf;
	
	pseqgroup = (dstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;
	if( pseqdesc->seqgroup == 0 )
		return (dstudioanim_t *)((byte *)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);
	paSequences = (void *)m_pSubModel->submodels;

	if( paSequences == NULL )
	{
		MsgDev( D_INFO, "loading %s\n", pseqgroup->name );
		buf = FS_LoadFile (pseqgroup->name, &filesize);
		if( !buf || !filesize )
		{
			MsgDev( D_ERROR, "R_StudioGetAnim: %s not found", pseqgroup->name );
			Mem_Set( pseqgroup->name, 0, sizeof(pseqgroup->name));
			return NULL;
		}
                    if( IDSEQGRPHEADER == LittleLong(*(uint *)buf))  //it's sequence group
                    {
			byte		*pin = (byte *)buf;
			dstudioseqgroup_t	*pseqhdr = (dstudioseqgroup_t *)pin;
			
			paSequences = (byte *)Mem_Alloc( m_pSubModel->mempool, filesize );
          		m_pSubModel->submodels = (void *)paSequences; // just a container
          		Mem_Copy( &paSequences[pseqdesc->seqgroup], buf, filesize );
			Mem_Free( buf );
		}		
	}
	return (dstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup] + pseqdesc->animindex );
}

/*
====================
StudioPlayerBlend

====================
*/
void R_StudioPlayerBlend( dstudioseqdesc_t *pseqdesc, float *pBlend, float *pPitch )
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3);

	if( *pBlend < pseqdesc->blendstart[0] )
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0f;
		*pBlend = 0.0f;
	}
	else if( *pBlend > pseqdesc->blendend[0] )
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0f;
		*pBlend = 255.0f;
	}
	else
	{
		if( pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1f ) // catch qc error
			*pBlend = 127;
		else *pBlend = 255.0f * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0;
	}
}

/*
====================
StudioSetUpTransform

====================
*/
void R_StudioSetUpTransform( ref_entity_t *e )
{
	int		i;
	vec3_t		angles;

	VectorCopy( e->angles, angles );

	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: should use a look-up table
	// TODO: could cache lazily, stored in the entity
	if( e->movetype == MOVETYPE_STEP ) 
	{
		float	d, f = 0;

		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.
		if(( RI.refdef.time < e->animtime + 1.0f ) && ( e->animtime != e->prev.animtime ) )
		{
			f = (RI.refdef.time - e->animtime) / (e->animtime - e->prev.animtime);
			Msg( "%4.2f %.2f %.2f\n", f, e->animtime, RI.refdef.time );
		}

		// calculate frontlerp value
		if( m_fDoInterp ) f = 1.0 - e->backlerp;
		else f = 0;

		for( i = 0; i < 3; i++ )
		{
			float	ang1, ang2;

			ang1 = e->angles[i];
			ang2 = e->prev.angles[i];

			d = ang1 - ang2;
			if( d > 180 ) d -= 360;
			else if( d < -180 ) d += 360;
			angles[i] += d * f;
		}
	}
	else if( e->ent_type == ED_CLIENT )
	{
		// don't rotate player model, only aim
		angles[PITCH] = 0;
	}

	Matrix4x4_CreateFromEntity( m_protationmatrix, 0.0f, 0.0f, 0.0f, angles[PITCH], angles[YAW], angles[ROLL], e->scale );
}


/*
====================
StudioEstimateInterpolant

====================
*/
float R_StudioEstimateInterpolant( void )
{
	float dadt = 1.0;

	if ( m_fDoInterp && ( RI.currententity->animtime >= RI.currententity->prev.animtime + 0.01 ) )
	{
		dadt = (RI.refdef.time - RI.currententity->animtime) / 0.1;
		if( dadt > 2.0 ) dadt = 2.0;
	}
	return dadt;
}


/*
====================
StudioCalcRotations

====================
*/
void R_StudioCalcRotations( float pos[][3], vec4_t *q, dstudioseqdesc_t *pseqdesc, dstudioanim_t *panim, float f )
{
	int		i;
	int		frame;
	dstudiobone_t	*pbone;

	float	s;
	float	adj[MAXSTUDIOCONTROLLERS];
	float	dadt;

	if( f > pseqdesc->numframes - 1 ) f = 0; // bah, fix this bug with changing sequences too fast
	else if ( f < -0.01f )
	{
		// BUG ( somewhere else ) but this code should validate this data.
		// This could cause a crash if the frame # is negative, so we'll go ahead
		//  and clamp it here
		MsgDev( D_ERROR, "f = %g\n", f );
		f = -0.01f;
	}

	frame = (int)f;

	// Msg("%d %.4f %.4f %.4f %.4f %d\n", RI.currententity->curstate.sequence, m_clTime, RI.currententity->animtime, RI.currententity->frame, f, frame );
	// Msg( "%f %f %f\n", RI.currententity->angles[ROLL], RI.currententity->angles[PITCH], RI.currententity->angles[YAW] );
	// Msg("frame %d %d\n", frame1, frame2 );

	dadt = R_StudioEstimateInterpolant();
	s = (f - frame);

	// add in programtic controllers
	pbone = (dstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	R_StudioCalcBoneAdj( dadt, adj, RI.currententity->controller, RI.currententity->prev.controller, RI.currententity->mouth.open );

	for (i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++) 
	{
		R_StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );
		R_StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
		// if (0 && i == 0) Msg("%d %d %d %d\n", RI.currententity->sequence, frame, j, k );
	}

	if (pseqdesc->motiontype & STUDIO_X) pos[pseqdesc->motionbone][0] = 0.0;
	if (pseqdesc->motiontype & STUDIO_Y) pos[pseqdesc->motionbone][1] = 0.0;
	if (pseqdesc->motiontype & STUDIO_Z) pos[pseqdesc->motionbone][2] = 0.0;

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * RI.currententity->framerate;

	if (pseqdesc->motiontype & STUDIO_LX) pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	if (pseqdesc->motiontype & STUDIO_LY) pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	if (pseqdesc->motiontype & STUDIO_LZ) pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
}

/*
====================
Studio_FxTransform

====================
*/
void R_StudioFxTransform( ref_entity_t *ent, matrix4x4 transform )
{
	if( ent->renderfx == kRenderFxHologram )
	{
		if(!Com_RandomLong( 0, 49 ))
		{
			int axis = Com_RandomLong( 0, 1 );
			if( axis == 1 ) axis = 2; // choose between x & z
			VectorScale( transform[axis], Com_RandomFloat( 1, 1.484 ), transform[axis] );
		}
		else if(!Com_RandomLong( 0, 49 ))
		{
			float offset;
			int axis = Com_RandomLong( 0, 1 );
			if( axis == 1 ) axis = 2; // choose between x & z
			offset = Com_RandomFloat( -10, 10 );
			transform[Com_RandomLong( 0, 2 )][3] += offset;
		}
	}
}

/*
====================
StudioEstimateFrame

====================
*/
float R_StudioEstimateFrame( dstudioseqdesc_t *pseqdesc )
{
	double dfdt, f;
	
	if( m_fDoInterp )
	{
		if( RI.refdef.time < RI.currententity->animtime ) dfdt = 0;
		else dfdt = (RI.refdef.time - RI.currententity->animtime) * RI.currententity->framerate * pseqdesc->fps;
	}
	else dfdt = 0;

	if( pseqdesc->numframes <= 1 ) f = 0;
	else f = (RI.currententity->frame * (pseqdesc->numframes - 1)) / 256.0;
 
	f += dfdt;

	if( pseqdesc->flags & STUDIO_LOOPING ) 
	{
		if( pseqdesc->numframes > 1 ) f -= (int)(f / (pseqdesc->numframes - 1)) *  (pseqdesc->numframes - 1);
		if( f < 0 ) f += (pseqdesc->numframes - 1);
	}
	else 
	{
		if( f >= pseqdesc->numframes - 1.001 ) f = pseqdesc->numframes - 1.001;
		if( f < 0.0 )  f = 0.0;
	}
	return f;
}

/*
====================
StudioSetupBones

====================
*/
float R_StudioSetupBones( ref_entity_t *e )
{
	int		i;
	double		f;

	dstudiobone_t	*pbones;
	dstudioseqdesc_t	*pseqdesc;
	dstudioanim_t	*panim;

	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix4x4		bonematrix;

	static float	pos2[MAXSTUDIOBONES][3];
	static vec4_t	q2[MAXSTUDIOBONES];
	static float	pos3[MAXSTUDIOBONES][3];
	static vec4_t	q3[MAXSTUDIOBONES];
	static float	pos4[MAXSTUDIOBONES][3];
	static vec4_t	q4[MAXSTUDIOBONES];

	if( e->sequence >= m_pStudioHeader->numseq ) e->sequence = 0;
	pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->sequence;

	f = R_StudioEstimateFrame( pseqdesc );

	if( e->prev.frame > f )
	{
		// Msg( "%f %f\n", e->prev.frame, f );
	}

	panim = R_StudioGetAnim( RI.currentmodel, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );

	if( pseqdesc->numblends > 1 )
	{
		float	s;
		float	dadt;

		panim += m_pStudioHeader->numbones;
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

		dadt = R_StudioEstimateInterpolant();
		s = (e->blending[0] * dadt + e->prev.blending[0] * (1.0 - dadt)) / 255.0;

		R_StudioSlerpBones( q, pos, q2, pos2, s );

		if (pseqdesc->numblends == 4)
		{
			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

			s = (e->blending[0] * dadt + e->prev.blending[0] * (1.0 - dadt)) / 255.0;
			R_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (e->blending[1] * dadt + e->prev.blending[1] * (1.0 - dadt)) / 255.0;
			R_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	if( m_fDoInterp && e->prev.sequencetime && ( e->prev.sequencetime + 0.2 > RI.refdef.time) && ( e->prev.sequence < m_pStudioHeader->numseq ))
	{
		// blend from last sequence
		static float  pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		float s;
                    
		pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->prev.sequence;
		panim = R_StudioGetAnim( RI.currentmodel, pseqdesc );
		// clip prevframe
		R_StudioCalcRotations( pos1b, q1b, pseqdesc, panim, e->prev.frame );

		if( pseqdesc->numblends > 1 )
		{
			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos2, q2, pseqdesc, panim, e->prev.frame );

			s = (e->prev.seqblending[0]) / 255.0;
			R_StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if( pseqdesc->numblends == 4 )
			{
				panim += m_pStudioHeader->numbones;
				R_StudioCalcRotations( pos3, q3, pseqdesc, panim, e->prev.frame );

				panim += m_pStudioHeader->numbones;
				R_StudioCalcRotations( pos4, q4, pseqdesc, panim, e->prev.frame );

				s = (e->prev.seqblending[0]) / 255.0;
				R_StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = (e->prev.seqblending[1]) / 255.0;
				R_StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = 1.0 - (RI.refdef.time - e->prev.sequencetime) / 0.2;
		R_StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		// MsgDev( D_INFO, "prevframe = %4.2f\n", f );
		e->prev.frame = f;
	}

	pbones = (dstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	// calc gait animation
	if( e->gaitsequence != 0 )
	{
		if( e->gaitsequence >= m_pStudioHeader->numseq ) 
			e->gaitsequence = 0;

		pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->gaitsequence;

		panim = R_StudioGetAnim( RI.currentmodel, pseqdesc );
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, e->gaitframe );

		for( i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			// g-cont. hey, what a hell ?
			if( !com.strcmp( pbones[i].name, "Bip01 Spine" ))
				break;
			Mem_Copy( pos[i], pos2[i], sizeof( pos[i] ));
			Mem_Copy( q[i], q2[i], sizeof( q[i] ));
		}
	}
          
	for (i = 0; i < m_pStudioHeader->numbones; i++) 
	{
		Matrix4x4_FromOriginQuat( bonematrix, pos[i][0], pos[i][1], pos[i][2], q[i][0], q[i][1], q[i][2], q[i][3] );
		if( pbones[i].parent == -1 ) 
		{
			Matrix4x4_ConcatTransforms( m_pbonestransform[i], m_protationmatrix, bonematrix );
			Matrix4x4_Copy( m_plighttransform[i], m_pbonestransform[i] );

			// apply client-side effects to the transformation matrix
			R_StudioFxTransform( RI.currententity, m_pbonestransform[i] );
		} 
		else 
		{
			Matrix4x4_ConcatTransforms( m_pbonestransform[i], m_pbonestransform[pbones[i].parent], bonematrix );
			Matrix4x4_ConcatTransforms( m_plighttransform[i], m_plighttransform[pbones[i].parent], bonematrix );
		}
	}
	return (float)f;
}

/*
====================
StudioSaveBones

====================
*/
void R_StudioSaveBones( ref_entity_t *e )
{
	int i;
	dstudiobone_t *pbones = (dstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	m_nCachedBones = m_pStudioHeader->numbones;

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		com.strncpy( m_nCachedBoneNames[i], pbones[i].name, 32 );
		Matrix4x4_Copy( m_rgCachedBonesTransform[i], m_pbonestransform[i] );
		Matrix4x4_Copy( m_rgCachedLightTransform[i], m_plighttransform[i] );
	}
}

/*
====================
StudioMergeBones

====================
*/
float R_StudioMergeBones ( ref_entity_t *e, ref_model_t *m_pSubModel )
{
	int		i, j;
	double		f;
	int		do_hunt = true;
	int		sequence = e->sequence;
	dstudiobone_t	*pbones;
	dstudioseqdesc_t	*pseqdesc;
	dstudioanim_t	*panim;
	matrix4x4		bonematrix;

	static vec4_t	q[MAXSTUDIOBONES];
	static float	pos[MAXSTUDIOBONES][3];

	// weaponmodel can't change e->sequence!
	if( sequence >= m_pStudioHeader->numseq ) sequence = 0;
	pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + sequence;

	f = R_StudioEstimateFrame( pseqdesc );

	if( e->prev.frame > f )
	{
		// Msg("%f %f\n", e->prev.frame, f );
	}

	panim = R_StudioGetAnim( m_pSubModel, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );
	pbones = (dstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		for( j = 0; j < m_nCachedBones; j++ )
		{
			if( !com.stricmp( pbones[i].name, m_nCachedBoneNames[j] ))
			{
				Matrix4x4_Copy( m_pbonestransform[i], m_rgCachedBonesTransform[j] );
				Matrix4x4_Copy( m_plighttransform[i], m_rgCachedLightTransform[j] );
				break;
			}
		}
		if( j >= m_nCachedBones )
		{
			Matrix4x4_FromOriginQuat( bonematrix, pos[i][0], pos[i][1], pos[i][2], q[i][0], q[i][1], q[i][2], q[i][3] );
			if( pbones[i].parent == -1 ) 
			{
				Matrix4x4_ConcatTransforms( m_pbonestransform[i], m_protationmatrix, bonematrix );
				Matrix4x4_Copy( m_plighttransform[i], m_pbonestransform[i] );

				// apply client-side effects to the transformation matrix
				R_StudioFxTransform( RI.currententity, m_pbonestransform[i] );
			} 
			else 
			{
				Matrix4x4_ConcatTransforms( m_pbonestransform[i], m_pbonestransform[pbones[i].parent], bonematrix );
				Matrix4x4_ConcatTransforms( m_plighttransform[i], m_plighttransform[pbones[i].parent], bonematrix );
			}
		}
	}
	return (float)f;
}


/*
====================
StudioCalcAttachments

====================
*/
void R_StudioCalcAttachments( ref_entity_t *e )
{
	int			i;
	dstudioattachment_t		*pattachment;

	if( m_pStudioHeader->numattachments > MAXSTUDIOATTACHMENTS )
	{
		MsgDev( D_WARN, "too many attachments on %s\n", e->model->name );
		m_pStudioHeader->numattachments = MAXSTUDIOATTACHMENTS; // reduce it
	}

	// calculate attachment points
	pattachment = (dstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
	for( i = 0; i < m_pStudioHeader->numattachments; i++ )
	{
		Matrix4x4_Transform( m_plighttransform[pattachment[i].bone], pattachment[i].org,  e->attachment[i] );
	}
}

bool R_StudioComputeBBox( vec3_t bbox[8] )
{
	vec3_t		vectors[3];
	ref_entity_t	*e = RI.currententity;
	vec3_t		tmp, angles;
	int		i, seq = RI.currententity->sequence;

	if(!R_ExtractBbox( seq, studio_mins, studio_maxs ))
		return false;
	
	studio_radius = RadiusFromBounds( studio_mins, studio_maxs );
		
	// compute a full bounding box
	for ( i = 0; i < 8; i++ )
	{
		if ( i & 1 ) tmp[0] = studio_mins[0];
		else tmp[0] = studio_maxs[0];
		if ( i & 2 ) tmp[1] = studio_mins[1];
		else tmp[1] = studio_maxs[1];
		if ( i & 4 ) tmp[2] = studio_mins[2];
		else tmp[2] = studio_maxs[2];
		VectorCopy( tmp, bbox[i] );
	}

	// rotate the bounding box
	VectorScale( e->angles, -1, angles );
	AngleVectorsFLU( angles, vectors[0], vectors[1], vectors[2] );

	for ( i = 0; i < 8; i++ )
	{
		VectorCopy( bbox[i], tmp );
		bbox[i][0] = DotProduct( vectors[0], tmp );
		bbox[i][1] = DotProduct( vectors[1], tmp );
		bbox[i][2] = DotProduct( vectors[2], tmp );
		VectorAdd( e->origin, bbox[i], bbox[i] );
	}
	return true;
}

static bool R_StudioCheckBBox( void )
{
	int i, j;
	vec3_t bbox[8];

	int aggregatemask = ~0;

	if( RI.currententity->ent_type == ED_VIEWMODEL )
		return true;          
	if(!R_StudioComputeBBox( bbox ))
          	return false;
	
	for ( i = 0; i < 8; i++ )
	{
		int mask = 0;
		for ( j = 0; j < 4; j++ )
		{
			float dp = DotProduct( RI.frustum[j].normal, bbox[i] );
			if ( ( dp - RI.frustum[j].dist ) < 0 ) mask |= ( 1 << j );
		}
		aggregatemask &= mask;
	}
          
	if ( aggregatemask )
		return false;
	return true;
}

/*
====================
StudioSetupModel

====================
*/
void R_StudioSetupModel( int body, int bodypart )
{
	int	index;

	if( bodypart > m_pStudioHeader->numbodyparts ) bodypart = 0;
	m_pBodyPart = (dstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;

	m_pSubModel = (dstudiomodel_t *)((byte *)m_pStudioHeader + m_pBodyPart->modelindex) + index;
}

void R_StudioSetupLighting( void )
{
	int i;
          dstudiobone_t *pbone;
	
	// get light from floor or ceil
	m_plightvec[0] = 0.0f;
	m_plightvec[1] = 0.0f;
	m_plightvec[2] = (RI.currententity->flags & EF_INVLIGHT) ? 1.0f : -1.0f;

	{
		vec3_t	light_org, dir;
		VectorCopy( RI.currententity->origin, light_org );
		light_org[2] += 3; // make sure what lightpoint is off the ground
		R_LightForOrigin( light_org, dir, m_plightcolor, NULL, RI.currentmodel->radius );
	}

	// TODO: only do it for bones that actually have textures
	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		pbone = (dstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex + i);
		//if( pbone->flags & STUDIO_HAS_CHROME )
		{
			Matrix4x4_Rotate3x3( m_pbonestransform[i], m_plightvec, m_blightvec[i] );
		}
	}
}

void R_StudioLighting( float *lv, int bone, int flags, vec3_t normal )
{
	float lightcos;

	float shadelight = 192.0f;
	float illum = 32.0f;//RF_MINLIGHT & RF_FULLBRIGHT

	if( flags & STUDIO_NF_FLATSHADE )
	{
		illum += shadelight * 0.8f;
	}
          else
          {
		lightcos = DotProduct (normal, m_blightvec[bone]);// -1 colinear, 1 opposite
		if (lightcos > 1.0) lightcos = 1;

		illum += shadelight;
		lightcos = (lightcos + 0.5f) / 1.5f;// do modified hemispherical lighting
		if (lightcos > 0.0) illum -= shadelight * lightcos; 
	}
	illum = bound( 0, illum, 255);

	*lv = illum / 255.0; // Light from 0 to 1.0
}

void R_StudioSetupChrome( float *pchrome, int bone, vec3_t normal )
{
	float n;

	if( g_chromeage[bone] != m_pStudioModelCount )
	{
		// calculate vectors from the viewer to the bone. This roughly adjusts for position
		vec3_t	chromeupvec;	// g_chrome t vector in world reference frame
		vec3_t	chromerightvec;	// g_chrome s vector in world reference frame
		vec3_t	tmp, tmp2;	// vector pointing at bone in world reference frame

		VectorScale( RI.currententity->origin, -1, tmp );
		Matrix4x4_OriginFromMatrix( m_pbonestransform[bone], tmp2 );
		VectorAdd( tmp, tmp2, tmp );

		VectorNormalize( tmp );
		CrossProduct( tmp, RI.vright, chromeupvec );
		VectorNormalize( chromeupvec );
		CrossProduct( tmp, chromeupvec, chromerightvec );
		VectorNormalize( chromerightvec );

		Matrix4x4_Rotate3x3( m_pbonestransform[bone], chromeupvec, g_chromeup[bone] );
		Matrix4x4_Rotate3x3( m_pbonestransform[bone], chromerightvec, g_chromeright[bone] );
		g_chromeage[bone] = m_pStudioModelCount;
	}

	// calc s coord
	n = DotProduct( normal, g_chromeright[bone] );
	pchrome[0] = (n + 1.0) * 32.0f;

	// calc t coord
	n = DotProduct( normal, g_chromeup[bone] );
	pchrome[1] = (n + 1.0) * 32.0f;
}

void R_StudioDrawMesh( const meshbuffer_t *mb, int meshnum, dstudiotexture_t * ptexture, short *pskinref )
{
	int		i;
	float		*av, *lv;
	float		lv_tmp;
	float		s, t;
	short		*ptricmds;
	ref_shader_t	*m_pSkinShader, *shader;
	ref_entity_t	*e = RI.currententity;
	int		flags, features;
	bool		tri_strip, calcNormals, calcSTVectors;
	dstudiomesh_t	*pmesh = (dstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex);
	byte		*pnormbone = ((byte *)m_pStudioHeader + m_pSubModel->norminfoindex);
	vec3_t		*pstudionorms = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->normindex);
	static int	studio_framecount, studio_riparams;
	static dstudiomesh_t	*studio_prevmesh;
	static ref_shader_t		*studio_prevshader;
	static int	max_vertexes = 0;
	int		vertexState = 0;
			
	if( studio_riparams != RI.params || RI.params & RP_SHADOWMAPVIEW )
	{
		studio_riparams = RI.params; // do not try to lock arrays between RI updates
		studio_framecount = !r_framecount;
	}

	lv = (float *)m_pvlightvalues;
	if( meshnum > m_pSubModel->nummesh ) return;

	MB_NUM2SHADER( mb->shaderkey, shader );
	flags = ptexture[pskinref[pmesh[meshnum].skinref]].flags;

	features = MF_NONBATCHED | shader->features;
	if( RI.params & RP_SHADOWMAPVIEW )
	{
		features &= ~( MF_COLORS|MF_SVECTORS|MF_ENABLENORMALS );
		if(!( shader->features & MF_DEFORMVS ))
			features &= ~MF_NORMALS;
	}
	else
	{
		if(( features & MF_SVECTORS ) || r_shownormals->integer )
			features |= MF_NORMALS;
		if( RI.currententity->outlineHeight )
			features |= MF_NORMALS|(GL_Support( R_SHADER_GLSL100_EXT ) ? MF_ENABLENORMALS : 0);
	}

	for( i = 0; i < pmesh[meshnum].numnorms; i++, lv += 3, pstudionorms++, pnormbone++)
	{
		R_StudioLighting (&lv_tmp, *pnormbone, flags, (float *)pstudionorms);
                             
		// FIXME: move this check out of the inner loop
		if( flags & STUDIO_NF_CHROME )
			R_StudioSetupChrome( g_chrome[(float (*)[3])lv - m_pvlightvalues], *pnormbone, (float *)pstudionorms );
		VectorScale(m_plightcolor, lv_tmp, lv );
	}

	pmesh = (dstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + meshnum;
	ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);

	calcNormals = calcSTVectors = false;
	calcNormals = (( features & MF_NORMALS ) != 0 ) && (( RI.currententity->frame != 0 ) || ( RI.currententity->prev.frame != 0 ));

	if( studio_framecount == r_framecount && RI.previousentity && RI.previousentity->model == RI.currententity->model && studio_prevmesh == pmesh && studio_prevshader == shader )
	{
		ref_entity_t *pe = RI.previousentity;
		if( pe->frame == RI.currententity->frame && pe->prev.frame == RI.currententity->prev.frame && ( pe->backlerp == RI.currententity->backlerp || RI.currententity->frame == RI.currententity->prev.frame ))
		{
			calcNormals = ( calcNormals && ( shader->features & SHADER_DEFORM_NORMAL ));
		}
	}

	flags = ptexture[pskinref[pmesh->skinref]].flags;
	s = 1.0 / (float)ptexture[pskinref[pmesh->skinref]].width;
	t = 1.0 / (float)ptexture[pskinref[pmesh->skinref]].height;
	m_pSkinShader = &r_shaders[ptexture[pskinref[pmesh->skinref]].shader];

	studio_prevmesh = pmesh;
	studio_prevshader = shader;
	studio_framecount = r_framecount;

	while( i = *(ptricmds++))
	{
		if( i < 0 )
		{
			i = -i;
			tri_strip = false;
		}
		else tri_strip = true;
		vertexState = 0;
			
		for( ; i > 0; i--, ptricmds += 4 )
		{
			if( flags & STUDIO_NF_CHROME )
				Vector2Set( inCoordsArray[r_backacc.numVerts], g_chrome[ptricmds[1]][0] * s, g_chrome[ptricmds[1]][1] * t );
			else Vector2Set( inCoordsArray[r_backacc.numVerts], ptricmds[2] * s, ptricmds[3] * t );

			lv = m_pvlightvalues[ptricmds[1]];
			//inColorsArray[0][r_backacc.numVerts][0] = (lv[0] * 255);
			//inColorsArray[0][r_backacc.numVerts][1] = (lv[1] * 255);
			//inColorsArray[0][r_backacc.numVerts][2] = (lv[2] * 255);
			//inColorsArray[0][r_backacc.numVerts][3] = 255;
				
			if( vertexState++ < 3 )
			{
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
			}
			else if( tri_strip )
			{
				// flip triangles between clockwise and counter clockwise
				if( vertexState & 1 )
				{
					// draw triangle [n-2 n-1 n]
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 2;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 1;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
				}
				else
				{
					// draw triangle [n-1 n-2 n]
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 1;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 2;
					inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
				}
			}
			else
			{
				// draw triangle fan [0 n-1 n]
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - ( vertexState - 1 );
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts - 1;
				inElemsArray[r_backacc.numElems++] = r_backacc.numVerts;
			}
			av = m_pxformverts[ptricmds[0]]; // verts
			Vector4Set( inVertsArray[r_backacc.numVerts], av[0], av[1], av[2], 1.0f );
			r_backacc.numVerts++;
		}
	}

	if( max_vertexes < r_backacc.numVerts )
	{
		Msg("Draw mesh max %i vertexes\n", r_backacc.numVerts );
		max_vertexes = r_backacc.numVerts;
	}

	studio_mesh.elems = inElemsArray;
	studio_mesh.numElems = r_backacc.numElems;
	studio_mesh.numVertexes = r_backacc.numVerts;
	studio_mesh.stArray = inCoordsArray;
	studio_mesh.xyzArray = inVertsArray;

	R_TranslateForEntity( RI.currententity );
// FIXME!	R_PushMesh( &studio_mesh, features );
	R_RenderMeshBuffer( mb );
}

void R_StudioDrawBones( void )
{

	dstudiobone_t	*pbones = (dstudiobone_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	vec3_t		point;
	int		i;

	for( i = 0; i < m_pStudioHeader->numbones; i++ )
	{
		if( pbones[i].parent >= 0 )
		{
			pglPointSize( 3.0f );
			pglColor3f( 1, 0.7f, 0 );
			pglBegin( GL_LINES );
			
			Matrix4x4_OriginFromMatrix( m_pbonestransform[pbones[i].parent], point );
			pglVertex3fv( point );
			Matrix4x4_OriginFromMatrix( m_pbonestransform[i], point );
			pglVertex3fv( point );
			
			pglEnd();

			pglColor3f( 0, 0, 0.8f );
			pglBegin( GL_POINTS );
			if( pbones[pbones[i].parent].parent != -1 )
			{
				Matrix4x4_OriginFromMatrix( m_pbonestransform[pbones[i].parent], point );
				pglVertex3fv( point );
			}
			Matrix4x4_OriginFromMatrix( m_pbonestransform[i], point );
			pglVertex3fv( point );
			pglEnd();
		}
		else
		{
			// draw parent bone node
			pglPointSize( 5.0f );
			pglColor3f( 0.8f, 0, 0 );
			pglBegin( GL_POINTS );
			Matrix4x4_OriginFromMatrix( m_pbonestransform[i], point );
			pglVertex3fv( point );
			pglEnd();
		}
	}
	pglPointSize( 1.0f );
}

void R_StudioDrawHitboxes( void )
{
	int i, j;

	pglColor4f (1, 0, 0, 0.5f);
	pglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);

	for (i = 0; i < m_pStudioHeader->numhitboxes; i++)
	{
		dstudiobbox_t *pbboxes = (dstudiobbox_t *) ((byte *) m_pStudioHeader + m_pStudioHeader->hitboxindex);
		vec3_t v[8], v2[8], bbmin, bbmax;

		VectorCopy (pbboxes[i].bbmin, bbmin);
		VectorCopy (pbboxes[i].bbmax, bbmax);

		v[0][0] = bbmin[0];
		v[0][1] = bbmax[1];
		v[0][2] = bbmin[2];

		v[1][0] = bbmin[0];
		v[1][1] = bbmin[1];
		v[1][2] = bbmin[2];

		v[2][0] = bbmax[0];
		v[2][1] = bbmax[1];
		v[2][2] = bbmin[2];

		v[3][0] = bbmax[0];
		v[3][1] = bbmin[1];
		v[3][2] = bbmin[2];

		v[4][0] = bbmax[0];
		v[4][1] = bbmax[1];
		v[4][2] = bbmax[2];

		v[5][0] = bbmax[0];
		v[5][1] = bbmin[1];
		v[5][2] = bbmax[2];

		v[6][0] = bbmin[0];
		v[6][1] = bbmax[1];
		v[6][2] = bbmax[2];

		v[7][0] = bbmin[0];
		v[7][1] = bbmin[1];
		v[7][2] = bbmax[2];

		Matrix4x4_Transform (m_pbonestransform[pbboxes[i].bone], v[0], v2[0]);
		Matrix4x4_Transform (m_pbonestransform[pbboxes[i].bone], v[1], v2[1]);
		Matrix4x4_Transform (m_pbonestransform[pbboxes[i].bone], v[2], v2[2]);
		Matrix4x4_Transform (m_pbonestransform[pbboxes[i].bone], v[3], v2[3]);
		Matrix4x4_Transform (m_pbonestransform[pbboxes[i].bone], v[4], v2[4]);
		Matrix4x4_Transform (m_pbonestransform[pbboxes[i].bone], v[5], v2[5]);
		Matrix4x4_Transform (m_pbonestransform[pbboxes[i].bone], v[6], v2[6]);
		Matrix4x4_Transform (m_pbonestransform[pbboxes[i].bone], v[7], v2[7]);

		pglBegin( GL_QUAD_STRIP );
		for (j = 0; j < 10; j++) pglVertex3fv (v2[j & 7]);
		pglEnd( );
	
		pglBegin( GL_QUAD_STRIP );
		pglVertex3fv (v2[6]);
		pglVertex3fv (v2[0]);
		pglVertex3fv (v2[4]);
		pglVertex3fv (v2[2]);
		pglEnd( );

		pglBegin( GL_QUAD_STRIP );
		pglVertex3fv (v2[1]);
		pglVertex3fv (v2[7]);
		pglVertex3fv (v2[3]);
		pglVertex3fv (v2[5]);
		pglEnd( );			
	}

	pglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
}

void R_StudioDrawAttachments( void )
{
	int i;
	
	for (i = 0; i < m_pStudioHeader->numattachments; i++)
	{
		dstudioattachment_t *pattachments = (dstudioattachment_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
		vec3_t v[4];
		
		Matrix4x4_Transform (m_pbonestransform[pattachments[i].bone], pattachments[i].org, v[0]);
		Matrix4x4_Transform (m_pbonestransform[pattachments[i].bone], pattachments[i].vectors[0], v[1]);
		Matrix4x4_Transform (m_pbonestransform[pattachments[i].bone], pattachments[i].vectors[1], v[2]);
		Matrix4x4_Transform (m_pbonestransform[pattachments[i].bone], pattachments[i].vectors[2], v[3]);
		
		pglBegin (GL_LINES);
		pglColor3f (1, 0, 0);
		pglVertex3fv (v[0]);
		pglColor3f (1, 1, 1);
		pglVertex3fv (v[1]);
		pglColor3f (1, 0, 0);
		pglVertex3fv (v[0]);
		pglColor3f (1, 1, 1);
		pglVertex3fv (v[2]);
		pglColor3f (1, 0, 0);
		pglVertex3fv (v[0]);
		pglColor3f (1, 1, 1);
		pglVertex3fv (v[3]);
		pglEnd ();

		pglPointSize (5.0f);
		pglColor3f (0, 1, 0);
		pglBegin (GL_POINTS);
		pglVertex3fv (v[0]);
		pglEnd ();
		pglPointSize (1.0f);
	}
}

void R_StudioDrawHulls ( void )
{
	int i;
	vec3_t		bbox[8];

	// we already have code for drawing hulls
	// make this go away

	if( RI.currententity->ent_type == ED_VIEWMODEL )
		return;

	if(!R_StudioComputeBBox( bbox )) return;

	pglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
	pglBegin( GL_LINES );
	for( i = 0; i < 2; i += 1 )
	{
		pglVertex3fv(bbox[i+0]);
		pglVertex3fv(bbox[i+2]);
		pglVertex3fv(bbox[i+4]);
		pglVertex3fv(bbox[i+6]);
		pglVertex3fv(bbox[i+0]);
		pglVertex3fv(bbox[i+4]);
		pglVertex3fv(bbox[i+2]);
		pglVertex3fv(bbox[i+6]);
		pglVertex3fv(bbox[i*2+0]);
		pglVertex3fv(bbox[i*2+1]);
		pglVertex3fv(bbox[i*2+4]);
		pglVertex3fv(bbox[i*2+5]);
	}
	pglEnd();
}

void R_StudioDrawShadow ( void )
{
	float an = RI.currententity->angles[1] / 180 * M_PI;
	
	m_pshadevector[0] = cos(-an);
	m_pshadevector[1] = sin(-an);
	m_pshadevector[2] = 1;
	VectorNormalize( m_pshadevector );
}

void R_StudioSetRemapColors( int m_topColor, int m_bottomColor )
{
	// FIXME: get some code from q1
}

/*
====================
StudioRenderModel

====================
*/
void R_StudioRenderModel( void )
{
	if( !RI.refdef.onlyClientDraw )
	{
		if( r_drawentities->integer < 2 )
			return;
		
		GL_SetState( GLSTATE_NO_DEPTH_TEST );

		pglDepthRange( 0, 0 );
		switch( r_drawentities->integer )
		{
		case 2: R_StudioDrawBones(); break;
		case 3: R_StudioDrawHitboxes(); break;
		case 4: R_StudioDrawAttachments(); break;
		case 5: R_StudioDrawHulls(); break;
		}
		pglDepthRange( 0, 1 );
	}
}

static void R_StudioSetupRender( ref_entity_t *e, ref_model_t *mod )
{
	// set global pointers
	m_pStudioHeader = mod->phdr;
          m_pTextureHeader = mod->thdr;

	// set intermediate vertex buffers
 	m_pvlightvalues = &g_lightvalues[0];

	// misc info
	m_fDoInterp = (RI.currententity->flags & EF_NOINTERP) ? false : true;
}

/*
====================
StudioDrawModel

====================
*/
bool R_StudioDrawModel( ref_entity_t *e, int flags )
{
	float	curframe = 0.0f;

	if( e->ent_type == ED_VIEWMODEL )
		flags |= STUDIO_EVENTS;

	R_StudioSetUpTransform ( e );

	if( flags & STUDIO_RENDER )
	{
		m_pStudioModelCount++; // render data cache cookie

		// nothing to draw
		if( m_pStudioHeader->numbodyparts == 0 )
			return 1;
	}

	if( e->movetype == MOVETYPE_FOLLOW )
		curframe = R_StudioMergeBones( e, e->model );
	else curframe = R_StudioSetupBones( e );
	R_StudioSaveBones( e );	

	if( flags & STUDIO_EVENTS )
	{
		edict_t		*ent = ri.GetClientEdict( e->index );
		float		flStart = curframe + e->framerate;
		float		flEnd = flStart + 0.4f;
		int		index = 0;
		dstudioevent_t	event;

		R_StudioCalcAttachments( e );
		Mem_Set( &event, 0, sizeof( event ));

		// copy attachments back to client entity
		Mem_Copy( ent->v.attachment, e->attachment, sizeof( vec3_t ) * MAXSTUDIOATTACHMENTS );
		while(( index = R_StudioGetEvent( e, &event, flStart, flEnd, index )) != 0 )
		{
			ri.StudioEvent( &event, ent );
		}
	}
	return 1;
	//////// AFTER THIS POINT GO STUDIO MODEL DRAW ///////
	//////// SAVE BONES INTO LOCAL ARRAY /////////////////

	if( flags & STUDIO_RENDER )
	{
		R_StudioSetupLighting( );
	
		// get remap colors
		m_nTopColor = RI.currententity->colormap & 0xFF;
		m_nBottomColor = (RI.currententity->colormap & 0xFF00)>>8;

		R_StudioSetRemapColors( m_nTopColor, m_nBottomColor );
		R_StudioRenderModel( );

		// draw weaponmodel for monsters
		if( RI.currententity->weaponmodel )
		{
			ref_entity_t saveent = *RI.currententity;
			ref_model_t *pweaponmodel = RI.currententity->weaponmodel;

			// get remap colors
			m_nTopColor = RI.currententity->colormap & 0xFF;
			m_nBottomColor = (RI.currententity->colormap & 0xFF00)>>8;
			R_StudioSetRemapColors( m_nTopColor, m_nBottomColor );
		
			m_pStudioHeader = pweaponmodel->phdr;
			m_pTextureHeader = pweaponmodel->thdr;
			R_StudioMergeBones( e, pweaponmodel );
			R_StudioSetupLighting( );

			R_StudioRenderModel( );
			R_StudioCalcAttachments( e );
			*RI.currententity = saveent;
		}
	}
	return 1;
}

/*
====================
StudioEstimateGait

====================
*/
void R_StudioEstimateGait( ref_entity_t *e, edict_t *pplayer )
{
	float	dt;
	vec3_t	est_velocity;

	dt = ( RI.refdef.time - RI.refdef.oldtime );
	if( dt < 0 ) dt = 0.0f;
	else if ( dt > 1.0 ) dt = 1.0f;

	if( dt == 0 || e->renderframe == r_framecount )
	{
		m_flGaitMovement = 0;
		return;
	}

	// VectorAdd( pplayer->v.velocity, pplayer->v.prediction_error, est_velocity );
	if( m_fGaitEstimation )
	{
		VectorSubtract( e->origin, e->prev.gaitorigin, est_velocity );
		VectorCopy( e->origin, e->prev.gaitorigin );

		m_flGaitMovement = VectorLength( est_velocity );
		if( dt <= 0 || m_flGaitMovement / dt < 5 )
		{
			m_flGaitMovement = 0;
			est_velocity[0] = 0;
			est_velocity[1] = 0;
		}
	}
	else
	{
		VectorCopy( pplayer->v.velocity, est_velocity );
		m_flGaitMovement = VectorLength( est_velocity ) * dt;
	}

	if( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		float flYawDiff = e->angles[YAW] - e->gaityaw;

		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if( flYawDiff > 180 ) flYawDiff -= 360;
		if( flYawDiff < -180 ) flYawDiff += 360;

		if( dt < 0.25 ) flYawDiff *= dt * 4;
		else flYawDiff *= dt;

		e->gaityaw += flYawDiff;
		e->gaityaw = e->gaityaw - (int)(e->gaityaw / 360) * 360;
		m_flGaitMovement = 0;
	}
	else
	{
		e->gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if( e->gaityaw > 180 ) e->gaityaw = 180;
		if( e->gaityaw < -180 ) e->gaityaw = -180;
	}

}

/*
====================
StudioProcessGait

====================
*/
void R_StudioProcessGait( ref_entity_t *e, edict_t *pplayer )
{
	dstudioseqdesc_t	*pseqdesc;
	float		dt, flYaw;	// view direction relative to movement
	float		fBlend;

	if( e->sequence >= m_pStudioHeader->numseq ) 
		e->sequence = 0;

	pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + e->sequence;
	R_StudioPlayerBlend( pseqdesc, &fBlend, &e->angles[PITCH] );

	e->prev.angles[PITCH] = e->angles[PITCH];
	e->blending[0] = fBlend;
	e->prev.blending[0] = e->blending[0];
	e->prev.seqblending[0] = e->blending[0];

	// MsgDev( D_INFO, "%f %d\n", e->angles[PITCH], e->blending[0] );

	dt = (RI.refdef.time - RI.refdef.oldtime);
	if( dt < 0 ) dt = 0.0f;
	else if( dt > 1.0 ) dt = 1.0f;

	R_StudioEstimateGait( e, pplayer );

	// MsgDev( D_INFO, "%f %f\n", e->angles[YAW], m_pPlayerInfo->gaityaw );

	// calc side to side turning
	flYaw = e->angles[YAW] - e->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if( flYaw < -180 ) flYaw = flYaw + 360;
	if( flYaw > 180 ) flYaw = flYaw - 360;

	if( flYaw > 120 )
	{
		e->gaityaw = e->gaityaw - 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw - 180;
	}
	else if( flYaw < -120 )
	{
		e->gaityaw = e->gaityaw + 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	e->controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	e->controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	e->controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	e->controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	e->prev.controller[0] = e->controller[0];
	e->prev.controller[1] = e->controller[1];
	e->prev.controller[2] = e->controller[2];
	e->prev.controller[3] = e->controller[3];

	e->angles[YAW] = e->gaityaw;
	if( e->angles[YAW] < -0 ) e->angles[YAW] += 360;
	e->prev.angles[YAW] = e->angles[YAW];

	if( pplayer->v.gaitsequence >= m_pStudioHeader->numseq ) 
		pplayer->v.gaitsequence = 0;

	pseqdesc = (dstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + pplayer->v.gaitsequence;

	// calc gait frame
	if( pseqdesc->linearmovement[0] > 0 )
	{
		e->gaitframe += (m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		e->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	e->gaitframe = e->gaitframe - (int)(e->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if( e->gaitframe < 0 ) e->gaitframe += pseqdesc->numframes;
}

/*
====================
StudioDrawPlayer

====================
*/
int R_StudioDrawPlayer( ref_entity_t *e, int flags )
{
	edict_t	*pplayer;
	float	curframe;

	pplayer = ri.GetClientEdict( e->index );

	// MsgDev( D_INFO, "DrawPlayer %d\n", e->blending[0] );
	// MsgDev( D_INFO, "DrawPlayer %d %d (%d)\n", r_framecount, pplayer->serialnumber, e->sequence );
	// MsgDev( D_INFO, "Player %.2f %.2f %.2f\n", pplayer->v.velocity[0], pplayer->v.velocity[1], pplayer->v.velocity[2] );

	if( pplayer->serialnumber < 0 || pplayer->serialnumber > ri.GetMaxClients())
		return 0;

	if( pplayer->v.gaitsequence > 0 )
	{
		vec3_t	orig_angles;

		VectorCopy( e->angles, orig_angles );
		R_StudioProcessGait( e, pplayer );

		e->gaitsequence = pplayer->v.gaitsequence;
		R_StudioSetUpTransform( e );
		VectorCopy( orig_angles, e->angles );
	}
	else
	{
		e->controller[0] = 127;
		e->controller[1] = 127;
		e->controller[2] = 127;
		e->controller[3] = 127;
		e->prev.controller[0] = e->controller[0];
		e->prev.controller[1] = e->controller[1];
		e->prev.controller[2] = e->controller[2];
		e->prev.controller[3] = e->controller[3];
		e->gaitsequence = 0;
		R_StudioSetUpTransform( e );
	}

	if( flags & STUDIO_RENDER )
	{
		m_pStudioModelCount++; // render data cache cookie

		// nothing to draw
		if( m_pStudioHeader->numbodyparts == 0 )
			return 1;
	}

	curframe = R_StudioSetupBones( e );
	R_StudioSaveBones( e );
	e->renderframe = r_framecount;

	if( flags & STUDIO_EVENTS )
	{
		edict_t		*ent = ri.GetClientEdict( e->index );
		float		flStart = curframe + e->framerate;
		float		flEnd = flStart + 0.4f;
		int		index = 0;
		dstudioevent_t	event;

		R_StudioCalcAttachments( e );
		Mem_Set( &event, 0, sizeof( event ));

		// copy attachments back to client entity
		Mem_Copy( ent->v.attachment, e->attachment, sizeof( vec3_t ) * MAXSTUDIOATTACHMENTS );
		while(( index = R_StudioGetEvent( e, &event, flStart, flEnd, index )) != 0 )
		{
			ri.StudioEvent( &event, ent );
		}
	}

	return 1;
	//////// AFTER THIS POINT GO STUDIO MODEL DRAW ///////
	//////// SAVE BONES INTO LOCAL ARRAY /////////////////

	if( flags & STUDIO_RENDER )
	{
		// show highest resolution multiplayer model
		if( r_himodels->integer && RI.currentmodel != e->model  )
			e->body = 255;

		if(!(glw_state.developer == 0 && ri.GetMaxClients() == 1 ) && ( RI.currentmodel == e->model ))
			e->body = 1; // force helmet

		R_StudioSetupLighting( );

		// get remap colors
		m_nTopColor = e->colormap & 0xFF;
		m_nBottomColor = (e->colormap & 0xFF00)>>8;
		
		if( m_nTopColor < 0 ) m_nTopColor = 0;
		if( m_nTopColor > 360 ) m_nTopColor = 360;
		if( m_nBottomColor < 0 ) m_nBottomColor = 0;
		if( m_nBottomColor > 360 ) m_nBottomColor = 360;

		R_StudioSetRemapColors( m_nTopColor, m_nBottomColor );

		R_StudioRenderModel( );

		if( e->weaponmodel )
		{
			ref_entity_t saveent = *RI.currententity;
			ref_model_t *pweaponmodel = e->weaponmodel;
			// get remap colors
			m_nTopColor = e->colormap & 0xFF;
			m_nBottomColor = (e->colormap & 0xFF00)>>8;
			R_StudioSetRemapColors( m_nTopColor, m_nBottomColor );

			m_pStudioHeader = pweaponmodel->phdr;
			m_pTextureHeader = pweaponmodel->thdr;
			R_StudioMergeBones( e, pweaponmodel );
			R_StudioSetupLighting( );

			R_StudioRenderModel( );
			R_StudioCalcAttachments( e );
			*RI.currententity = saveent;
		}
	}
	return 1;
}

void R_StudioDrawPoints( const meshbuffer_t *mb )
{
	int		i, m_skinnum = RI.currententity->skin;
	int		modelnum;
	int		meshnum;
	int		infokey = -mb->infokey - 1;
	byte		*pvertbone;
	byte		*pnormbone;
	vec3_t		*pstudioverts;
	vec3_t		*pstudionorms;
	ref_entity_t	*e = RI.currententity;
	dstudiotexture_t	*ptexture;
	short		*pskinref;
	static int	old_model;
	static int	old_submodel;
	static ref_entity_t	*old_entity;
	static int	studio_framecount = 0;
	bool		doTransform = false;
	bool		doBonesSetup = false;

	modelnum = (infokey & 0xFF);
	meshnum = ((infokey & 0xFF00)>>8);

	if( studio_framecount != r_framecount )
	{
		// invalidate at new frame to takes update
		old_model = old_submodel = 0;
		old_entity = NULL;
		studio_framecount = r_framecount;
	}

	R_StudioSetupRender( e, Mod_ForHandle( mb->LODModelHandle ));
	if( m_pStudioHeader->numbodyparts == 0 ) return; // nothing to draw

	if( mb->LODModelHandle != old_model || old_entity != RI.currententity )
	{
		Mem_Set( g_cachestate, 0, sizeof( g_cachestate )); // model has changed - clear the cache
		doBonesSetup = doTransform = true;
	}

	if( modelnum != old_submodel )
	{
		old_submodel = modelnum;
		doTransform = true;
	}

	if( doBonesSetup )
	{
		// first we need to precalculate all bones for model and place result into entity->extradata
		// also we can pre-calc attachments and events if need
		if( e->weaponmodel && Mod_ForHandle( mb->LODModelHandle ) == e->weaponmodel )
		{ 
			// Damn! R_SortMeshes insert between model and weaponmodel something else
			if( old_entity != RI.currententity )
			{
				R_StudioSetupRender( e, e->model );
				if( e->ent_type == ED_CLIENT )
					R_StudioDrawPlayer( e, STUDIO_RENDER );
				else R_StudioDrawModel( e, STUDIO_RENDER );
			}
			R_StudioSetupRender( e, Mod_ForHandle( mb->LODModelHandle ));
			R_StudioMergeBones( e, Mod_ForHandle( mb->LODModelHandle ));
			R_StudioCalcAttachments( e );
		}
		else
		{
			R_StudioSetupRender( e, Mod_ForHandle( mb->LODModelHandle ));
			if( e->ent_type == ED_CLIENT )
				R_StudioDrawPlayer( e, STUDIO_RENDER );
			else R_StudioDrawModel( e, STUDIO_RENDER );
		}
		if( mb->LODModelHandle != old_model ) old_model = mb->LODModelHandle;
		if( old_entity != RI.currententity ) old_entity = RI.currententity;
	}

	R_StudioSetupModel( RI.currententity->body, modelnum );

	pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	pnormbone = ((byte *)m_pStudioHeader + m_pSubModel->norminfoindex);
	ptexture = (dstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);

	pstudioverts = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	pstudionorms = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->normindex);

	pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
	if( m_skinnum != 0 && m_skinnum < m_pTextureHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pTextureHeader->numskinref);

	if( doTransform )
	{
		m_pxformverts = g_xformverts[modelnum];

		if( !g_cachestate[modelnum] )
		{
			for( i = 0; i < m_pSubModel->numverts; i++ )
				Matrix4x4_Transform( m_pbonestransform[pvertbone[i]], pstudioverts[i], m_pxformverts[i]);
			g_cachestate[modelnum] = true;
		}
	}
	R_StudioDrawMesh( mb, meshnum, ptexture, pskinref );
}

void R_DrawStudioModel( const meshbuffer_t *mb )
{
	ref_entity_t	*e = RI.currententity;
	int		meshnum = -mb->infokey - 1;

	if( OCCLUSION_QUERIES_ENABLED( RI ) && OCCLUSION_TEST_ENTITY( e ))
	{
		ref_shader_t	*shader;

		MB_NUM2SHADER( mb->shaderkey, shader );
		if( !R_GetOcclusionQueryResultBool( shader->type == 1 ? OQ_PLANARSHADOW : OQ_ENTITY, e - r_entities, true ))
			return;
	}

	// hack the depth range to prevent view model from poking into walls
	if( e->ent_type == ED_VIEWMODEL )
	{
		pglDepthRange( gldepthmin, gldepthmin + 0.3 * ( gldepthmax - gldepthmin ) );

		// backface culling for left-handed weapons
		if( r_lefthand->integer == 1 )
			GL_FrontFace( !glState.frontFace );
          }

	R_StudioDrawPoints( mb );

	if( e->ent_type == ED_VIEWMODEL )
	{
		pglDepthRange( gldepthmin, gldepthmax );

		// backface culling for left-handed weapons
		if( r_lefthand->integer == 1 )
			GL_FrontFace( !glState.frontFace );
	}
}

bool R_CullStudioModel( ref_entity_t *e )
{
	int		i, j, clipped;
	bool		frustum, query;
	uint		modhandle;
	dstudiotexture_t	*ptexture;
	short		*pskinref;
	meshbuffer_t	*mb;
	
	R_StudioSetupRender( e, e->model );

	if( !e->model->phdr )
		return true;

	modhandle = Mod_Handle( e->model );
	if(!R_ExtractBbox( e->sequence, studio_mins, studio_maxs ))
		return true; // invalid sequence
	
	studio_radius = RadiusFromBounds( studio_mins, studio_maxs );
	clipped = R_CullModel( e, studio_mins, studio_maxs, studio_radius );
	frustum = clipped & 1;
	if( clipped & 2 )
		return true;

	query = OCCLUSION_QUERIES_ENABLED( RI ) && OCCLUSION_TEST_ENTITY( e ) ? true : false;
	if( !frustum && query ) R_IssueOcclusionQuery( R_GetOcclusionQueryNum( OQ_ENTITY, e - r_entities ), e, studio_mins, studio_maxs );

	if((RI.refdef.rdflags & RDF_NOWORLDMODEL) || (r_shadows->integer != 1 && !(r_shadows->integer == 2 && (e->flags & EF_PLANARSHADOW))) || R_CullPlanarShadow( e, studio_mins, studio_maxs, query ))
		return frustum; // entity is not in PVS or shadow is culled away by frustum culling

	// add it
	ptexture = (dstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);
	for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		R_StudioSetupModel( RI.currententity->body, i );
		pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
		if( e->skin != 0 && e->skin < m_pTextureHeader->numskinfamilies )
			pskinref += (e->skin * m_pTextureHeader->numskinref);

		for( j = 0; j < m_pSubModel->nummesh; j++ ) 
		{
			ref_shader_t	*shader;
			dstudiomesh_t	*pmesh;
	
			pmesh = (dstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
			shader = &r_shaders[ptexture[pskinref[pmesh->skinref]].shader];

			if( shader && ( shader->sort <= SORT_ALPHATEST ))
			{
				mb = R_AddMeshToList( MB_MODEL, NULL, R_PlanarShadowShader(), -(((j<<8)|i)+1 ));
				if( mb ) mb->LODModelHandle = modhandle;
			}
		}
	}

	if( e->weaponmodel )
	{
		// add weaponmodel
		modhandle = Mod_Handle( e->weaponmodel );
		R_StudioSetupRender( e, e->weaponmodel );
		ptexture = (dstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);
		for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
		{
			R_StudioSetupModel( e->body, i );
			pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
			if( e->skin != 0 && e->skin < m_pTextureHeader->numskinfamilies )
				pskinref += (e->skin * m_pTextureHeader->numskinref);

			for( j = 0; j < m_pSubModel->nummesh; j++ ) 
			{
				ref_shader_t	*shader;
				dstudiomesh_t	*pmesh;
	
				pmesh = (dstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
				shader = &r_shaders[ptexture[pskinref[pmesh->skinref]].shader];
				if( shader && ( shader->sort <= SORT_ALPHATEST ))
				{
					mb = R_AddMeshToList( MB_MODEL, NULL, R_PlanarShadowShader(), -(((j<<8)|i)+1 ));
					if( mb ) mb->LODModelHandle = modhandle;
				}
			}
		}
	}

	return frustum;
}

void R_AddStudioModelToList( ref_entity_t *e )
{
	uint		modhandle;
	uint		entnum = e - r_entities;
	ref_model_t	*mod = e->model;
	mfog_t		*fog = NULL;
	dstudiotexture_t	*ptexture;
	short		*pskinref;
	int		i, j;

	R_StudioSetupRender( e, e->model );
	if( !e->model->phdr ) return;

	if(!R_ExtractBbox( e->sequence, studio_mins, studio_maxs )) return; // invalid sequence
	studio_radius = RadiusFromBounds( studio_mins, studio_maxs );
	modhandle = Mod_Handle( mod );

	if( RI.params & RP_SHADOWMAPVIEW )
	{
		if( r_entShadowBits[entnum] & RI.shadowGroup->bit )
		{
			if( !r_shadows_self_shadow->integer )
				r_entShadowBits[entnum] &= ~RI.shadowGroup->bit;
			if( e->ent_type == ED_VIEWMODEL )
				return;
		}
		else
		{
//			R_StudioModelLerpBBox( e, mod );
			if( !R_CullModel( e, studio_mins, studio_maxs, studio_radius ))
				r_entShadowBits[entnum] |= RI.shadowGroup->bit;
			return; // mark as shadowed, proceed with caster otherwise
		}
	}
	else
	{
		fog = R_FogForSphere( e->origin, studio_radius );
#if 0
		if( !( e->ent_type == ED_VIEWMODEL ) && fog )
		{
//			R_StudioModelLerpBBox( e, mod );
			if( R_CompletelyFogged( fog, e->origin, studio_radius ))
				return;
		}
#endif
	}

	// add base model
	ptexture = (dstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);
	for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
	{
		R_StudioSetupModel( e->body, i );
		pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
		if( e->skin != 0 && e->skin < m_pTextureHeader->numskinfamilies )
			pskinref += (e->skin * m_pTextureHeader->numskinref);

		for( j = 0; j < m_pSubModel->nummesh; j++ ) 
		{
			ref_shader_t	*shader;
			dstudiomesh_t	*pmesh;
	
			pmesh = (dstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
			shader = &r_shaders[ptexture[pskinref[pmesh->skinref]].shader];
			if( shader ) R_AddModelMeshToList( modhandle, fog, shader, ((j<<8)|i));
		}
	}
			
	if( e->weaponmodel )
	{
		// add weaponmodel
		modhandle = Mod_Handle( e->weaponmodel );
		R_StudioSetupRender( e, e->weaponmodel );
		ptexture = (dstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);
		for( i = 0; i < m_pStudioHeader->numbodyparts; i++ )
		{
			R_StudioSetupModel( e->body, i );
			pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
			if( e->skin != 0 && e->skin < m_pTextureHeader->numskinfamilies )
				pskinref += (e->skin * m_pTextureHeader->numskinref);

			for( j = 0; j < m_pSubModel->nummesh; j++ ) 
			{
				ref_shader_t	*shader;
				dstudiomesh_t	*pmesh;
	
				pmesh = (dstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
				shader = &r_shaders[ptexture[pskinref[pmesh->skinref]].shader];
				if( shader ) R_AddModelMeshToList( modhandle, fog, shader, ((j<<8)|i));
			}
		}
	}

	R_StudioAddEntityToRadar( e );
}