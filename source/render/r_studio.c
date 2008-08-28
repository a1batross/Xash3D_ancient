//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_studio.c - render studio models
//=======================================================================

#include "r_local.h"
#include "byteorder.h"
#include "mathlib.h"
#include "matrixlib.h"
#include "const.h"

/*
=============================================================

  STUDIO MODELS

=============================================================
*/

#define STUDIO_API_VERSION		0.2

matrix4x4 m_pbonestransform [MAXSTUDIOBONES];
matrix4x4 m_plighttransform [MAXSTUDIOBONES];
matrix4x4 m_protationmatrix;
vec3_t m_plightcolor;		// ambient light color
vec3_t m_plightvec;			// ambleint light vector
vec3_t m_pshadevector;		// shadow vector

// lighting stuff
vec3_t *m_pxformverts;
vec3_t *m_pxformnorms;
vec3_t *m_pvlightvalues;
vec3_t m_blightvec [MAXSTUDIOBONES];
vec3_t g_xformverts[MAXSTUDIOVERTS];
vec3_t g_xformnorms[MAXSTUDIOVERTS];
vec3_t g_lightvalues[MAXSTUDIOVERTS];

// chrome stuff
float g_chrome[MAXSTUDIOVERTS][2];	// texture coords for surface normals
int g_chromeage[MAXSTUDIOBONES];	// last time chrome vectors were updated
vec3_t g_chromeup[MAXSTUDIOBONES];	// chrome vector "up" in bone reference frames
vec3_t g_chromeright[MAXSTUDIOBONES];	// chrome vector "right" in bone reference frames

int m_fDoInterp;			
int m_pStudioModelCount;
int m_PassNum;
int m_nTopColor;			// palette substition for top and bottom of model			
int m_nBottomColor;
rmodel_t *m_pRenderModel;
ref_entity_t *m_pCurrentEntity;
mstudiomodel_t *m_pSubModel;
studiohdr_t *m_pStudioHeader;
studiohdr_t *m_pTextureHeader;
mstudiobodyparts_t *m_pBodyPart;

int m_nCachedBones;			// number of bones in cache
char m_nCachedBoneNames[MAXSTUDIOBONES][32];
matrix4x4 m_rgCachedBonesTransform [MAXSTUDIOBONES];
matrix4x4 m_rgCachedLightTransform [MAXSTUDIOBONES];

// sprite model used for drawing studio model chrome
rmodel_t *m_pChromeSprite;

// player gait sequence stuff
int m_fGaitEstimation;
float m_flGaitMovement;
	
/*
====================
R_StudioInit

====================
*/
void R_StudioInit( void )
{
	m_pBodyPart = NULL;
	m_pRenderModel  = NULL;
	m_pStudioHeader = NULL;
          m_pCurrentEntity = NULL;
	m_flGaitMovement = 1;
}

void R_StudioShutdown( void )
{
}

/*
====================
MISC STUDIO UTILS
====================
*/
// extract texture filename from modelname
char *R_ExtName( rmodel_t *mod )
{
	static char texname[MAX_QPATH];
	com.strcpy( texname, mod->name );
	com.strcpy( &texname[com.strlen(texname) - 4], "T.mdl" );
	return texname;
}

int R_StudioExtractBbox( studiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if( sequence == -1 ) return 0;
	
	VectorCopy( pseqdesc[sequence].bbmin, mins );
	VectorCopy( pseqdesc[sequence].bbmax, maxs );

	return 1;
}

/*
====================
Studio model loader
====================
*/
texture_t *R_StudioLoadTexture( rmodel_t *mod, mstudiotexture_t *ptexture, byte *pin )
{
	rgbdata_t	r_skin;
	texture_t	*image;

	r_skin.width = ptexture->width;
          r_skin.height = ptexture->height;
	r_skin.flags = (ptexture->flags & STUDIO_NF_TRANSPARENT) ? IMAGE_HAS_ALPHA : 0;
	r_skin.type = PF_INDEXED_24;
	r_skin.numMips = 1;
	r_skin.palette = pin + ptexture->width * ptexture->height + ptexture->index;
	r_skin.buffer = pin + ptexture->index; // texdata
	r_skin.size = ptexture->width * ptexture->height; // for bounds cheking
		
	// load studio texture and bind it
	image = R_LoadTexture( ptexture->name, &r_skin, 0, 0 );
	if( !image ) 
	{
		MsgDev( D_WARN, "%s has null texture %s\n", mod->name, ptexture->name );
		image = r_defaultTexture;
	}
	ptexture->index = mod->numTextures++; // internal texture index, not gl_texturenum

	return image;
}

studiohdr_t *R_StudioLoadHeader( rmodel_t *mod, const uint *buffer )
{
	int		i;
	byte		*pin;
	studiohdr_t	*phdr;
	mstudiotexture_t	*ptexture;
	texture_t		*in;
	mipTex_t		*out;
	
	pin = (byte *)buffer;
	phdr = (studiohdr_t *)pin;

	if( phdr->version != STUDIO_VERSION )
	{
		Msg("%s has wrong version number (%i should be %i)\n", phdr->name, phdr->version, STUDIO_VERSION);
		return NULL;
	}	

	ptexture = (mstudiotexture_t *)(pin + phdr->textureindex);
	if( phdr->textureindex > 0 && phdr->numtextures <= MAXSTUDIOSKINS )
	{
		out = mod->textures = (mipTex_t *)Mem_Alloc( mod->mempool, phdr->numtextures * sizeof(*out));
		for( i = 0; i < phdr->numtextures; i++, out++ )
		{
			in = R_StudioLoadTexture( mod, &ptexture[i], pin );
			com.strncpy( out->name, ptexture->name, 64 );
			out->width = in->width;
			out->height = in->height;
			out->next = NULL; // animchains not using in studio models
			out->numframes = 1;
			out->image = in;
		}
	}
	return (studiohdr_t *)buffer;
}

void R_StudioLoadModel( rmodel_t *mod, const void *buffer )
{
	studiohdr_t *phdr = R_StudioLoadHeader( mod, buffer );
	studiohdr_t *thdr;
	void *texbuf;
	
	if( !phdr ) return; // there were problems
	mod->phdr = (studiohdr_t *)Mem_Alloc(mod->mempool, LittleLong(phdr->length));
	Mem_Copy( mod->phdr, buffer, LittleLong( phdr->length ));
	
	if( phdr->numtextures == 0 )
	{
		texbuf = FS_LoadFile( R_ExtName( mod ), NULL ); // use buffer again
		if( texbuf ) thdr = R_StudioLoadHeader( mod, texbuf );
		else MsgDev( D_WARN, "textures for %s not found!\n", mod->name ); 

		if( !thdr ) return; // there were problems
		mod->thdr = (studiohdr_t *)Mem_Alloc( mod->mempool, LittleLong( thdr->length ));
          	Mem_Copy( mod->thdr, texbuf, LittleLong( thdr->length ));
		Mem_Free( texbuf );
	}
          else mod->thdr = mod->phdr; // just make link

	R_StudioExtractBbox( phdr, 0, mod->mins, mod->maxs );
	mod->registration_sequence = registration_sequence;
}

// extract bbox from animation
int R_ExtractBbox( int sequence, float *mins, float *maxs )
{
	return R_StudioExtractBbox( m_pStudioHeader, sequence, mins, maxs );
}

void SetBodygroup( int iGroup, int iValue )
{
	int iCurrent;
	mstudiobodyparts_t *m_pBodyPart;

	if( iGroup > m_pStudioHeader->numbodyparts )
		return;

	m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + iGroup;

	if (iValue >= m_pBodyPart->nummodels)
		return;

	iCurrent = (m_pCurrentEntity->body / m_pBodyPart->base) % m_pBodyPart->nummodels;
	m_pCurrentEntity->body = (m_pCurrentEntity->body - (iCurrent * m_pBodyPart->base) + (iValue * m_pBodyPart->base));
}

int R_StudioGetBodygroup( int iGroup )
{
	mstudiobodyparts_t *m_pBodyPart;
	
	if (iGroup > m_pStudioHeader->numbodyparts)
		return 0;

	m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + iGroup;

	if (m_pBodyPart->nummodels <= 1)
		return 0;

	return (m_pCurrentEntity->body / m_pBodyPart->base) % m_pBodyPart->nummodels;
}

void R_StudioAddEntityToRadar( void )
{
	if( r_minimap->value < 2 ) return; 

	if( numRadarEnts >= MAX_RADAR_ENTS ) return;
	if( m_pCurrentEntity->renderfx & RF_VIEWMODEL ) return;

	if( m_pCurrentEntity->ent_type == ED_MONSTER )
	{ 
		Vector4Set(RadarEnts[numRadarEnts].color, 1.0f, 0.0f, 2.0f, 1.0f ); 
	}
	else
	{
		Vector4Set(RadarEnts[numRadarEnts].color, 0.0f, 1.0f, 1.0f, 0.5f ); 
	}
	VectorCopy( m_pCurrentEntity->origin, RadarEnts[numRadarEnts].origin );
	VectorCopy( m_pCurrentEntity->angles, RadarEnts[numRadarEnts].angles );
	numRadarEnts++;
}

/*
====================
R_StudioGetSequenceInfo

used for client animation
====================
*/
void R_StudioGetSequenceInfo( studiohdr_t *hdr, ref_entity_t *ent, float *pflFrameRate, float *pflGroundSpeed )
{
	mstudioseqdesc_t	*pseqdesc;

	if( !hdr ) return;

	if( ent->sequence >= hdr->numseq )
	{
		*pflFrameRate = 0.0;
		*pflGroundSpeed = 0.0;
		return;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)hdr + hdr->seqindex) + ent->sequence;

	if( pseqdesc->numframes > 1 )
	{
		*pflFrameRate = 256 * pseqdesc->fps / (pseqdesc->numframes - 1);
		*pflGroundSpeed = VectorLength( pseqdesc->linearmovement ); 
		*pflGroundSpeed = *pflGroundSpeed * pseqdesc->fps / (pseqdesc->numframes - 1);
	}
	else
	{
		*pflFrameRate = 256.0;
		*pflGroundSpeed = 0.0;
	}
}

int R_StudioGetSequenceFlags( studiohdr_t *hdr, ref_entity_t *ent )
{
	mstudioseqdesc_t	*pseqdesc;

	if( !hdr || ent->sequence >= hdr->numseq )
		return 0;
	
	pseqdesc = (mstudioseqdesc_t *)((byte *)hdr + hdr->seqindex) + (int)ent->sequence;
	return pseqdesc->flags;
}

float R_StudioFrameAdvance( ref_entity_t *ent, float framerate, float flInterval )
{
	if( flInterval == 0.0 )
	{
		flInterval = (r_refdef.time - ent->animtime );
		if( flInterval <= 0.001 )
		{
			ent->animtime = r_refdef.time;
			return 0.0;
		}
	}
	if( !ent->animtime ) flInterval = 0.0;
	
	ent->frame += flInterval * framerate * ent->framerate;
	ent->animtime = r_refdef.time;

	if( ent->frame < 0.0 || ent->frame >= 256.0 ) 
	{
		if( ent->m_fSequenceLoops )
			ent->frame -= (int)(ent->frame / 256.0) * 256.0;
		else ent->frame = (ent->frame < 0.0) ? 0 : 255;
		ent->m_fSequenceFinished = true;
	}
	return flInterval;
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
	mstudiobonecontroller_t *pbonecontroller;
	
	pbonecontroller = (mstudiobonecontroller_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bonecontrollerindex);

	for (j = 0; j < m_pStudioHeader->numbonecontrollers; j++)
	{
		i = pbonecontroller[j].index;

		if( i == STUDIO_MOUTH )
		{
			// FIXME: convert to float
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
			// Msg("%d %d %f : %f\n", m_pCurrentEntity->curstate.controller[j], m_pCurrentEntity->latched.prevcontroller[j], value, dadt );
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
void R_StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *q )
{
	int	j, k;
	vec4_t	q1, q2;
	vec3_t	angle1, angle2;
	mstudioanimvalue_t	*panimvalue;

	for (j = 0; j < 3; j++)
	{
		if (panim->offset[j+3] == 0)
		{
			angle2[j] = angle1[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
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
void R_StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *pos )
{
	int j, k;
	mstudioanimvalue_t	*panimvalue;

	for (j = 0; j < 3; j++)
	{
		pos[j] = pbone->value[j]; // default;
		if (panim->offset[j] != 0)
		{
			panimvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			
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

	if (s < 0) s = 0;
	else if (s > 1.0) s = 1.0;

	s1 = 1.0 - s;

	for (i = 0; i < m_pStudioHeader->numbones; i++)
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
mstudioanim_t *R_StudioGetAnim( rmodel_t *m_pSubModel, mstudioseqdesc_t *pseqdesc )
{
	mstudioseqgroup_t	*pseqgroup;
	byte		*paSequences;
          size_t		filesize;
          byte		*buf;
	
	pseqgroup = (mstudioseqgroup_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqgroupindex) + pseqdesc->seqgroup;
	if( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t *)((byte *)m_pStudioHeader + pseqgroup->data + pseqdesc->animindex);
	paSequences = (void *)m_pSubModel->submodels;

	if( paSequences == NULL )
	{
		MsgDev( D_INFO, "loading %s\n", pseqgroup->name );
		buf = FS_LoadFile (pseqgroup->name, &filesize);
		if( !buf || !filesize )
		{
			MsgDev( D_ERROR, "R_StudioGetAnim: %s not found", pseqgroup->name );
			memset( pseqgroup->name, 0, sizeof(pseqgroup->name));
			return NULL;
		}
                    if( IDSEQGRPHEADER == LittleLong(*(uint *)buf))  //it's sequence group
                    {
			byte		*pin = (byte *)buf;
			mstudioseqgroup_t	*pseqhdr = (mstudioseqgroup_t *)pin;
			
			paSequences = (byte *)Mem_Alloc( m_pSubModel->mempool, filesize );
          		m_pSubModel->submodels = (submodel_t *)paSequences; // just a container
          		Mem_Copy( &paSequences[pseqdesc->seqgroup], buf, filesize );
			Mem_Free( buf );
		}		
	}
	return (mstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup] + pseqdesc->animindex );
}

/*
====================
StudioPlayerBlend

====================
*/
void R_StudioPlayerBlend( mstudioseqdesc_t *pseqdesc, float *pBlend, float *pPitch )
{
	// calc up/down pointing
	*pBlend = (*pPitch * 3);

	if( *pBlend < pseqdesc->blendstart[0] )
	{
		*pPitch -= pseqdesc->blendstart[0] / 3.0;
		*pBlend = 0;
	}
	else if( *pBlend > pseqdesc->blendend[0] )
	{
		*pPitch -= pseqdesc->blendend[0] / 3.0;
		*pBlend = 255;
	}
	else
	{
		if( pseqdesc->blendend[0] - pseqdesc->blendstart[0] < 0.1f ) // catch qc error
			*pBlend = 127;
		else *pBlend = 255 * (*pBlend - pseqdesc->blendstart[0]) / (pseqdesc->blendend[0] - pseqdesc->blendstart[0]);
		*pPitch = 0;
	}
}

/*
====================
StudioSetUpTransform

====================
*/
void R_StudioSetUpTransform( void )
{
	int	i;
	vec3_t	angles, modelpos;

	VectorCopy( m_pCurrentEntity->origin, modelpos );
	VectorCopy( m_pCurrentEntity->angles, angles );

	// TODO: should really be stored with the entity instead of being reconstructed
	// TODO: should use a look-up table
	// TODO: could cache lazily, stored in the entity
	if( m_pCurrentEntity->movetype == MOVETYPE_STEP ) 
	{
		float		d, f = 0;

		// NOTE:  Because we need to interpolate multiplayer characters, the interpolation time limit
		//  was increased to 1.0 s., which is 2x the max lag we are accounting for.
		if(( r_refdef.time < m_pCurrentEntity->animtime + 1.0f ) && ( m_pCurrentEntity->animtime != m_pCurrentEntity->prev.animtime ) )
		{
			f = (r_refdef.time - m_pCurrentEntity->animtime) / (m_pCurrentEntity->animtime - m_pCurrentEntity->prev.animtime);
			Msg( "%4.2f %.2f %.2f\n", f, m_pCurrentEntity->animtime, r_refdef.time );
		}

		// calculate frontlerp value
		if( m_fDoInterp ) f = 1.0 - m_pCurrentEntity->backlerp;
		else f = 0;

		for( i = 0; i < 3; i++ )
			modelpos[i] += (m_pCurrentEntity->origin[i] - m_pCurrentEntity->prev.origin[i]) * f;

		for( i = 0; i < 3; i++ )
		{
			float ang1, ang2;

			ang1 = m_pCurrentEntity->angles[i];
			ang2 = m_pCurrentEntity->prev.angles[i];

			d = ang1 - ang2;
			if( d > 180 ) d -= 360;
			else if( d < -180 ) d += 360;
			angles[i] += d * f;
		}
	}
	else if( m_pCurrentEntity->ent_type == ED_CLIENT )
	{
		// don't rotate player model, only aim
		angles[PITCH] = 0;
	}
	else if( m_pCurrentEntity->movetype != MOVETYPE_NONE ) 
	{
		VectorCopy( m_pCurrentEntity->angles, angles );
	}
	Matrix4x4_CreateFromEntity( m_protationmatrix, modelpos[0], modelpos[1], modelpos[2], angles[PITCH], angles[YAW], angles[ROLL], m_pCurrentEntity->scale );
}


/*
====================
StudioEstimateInterpolant

====================
*/
float R_StudioEstimateInterpolant( void )
{
	float dadt = 1.0;

	if ( m_fDoInterp && ( m_pCurrentEntity->animtime >= m_pCurrentEntity->prev.animtime + 0.01 ) )
	{
		dadt = (r_refdef.time - m_pCurrentEntity->animtime) / 0.1;
		if( dadt > 2.0 ) dadt = 2.0;
	}
	return dadt;
}


/*
====================
StudioCalcRotations

====================
*/
void R_StudioCalcRotations( float pos[][3], vec4_t *q, mstudioseqdesc_t *pseqdesc, mstudioanim_t *panim, float f )
{
	int		i;
	int		frame;
	mstudiobone_t	*pbone;

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

	// Msg("%d %.4f %.4f %.4f %.4f %d\n", m_pCurrentEntity->curstate.sequence, m_clTime, m_pCurrentEntity->animtime, m_pCurrentEntity->frame, f, frame );
	// Msg( "%f %f %f\n", m_pCurrentEntity->angles[ROLL], m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->angles[YAW] );
	// Msg("frame %d %d\n", frame1, frame2 );

	dadt = R_StudioEstimateInterpolant();
	s = (f - frame);

	// add in programtic controllers
	pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	R_StudioCalcBoneAdj( dadt, adj, m_pCurrentEntity->controller, m_pCurrentEntity->prev.controller, m_pCurrentEntity->mouth.open );

	for (i = 0; i < m_pStudioHeader->numbones; i++, pbone++, panim++) 
	{
		R_StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );
		R_StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
		// if (0 && i == 0) Msg("%d %d %d %d\n", m_pCurrentEntity->sequence, frame, j, k );
	}

	if (pseqdesc->motiontype & STUDIO_X) pos[pseqdesc->motionbone][0] = 0.0;
	if (pseqdesc->motiontype & STUDIO_Y) pos[pseqdesc->motionbone][1] = 0.0;
	if (pseqdesc->motiontype & STUDIO_Z) pos[pseqdesc->motionbone][2] = 0.0;

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * m_pCurrentEntity->framerate;

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
	if( ent->renderfx & RF_HOLOGRAMM )
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
float R_StudioEstimateFrame( mstudioseqdesc_t *pseqdesc )
{
	double dfdt, f;
	
	if ( m_fDoInterp )
	{
		if ( r_refdef.time < m_pCurrentEntity->animtime ) dfdt = 0;
		else dfdt = (r_refdef.time - m_pCurrentEntity->animtime) * m_pCurrentEntity->framerate * pseqdesc->fps;
	}
	else dfdt = 0;

	if( pseqdesc->numframes <= 1 ) f = 0;
	else f = (m_pCurrentEntity->frame * (pseqdesc->numframes - 1)) / 256.0;
 
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
void R_StudioSetupBones( void )
{
	int		i;
	double		f;

	mstudiobone_t	*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t	*panim;

	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix4x4		bonematrix;

	static float	pos2[MAXSTUDIOBONES][3];
	static vec4_t	q2[MAXSTUDIOBONES];
	static float	pos3[MAXSTUDIOBONES][3];
	static vec4_t	q3[MAXSTUDIOBONES];
	static float	pos4[MAXSTUDIOBONES][3];
	static vec4_t	q4[MAXSTUDIOBONES];

	if( m_pCurrentEntity->sequence >=  m_pStudioHeader->numseq) m_pCurrentEntity->sequence = 0;
	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->sequence;

	f = R_StudioEstimateFrame( pseqdesc );

	if( m_pCurrentEntity->prev.frame > f )
	{
		//Msg("%f %f\n", m_pCurrentEntity->prev.frame, f );
	}

	panim = R_StudioGetAnim( m_pRenderModel, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );

	if( pseqdesc->numblends > 1 )
	{
		float	s;
		float	dadt;

		panim += m_pStudioHeader->numbones;
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, f );

		dadt = R_StudioEstimateInterpolant();
		s = (m_pCurrentEntity->blending[0] * dadt + m_pCurrentEntity->prev.blending[0] * (1.0 - dadt)) / 255.0;

		R_StudioSlerpBones( q, pos, q2, pos2, s );

		if (pseqdesc->numblends == 4)
		{
			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos3, q3, pseqdesc, panim, f );

			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos4, q4, pseqdesc, panim, f );

			s = (m_pCurrentEntity->blending[0] * dadt + m_pCurrentEntity->prev.blending[0] * (1.0 - dadt)) / 255.0;
			R_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (m_pCurrentEntity->blending[1] * dadt + m_pCurrentEntity->prev.blending[1] * (1.0 - dadt)) / 255.0;
			R_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	if( m_fDoInterp && m_pCurrentEntity->prev.sequencetime && ( m_pCurrentEntity->prev.sequencetime + 0.2 > r_refdef.time) && ( m_pCurrentEntity->prev.sequence < m_pStudioHeader->numseq ))
	{
		// blend from last sequence
		static float  pos1b[MAXSTUDIOBONES][3];
		static vec4_t q1b[MAXSTUDIOBONES];
		float s;
                    
		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->prev.sequence;
		panim = R_StudioGetAnim( m_pRenderModel, pseqdesc );
		// clip prevframe
		R_StudioCalcRotations( pos1b, q1b, pseqdesc, panim, m_pCurrentEntity->prev.frame );

		if (pseqdesc->numblends > 1)
		{
			panim += m_pStudioHeader->numbones;
			R_StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pCurrentEntity->prev.frame );

			s = (m_pCurrentEntity->prev.seqblending[0]) / 255.0;
			R_StudioSlerpBones( q1b, pos1b, q2, pos2, s );

			if (pseqdesc->numblends == 4)
			{
				panim += m_pStudioHeader->numbones;
				R_StudioCalcRotations( pos3, q3, pseqdesc, panim, m_pCurrentEntity->prev.frame );

				panim += m_pStudioHeader->numbones;
				R_StudioCalcRotations( pos4, q4, pseqdesc, panim, m_pCurrentEntity->prev.frame );

				s = (m_pCurrentEntity->prev.seqblending[0]) / 255.0;
				R_StudioSlerpBones( q3, pos3, q4, pos4, s );

				s = (m_pCurrentEntity->prev.seqblending[1]) / 255.0;
				R_StudioSlerpBones( q1b, pos1b, q3, pos3, s );
			}
		}

		s = 1.0 - (r_refdef.time - m_pCurrentEntity->prev.sequencetime) / 0.2;
		R_StudioSlerpBones( q, pos, q1b, pos1b, s );
	}
	else
	{
		// MsgDev( D_INFO, "prevframe = %4.2f\n", f );
		m_pCurrentEntity->prev.frame = f;
	}

	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	// calc gait animation
	if( m_pCurrentEntity->gaitsequence != 0 )
	{
		if( m_pCurrentEntity->gaitsequence >= m_pStudioHeader->numseq ) 
			m_pCurrentEntity->gaitsequence = 0;

		pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->gaitsequence;

		panim = R_StudioGetAnim( m_pRenderModel, pseqdesc );
		R_StudioCalcRotations( pos2, q2, pseqdesc, panim, m_pCurrentEntity->gaitframe );

		for( i = 0; i < m_pStudioHeader->numbones; i++ )
		{
			// g-cont. hey, what a hell ?
			if(!com.strcmp( pbones[i].name, "Bip01 Spine"))
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
			R_StudioFxTransform( m_pCurrentEntity, m_pbonestransform[i] );
		} 
		else 
		{
			Matrix4x4_ConcatTransforms( m_pbonestransform[i], m_pbonestransform[pbones[i].parent], bonematrix );
			Matrix4x4_ConcatTransforms( m_plighttransform[i], m_plighttransform[pbones[i].parent], bonematrix );
		}
	}
}

/*
====================
StudioSaveBones

====================
*/
void R_StudioSaveBones( void )
{
	int i;
	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
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
void R_StudioMergeBones ( rmodel_t *m_pSubModel )
{
	int	i, j;
	double	f;
	int	do_hunt = true;

	mstudiobone_t	*pbones;
	mstudioseqdesc_t	*pseqdesc;
	mstudioanim_t	*panim;
	matrix4x4		bonematrix;

	static vec4_t	q[MAXSTUDIOBONES];
	static float	pos[MAXSTUDIOBONES][3];

	if( m_pCurrentEntity->sequence >=  m_pStudioHeader->numseq ) m_pCurrentEntity->sequence = 0;
	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->sequence;

	f = R_StudioEstimateFrame( pseqdesc );

	//if (m_pCurrentEntity->prev.frame > f) Msg("%f %f\n", m_pCurrentEntity->prev.frame, f );

	panim = R_StudioGetAnim( m_pSubModel, pseqdesc );
	R_StudioCalcRotations( pos, q, pseqdesc, panim, f );
	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	for( i = 0; i < m_pStudioHeader->numbones; i++ ) 
	{
		for( j = 0; j < m_nCachedBones; j++ )
		{
			if(!com.stricmp(pbones[i].name, m_nCachedBoneNames[j]))
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
				R_StudioFxTransform( m_pCurrentEntity, m_pbonestransform[i] );
			} 
			else 
			{
				Matrix4x4_ConcatTransforms( m_pbonestransform[i], m_pbonestransform[pbones[i].parent], bonematrix );
				Matrix4x4_ConcatTransforms( m_plighttransform[i], m_plighttransform[pbones[i].parent], bonematrix );
			}
		}
	}
}


/*
====================
StudioCalcAttachments

====================
*/
void R_StudioCalcAttachments( void )
{
	int i;
	mstudioattachment_t *pattachment;

	if ( m_pStudioHeader->numattachments > MAXSTUDIOATTACHMENTS )
	{
		Msg("Warning: Too many attachments on %s\n", m_pCurrentEntity->model->name );
		m_pStudioHeader->numattachments = MAXSTUDIOATTACHMENTS; //reduce it
	}

	// calculate attachment points
	pattachment = (mstudioattachment_t *)((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
	for (i = 0; i < m_pStudioHeader->numattachments; i++)
	{
		Matrix4x4_Transform( m_plighttransform[pattachment[i].bone], pattachment[i].org,  m_pCurrentEntity->attachment[i] );
	}
}

bool R_StudioComputeBBox( vec3_t bbox[8] )
{
	vec3_t vectors[3];
	ref_entity_t *e = m_pCurrentEntity;
	vec3_t mins, maxs, tmp, angles;
	int i, seq = m_pCurrentEntity->sequence;

	if(!R_ExtractBbox( seq, mins, maxs ))
		return false;

	// compute a full bounding box
	for ( i = 0; i < 8; i++ )
	{
		if ( i & 1 ) tmp[0] = mins[0];
		else tmp[0] = maxs[0];
		if ( i & 2 ) tmp[1] = mins[1];
		else tmp[1] = maxs[1];
		if ( i & 4 ) tmp[2] = mins[2];
		else tmp[2] = maxs[2];
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

	if( m_pCurrentEntity->renderfx & RF_VIEWMODEL )
		return true;          
	if(!R_StudioComputeBBox( bbox ))
          	return false;
	
	for ( i = 0; i < 8; i++ )
	{
		int mask = 0;
		for ( j = 0; j < 4; j++ )
		{
			float dp = DotProduct( r_frustum[j].normal, bbox[i] );
			if ( ( dp - r_frustum[j].dist ) < 0 ) mask |= ( 1 << j );
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
void R_StudioSetupModel( int bodypart )
{
	int index;

	if (bodypart > m_pStudioHeader->numbodyparts) bodypart = 0;
	m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = m_pCurrentEntity->body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;

	m_pSubModel = (mstudiomodel_t *)((byte *)m_pStudioHeader + m_pBodyPart->modelindex) + index;
}

void R_StudioSetupLighting( void )
{
	int i;
          mstudiobone_t *pbone;
	
	// get light from floor or ceil
	m_plightvec[0] = 0.0f;
	m_plightvec[1] = 0.0f;
	m_plightvec[2] = (m_pCurrentEntity->effects & EF_INVLIGHT) ? 1.0f : -1.0f;

	if( m_pCurrentEntity->renderfx & RF_FULLBRIGHT )
	{
		for (i = 0; i < 3; i++)
			m_plightcolor[i] = 1.0f;
	}
	else
	{
		vec3_t light_org;
		VectorCopy( m_pCurrentEntity->origin, light_org );
		light_org[2] += 3; // make sure what lightpoint is off the ground
		R_LightForPoint( light_org, m_plightcolor );
		if ( m_pCurrentEntity->renderfx & RF_VIEWMODEL )
			r_lightlevel->value = bound(0, VectorLength(m_plightcolor) * 75.0f, 255); 

	}

	// TODO: only do it for bones that actually have textures
	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex + i);
		//if(pbone->flags & STUDIO_HAS_CHROME)
		{
			Matrix4x4_TransposeRotate( m_pbonestransform[i], m_plightvec, m_blightvec[i] );
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
		vec3_t	tmp;		// vector pointing at bone in world reference frame

		VectorScale( m_pCurrentEntity->origin, -1, tmp );
		tmp[0] += m_pbonestransform[bone][0][3];
		tmp[1] += m_pbonestransform[bone][1][3];
		tmp[2] += m_pbonestransform[bone][2][3];

		VectorNormalize( tmp );
		CrossProduct( tmp, r_right, chromeupvec );
		VectorNormalize( chromeupvec );
		CrossProduct( tmp, chromeupvec, chromerightvec );
		VectorNormalize( chromerightvec );

		Matrix4x4_TransposeRotate( m_pbonestransform[bone], chromeupvec, g_chromeup[bone] );
		Matrix4x4_TransposeRotate( m_pbonestransform[bone], chromerightvec, g_chromeright[bone] );
		g_chromeage[bone] = m_pStudioModelCount;
	}

	// calc s coord
	n = DotProduct( normal, g_chromeright[bone] );
	pchrome[0] = (n + 1.0) * 32.0f;

	// calc t coord
	n = DotProduct( normal, g_chromeup[bone] );
	pchrome[1] = (n + 1.0) * 32.0f;
}

bool R_AcceptStudioPass( int flags, int pass )
{
	if( pass == RENDERPASS_SOLID )
	{
		if(!flags) return true;			// draw all
		if(flags & STUDIO_NF_ADDITIVE) return false;	// draw it at second pass (and chrome too)
		if(flags & STUDIO_NF_CHROME) return true;	// chrome drawing once (without additive)
		if(flags & STUDIO_NF_TRANSPARENT) return true;	// must be draw first always
	}	
	if( pass == RENDERPASS_ALPHA )
	{
		//pass for blended ents
		if(m_pCurrentEntity->renderfx & RF_TRANSLUCENT) 	return true;
		if(!flags) return false;			// skip all
		if(flags & STUDIO_NF_TRANSPARENT) return false;	// must be draw first always
		if(flags & STUDIO_NF_ADDITIVE) return true;	// draw it at second pass
		if(flags & STUDIO_NF_CHROME) return false;	// skip chrome without additive
	}
	return true;
}

void R_StudioDrawMeshes( mstudiotexture_t * ptexture, short *pskinref, int pass )
{
	int i, j;
	float *av, *lv;
	float lv_tmp;
	vec3_t fbright = { 0.95f, 0.95f, 0.95f };
	vec3_t irgoggles = { 0.95f, 0.0f, 0.0f }; //predefined lightcolor
	int flags;

	mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex);
	byte *pnormbone = ((byte *)m_pStudioHeader + m_pSubModel->norminfoindex);
	vec3_t *pstudionorms = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->normindex);
	
	lv = (float *)m_pvlightvalues;
	for (j = 0; j < m_pSubModel->nummesh; j++) 
	{
		flags = ptexture[pskinref[pmesh[j].skinref]].flags;
		if(!R_AcceptStudioPass( flags, pass )) continue;
		
		for (i = 0; i < pmesh[j].numnorms; i++, lv += 3, pstudionorms++, pnormbone++)
		{
			R_StudioLighting (&lv_tmp, *pnormbone, flags, (float *)pstudionorms);
                             
			// FIXME: move this check out of the inner loop
			if( flags & STUDIO_NF_CHROME )
				R_StudioSetupChrome( g_chrome[(float (*)[3])lv - m_pvlightvalues], *pnormbone, (float *)pstudionorms );
			VectorScale(m_plightcolor, lv_tmp, lv );
		}
	}

	for( j = 0; j < m_pSubModel->nummesh; j++ ) 
	{
		float	s, t;
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
		ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);

		flags = ptexture[pskinref[pmesh->skinref]].flags;
		if(!R_AcceptStudioPass(flags, pass )) 
			continue;
		s = 1.0/(float)ptexture[pskinref[pmesh->skinref]].width;
		t = 1.0/(float)ptexture[pskinref[pmesh->skinref]].height;

		GL_BindTexture( m_pRenderModel->textures[ptexture[pskinref[pmesh->skinref]].index].image );

		while (i = *(ptricmds++))
		{
			if (i < 0)
			{
				pglBegin( GL_TRIANGLE_FAN );
				i = -i;
			}
			else
			{
				pglBegin( GL_TRIANGLE_STRIP );
			}

			for(; i > 0; i--, ptricmds += 4)
			{
				if (flags & STUDIO_NF_CHROME)
					pglTexCoord2f(g_chrome[ptricmds[1]][0]*s, g_chrome[ptricmds[1]][1]*t);
				else pglTexCoord2f(ptricmds[2]*s, ptricmds[3]*t);
				lv = m_pvlightvalues[ptricmds[1]];
                                        
                                        if ( m_pCurrentEntity->renderfx & RF_FULLBRIGHT )
					lv = &fbright[0];
                                        if ( m_pCurrentEntity->renderfx & RF_MINLIGHT ) // used for viewmodel only
					VectorBound(0.01f, lv, 1.0f );

				if ( r_refdef.rdflags & RDF_IRGOGGLES && m_pCurrentEntity->renderfx & RF_IR_VISIBLE)
					lv = &irgoggles[0];

				if (flags & STUDIO_NF_ADDITIVE) // additive is self-lighting texture
					pglColor4f( 1.0f, 1.0f, 1.0f, 0.8f );
				else if(m_pCurrentEntity->renderfx & RF_TRANSLUCENT)
					pglColor4f( 1.0f, 1.0f, 1.0f, m_pCurrentEntity->renderamt );
				else pglColor4f( lv[0], lv[1], lv[2], 1.0f ); // get light from floor

				av = m_pxformverts[ptricmds[0]];
				pglVertex3f( av[0], av[1], av[2] );
			}
			pglEnd();
		}
	}
}

void R_StudioDrawPoints ( void )
{
	int		i;
	int		m_skinnum = m_pCurrentEntity->skin;
	byte		*pvertbone;
	byte		*pnormbone;
	vec3_t		*pstudioverts;
	vec3_t		*pstudionorms;
	mstudiotexture_t	*ptexture;
	short		*pskinref;
	    
	pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	pnormbone = ((byte *)m_pStudioHeader + m_pSubModel->norminfoindex);
	ptexture = (mstudiotexture_t *)((byte *)m_pTextureHeader + m_pTextureHeader->textureindex);

	pstudioverts = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);
	pstudionorms = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->normindex);

	pskinref = (short *)((byte *)m_pTextureHeader + m_pTextureHeader->skinindex);
	if( m_skinnum != 0 && m_skinnum < m_pTextureHeader->numskinfamilies )
		pskinref += (m_skinnum * m_pTextureHeader->numskinref);

	for( i = 0; i < m_pSubModel->numverts; i++ )
	{
		Matrix4x4_Transform( m_pbonestransform[pvertbone[i]], pstudioverts[i], m_pxformverts[i]);
	}
	for( i = 0; i < m_pSubModel->numnorms; i++ )
	{
		Matrix4x4_Transform( m_pbonestransform[pnormbone[i]], pstudionorms[i], m_pxformnorms[i]);
	}

	// hack the depth range to prevent view model from poking into walls
	if( m_pCurrentEntity->renderfx & RF_DEPTHHACK) pglDepthRange( 0.0, 0.3 );
	if(( m_pCurrentEntity->renderfx & RF_VIEWMODEL ) && ( r_lefthand->value == 1.0F ))
	{
		/*pglMatrixMode( GL_PROJECTION );
		pglPushMatrix();
		pglLoadIdentity();
		pglScalef( -1, 1, 1 );
	    	pglPerspective( r_refdef.fov_y, ( float ) r_refdef.rect.width / r_refdef.rect.height,  4, 131072 );
		pglMatrixMode( GL_MODELVIEW );
		pglCullFace( GL_BACK );
		*/
	}          
	if(m_PassNum == RENDERPASS_SOLID && !(m_pCurrentEntity->renderfx & RF_TRANSLUCENT)) //setup for solid format
	{
		GL_Enable( GL_ALPHA_TEST );
		GL_BlendFunc( GL_ZERO, GL_ZERO );
                    R_StudioDrawMeshes( ptexture, pskinref, m_PassNum );
		pglColor4f( 1, 1, 1, 1 ); //reset color
		GL_Disable( GL_ALPHA_TEST );
	}
	if(m_PassNum == RENDERPASS_ALPHA) //setup for alpha format
	{
		GL_Enable( GL_BLEND );
		GL_BlendFunc( GL_SRC_ALPHA, GL_ONE );
		R_StudioDrawMeshes( ptexture, pskinref, m_PassNum );
		pglColor4f( 1, 1, 1, 1 ); //reset color
		GL_Disable( GL_BLEND );
	}	
	if (( m_pCurrentEntity->renderfx & RF_VIEWMODEL ) && ( r_lefthand->value == 1.0F ))
	{
		pglMatrixMode( GL_PROJECTION );
		pglPopMatrix();
		pglMatrixMode( GL_MODELVIEW );
		GL_CullFace( GL_FRONT );
	}

	// hack the depth range to prevent view model from poking into walls
	if( m_pCurrentEntity->renderfx & RF_DEPTHHACK ) pglDepthRange( 0.0, 1.0 );
}

void R_StudioDrawBones( void )
{
	int i;
	mstudiobone_t *pbones = (mstudiobone_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	pglDisable (GL_TEXTURE_2D);
	pglDisable (GL_DEPTH_TEST);

	for (i = 0; i < m_pStudioHeader->numbones; i++)
	{
		if (pbones[i].parent >= 0)
		{
			pglPointSize( 3.0f );
			pglColor3f( 1, 0.7f, 0 );
			pglBegin( GL_LINES );
			pglVertex3f( m_pbonestransform[pbones[i].parent][0][3], m_pbonestransform[pbones[i].parent][1][3], m_pbonestransform[pbones[i].parent][2][3]);
			pglVertex3f( m_pbonestransform[i][0][3], m_pbonestransform[i][1][3], m_pbonestransform[i][2][3]);
			pglEnd();

			pglColor3f( 0, 0, 0.8f );
			pglBegin( GL_POINTS );
			if (pbones[pbones[i].parent].parent != -1)
				pglVertex3f (m_pbonestransform[pbones[i].parent][0][3], m_pbonestransform[pbones[i].parent][1][3], m_pbonestransform[pbones[i].parent][2][3]);
			pglVertex3f( m_pbonestransform[i][0][3], m_pbonestransform[i][1][3], m_pbonestransform[i][2][3]);
			pglEnd();
		}
		else
		{
			// draw parent bone node
			pglPointSize( 5.0f );
			pglColor3f( 0.8f, 0, 0 );
			pglBegin( GL_POINTS );
			pglVertex3f( m_pbonestransform[i][0][3], m_pbonestransform[i][1][3], m_pbonestransform[i][2][3]);
			pglEnd();
		}
	}

	pglPointSize (1.0f);
	GL_Enable( GL_DEPTH_TEST );
	pglEnable( GL_TEXTURE_2D );
}

void R_StudioDrawHitboxes( void )
{
	int i, j;

	pglDisable( GL_TEXTURE_2D);
	GL_Disable( GL_CULL_FACE );

	if( m_pCurrentEntity->renderamt < 1.0f ) pglDisable( GL_DEPTH_TEST );
	else GL_Enable( GL_DEPTH_TEST );

	pglColor4f (1, 0, 0, 0.5f);

	pglPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	GL_Enable( GL_BLEND );
	GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	for (i = 0; i < m_pStudioHeader->numhitboxes; i++)
	{
		mstudiobbox_t *pbboxes = (mstudiobbox_t *) ((byte *) m_pStudioHeader + m_pStudioHeader->hitboxindex);
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

		pglBegin (GL_QUAD_STRIP);
		for (j = 0; j < 10; j++) pglVertex3fv (v2[j & 7]);
		pglEnd ();
	
		pglBegin  (GL_QUAD_STRIP);
		pglVertex3fv (v2[6]);
		pglVertex3fv (v2[0]);
		pglVertex3fv (v2[4]);
		pglVertex3fv (v2[2]);
		pglEnd ();

		pglBegin  (GL_QUAD_STRIP);
		pglVertex3fv (v2[1]);
		pglVertex3fv (v2[7]);
		pglVertex3fv (v2[3]);
		pglVertex3fv (v2[5]);
		pglEnd ();			
	}

	pglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	pglEnable(GL_TEXTURE_2D);
	GL_Enable( GL_CULL_FACE );
	GL_Enable( GL_DEPTH_TEST);
}

void R_StudioDrawAttachments( void )
{
	int i;
	
	pglDisable (GL_TEXTURE_2D);
	GL_Disable( GL_CULL_FACE );
	GL_Disable( GL_DEPTH_TEST);
	
	for (i = 0; i < m_pStudioHeader->numattachments; i++)
	{
		mstudioattachment_t *pattachments = (mstudioattachment_t *) ((byte *)m_pStudioHeader + m_pStudioHeader->attachmentindex);
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
	pglEnable(GL_TEXTURE_2D);
	GL_Enable( GL_CULL_FACE );
	GL_Enable( GL_DEPTH_TEST);
}

void R_StudioDrawHulls ( void )
{
	int i;
	vec3_t		bbox[8];

	if(m_pCurrentEntity->renderfx & RF_VIEWMODEL) return;
	if(!R_StudioComputeBBox( bbox )) return;

	GL_Disable( GL_CULL_FACE );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	pglDisable( GL_TEXTURE_2D );
	pglBegin (GL_QUAD_STRIP);

	for (i = 0; i < 10; i++)
	{
		pglVertex3fv( bbox[i & 7] );
	}

	pglEnd();
	pglEnable( GL_TEXTURE_2D );
	pglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	GL_Enable( GL_CULL_FACE );
}

void R_StudioDrawShadow ( void )
{
	float an = m_pCurrentEntity->angles[1] / 180 * M_PI;
	
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
	int i;

	pglShadeModel( GL_SMOOTH );
	GL_TexEnv( GL_MODULATE );
	
	for (i = 0; i < m_pStudioHeader->numbodyparts ; i++)
	{
		R_StudioSetupModel( i );
		R_StudioDrawPoints();
	}

	if(!( r_refdef.rdflags & RDF_NOWORLDMODEL ))
	{
		switch((int)r_drawentities->value)
		{
		case 2: R_StudioDrawBones(); break;
		case 3: R_StudioDrawHitboxes(); break;
		case 4: R_StudioDrawAttachments(); break;
		case 5: R_StudioDrawHulls(); break;
		}
	}
	GL_TexEnv( GL_REPLACE );
	pglShadeModel( GL_FLAT );
}

void R_StudioSetupRender( int passnum )
{
	// set global pointers
	m_pRenderModel = m_pCurrentEntity->model;
	m_pStudioHeader = m_pRenderModel->phdr;
          m_pTextureHeader = m_pRenderModel->thdr;

	// set intermediate vertex buffers
	m_pxformverts = &g_xformverts[0];
	m_pxformnorms = &g_xformnorms[0];
	m_pvlightvalues = &g_lightvalues[0];

	// misc info
	m_fDoInterp = r_interpolate->integer;
	m_PassNum = passnum;
}

/*
====================
StudioDrawModel

====================
*/
bool R_StudioDrawModel( int pass, int flags )
{
	//if( !mirror_render && m_pCurrentEntity->renderfx & RF_PLAYERMODEL )
	//	return 0;

	if( m_pCurrentEntity->renderfx & RF_VIEWMODEL )
	{
		if( /*mirror_render ||*/ r_lefthand->value == 2 )
			return 0;
	}

	R_StudioSetupRender( pass );	
	R_StudioSetUpTransform();
	if( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if(!R_StudioCheckBBox())
			return 0;

		m_pStudioModelCount++; // render data cache cookie

		// nothing to draw
		if( m_pStudioHeader->numbodyparts == 0 )
			return 1;
	}

	if( m_pCurrentEntity->movetype == MOVETYPE_FOLLOW )
	{
		R_StudioMergeBones( m_pRenderModel );
	}
	else
	{
		R_StudioSetupBones();
	}
	R_StudioSaveBones();	

	if( flags & STUDIO_EVENTS )
	{
		R_StudioCalcAttachments();

		//FIXME:
		//ri.StudioEvent( mstudioevent_t *event, ent );

		if( m_pCurrentEntity->index > 0 )
		{
			// copy attachments into global entity array
			entity_state_t *ent = ri.GetClientEdict( m_pCurrentEntity->index );
			Mem_Copy( m_pCurrentEntity->attachment, m_pCurrentEntity->attachment, sizeof(vec3_t) * MAXSTUDIOATTACHMENTS );
		}
	}

	if( flags & STUDIO_RENDER )
	{
		R_StudioSetupLighting( );
	
		// get remap colors
		m_nTopColor = m_pCurrentEntity->colormap & 0xFF;
		m_nBottomColor = (m_pCurrentEntity->colormap & 0xFF00)>>8;

		R_StudioSetRemapColors( m_nTopColor, m_nBottomColor );
		R_StudioRenderModel( );

		// draw weaponmodel for monsters
		if( m_pCurrentEntity->weaponmodel )
		{
			ref_entity_t saveent = *m_pCurrentEntity;
			rmodel_t *pweaponmodel = m_pCurrentEntity->weaponmodel;

			// get remap colors
			m_nTopColor = m_pCurrentEntity->colormap & 0xFF;
			m_nBottomColor = (m_pCurrentEntity->colormap & 0xFF00)>>8;
			R_StudioSetRemapColors( m_nTopColor, m_nBottomColor );
		
			m_pStudioHeader = pweaponmodel->phdr;
			m_pTextureHeader = pweaponmodel->thdr;
			R_StudioMergeBones( pweaponmodel );
			R_StudioSetupLighting( );

			R_StudioRenderModel( );
			R_StudioCalcAttachments( );
			*m_pCurrentEntity = saveent;
		}
	}
	return 1;
}

/*
====================
StudioEstimateGait

====================
*/
void R_StudioEstimateGait( entity_state_t *pplayer )
{
	float	dt;
	vec3_t	est_velocity;

	dt = ( r_refdef.time - r_refdef.oldtime );
	if( dt < 0 ) dt = 0.0f;
	else if ( dt > 1.0 ) dt = 1.0f;

	if( dt == 0 || m_pCurrentEntity->renderframe == r_frameCount )
	{
		m_flGaitMovement = 0;
		return;
	}

	// VectorAdd( pplayer->velocity, pplayer->prediction_error, est_velocity );
	if( m_fGaitEstimation )
	{
		VectorSubtract( m_pCurrentEntity->origin, m_pCurrentEntity->prev.gaitorigin, est_velocity );
		VectorCopy( m_pCurrentEntity->origin, m_pCurrentEntity->prev.gaitorigin );

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
		VectorCopy( pplayer->velocity, est_velocity );
		m_flGaitMovement = VectorLength( est_velocity ) * dt;
	}

	if( est_velocity[1] == 0 && est_velocity[0] == 0 )
	{
		float flYawDiff = m_pCurrentEntity->angles[YAW] - m_pCurrentEntity->gaityaw;

		flYawDiff = flYawDiff - (int)(flYawDiff / 360) * 360;
		if( flYawDiff > 180 ) flYawDiff -= 360;
		if( flYawDiff < -180 ) flYawDiff += 360;

		if( dt < 0.25 ) flYawDiff *= dt * 4;
		else flYawDiff *= dt;

		m_pCurrentEntity->gaityaw += flYawDiff;
		m_pCurrentEntity->gaityaw = m_pCurrentEntity->gaityaw - (int)(m_pCurrentEntity->gaityaw / 360) * 360;
		m_flGaitMovement = 0;
	}
	else
	{
		m_pCurrentEntity->gaityaw = (atan2(est_velocity[1], est_velocity[0]) * 180 / M_PI);
		if( m_pCurrentEntity->gaityaw > 180 ) m_pCurrentEntity->gaityaw = 180;
		if( m_pCurrentEntity->gaityaw < -180 ) m_pCurrentEntity->gaityaw = -180;
	}

}

/*
====================
StudioProcessGait

====================
*/
void R_StudioProcessGait( entity_state_t *pplayer )
{
	mstudioseqdesc_t	*pseqdesc;
	float		dt, flYaw;	// view direction relative to movement
	float		fBlend;

	if( m_pCurrentEntity->sequence >= m_pStudioHeader->numseq ) 
		m_pCurrentEntity->sequence = 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + m_pCurrentEntity->sequence;
	R_StudioPlayerBlend( pseqdesc, &fBlend, &m_pCurrentEntity->angles[PITCH] );

	m_pCurrentEntity->prev.angles[PITCH] = m_pCurrentEntity->angles[PITCH];
	m_pCurrentEntity->blending[0] = fBlend;
	m_pCurrentEntity->prev.blending[0] = m_pCurrentEntity->blending[0];
	m_pCurrentEntity->prev.seqblending[0] = m_pCurrentEntity->blending[0];

	// MsgDev( D_INFO, "%f %d\n", m_pCurrentEntity->angles[PITCH], m_pCurrentEntity->blending[0] );

	dt = (r_refdef.time - r_refdef.oldtime);
	if( dt < 0 ) dt = 0.0f;
	else if( dt > 1.0 ) dt = 1.0f;

	R_StudioEstimateGait( pplayer );

	// MsgDev( D_INFO, "%f %f\n", m_pCurrentEntity->angles[YAW], m_pPlayerInfo->gaityaw );

	// calc side to side turning
	flYaw = m_pCurrentEntity->angles[YAW] - m_pCurrentEntity->gaityaw;
	flYaw = flYaw - (int)(flYaw / 360) * 360;
	if( flYaw < -180 ) flYaw = flYaw + 360;
	if( flYaw > 180 ) flYaw = flYaw - 360;

	if( flYaw > 120 )
	{
		m_pCurrentEntity->gaityaw = m_pCurrentEntity->gaityaw - 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw - 180;
	}
	else if( flYaw < -120 )
	{
		m_pCurrentEntity->gaityaw = m_pCurrentEntity->gaityaw + 180;
		m_flGaitMovement = -m_flGaitMovement;
		flYaw = flYaw + 180;
	}

	// adjust torso
	m_pCurrentEntity->controller[0] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->controller[1] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->controller[2] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->controller[3] = ((flYaw / 4.0) + 30) / (60.0 / 255.0);
	m_pCurrentEntity->prev.controller[0] = m_pCurrentEntity->controller[0];
	m_pCurrentEntity->prev.controller[1] = m_pCurrentEntity->controller[1];
	m_pCurrentEntity->prev.controller[2] = m_pCurrentEntity->controller[2];
	m_pCurrentEntity->prev.controller[3] = m_pCurrentEntity->controller[3];

	m_pCurrentEntity->angles[YAW] = m_pCurrentEntity->gaityaw;
	if( m_pCurrentEntity->angles[YAW] < -0 ) m_pCurrentEntity->angles[YAW] += 360;
	m_pCurrentEntity->prev.angles[YAW] = m_pCurrentEntity->angles[YAW];

	if( pplayer->model.gaitsequence >= m_pStudioHeader->numseq ) 
		pplayer->model.gaitsequence = 0;

	pseqdesc = (mstudioseqdesc_t *)((byte *)m_pStudioHeader + m_pStudioHeader->seqindex) + pplayer->model.gaitsequence;

	// calc gait frame
	if( pseqdesc->linearmovement[0] > 0 )
	{
		m_pCurrentEntity->gaitframe += (m_flGaitMovement / pseqdesc->linearmovement[0]) * pseqdesc->numframes;
	}
	else
	{
		m_pCurrentEntity->gaitframe += pseqdesc->fps * dt;
	}

	// do modulo
	m_pCurrentEntity->gaitframe = m_pCurrentEntity->gaitframe - (int)(m_pCurrentEntity->gaitframe / pseqdesc->numframes) * pseqdesc->numframes;
	if( m_pCurrentEntity->gaitframe < 0 ) m_pCurrentEntity->gaitframe += pseqdesc->numframes;
}

/*
====================
StudioDrawPlayer

====================
*/
int R_StudioDrawPlayer( int pass, int flags )
{
	entity_state_t	*pplayer;

	//if( !mirror_render && m_pCurrentEntity->renderfx & RF_PLAYERMODEL )
	//	return 0;

	if(!(flags & STUDIO_MIRROR))
	{
		//m_pCurrentEntity = IEngineStudio.GetCurrentEntity();
	}

	pplayer = ri.GetClientEdict( m_pCurrentEntity->index );
	R_StudioSetupRender( pass );

	// MsgDev( D_INFO, "DrawPlayer %d\n", m_pCurrentEntity->blending[0] );
	// MsgDev( D_INFO, "DrawPlayer %d %d (%d)\n", r_framecount, pplayer->number, m_pCurrentEntity->sequence );
	// MsgDev( D_INFO, "Player %.2f %.2f %.2f\n", pplayer->velocity[0], pplayer->velocity[1], pplayer->velocity[2] );

	if( pplayer->number < 0 || pplayer->number > ri.GetMaxClients())
		return 0;

	if( pplayer->model.gaitsequence )
	{
		vec3_t orig_angles;

		VectorCopy( m_pCurrentEntity->angles, orig_angles );
		R_StudioProcessGait( pplayer );

		m_pCurrentEntity->gaitsequence = pplayer->model.gaitsequence;
		R_StudioSetUpTransform( );
		VectorCopy( orig_angles, m_pCurrentEntity->angles );
	}
	else
	{
		m_pCurrentEntity->controller[0] = 127;
		m_pCurrentEntity->controller[1] = 127;
		m_pCurrentEntity->controller[2] = 127;
		m_pCurrentEntity->controller[3] = 127;
		m_pCurrentEntity->prev.controller[0] = m_pCurrentEntity->controller[0];
		m_pCurrentEntity->prev.controller[1] = m_pCurrentEntity->controller[1];
		m_pCurrentEntity->prev.controller[2] = m_pCurrentEntity->controller[2];
		m_pCurrentEntity->prev.controller[3] = m_pCurrentEntity->controller[3];
		m_pCurrentEntity->gaitsequence = 0;
		R_StudioSetUpTransform( );
	}

	if( flags & STUDIO_RENDER )
	{
		// see if the bounding box lets us trivially reject, also sets
		if(!R_StudioCheckBBox())
			return 0;

		m_pStudioModelCount++; // render data cache cookie

		// nothing to draw
		if( m_pStudioHeader->numbodyparts == 0 )
			return 1;
	}

	R_StudioSetupBones( );
	R_StudioSaveBones( );
	m_pCurrentEntity->renderframe = r_frameCount;

	if( flags & STUDIO_EVENTS )
	{
		R_StudioCalcAttachments( );

		//FIXME:
		//ri.StudioEvent( mstudioevent_t *event, ent );

		if( m_pCurrentEntity->index > 0 )
		{
			// copy attachments into global entity array
			entity_state_t *ent = ri.GetClientEdict( m_pCurrentEntity->index );
			Mem_Copy( m_pCurrentEntity->attachment, m_pCurrentEntity->attachment, sizeof(vec3_t) * MAXSTUDIOATTACHMENTS );
		}
	}

	if( flags & STUDIO_RENDER )
	{
		// show highest resolution multiplayer model
		if( r_himodels->integer && m_pRenderModel != m_pCurrentEntity->model  )
			m_pCurrentEntity->body = 255;

		if(!(glw_state.developer == 0 && ri.GetMaxClients() == 1 ) && ( m_pRenderModel == m_pCurrentEntity->model ))
			m_pCurrentEntity->body = 1; // force helmet

		R_StudioSetupLighting( );

		// get remap colors
		m_nTopColor = m_pCurrentEntity->colormap & 0xFF;
		m_nBottomColor = (m_pCurrentEntity->colormap & 0xFF00)>>8;
		
		if( m_nTopColor < 0 ) m_nTopColor = 0;
		if( m_nTopColor > 360 ) m_nTopColor = 360;
		if( m_nBottomColor < 0 ) m_nBottomColor = 0;
		if( m_nBottomColor > 360 ) m_nBottomColor = 360;

		R_StudioSetRemapColors( m_nTopColor, m_nBottomColor );

		R_StudioRenderModel( );

		if( m_pCurrentEntity->weaponmodel )
		{
			ref_entity_t saveent = *m_pCurrentEntity;
			rmodel_t *pweaponmodel = m_pCurrentEntity->weaponmodel;

			m_pStudioHeader = pweaponmodel->phdr;
			m_pTextureHeader = pweaponmodel->thdr;
			R_StudioMergeBones( pweaponmodel );
			R_StudioSetupLighting( );

			R_StudioRenderModel( );
			R_StudioCalcAttachments( );

			*m_pCurrentEntity = saveent;
		}
	}
	return 1;
}

void R_DrawStudioModel( int passnum )
{
	if( m_pCurrentEntity->ent_type == ED_CLIENT )
		R_StudioDrawPlayer( passnum, STUDIO_RENDER );
	else R_StudioDrawModel( passnum, STUDIO_RENDER );

	R_StudioAddEntityToRadar( );
}