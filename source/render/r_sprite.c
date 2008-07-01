//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        r_sprite.c - render sprite models
//=======================================================================

#include "gl_local.h"

/*
=============================================================

  SPRITE MODELS

=============================================================
*/
string	frame_prefix;

/*
====================
Sprite model loader
====================
*/
dframetype_t *R_SpriteLoadFrame( model_t *mod, void *pin, mspriteframe_t **ppframe, int framenum )
{
	dframe_t		*pinframe;
	mspriteframe_t	*pspriteframe;
	int		width, height, size, origin[2];
	char		name[64];
	rgbdata_t		*spr_frame;
	image_t		*image;
	
	pinframe = (dframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height * 4;
          
	pspriteframe = Mem_Alloc(mod->mempool, sizeof (mspriteframe_t));
	memset (pspriteframe, 0, sizeof (mspriteframe_t));
	spr_frame = (rgbdata_t *)Mem_Alloc( mod->mempool, sizeof(rgbdata_t));
	*ppframe = pspriteframe;

	pspriteframe->width = spr_frame->width = width;
	pspriteframe->height = spr_frame->height = height;
	origin[0] = LittleLong(pinframe->origin[0]);
	origin[1] = LittleLong(pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];
          pspriteframe->texnum = 0;
	spr_frame->type = PF_RGBA_32;
	spr_frame->flags = IMAGE_HAS_ALPHA;
	spr_frame->numMips = 1;
	
	// extract sprite name from path
	FS_FileBase( mod->name, name );
	com.strcat(name, va("_%s_%i%i", frame_prefix, framenum/10, framenum%10 ));
	spr_frame->size = width * height * 4; // for bounds checking
	spr_frame->buffer = (byte *)Mem_Alloc( mod->mempool, spr_frame->size );

	if(!VFS_Unpack((byte *)(pinframe + 1), pinframe->compsize, &spr_frame->buffer, spr_frame->size ))
	{
		Image->FreeImage( spr_frame );
		MsgDev(D_WARN, "R_SpriteLoadFrame: %s probably corrupted\n", name );
		return (void *)((byte *)(pinframe + 1) + pinframe->compsize);
	}

	image = R_LoadImage( name, spr_frame, it_sprite );
	if( image )
	{
		pspriteframe->texnum = image->texnum[0];
		mod->skins[mod->numtexinfo] = image;
		mod->numtexinfo++;
	}
          else MsgDev(D_WARN, "%s has null frame %d\n", image->name, framenum );

	Image->FreeImage( spr_frame );          
	return (dframetype_t *)((byte *)(pinframe + 1) + pinframe->compsize);
}

dframetype_t *R_SpriteLoadGroup (model_t *mod, void * pin, mspriteframe_t **ppframe, int framenum )
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

void R_SpriteLoadModel( model_t *mod, void *buffer )
{
	dsprite_t		*pin;
	msprite_t		*psprite;
	dframetype_t	*pframetype;
	int		i, size, numframes;
	
	pin = (dsprite_t *)buffer;
	i = LittleLong(pin->version);
		
	if( i != SPRITE_VERSION)
	{
		Msg("Warning: %s has wrong version number (%i should be %i)\n", mod->name, i, SPRITE_VERSION );
		return;
	}

	numframes = LittleLong (pin->numframes);
	size = sizeof (msprite_t) + (numframes - 1) * sizeof (psprite->frames);

	psprite = Mem_Alloc(mod->mempool, size );
	mod->extradata = psprite; //make link to extradata
	mod->numtexinfo = 0; // reset frames
	
	psprite->type	= LittleLong(pin->type);
	psprite->rendermode = LittleLong(pin->rendermode);
	psprite->numframes	= numframes;
	mod->mins[0] = mod->mins[1] = -LittleLong(pin->bounds[0]) / 2;
	mod->maxs[0] = mod->maxs[1] = LittleLong(pin->bounds[0]) / 2;
	mod->mins[2] = -LittleLong(pin->bounds[1]) / 2;
	mod->maxs[2] = LittleLong(pin->bounds[1]) / 2;
	pframetype = (dframetype_t *)(pin + 1);

	MsgDev(D_LOAD, "%s, rendermode %d\n", mod->name, psprite->rendermode );
		
	if(numframes < 1)
	{
		MsgDev( D_ERROR, "%s has invalid # of frames: %d\n", mod->name, numframes );
		return;
	}	

	mod->registration_sequence = registration_sequence;

	for (i = 0; i < numframes; i++ )
	{
		frametype_t frametype = LittleLong (pframetype->type);
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
		if(pframetype == NULL) break; // technically an error
	}
}

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int		i, numframes, frame;
	float		*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->extradata;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		MsgDev(D_WARN, "R_GetSpriteFrame: no such frame %d (%s)\n", frame, currententity->model->name);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if (psprite->frames[frame].type == SPR_GROUP) 
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];
		time = r_newrefdef.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++)
		{
			if (pintervals[i] > targettime)
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	else if (psprite->frames[frame].type == SPR_ANGLED)
	{
		// e.g. doom-style sprite monsters
		int angleframe = (int)((r_newrefdef.viewangles[1] - currententity->angles[1])/360*8 + 0.5 - 4) & 7;
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[angleframe];
	}
	return pspriteframe;
}

bool R_AcceptSpritePass( entity_t *e, int pass )
{
	msprite_t	*psprite = (msprite_t *)currentmodel->extradata;

	if(pass == RENDERPASS_SOLID)
	{
		// pass for solid ents
		if(psprite->rendermode == SPR_SOLID) return true;	// solid sprite
		if(psprite->rendermode == SPR_GLOW) return false;	// draw it at second pass
		if(psprite->rendermode == SPR_ADDITIVE) return false;	// must be draw first always
		if(psprite->rendermode == SPR_ALPHA) return false;	// already blended by alphatest
	}	
	if(pass == RENDERPASS_ALPHA)
	{
		// pass for blended ents
		if(e->flags & RF_TRANSLUCENT) return true;		// solid sprite with custom blend
		if(psprite->rendermode == SPR_SOLID) return false;	// solid sprite
		if(psprite->rendermode == SPR_GLOW) return true;		// can draw
		if(psprite->rendermode == SPR_ADDITIVE) return true;	// can draw
		if(psprite->rendermode == SPR_ALPHA) return true;		// already drawed
	}
	return true;
}

void R_SpriteSetupLighting( model_t *mod )
{
	int i;
	vec3_t	vlight = {0.0f, 0.0f, -1.0f}; // get light from floor

	if(currententity->flags & RF_FULLBRIGHT)
	{
		for (i = 0; i < 3; i++)
			mod->lightcolor[i] = 1.0f;
	}
	else
	{
		R_LightPoint( currententity->origin, mod->lightcolor );

		// doom sprite models	
		if( currententity->flags & RF_VIEWMODEL )
			r_lightlevel->value = bound(0, VectorLength(mod->lightcolor) * 75.0f, 255); 

	}
}

void R_DrawSpriteModel( int passnum )
{
	mspriteframe_t	*frame;
	vec3_t		point, forward, right, up;
	entity_t 		*e = currententity;
	model_t		*mod = currentmodel;
	float		alpha = 1.0f, angle, sr, cr;
	vec3_t		distance;
	msprite_t		*psprite;
	int		i;

	if(!R_AcceptSpritePass( e, passnum )) return;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = (msprite_t *)mod->extradata;
	e->frame = fmod(e->frame, psprite->numframes);
	frame = R_GetSpriteFrame(e);

	// merge alpha value
	if( e->flags & RF_TRANSLUCENT ) alpha = e->alpha;
	if( e->scale == 0 ) e->scale = 1.0f; // merge scale

	R_SpriteSetupLighting( mod );
	
	// setup oriented
	switch( psprite->type )
	{
	case SPR_ORIENTED:
		AngleVectors(e->angles, forward, right, up);
		VectorScale(forward, 0.01, forward );// offset for decals
		VectorSubtract(e->origin, forward, e->origin );
		break;
	case SPR_FACING_UPRIGHT:
		up[0] = up[1] = 0;
		up[2] = 1;
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalize (right);
		VectorCopy (vforward, forward);
		break;
	case SPR_FWD_PARALLEL_UPRIGHT:
		up[0] = up[1] = 0;
		up[2] = 1;
		VectorCopy (vup, right);
		VectorCopy (vforward, forward);
		break;
	case SPR_FWD_PARALLEL_ORIENTED:
		angle = e->angles[ROLL] * (M_PI*2/360);
		sr = sin(angle);
		cr = cos(angle);
		for (i = 0; i < 3; i++)
		{
			forward[i] = vforward[i];
			right[i] = vright[i] * cr + vup[i] * sr;
			up[i] = vright[i] * -sr + vup[i] * cr;
		}
		break;
	case SPR_FWD_PARALLEL:
	default: // normal sprite
		VectorCopy (vup, up);
		VectorCopy (vright, right);
		VectorCopy (vforward, forward);
		break;
	}

	GL_Bind( frame->texnum );
	GL_TexEnv( GL_MODULATE );

	// setup rendermode
	switch( psprite->rendermode )
	{
	case SPR_SOLID:
		// solid sprite ignore color and light values
		break;
	case SPR_GLOW:
		VectorCopy( r_origin, distance );
		alpha = VectorLength( distance ) * 0.001;
		e->scale = 1/(alpha * 1.5);
		// intentional falltrough
	case SPR_ADDITIVE:
		GL_EnableBlend();
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE);
		qglColor4f( 1.0f, 1.0f, 1.0f, alpha );
		break;
	case SPR_ALPHA:
		GL_EnableBlend();
		qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		qglColor4f( mod->lightcolor[0], mod->lightcolor[1], mod->lightcolor[2], 1.0f );
		break;
	}                    			

	if (currententity->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		qglDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax-gldepthmin));

	qglDisable(GL_CULL_FACE);
	qglBegin (GL_QUADS);
		qglTexCoord2f (0, 0);
		VectorMA (e->origin, frame->up * e->scale, up, point);
		VectorMA (point, frame->left * e->scale, right, point);
		qglVertex3fv (point);

		qglTexCoord2f (1, 0);
		VectorMA (e->origin, frame->up * e->scale, up, point);
		VectorMA (point, frame->right * e->scale, right, point);
		qglVertex3fv (point);

		qglTexCoord2f (1, 1);
		VectorMA (e->origin, frame->down * e->scale, up, point);
		VectorMA (point, frame->right * e->scale, right, point);
		qglVertex3fv (point);

          	qglTexCoord2f (0, 1);
		VectorMA (e->origin, frame->down * e->scale, up, point);
		VectorMA (point, frame->left * e->scale, right, point);
		qglVertex3fv (point);
	qglEnd();

	// restore states
	GL_DisableBlend();
	GL_EnableDepthTest();
	GL_TexEnv(GL_REPLACE);
	qglEnable(GL_CULL_FACE);
	if(e->flags & RF_DEPTHHACK) qglDepthRange (gldepthmin, gldepthmax);
	qglColor4f( 1, 1, 1, 1 );

}

