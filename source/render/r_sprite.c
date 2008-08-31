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
	pspriteframe->radius = sqrt((width>>1) * (width>>1) + (height>>1) * (height>>1));
	
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

	frames = Mem_Realloc( mod->mempool, frames, sizeof(*frames) * (mod->numTextures + 1));
	out = frames + mod->numTextures++;
	spr_frametype = (dframetype_t *)((byte *)(pinframe + 1) + size );

	com.strncpy( out->name, name, 64 );
	out->width = width;
	out->height = height;
	out->image = image;
	out->numframes = 1;
	out->next = NULL;

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
	sprite_t		*psprite;
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
	size = sizeof (sprite_t) + (numframes - 1) * sizeof( psprite->frames );

	psprite = Mem_Alloc( mod->mempool, size );
	mod->extradata = psprite; // make link to extradata
	mod->numTexInfo = surfaceParm = 0; // reset frames
	
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
	mod->textures = frames; // setup texture links
}

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame( ref_entity_t *ent )
{
	sprite_t		*psprite;
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
	vec3_t		vec;

	frame = R_GetSpriteFrame( entity );

	// cull
	if( !r_nocull->integer )
	{
		VectorSubtract( entity->origin, r_refdef.vieworg, vec );
		VectorNormalizeFast( vec );

		if( DotProduct( vec, r_forward ) < 0 )
			return;
	}

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
	sprite_t		*psprite;
	mspriteframe_t	*frame;
	float		angle, sr, cr;
	vec3_t		axis[3], origin;
	float		scale;
	int		i;

	psprite = (sprite_t *)m_pRenderModel->extradata;
	frame = R_GetSpriteFrame( m_pCurrentEntity );
	scale = m_pCurrentEntity->scale;
	VectorSubtract( m_pCurrentEntity->origin, r_forward, origin );

	// setup orientation
	switch( psprite->type )
	{
	case SPR_ORIENTED:
		AxisCopy( m_pCurrentEntity->axis, axis );
		VectorScale( axis[0], -0.01, axis[0] ); // to avoid z-fighting
		VectorSubtract( m_pCurrentEntity->origin, axis[0], m_pCurrentEntity->origin );
		break;
	case SPR_FACING_UPRIGHT:
		// flames and such vertical beam sprite, faces viewer's origin (not the view plane)
		//scale /= sqrt((origin[0] - r_origin[0])*(origin[0] - r_origin[0])+(origin[1] - r_origin[1])*(origin[1] - r_origin[1]));
		VectorSet( axis[1], (origin[1] - r_origin[1]) * scale, -(origin[0] - r_origin[0]) * scale, 0 );
		VectorSet( axis[2], 0, 0, scale );
		VectorNormalize( axis[1] );
		VectorNegate( r_forward, axis[0] );
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		scale = m_pCurrentEntity->scale / sqrt(r_forward[0]*r_forward[0]+r_forward[1]*r_forward[1]);
		VectorSet( axis[2], 0, 0, scale );
		VectorSet( axis[1], -r_origin[1] * scale, r_origin[0] * scale, 0 );
		VectorNegate( r_forward, axis[0] );
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
		angle = m_pCurrentEntity->angles[ROLL] * (M_PI*2/360);
		sr = sin( angle );
		cr = cos( angle );
		for( i = 0; i < 3; i++ )
		{
			axis[0][i] = -r_forward[i];
			axis[1][i] = r_right[i] * cr + r_up[i] * sr;
			axis[2][i] = r_right[i] * -sr + r_up[i] * cr;
		}
		break;
	case SPR_FWD_PARALLEL:
	default: // normal sprite
		VectorNegate( r_forward, axis[0] );
		VectorCopy( r_right, axis[1] );
		VectorCopy( r_up, axis[2] );
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

	vertexArray[numVertex+0][0] = origin[0] + axis[1][0] * frame->right + axis[2][0] * frame->up;
	vertexArray[numVertex+0][1] = origin[1] + axis[1][1] * frame->right + axis[2][1] * frame->up;
	vertexArray[numVertex+0][2] = origin[2] + axis[1][2] * frame->right + axis[2][2] * frame->up;
	vertexArray[numVertex+1][0] = origin[0] + axis[1][0] * frame->left + axis[2][0] * frame->up;
	vertexArray[numVertex+1][1] = origin[1] + axis[1][1] * frame->left + axis[2][1] * frame->up;
	vertexArray[numVertex+1][2] = origin[2] + axis[1][2] * frame->left + axis[2][2] * frame->up;
	vertexArray[numVertex+2][0] = origin[0] + axis[1][0] * frame->left + axis[2][0] * frame->down;
	vertexArray[numVertex+2][1] = origin[1] + axis[1][1] * frame->left + axis[2][1] * frame->down;
	vertexArray[numVertex+2][2] = origin[2] + axis[1][2] * frame->left + axis[2][2] * frame->down;
	vertexArray[numVertex+3][0] = origin[0] + axis[1][0] * frame->right + axis[2][0] * frame->down;
	vertexArray[numVertex+3][1] = origin[1] + axis[1][1] * frame->right + axis[2][1] * frame->down;
	vertexArray[numVertex+3][2] = origin[2] + axis[1][2] * frame->right + axis[2][2] * frame->down;

	inTexCoordArray[numVertex+0][0] = 0;
	inTexCoordArray[numVertex+0][1] = 0;
	inTexCoordArray[numVertex+1][0] = 1;
	inTexCoordArray[numVertex+1][1] = 0;
	inTexCoordArray[numVertex+2][0] = 1;
	inTexCoordArray[numVertex+2][1] = 1;
	inTexCoordArray[numVertex+3][0] = 0;
	inTexCoordArray[numVertex+3][1] = 1;

	for( i = 0; i < 4; i++ )
	{
		VectorCopy( axis[0], normalArray[numVertex] );
		inColorArray[numVertex][0] = m_pCurrentEntity->rendercolor[0];
		inColorArray[numVertex][1] = m_pCurrentEntity->rendercolor[1];
		inColorArray[numVertex][2] = m_pCurrentEntity->rendercolor[2];
		inColorArray[numVertex][3] = m_pCurrentEntity->renderamt;
		numVertex++;
	}

}