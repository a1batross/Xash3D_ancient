//=======================================================================
//			Copyright XashXT Group 2009 ©
//		        cm_studio.c - stduio models tracing
//=======================================================================

#include "cm_local.h"
#include "mathlib.h"
#include "matrix_lib.h"
#include "byteorder.h"
#include "const.h"

struct
{
	dstudiohdr_t	*hdr;
	dstudiomodel_t	*submodel;
	dstudiobodyparts_t	*bodypart;
	matrix4x4		rotmatrix;
	matrix4x4		bones[MAXSTUDIOBONES];
	cplane_t		planes[12];
	trace_t		trace;
} studio;

/*
===============================================================================

	STUDIO MODELS

===============================================================================
*/
int CM_StudioExtractBbox( dstudiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	dstudioseqdesc_t	*pseqdesc;
	pseqdesc = (dstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if(sequence == -1) return 0;
	VectorCopy( pseqdesc[sequence].bbmin, mins );
	VectorCopy( pseqdesc[sequence].bbmax, maxs );

	return 1;
}

int CM_StudioBodyVariations( model_t handle )
{
	dstudiohdr_t	*pstudiohdr;
	dstudiobodyparts_t	*pbodypart;
	int		i, count;

	pstudiohdr = (dstudiohdr_t *)CM_Extradata( handle );
	if( !pstudiohdr ) return 0;

	count = 1;
	pbodypart = (dstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex);

	// each body part has nummodels variations so there are as many total variations as there
	// are in a matrix of each part by each other part
	for( i = 0; i < pstudiohdr->numbodyparts; i++ )
	{
		count = count * pbodypart[i].nummodels;
	}
	return count;
}

/*
====================
CM_StudioSetUpTransform
====================
*/
void CM_StudioSetUpTransform( edict_t *e )
{
	float	*ang, *org, scale = 1.0f;

	org = e->v.origin;
	ang = e->v.angles;
	if( e->v.scale != 0.0f )
		scale = e->v.scale;

	Matrix4x4_CreateFromEntity( studio.rotmatrix, org[0], org[1], org[2], ang[PITCH], ang[YAW], ang[ROLL], scale );
}

/*
====================
StudioCalcBoneAdj

====================
*/
void CM_StudioCalcBoneAdj( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2 )
{
	int			i, j;
	float			value;
	dstudiobonecontroller_t	*pbonecontroller;
	
	pbonecontroller = (dstudiobonecontroller_t *)((byte *)studio.hdr + studio.hdr->bonecontrollerindex);

	for( j = 0; j < studio.hdr->numbonecontrollers; j++ )
	{
		i = pbonecontroller[j].index;

		if( i == 4 ) continue; // ignore mouth
		if( i <= MAXSTUDIOCONTROLLERS )
		{
			// check for 360% wrapping
			if( pbonecontroller[j].type & STUDIO_RLOOP )
			{
				if( abs( pcontroller1[i] - pcontroller2[i] ) > 128 )
				{
					int	a, b;

					a = (pcontroller1[j] + 128) % 256;
					b = (pcontroller2[j] + 128) % 256;
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
				if( value < 0 ) value = 0;
				if( value > 1.0 ) value = 1.0;
				value = (1.0 - value) * pbonecontroller[j].start + value * pbonecontroller[j].end;
			}
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
CM_StudioCalcBoneQuaterion

====================
*/
void CM_StudioCalcBoneQuaterion( int frame, float s, dstudiobone_t *pbone, dstudioanim_t *panim, float *adj, float *q )
{
	int		j, k;
	vec4_t		q1, q2;
	vec3_t		angle1, angle2;
	dstudioanimvalue_t	*panimvalue;

	for( j = 0; j < 3; j++ )
	{
		if( panim->offset[j+3] == 0 )
		{
			angle2[j] = angle1[j] = pbone->value[j+3]; // default;
		}
		else
		{
			panimvalue = (dstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			k = frame;
			
			// debug
			if( panimvalue->num.total < panimvalue->num.valid )
				k = 0;
			
			while( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;
				// DEBUG
				if( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}
			// Bah, missing blend!
			if( panimvalue->num.valid > k )
			{
				angle1[j] = panimvalue[k+1].value;

				if( panimvalue->num.valid > k + 1 )
				{
					angle2[j] = panimvalue[k+2].value;
				}
				else
				{
					if( panimvalue->num.total > k + 1 )
						angle2[j] = angle1[j];
					else angle2[j] = panimvalue[panimvalue->num.valid+2].value;
				}
			}
			else
			{
				angle1[j] = panimvalue[panimvalue->num.valid].value;
				if( panimvalue->num.total > k + 1 )
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

		if( pbone->bonecontroller[j+3] != -1 )
		{
			angle1[j] += adj[pbone->bonecontroller[j+3]];
			angle2[j] += adj[pbone->bonecontroller[j+3]];
		}
	}

	if( !VectorCompare( angle1, angle2 ))
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
CM_StudioCalcBonePosition

====================
*/
void CM_StudioCalcBonePosition( int frame, float s, dstudiobone_t *pbone, dstudioanim_t *panim, float *adj, float *pos )
{
	int		j, k;
	dstudioanimvalue_t	*panimvalue;

	for( j = 0; j < 3; j++ )
	{
		pos[j] = pbone->value[j]; // default;
		if( panim->offset[j] != 0.0f )
		{
			panimvalue = (dstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			
			k = frame;

			// debug
			if( panimvalue->num.total < panimvalue->num.valid )
				k = 0;
			// find span of values that includes the frame we want
			while( panimvalue->num.total <= k )
			{
				k -= panimvalue->num.total;
				panimvalue += panimvalue->num.valid + 1;

  				// DEBUG
				if( panimvalue->num.total < panimvalue->num.valid )
					k = 0;
			}
			// if we're inside the span
			if( panimvalue->num.valid > k )
			{
				// and there's more data in the span
				if( panimvalue->num.valid > k + 1 )
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
				if( panimvalue->num.total <= k + 1 )
				{
					pos[j] += (panimvalue[panimvalue->num.valid].value * (1.0 - s) + s * panimvalue[panimvalue->num.valid + 2].value) * pbone->scale[j];
				}
				else
				{
					pos[j] += panimvalue[panimvalue->num.valid].value * pbone->scale[j];
				}
			}
		}
		if( pbone->bonecontroller[j] != -1 && adj )
		{
			pos[j] += adj[pbone->bonecontroller[j]];
		}
	}
}

/*
====================
CM_StudioCalcRotations

====================
*/
void CM_StudioCalcRotations( edict_t *e, float pos[][3], vec4_t *q, dstudioseqdesc_t *pseqdesc, dstudioanim_t *panim, float f )
{
	int		i;
	int		frame;
	dstudiobone_t	*pbone;
	float		adj[MAXSTUDIOCONTROLLERS];
	float		s, dadt = 1.0f; // noInterp

	if( f > pseqdesc->numframes - 1 )
		f = 0;
	else if( f < -0.01 )
		f = -0.01;

	frame = (int)f;
	s = (f - frame);

	// add in programtic controllers
	pbone = (dstudiobone_t *)((byte *)studio.hdr + studio.hdr->boneindex);

	CM_StudioCalcBoneAdj( dadt, adj, e->v.controller, e->v.controller );

	for (i = 0; i < studio.hdr->numbones; i++, pbone++, panim++) 
	{
		CM_StudioCalcBoneQuaterion( frame, s, pbone, panim, adj, q[i] );
		CM_StudioCalcBonePosition( frame, s, pbone, panim, adj, pos[i] );
	}

	if( pseqdesc->motiontype & STUDIO_X ) pos[pseqdesc->motionbone][0] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Y ) pos[pseqdesc->motionbone][1] = 0.0f;
	if( pseqdesc->motiontype & STUDIO_Z ) pos[pseqdesc->motionbone][2] = 0.0f;

	s = 0 * ((1.0 - (f - (int)(f))) / (pseqdesc->numframes)) * e->v.framerate;

	if( pseqdesc->motiontype & STUDIO_LX ) pos[pseqdesc->motionbone][0] += s * pseqdesc->linearmovement[0];
	if( pseqdesc->motiontype & STUDIO_LY ) pos[pseqdesc->motionbone][1] += s * pseqdesc->linearmovement[1];
	if( pseqdesc->motiontype & STUDIO_LZ ) pos[pseqdesc->motionbone][2] += s * pseqdesc->linearmovement[2];
}

/*
====================
StudioEstimateFrame

====================
*/
float CM_StudioEstimateFrame( edict_t *e, dstudioseqdesc_t *pseqdesc )
{
	double	f;
	
	if( pseqdesc->numframes <= 1 )
		f = 0;
	else f = (e->v.frame * (pseqdesc->numframes - 1)) / 256.0;
 
	if( pseqdesc->flags & STUDIO_LOOPING ) 
	{
		if( pseqdesc->numframes > 1 )
			f -= (int)(f / (pseqdesc->numframes - 1)) *  (pseqdesc->numframes - 1);
		if( f < 0 ) f += (pseqdesc->numframes - 1);
	}
	else 
	{
		if( f >= pseqdesc->numframes - 1.001 )
			f = pseqdesc->numframes - 1.001;
		if( f < 0.0 )  f = 0.0;
	}
	return f;
}

/*
====================
CM_StudioSlerpBones

====================
*/
void CM_StudioSlerpBones( vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s )
{
	int	i;
	vec4_t	q3;
	float	s1;

	s = bound( 0.0f, s, 1.0f );
	s1 = 1.0f - s;

	for( i = 0; i < studio.hdr->numbones; i++ )
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
CM_StudioGetAnim

====================
*/
dstudioanim_t *CM_StudioGetAnim( cmodel_t *m_pSubModel, dstudioseqdesc_t *pseqdesc )
{
	dstudioseqgroup_t	*pseqgroup;
	byte		*paSequences;
	size_t		filesize;
          byte		*buf;

	pseqgroup = (dstudioseqgroup_t *)((byte *)studio.hdr + studio.hdr->seqgroupindex) + pseqdesc->seqgroup;
	if( pseqdesc->seqgroup == 0 )
		return (dstudioanim_t *)((byte *)studio.hdr + pseqgroup->data + pseqdesc->animindex);
	paSequences = (void *)m_pSubModel->submodels;

	if( paSequences == NULL )
	{
		// allocate sequence groups if needs
		paSequences = (byte *)Mem_Alloc( m_pSubModel->mempool, sizeof( paSequences ) * MAXSTUDIOGROUPS );
          	m_pSubModel->submodels = (void *)paSequences; // just a container
	}

	if(((dstudiomodel_t *)&(paSequences[pseqdesc->seqgroup])) == NULL )
	{
		dstudioseqgroup_t	*pseqhdr;

		buf = FS_LoadFile( pseqgroup->name, &filesize );
		if( !buf || !filesize || IDSEQGRPHEADER != LittleLong(*(uint *)buf ))
			Host_Error( "CM_StudioGetAnim: can't load %s\n", pseqgroup->name );

		pseqhdr = (dstudioseqgroup_t *)buf;
		MsgDev( D_INFO, "loading %s\n", pseqgroup->name );
			
		paSequences = (byte *)Mem_Alloc( m_pSubModel->mempool, filesize );
          	m_pSubModel->submodels = (void *)paSequences; // just a container
		Mem_Copy( &paSequences[pseqdesc->seqgroup], buf, filesize );
		Mem_Free( buf );
	}
	return (dstudioanim_t *)((byte *)paSequences[pseqdesc->seqgroup] + pseqdesc->animindex );
}

/*
====================
CM_StudioSetupBones
====================
*/
void CM_StudioSetupBones( edict_t *e )
{
	int		i, oldseq;
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

	oldseq = e->v.sequence; // TraceCode can't change sequence

	if( e->v.sequence >= studio.hdr->numseq ) e->v.sequence = 0;
	pseqdesc = (dstudioseqdesc_t *)((byte *)studio.hdr + studio.hdr->seqindex) + e->v.sequence;

	f = CM_StudioEstimateFrame( e, pseqdesc );

	panim = CM_StudioGetAnim( CM_ClipHandleToModel( e->v.modelindex ), pseqdesc );
	CM_StudioCalcRotations( e, pos, q, pseqdesc, panim, f );

	if( pseqdesc->numblends > 1 )
	{
		float	s;
		float	dadt = 1.0f;

		panim += studio.hdr->numbones;
		CM_StudioCalcRotations( e, pos2, q2, pseqdesc, panim, f );

		s = (e->v.blending[0] * dadt + e->v.blending[0] * (1.0 - dadt)) / 255.0;

		CM_StudioSlerpBones( q, pos, q2, pos2, s );

		if( pseqdesc->numblends == 4 )
		{
			panim += studio.hdr->numbones;
			CM_StudioCalcRotations( e, pos3, q3, pseqdesc, panim, f );

			panim += studio.hdr->numbones;
			CM_StudioCalcRotations( e, pos4, q4, pseqdesc, panim, f );

			s = (e->v.blending[0] * dadt + e->v.blending[0] * (1.0 - dadt)) / 255.0;
			CM_StudioSlerpBones( q3, pos3, q4, pos4, s );

			s = (e->v.blending[1] * dadt + e->v.blending[1] * (1.0 - dadt)) / 255.0;
			CM_StudioSlerpBones( q, pos, q3, pos3, s );
		}
	}

	pbones = (dstudiobone_t *)((byte *)studio.hdr + studio.hdr->boneindex);

	for( i = 0; i < studio.hdr->numbones; i++ ) 
	{
		Matrix4x4_FromOriginQuat( bonematrix, pos[i][0], pos[i][1], pos[i][2], q[i][0], q[i][1], q[i][2], q[i][3] );
		if( pbones[i].parent == -1 ) 
			Matrix4x4_ConcatTransforms( studio.bones[i], studio.rotmatrix, bonematrix );
		else Matrix4x4_ConcatTransforms( studio.bones[i], studio.bones[pbones[i].parent], bonematrix );
	}

	e->v.sequence = oldseq; // restore original value
}

/*
====================
StudioCalcAttachments

====================
*/
static void CM_StudioCalcAttachments( edict_t *e, int iAttachment, float *org, float *ang )
{
	int			i;
	dstudioattachment_t		*pAtt;
	vec3_t			axis[3];
	vec3_t			localOrg, localAng;

	if( studio.hdr->numattachments > MAXSTUDIOATTACHMENTS )
	{
		studio.hdr->numattachments = MAXSTUDIOATTACHMENTS; // reduce it
		MsgDev( D_WARN, "CM_StudioCalcAttahments: too many attachments on %s\n", studio.hdr->name );
	}

	iAttachment = bound( 0, iAttachment, studio.hdr->numattachments );

	// calculate attachment points
	pAtt = (dstudioattachment_t *)((byte *)studio.hdr + studio.hdr->attachmentindex);

	for( i = 0; i < studio.hdr->numattachments; i++ )
	{
		if( i == iAttachment )
		{
			// compute pos and angles
			Matrix4x4_VectorTransform( studio.bones[pAtt[i].bone], pAtt[i].org, localOrg );
			Matrix4x4_VectorTransform( studio.bones[pAtt[i].bone], pAtt[i].vectors[0], axis[0] );
			Matrix4x4_VectorTransform( studio.bones[pAtt[i].bone], pAtt[i].vectors[1], axis[1] );
			Matrix4x4_VectorTransform( studio.bones[pAtt[i].bone], pAtt[i].vectors[2], axis[2] );
			Matrix3x3_ToAngles( axis, localAng, true ); // FIXME: dll's uses FLU ?
			if( org ) VectorCopy( localOrg, org );
			if( ang ) VectorCopy( localAng, ang );
			break; // done
		}
	}
}

void CM_StudioInitBoxHull( void )
{
	int	i, side;
	cplane_t    *p;

	for( i = 0; i < 6; i++ )
	{
		side = i & 1;

		// planes
		p = &studio.planes[i*2];
		p->type = i>>1;
		p->signbits = 0;
		VectorClear( p->normal );
		p->normal[i>>1] = 1.0f;

		p = &studio.planes[i*2+1];
		p->type = 3 + (i>>1);
		p->signbits = 0;
		VectorClear( p->normal );
		p->normal[i>>1] = -1;

		p->signbits = SignbitsForPlane( p->normal );
	}    
}

void CM_StudioBoxHullFromBounds( const vec3_t mins, const vec3_t maxs ) 
{
	studio.planes[0].dist = maxs[0];
	studio.planes[1].dist = -maxs[0];
	studio.planes[2].dist = mins[0];
	studio.planes[3].dist = -mins[0];
	studio.planes[4].dist = maxs[1];
	studio.planes[5].dist = -maxs[1];
	studio.planes[6].dist = mins[1];
	studio.planes[7].dist = -mins[1];
	studio.planes[8].dist = maxs[2];
	studio.planes[9].dist = -maxs[2];
	studio.planes[10].dist = mins[2];
	studio.planes[11].dist = -mins[2];
}

bool CM_StudioSetup( edict_t *e )
{
	cmodel_t	*mod = CM_ClipHandleToModel( e->v.modelindex );

	if( mod && mod->type == mod_studio && mod->extradata )
	{
		studio.hdr = (dstudiohdr_t *)mod->extradata;
		CM_StudioSetUpTransform( e );
		CM_StudioSetupBones( e );
		return true;
	}
	return false;
}

bool CM_StudioTraceBox( vec3_t start, vec3_t end ) 
{
	int	i;
	cplane_t	*plane, *clipplane;
	float	enterFrac, leaveFrac;
	bool	getout, startout;
	float	d1, d2;
	float	f;

	enterFrac = -1.0;
	leaveFrac = 1.0;
	clipplane = NULL;

	getout = false;
	startout = false;

	// compare the trace against all planes of the brush
	// find the latest time the trace crosses a plane towards the interior
	// and the earliest time the trace crosses a plane towards the exterior
	for( i = 0; i < 6; i++ ) 
	{
		plane = studio.planes + i * 2 + (i & 1);

		d1 = DotProduct( start, plane->normal ) - plane->dist;
		d2 = DotProduct( end, plane->normal ) - plane->dist;

		if( d2 > 0.0f ) getout = TRUE; // endpoint is not in solid
		if( d1 > 0.0f ) startout = TRUE;

		// if completely in front of face, no intersection with the entire brush
		if( d1 > 0 && ( d2 >= SURFACE_CLIP_EPSILON || d2 >= d1 ))
			return false;

		// if it doesn't cross the plane, the plane isn't relevent
		if( d1 <= 0 && d2 <= 0 )
			continue;

		// crosses face
		if( d1 > d2 ) 
		{
			// enter
			f = (d1 - SURFACE_CLIP_EPSILON) / (d1 - d2);
			if( f < 0.0f ) f = 0.0f;

			if( f > enterFrac ) 
			{
				enterFrac = f;
				clipplane = plane;
			}
		} 
		else 
		{
			// leave
			f = (d1 + SURFACE_CLIP_EPSILON) / (d1 - d2);
			if( f > 1.0f ) f = 1.0f;

			if( f < leaveFrac )
			{
				leaveFrac = f;
			}
		}
	}

	// all planes have been checked, and the trace was not
	// completely outside the brush
	if( !startout ) 
	{
		// original point was inside brush
		if( !getout ) studio.trace.flFraction = 0.0f;
		return true;
	}
    
	if( enterFrac < leaveFrac ) 
	{
		if( enterFrac > -1 && enterFrac < studio.trace.flFraction ) 
		{
			if( enterFrac < 0.0f )
				enterFrac = 0.0f;

			studio.trace.flFraction = enterFrac;
            		VectorCopy( clipplane->normal, studio.trace.vecPlaneNormal );
            		studio.trace.flPlaneDist = clipplane->dist;
			return true;
		}
	}
	return false;
}

bool CM_StudioTrace( trace_t *tr, edict_t *e, const vec3_t start, const vec3_t end )
{
	matrix4x4		m;
	vec3_t		transformedStart, transformedEnd;
	int		i, outBone;

	if( !CM_StudioSetup( e ) || !studio.hdr->numhitboxes )
	{
		tr->iHitgroup = -1;
		return false;
	}

	Mem_Set( &studio.trace, 0, sizeof( trace_t ));
	studio.trace.flFraction = 1.0f;
	studio.trace.iHitgroup = -1;
	outBone = -1;

	for( i = 0; i < studio.hdr->numhitboxes; i++ )
	{
		dstudiobbox_t	*phitbox = (dstudiobbox_t *)((byte*)studio.hdr + studio.hdr->hitboxindex) + i;

		Matrix4x4_Invert_Simple( m, studio.bones[phitbox->bone] );
		Matrix4x4_VectorTransform( m, start, transformedStart );
		Matrix4x4_VectorTransform( m, end, transformedEnd );

		CM_StudioBoxHullFromBounds( phitbox->bbmin, phitbox->bbmax );

		if( CM_StudioTraceBox( transformedStart, transformedEnd ))
		{
			outBone = phitbox->bone;
			studio.trace.iHitgroup = phitbox->group;
		}

		if( studio.trace.flFraction == 0.0f )
			break;
	}

	// all hitboxes were swept, get trace result
	if( outBone >= 0 )
	{
		if( tr )
		{
			tr->flFraction = studio.trace.flFraction;
			tr->iHitgroup = studio.trace.iHitgroup;

			Matrix4x4_VectorRotate( studio.bones[outBone], studio.trace.vecEndPos, tr->vecEndPos );
			if( tr->flFraction == 1.0f ) VectorCopy( end, tr->vecEndPos );
			else
			{
				dstudiobone_t *pbone = (dstudiobone_t *)((byte*)studio.hdr + studio.hdr->boneindex) + outBone;

				tr->pTexName = pbone->name; // debug
				tr->iContents = (e->v.health > 0.0f ) ? BASECONT_BODY : BASECONT_CORPSE;
				VectorLerp( start, tr->flFraction, end, tr->vecEndPos );
			}
			tr->flPlaneDist = DotProduct( tr->vecEndPos, tr->vecPlaneNormal );
		}
		return true;
	}
	return false;
}

void CM_StudioGetAttachment( edict_t *e, int iAttachment, float *org, float *ang )
{
	if( !CM_StudioSetup( e ) || studio.hdr->numattachments <= 0 )
	{
		// reset attachments
		if( org ) VectorCopy( e->v.origin, org );
		if( ang ) VectorCopy( e->v.angles, ang );
		return;
	}
	CM_StudioCalcAttachments( e, iAttachment, org, ang );
}

void CM_GetBonePosition( edict_t* e, int iBone, float *org, float *ang )
{
	matrix3x3	axis;

	if( !CM_StudioSetup( e ) || studio.hdr->numbones <= 0 )
	{
		// reset bones
		if( org ) VectorCopy( e->v.origin, org );
		if( ang ) VectorCopy( e->v.angles, ang );
		return;
	}

	iBone = bound( 0, iBone, studio.hdr->numbones );
	Matrix3x3_FromMatrix4x4( axis, studio.bones[iBone] );
	if( org ) Matrix4x4_OriginFromMatrix( studio.bones[iBone], org );
	if( ang ) Matrix3x3_ToAngles( axis, ang, true );
	
}

bool CM_StudioModel( byte *buffer, uint filesize )
{
	dstudiohdr_t	*phdr;
	dstudioseqdesc_t	*pseqdesc;

	phdr = (dstudiohdr_t *)buffer;
	if( phdr->version != STUDIO_VERSION )
	{
		MsgDev( D_ERROR, "CM_StudioModel: %s has wrong version number (%i should be %i)\n", loadmodel->name, phdr->version, STUDIO_VERSION );
		return false;
	}

	loadmodel->type = mod_studio;
	pseqdesc = (dstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);
	loadmodel->numframes = pseqdesc[0].numframes;
	loadmodel->registration_sequence = cms.registration_sequence;

	loadmodel->mempool = Mem_AllocPool( va("^2%s^7", loadmodel->name ));
	loadmodel->extradata = Mem_Alloc( loadmodel->mempool, filesize );
	Mem_Copy( loadmodel->extradata, buffer, filesize );

	// setup bounding box
	CM_StudioExtractBbox( phdr, 0, loadmodel->mins, loadmodel->maxs );

	return true;
}

bool CM_SpriteModel( byte *buffer, uint filesize )
{
	dsprite_t		*phdr;

	phdr = (dsprite_t *)buffer;

	if( phdr->version != SPRITE_VERSION )
	{
		MsgDev( D_ERROR, "CM_SpriteModel: %s has wrong version number (%i should be %i)\n", loadmodel->name, phdr->version, SPRITE_VERSION );
		return false;
	}
          
	loadmodel->type = mod_sprite;
	loadmodel->numframes = phdr->numframes;
	loadmodel->registration_sequence = cms.registration_sequence;

	// setup bounding box
	loadmodel->mins[0] = loadmodel->mins[1] = -phdr->bounds[0] / 2;
	loadmodel->maxs[0] = loadmodel->maxs[1] = phdr->bounds[0] / 2;
	loadmodel->mins[2] = -phdr->bounds[1] / 2;
	loadmodel->maxs[2] = phdr->bounds[1] / 2;

	return true;
}