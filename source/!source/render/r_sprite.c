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

/*
====================
Sprite model loader
====================
*/
void *R_SpriteLoadFrame (model_t *mod, void *pin, mspriteframe_t **ppframe, int framenum, byte *pal )
{
	dspriteframe_t	*pinframe;
	mspriteframe_t	*pspriteframe;
	int		width, height, size, origin[2];
	char		name[64];
          image_t		*image;
	
	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;
          
	pspriteframe = Mem_Alloc(mod->mempool, sizeof (mspriteframe_t));
	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];
          pspriteframe->texnum = 0;
	
	//extract sprite name from path
	ri.FS_FileBase( mod->name, name );
	strcat(name, va("_%i", framenum));

	image = R_LoadImage( name, (byte *)(pinframe + 1), pal, width, height, it_sprite, 32, true );
	
	if(image)
	{
		pspriteframe->texnum = image->texnum;
		mod->skins[framenum] = image;
          }
          else Msg("Warning: %s has null frame %d\n", image->name, framenum );
          
	return (void *)((byte *)(pinframe+1) + size);
}

void *R_SpriteLoadGroup (model_t *mod, void * pin, mspriteframe_t **ppframe, int framenum, byte *pal )
{
	dspritegroup_t	*pingroup;
	mspritegroup_t	*pspritegroup;
	int		i, numframes;
	dspriteinterval_t	*pin_intervals;
	float		*poutintervals;
	void		*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Mem_Alloc(mod->mempool, sizeof (mspritegroup_t) + (numframes - 1) * sizeof (pspritegroup->frames[0]));
	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = Mem_Alloc(mod->mempool, numframes * sizeof (float));

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			ri.Sys_Error (ERR_DROP, "%s has interval<=0\n", mod->name );
		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i = 0; i < numframes; i++ )
	{
		ptemp = R_SpriteLoadFrame(mod, ptemp, &pspritegroup->frames[i], framenum + i, pal);
	}
	return ptemp;
}

void R_SpriteLoadModel( model_t *mod, void *buffer )
{
	int		i;
	int		version;
	dsprite_t		*pin;
	msprite_t		*psprite;
	int		numframes;
	int		size;
	dspriteframetype_t	*pframetype;
	int		rendertype = 0;
	byte		pal[256][4];
	int		sptype;
	
	pin = (dsprite_t *)buffer;
	version = LittleLong (pin->version);
		
	if (version != SPRITE_VERSION)
		ri.Sys_Error (ERR_DROP, "%s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION);

	sptype = LittleLong (pin->type);
	rendertype = LittleLong (pin->texFormat);
	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) + (numframes - 1) * sizeof (psprite->frames);

	psprite = Mem_Alloc(mod->mempool, size );
	mod->extradata = psprite; //make link to extradata
          psprite->type = sptype;
	
	psprite->type	= LittleLong(pin->type);
	psprite->maxwidth	= LittleLong (pin->width);
	psprite->maxheight	= LittleLong (pin->height);
	psprite->beamlength	= LittleFloat (pin->beamlength);//remove this ?
	psprite->numframes	= numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;
	
	if (pin->version == SPRITE_VERSION)
	{	
		int i;
		short *numi = (short*)(pin+1);
		byte *src = (unsigned char *)(numi+1);
		
		if (LittleShort(*numi) != 256)
			ri.Sys_Error (ERR_DROP, "%s has unexpected number of palette colors %i (should be 256)", mod->name, numi);

		switch( rendertype )
		{
		case SPR_NORMAL:
			for (i = 0; i < 256; i++)
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 0;
			}
			break;
		case SPR_ADDITIVE:
			for (i = 0; i < 256; i++)
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 255;
			}
			break;
                    case SPR_INDEXALPHA:
			for (i = 0; i < 256; i++)
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = i;
			}
			break;
		case SPR_ALPHTEST:		
			for (i = 0; i < 256; i++)
			{
				pal[i][0] = *src++;
				pal[i][1] = *src++;
				pal[i][2] = *src++;
				pal[i][3] = 255;
			}
			pal[255][0] = pal[255][1] = pal[255][2] = pal[255][3] = 0;
                              break;
		default:
			ri.Sys_Error (ERR_DROP, "%s has unknown texFormat (%i, should be 0, 1, 2, or 3)", mod->name, rendertype);
		}
		pframetype = (dspriteframetype_t *)(src);
	}
		
	if(numframes < 1)			
		ri.Sys_Error (ERR_DROP, "%s has invalid # of frames: %d\n", mod->name, numframes );
	
	mod->numframes = numframes;

	for (i = 0; i < numframes; i++ )
	{
		frametype_t frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)R_SpriteLoadFrame(mod, pframetype + 1, &psprite->frames[i].frameptr, i, (byte *)(&pal[0][0]));
		}
		else if(frametype == SPR_GROUP)
		{
			pframetype = (dspriteframetype_t *)R_SpriteLoadGroup(mod, pframetype + 1, &psprite->frames[i].frameptr, i, (byte *)(&pal[0][0]));
		}
		if(pframetype == NULL) break;                                                   
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
		Com_Printf ("R_DrawSprite: no such frame %d (%s)\n", frame, currententity->model->name);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = r_newrefdef.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


void R_SpriteDrawModel( int passnum )
{
	float alpha = 1.0F;
	mspriteframe_t	*frame;
	vec3_t		point, forward, right, up;
	msprite_t		*psprite;
	entity_t 		*e = currententity;

	if ( (e->flags & RF_TRANSLUCENT) && (passnum == RENDERPASS_SOLID)) return;// solid
	if (!(e->flags & RF_TRANSLUCENT) && (passnum == RENDERPASS_ALPHA)) return;// solid
	
	// don't even bother culling, because it's just a single
	// polygon without a surface cache

	psprite = (msprite_t *)currentmodel->extradata;

	e->frame %= psprite->numframes;
	frame = R_GetSpriteFrame(e);
          
          //Msg("render frame %d bindnum %d\n", e->frame, currentmodel->skins[e->frame]->texnum );
	
	switch( psprite->type )
	{
	case SPR_ORIENTED:
		AngleVectors (e->angles, forward, right, up);
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
	case SPR_VP_PARALLEL_UPRIGHT:
		up[0] = up[1] = 0;
		up[2] = 1;
		VectorCopy (vup, right);
		VectorCopy (vforward, forward);
		break;
	default:
		// normal sprite
		VectorCopy (vup, up);
		VectorCopy (vright, right);
		VectorCopy (vforward, forward);
		break;
	}
	
	if ( e->flags & RF_TRANSLUCENT ) alpha = e->alpha;
	if ( alpha != 1.0F ) qglEnable( GL_BLEND );
	qglColor4f( 1, 1, 1, alpha );
          
          //qglDisable(GL_DEPTH_TEST);
	
	GL_Bind(currentmodel->skins[e->frame]->texnum);
	GL_TexEnv( GL_MODULATE );

	if ( alpha == 1.0 ) qglEnable (GL_ALPHA_TEST);
	else qglDisable( GL_ALPHA_TEST );
        
	qglDisable(GL_CULL_FACE);
	qglBegin (GL_QUADS);
          
          qglTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	qglVertex3fv (point);

	qglTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	qglVertex3fv (point);
	
	qglEnd ();

	qglDisable(GL_BLEND);
	qglDisable (GL_ALPHA_TEST);
	qglEnable(GL_DEPTH_TEST);
	GL_TexEnv( GL_REPLACE );

	if ( alpha != 1.0F ) qglDisable( GL_BLEND );
	qglColor4f( 1, 1, 1, 1 );
}

