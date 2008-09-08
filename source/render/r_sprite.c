//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        r_sprite.c - render sprite models
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "byteorder.h"
#include "const.h"

/*
=============================================================

  SPRITE MODELS

=============================================================
*/
string		frame_prefix;
byte		*spr_palette;
static byte	pal[256][4];
uint		surfaceParm;
mipTex_t		*frames;
	
/*
====================
Sprite model loader
====================
*/
dframetype_t *R_SpriteLoadFrame( rmodel_t *mod, void *pin, mspriteframe_t **ppframe, int framenum )
{
	dframe_t		*pinframe;
	mspriteframe_t	*pspriteframe;
	int		width, height, size, origin[2];
	dframetype_t	*spr_frametype;
	rgbdata_t		*spr_frame;
	texture_t		*image;
	string		name;
	mipTex_t		*out;
	
	pinframe = (dframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;
          
	pspriteframe = Mem_Alloc(mod->mempool, sizeof (mspriteframe_t));
	spr_frame = (rgbdata_t *)Mem_Alloc( mod->mempool, sizeof(rgbdata_t));
	spr_frame->buffer = (byte *)Mem_Alloc( mod->mempool, size );
	spr_frame->palette = (byte *)Mem_Alloc( mod->mempool, 1024 );
	Mem_Copy( spr_frame->buffer, (byte *)(pinframe + 1), size );
	Mem_Copy( spr_frame->palette, spr_palette, 1024 );
	spr_frame->flags = IMAGE_HAS_ALPHA;
	spr_frame->type = PF_INDEXED_32;
	spr_frame->size = size; // for bounds checking
	spr_frame->numMips = 1;
	*ppframe = pspriteframe;

	pspriteframe->width = spr_frame->width = width;
	pspriteframe->height = spr_frame->height = height;
	origin[0] = LittleLong(pinframe->origin[0]);
	origin[1] = LittleLong(pinframe->origin[1]);
	
	// extract sprite name from path
	FS_FileBase( mod->name, name );
	com.strcat(name, va("_%s_%i%i", frame_prefix, framenum/10, framenum%10 ));
	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];
	pspriteframe->radius = sqrt( (width * width) + (height * height) );
	
	image = R_LoadTexture( name, spr_frame, 0, 0 );
	if( image )
	{
		pspriteframe->texture = image;
	}
          else
          {
		MsgDev( D_WARN, "%s has null frame %d\n", image->name, framenum );
		pspriteframe->texture = r_defaultTexture;
	}

	R_SetInternalMap( image );
	pspriteframe->shader = R_FindShader( name, SHADER_SPRITE, surfaceParm );

	frames = Mem_Realloc( mod->mempool, frames, sizeof(*frames) * (mod->numShaders + 1));
	out = frames + mod->numShaders++;
	spr_frametype = (dframetype_t *)((byte *)(pinframe + 1) + size );

	com.strncpy( out->name, name, 64 );
	out->shader = pspriteframe->shader;
	out->flags = surfaceParm;

	FS_FreeImage( spr_frame );
	return spr_frametype;
}

dframetype_t *R_SpriteLoadGroup( rmodel_t *mod, void * pin, mspriteframe_t **ppframe, int framenum )
{
	dspritegroup_t	*pingroup;
	mspritegroup_t	*pspritegroup;
	dspriteinterval_t	*pin_intervals;
	float		*poutintervals;
	int		i, groupsize, numframes;
	void		*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = LittleLong(pingroup->numframes);

	groupsize = sizeof(mspritegroup_t) + (numframes - 1) * sizeof(pspritegroup->frames[0]);
	pspritegroup = Mem_Alloc( mod->mempool, groupsize );
	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;
	pin_intervals = (dspriteinterval_t *)(pingroup + 1);
	poutintervals = Mem_Alloc(mod->mempool, numframes * sizeof (float));
	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat(pin_intervals->interval);
		if(*poutintervals <= 0.0) *poutintervals = 1.0f; // set error value
		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;
	for( i = 0; i < numframes; i++ )
	{
		ptemp = R_SpriteLoadFrame(mod, ptemp, &pspritegroup->frames[i], framenum * 10 + i );
	}
	return (dframetype_t *)ptemp;
}

void R_SpriteLoadModel( rmodel_t *mod, const void *buffer )
{
	dsprite_t		*pin;
	short		*numi;
	msprite_t		*psprite;
	dframetype_t	*pframetype;
	int		i, size, numframes;
	
	pin = (dsprite_t *)buffer;
	i = LittleLong(pin->version);
		
	if( i != SPRITE_VERSION )
	{
		Msg("Warning: %s has wrong version number (%i should be %i)\n", mod->name, i, SPRITE_VERSION );
		return;
	}

	numframes = LittleLong (pin->numframes);
	size = sizeof (msprite_t) + (numframes - 1) * sizeof( psprite->frames );

	psprite = Mem_Alloc( mod->mempool, size );
	mod->extradata = psprite; // make link to extradata
	mod->numShaders = surfaceParm = 0; // reset frames
	
	psprite->type	= LittleLong(pin->type);
	psprite->rendermode = LittleLong(pin->texFormat);
	psprite->numframes	= numframes;
	mod->mins[0] = mod->mins[1] = -LittleLong(pin->bounds[0]) / 2;
	mod->maxs[0] = mod->maxs[1] = LittleLong(pin->bounds[0]) / 2;
	mod->mins[2] = -LittleLong(pin->bounds[1]) / 2;
	mod->maxs[2] = LittleLong(pin->bounds[1]) / 2;
	numi = (short *)(pin + 1);

	if( LittleShort( *numi ) == 256 )
	{	
		byte *src = (byte *)(numi+1);

		switch( psprite->rendermode )
		{
		case SPR_NORMAL:
			for( i = 0; i < 256; i++ )
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 0;
			}
			break;
		case SPR_ADDGLOW:
		case SPR_ADDITIVE:
			for( i = 0; i < 256; i++ )
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 255;
			}
			surfaceParm |= SURFACEPARM_ADDITIVE;
			break;
                    case SPR_INDEXALPHA:
			for( i = 0; i < 256; i++ )
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = i;
			}
			surfaceParm |= SURFACEPARM_ALPHA;
			break;
		case SPR_ALPHTEST:		
			for( i = 0; i < 256; i++ )
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 255;
			}
			pal[255][0] = pal[255][1] = pal[255][2] = pal[255][3] = 0;
			surfaceParm |= SURFACEPARM_ALPHA;
                              break;
		default:
			for (i = 0; i < 256; i++)
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 0;
			}
			MsgDev( D_ERROR, "%s has unknown texFormat (%i, should be in range 0-4 )\n", mod->name, psprite->rendermode );
			break;
		}
		pframetype = (dframetype_t *)(src);
	}
	else 
	{
		MsgDev( D_ERROR, "%s has wrong number of palette colors %i (should be 256)\n", mod->name, numi );
		return;
	}		
	if( numframes < 1 )
	{
		MsgDev( D_ERROR, "%s has invalid # of frames: %d\n", mod->name, numframes );
		return;
	}

	MsgDev( D_LOAD, "%s, rendermode %d\n", mod->name, psprite->rendermode );

	mod->registration_sequence = registration_sequence;
	spr_palette = (byte *)(&pal[0][0]);

	for( i = 0; i < numframes; i++ )
	{
		frametype_t frametype = LittleLong( pframetype->type );
		psprite->frames[i].type = frametype;

		switch( frametype )
		{
		case SPR_SINGLE:
			com.strncpy( frame_prefix, "one", MAX_STRING );
			pframetype = R_SpriteLoadFrame(mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case SPR_GROUP:
			com.strncpy( frame_prefix, "grp", MAX_STRING );
			pframetype = R_SpriteLoadGroup(mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case SPR_ANGLED:
			com.strncpy( frame_prefix, "ang", MAX_STRING );
			pframetype = R_SpriteLoadGroup(mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		}
		if( pframetype == NULL ) break; // technically an error
	}
	mod->shaders = frames; // setup texture links
}

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame( ref_entity_t *ent )
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int		i, numframes, frame;
	float		*pintervals, fullinterval, targettime, time;

	psprite = ent->model->extradata;
	frame = ent->frame;

	if((frame >= psprite->numframes) || (frame < 0))
	{
		MsgDev( D_WARN, "R_GetSpriteFrame: no such frame %d (%s)\n", frame, ent->model->name );
		frame = 0;
	}

	if( psprite->frames[frame].type == SPR_SINGLE )
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if (psprite->frames[frame].type == SPR_GROUP) 
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];
		time = r_refdef.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for( i = 0; i < (numframes - 1); i++ )
		{
			if (pintervals[i] > targettime)
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	else if( psprite->frames[frame].type == SPR_ANGLED )
	{
		// e.g. doom-style sprite monsters
		int angleframe = (int)((r_refdef.viewangles[1] - ent->angles[1])/360*8 + 0.5 - 4) & 7;
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[angleframe];
	}
	return pspriteframe;
}

/*
=================
R_AddSpriteModelToList

add spriteframe to list
=================
*/
void R_AddSpriteModelToList( ref_entity_t *entity )
{
	mspriteframe_t	*frame;

	frame = R_GetSpriteFrame( entity );

	if( R_CullSphere( entity->origin, frame->radius, MAX_CLIPFLAGS ))
		return;

	// copy frame params
	entity->radius = frame->radius;
	entity->rotation = 0;
	entity->shader = frame->shader;

	// add it
	R_AddMeshToList( MESH_SPRITE, NULL, entity->shader, entity, 0 );
}

/*
=================
R_DrawSpriteModel
=================
*/
void R_DrawSpriteModel( void )
{
	mspriteframe_t	*frame;
	msprite_t		*psprite;
	vec3_t		forward, right, up;
	vec3_t		point;
	ref_entity_t	*e;
	int		i;

	e = m_pCurrentEntity;
	frame = R_GetSpriteFrame( e );
	psprite = (msprite_t *)m_pRenderModel->extradata;

	// setup orientation
	switch( psprite->type )
	{
	case SPR_ORIENTED:
		VectorCopy( e->axis[0], forward );
		VectorCopy( e->axis[1], right );
		VectorCopy( e->axis[2], up );
		VectorScale( forward, 0.01, forward ); // to avoid z-fighting
		VectorSubtract( e->origin, forward, e->origin );
		break;
	case SPR_FACING_UPRIGHT:
		VectorSet( right, e->origin[1] - r_origin[1], -(e->origin[0] - r_origin[0]), 0 );
		VectorNegate( r_forward, forward );
		VectorSet( up, 0, 0, 1 );
		VectorNormalize( right );
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		VectorSet( right, r_forward[1], -r_forward[0], 0 );
		VectorNegate( r_forward, forward );
		VectorSet( up, 0, 0, 1 );
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
		right[0] = e->axis[1][0] * r_forward[0] + e->axis[1][1] * r_right[0] + e->axis[1][2] * r_up[0];
		right[1] = e->axis[1][0] * r_forward[1] + e->axis[1][1] * r_right[1] + e->axis[1][2] * r_up[1];
		right[2] = e->axis[1][0] * r_forward[2] + e->axis[1][1] * r_right[2] + e->axis[1][2] * r_up[2];
		up[0] = e->axis[2][0] * r_forward[0] + e->axis[2][1] * r_right[0] + e->axis[2][2] * r_up[0];
		up[1] = e->axis[2][0] * r_forward[1] + e->axis[2][1] * r_right[1] + e->axis[2][2] * r_up[1];
		up[2] = e->axis[2][0] * r_forward[2] + e->axis[2][1] * r_right[2] + e->axis[2][2] * r_up[2];
		break;
	case SPR_FWD_PARALLEL:
	default: // normal sprite
		VectorNegate( r_forward, forward );
		VectorNegate( r_right, right );
		VectorCopy( r_up, up );
		break;
	}

	// draw it
	RB_CheckMeshOverflow( 6, 4 );
	
	for( i = 2; i < 4; i++ )
	{
		indexArray[numIndex++] = numVertex + 0;
		indexArray[numIndex++] = numVertex + i-1;
		indexArray[numIndex++] = numVertex + i;
	}

	GL_Begin( GL_QUADS );
		GL_TexCoord2f( 0, 0 );
		GL_Color4f( e->rendercolor[0], e->rendercolor[1], e->rendercolor[2], e->renderamt );
		VectorMA( e->origin, frame->up * e->scale, up, point );
		VectorMA( point, frame->left * e->scale, right, point );
		GL_Normal3fv( forward );
		GL_Vertex3fv( point );

		GL_TexCoord2f( 1, 0 );		
		GL_Color4f( e->rendercolor[0], e->rendercolor[1], e->rendercolor[2], e->renderamt );
		VectorMA( e->origin, frame->up * e->scale, up, point );
		VectorMA( point, frame->right * e->scale, right, point );
		GL_Normal3fv( forward );
		GL_Vertex3fv( point );

		GL_TexCoord2f( 1, 1 );
		GL_Color4f( e->rendercolor[0], e->rendercolor[1], e->rendercolor[2], e->renderamt );
		VectorMA( e->origin, frame->down * e->scale, up, point );
		VectorMA( point, frame->right * e->scale, right, point );
		GL_Normal3fv( forward );
		GL_Vertex3fv( point );

          	GL_TexCoord2f( 0, 1 );
		GL_Color4f( e->rendercolor[0], e->rendercolor[1], e->rendercolor[2], e->renderamt );
		VectorMA( e->origin, frame->down * e->scale, up, point );
		VectorMA( point, frame->left * e->scale, right, point );
		GL_Normal3fv( forward );
		GL_Vertex3fv( point );
	GL_End();
}