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
string	frame_prefix;
byte	*spr_palette;
static byte pal[256][4];
mipTex_t *frames;
	
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
	spr_frame->numLayers = 1;
	spr_frame->numMips = 1;
	*ppframe = pspriteframe;

	pspriteframe->width = spr_frame->width = width;
	pspriteframe->height = spr_frame->height = height;
	origin[0] = LittleLong(pinframe->origin[0]);
	origin[1] = LittleLong(pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];
	pspriteframe->radius = sqrt(((width>>1) * (width>>1)) + ((height>>1) * (height>>1)));
	pspriteframe->shader = r_defaultShader;
	
	// extract sprite name from path
	FS_FileBase( mod->name, name );
	com.strcat(name, va("_%s_%i%i", frame_prefix, framenum/10, framenum%10 ));

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

	frames = Mem_Realloc( mod->mempool, frames, sizeof(*frames) * (mod->numTextures + 1));
	out = frames + mod->numTextures++;
	spr_frametype = (dframetype_t *)((byte *)(pinframe + 1) + size );

	com.strncpy( out->name, name, 64 );
	out->width = width;
	out->height = height;
	out->image = image;

	if( LittleLong( spr_frametype->type ) == SPR_GROUP )
	{
		// FIXME: validate this pointer, probably it's wrong
		// but this link currently unused, just in case
		out->next = frames + mod->numTextures; 
		out->numframes = framenum;
	} 
	else
	{
		out->numframes = 1;
		out->next = NULL;
	}
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
	for (i = 0; i < numframes; i++ )
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
	mod->extradata = psprite; //make link to extradata
	mod->numTexInfo = 0; // reset frames
	
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
			break;
                    case SPR_INDEXALPHA:
			for( i = 0; i < 256; i++ )
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = i;
			}
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

	MsgDev(D_LOAD, "%s, rendermode %d\n", mod->name, psprite->rendermode );

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

bool R_AcceptSpritePass( ref_entity_t *e, int pass )
{
	sprite_t	*psprite = (sprite_t *)m_pRenderModel->extradata;

	if( pass == RENDERPASS_SOLID )
	{
		// pass for solid ents
		if( e->renderfx & RF_TRANSLUCENT ) return false;		// solid sprite with custom blend
		if( psprite->rendermode == SPR_NORMAL ) return true;	// solid sprite
		if( psprite->rendermode == SPR_ADDGLOW ) return false;	// draw it at second pass
		if( psprite->rendermode == SPR_ADDITIVE ) return false;	// must be draw first always
		if( psprite->rendermode == SPR_ALPHTEST ) return false;	// already blended by alphatest
		if( psprite->rendermode == SPR_INDEXALPHA ) return false;	// already blended by alphatest
	}	
	if( pass == RENDERPASS_ALPHA )
	{
		// pass for blended ents
		if( e->renderfx & RF_TRANSLUCENT ) return true;		// solid sprite with custom blend
		if( psprite->rendermode == SPR_NORMAL ) return false;	// solid sprite
		if( psprite->rendermode == SPR_ADDGLOW ) return true;	// can draw
		if( psprite->rendermode == SPR_ADDITIVE ) return true;	// can draw
		if( psprite->rendermode == SPR_ALPHTEST ) return true;	// already drawed
		if( psprite->rendermode == SPR_INDEXALPHA ) return true;	// already drawed
	}
	return true;
}

void R_SpriteSetupLighting( rmodel_t *mod )
{
	int i;
	vec3_t	vlight = {0.0f, 0.0f, -1.0f}; // get light from floor

	if(m_pCurrentEntity->renderfx & RF_FULLBRIGHT)
	{
		for (i = 0; i < 3; i++)
			mod->lightcolor[i] = 1.0f;
	}
	else
	{
		R_LightForPoint( m_pCurrentEntity->origin, mod->lightcolor );

		// doom sprite viewmodels	
		if( m_pCurrentEntity->renderfx & RF_VIEWMODEL )
			r_lightlevel->value = bound(0, VectorLength(mod->lightcolor) * 75.0f, 255); 

	}
}

void R_DrawSpriteModel( int passnum )
{
	mspriteframe_t	*frame;
	vec3_t		point, forward, right, up;
	ref_entity_t 	*e = m_pCurrentEntity;
	rmodel_t		*mod = m_pRenderModel;
	float		alpha = 1.0f, angle, sr, cr;
	vec3_t		distance;
	sprite_t		*psprite;
	int		i;

	if(!R_AcceptSpritePass( e, passnum )) return;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = (sprite_t *)mod->extradata;
	e->frame = fmod( e->frame, psprite->numframes );
	frame = R_GetSpriteFrame( e );

	// merge alpha value
	if( e->renderfx & RF_TRANSLUCENT ) alpha = e->renderamt;
	if( e->scale == 0 ) e->scale = 1.0f; // merge scale

	R_SpriteSetupLighting( mod );
	
	// setup oriented
	switch( psprite->type )
	{
	case SPR_ORIENTED:
		AngleVectors( e->angles, forward, right, up );
		VectorScale( forward, 0.01, forward );// offset for decals
		VectorSubtract( e->origin, forward, e->origin );
		break;
	case SPR_FACING_UPRIGHT:
		up[0] = up[1] = 0;
		up[2] = 1;
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalize( right );
		VectorCopy( r_forward, forward );
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		up[0] = up[1] = 0;
		up[2] = 1;
		VectorCopy( r_up, right );
		VectorCopy( r_forward, forward );
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
		angle = e->angles[ROLL] * (M_PI*2/360);
		sr = sin(angle);
		cr = cos(angle);
		for( i = 0; i < 3; i++ )
		{
			forward[i] = r_forward[i];
			right[i] = r_right[i] * cr + r_up[i] * sr;
			up[i] = r_right[i] * -sr + r_up[i] * cr;
		}
		break;
	case SPR_FWD_PARALLEL:
	default: // normal sprite
		VectorCopy( r_up, up );
		VectorCopy( r_right, right );
		VectorCopy( r_forward, forward );
		break;
	}

	GL_BindTexture( frame->texture );
	GL_TexEnv( GL_MODULATE );

	// setup rendermode
	switch( psprite->rendermode )
	{
	case SPR_NORMAL:
		// solid sprite ignore color and light values
		break;
	case SPR_ADDGLOW:
		//FIXME: get to work
		VectorCopy( r_origin, distance );
		alpha = VectorLength( distance ) * 0.001;
		e->scale = 1/(alpha * 1.5);
		// intentional falltrough
	case SPR_ADDITIVE:
		GL_Enable( GL_BLEND );
		GL_BlendFunc( GL_SRC_ALPHA, GL_ONE );
		pglColor4f( 1.0f, 1.0f, 1.0f, alpha );
		break;
	case SPR_ALPHTEST:
	case SPR_INDEXALPHA:
		GL_Enable( GL_BLEND );
		GL_BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		pglColor4f( mod->lightcolor[0], mod->lightcolor[1], mod->lightcolor[2], 1.0f );
		break;
	}                    			

	// hack the depth range to prevent view model from poking into walls
	if( m_pCurrentEntity->renderfx & RF_DEPTHHACK ) pglDepthRange( 0.0, 0.3 );

	GL_Disable( GL_CULL_FACE );
	pglBegin( GL_QUADS );
		pglTexCoord2f (0, 0);
		VectorMA( e->origin, frame->up * e->scale, up, point  );
		VectorMA( point, frame->left * e->scale, right, point );
		pglVertex3fv( point );

		pglTexCoord2f( 1, 0 );
		VectorMA( e->origin, frame->up * e->scale, up, point  );
		VectorMA( point, frame->right * e->scale, right, point);
		pglVertex3fv( point );

		pglTexCoord2f( 1, 1 );
		VectorMA( e->origin, frame->down * e->scale, up, point);
		VectorMA( point, frame->right * e->scale, right, point);
		pglVertex3fv( point );

          	pglTexCoord2f( 0, 1 );
		VectorMA( e->origin, frame->down * e->scale, up, point);
		VectorMA( point, frame->left * e->scale, right, point );
		pglVertex3fv( point );
	pglEnd();

	// restore states
	GL_Disable( GL_BLEND );
	GL_Enable( GL_DEPTH_TEST );
	GL_TexEnv( GL_REPLACE );
	GL_Enable( GL_CULL_FACE );
	if(e->renderfx & RF_DEPTHHACK) pglDepthRange( 0, 1 );
	pglColor4f( 1, 1, 1, 1 );

}