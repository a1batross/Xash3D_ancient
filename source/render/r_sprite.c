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
uint		frame_type;
uint		group_num;
string		sp_name;
bool		cull_none = false;
ref_shader_t	**frames = NULL;
	
/*
====================
Sprite model loader
====================
*/
dframetype_t *R_SpriteLoadFrame( ref_model_t *mod, void *pin, mspriteframe_t **ppframe, int framenum )
{
	texture_t		*tex;
	dspriteframe_t	*pinframe;
	mspriteframe_t	*pspriteframe;
	string		name, shadername;

	// build uinque frame name
	if( !sp_name[0] ) FS_FileBase( mod->name, sp_name );
	com.snprintf( name, MAX_STRING, "Sprite( %s_%s_%i%i )", sp_name, frame_prefix, framenum/10, framenum%10 );
	
	pinframe = (dspriteframe_t *)pin;
	SwapBlock((int *)pinframe, sizeof( dspriteframe_t ));

	// setup frame description
	pspriteframe = Mem_Alloc( mod->mempool, sizeof( mspriteframe_t ));
	pspriteframe->width = pinframe->width;
	pspriteframe->height = pinframe->height;
	pspriteframe->up = pinframe->origin[1];
	pspriteframe->left = pinframe->origin[0];
	pspriteframe->down = pinframe->origin[1] - pinframe->height;
	pspriteframe->right = pinframe->width + pinframe->origin[0];
	pspriteframe->radius = com.sqrt((pinframe->width * pinframe->width) + (pinframe->height * pinframe->height));
	tex = R_FindTexture( name, (byte *)pin, pinframe->width * pinframe->height, 0 );
	*ppframe = pspriteframe;

	R_ShaderSetSpriteTexture( tex, cull_none );

	if( frame_type == FRAME_SINGLE )
	{
		com.snprintf( shadername, MAX_STRING, "sprites/%s.spr/%s_%i%i )", sp_name, frame_prefix, framenum/10, framenum%10 );
		pspriteframe->shader = R_LoadShader( shadername, SHADER_SPRITE, true, tex->flags, SHADER_INVALID )->shadernum;
		frames = Mem_Realloc( mod->mempool, frames, sizeof( ref_shader_t* ) * (mod->numshaders + 1));
		frames[mod->numshaders++] = &r_shaders[pspriteframe->shader];
	}
	return (dframetype_t *)((byte *)(pinframe + 1) + pinframe->width * pinframe->height );
}

dframetype_t *R_SpriteLoadGroup( ref_model_t *mod, void * pin, mspriteframe_t **ppframe, int framenum )
{
	dspritegroup_t	*pingroup;
	mspritegroup_t	*pspritegroup;
	dspriteinterval_t	*pin_intervals;
	float		*poutintervals;
	shader_t		group_shader;
	int		i, groupsize, numframes;
	string		shadername;
	void		*ptemp;

	pingroup = (dspritegroup_t *)pin;
	numframes = LittleLong( pingroup->numframes );

	groupsize = sizeof(mspritegroup_t) + (numframes - 1) * sizeof( pspritegroup->frames[0] );
	pspritegroup = Mem_Alloc( mod->mempool, groupsize );
	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;
	pin_intervals = (dspriteinterval_t *)(pingroup + 1);
	poutintervals = Mem_Alloc( mod->mempool, numframes * sizeof( float ));
	pspritegroup->intervals = poutintervals;

	for( i = 0; i < numframes; i++ )
	{
		*poutintervals = LittleFloat( pin_intervals->interval );
		if( *poutintervals <= 0.0 ) *poutintervals = 1.0f; // set error value
		if( frame_type == FRAME_GROUP ) R_ShaderAddSpriteIntervals( *poutintervals );
		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;
	for( i = 0; i < numframes; i++ )
	{
		ptemp = R_SpriteLoadFrame( mod, ptemp, &pspritegroup->frames[i], framenum * 10 + i );
	}

	com.snprintf( shadername, MAX_STRING, "sprites/%s.spr/%s_%i%i", sp_name, frame_prefix, group_num/10, group_num%10 );
	group_shader = R_LoadShader( shadername, SHADER_SPRITE, true, 0, SHADER_INVALID )->shadernum;
	frames = Mem_Realloc( mod->mempool, frames, sizeof( ref_shader_t* ) * (mod->numshaders + 1));
	frames[mod->numshaders++] = &r_shaders[group_shader];

	// apply this shader for all frames in group
	for( i = 0; i < numframes; i++ )
		pspritegroup->frames[i]->shader = group_shader;
	group_num++;

	return (dframetype_t *)ptemp;
}

void Mod_SpriteLoadModel( ref_model_t *mod, ref_model_t *parent, const void *buffer )
{
	dsprite_t		*pin;
	short		*numi;
	msprite_t		*psprite;
	dframetype_t	*pframetype;
	int		i, size, numframes;

	pin = (dsprite_t *)buffer;
	i = LittleLong( pin->version );
		
	if( i != SPRITE_VERSION )
	{
		MsgDev( D_ERROR, "%s has wrong version number (%i should be %i)\n", mod->name, i, SPRITE_VERSION );
		return;
	}

	numframes = LittleLong( pin->numframes );
	size = sizeof (msprite_t) + (numframes - 1) * sizeof( psprite->frames );

	psprite = Mem_Alloc( mod->mempool, size );
	mod->extradata = psprite;	// make link to extradata
	mod->numshaders = 0;	// reset frames
	
	psprite->type = LittleLong( pin->type );
	psprite->rendermode = LittleLong( pin->texFormat );
	psprite->numframes = numframes;
	cull_none = (LittleLong( pin->facetype == SPR_CULL_NONE )) ? true : false;
	mod->mins[0] = mod->mins[1] = -LittleLong( pin->bounds[0] ) / 2;
	mod->maxs[0] = mod->maxs[1] = LittleLong( pin->bounds[0] ) / 2;
	mod->mins[2] = -LittleLong( pin->bounds[1] ) / 2;
	mod->maxs[2] = LittleLong( pin->bounds[1] ) / 2;
	numi = (short *)(pin + 1);

	if( LittleShort( *numi ) == 256 )
	{	
		byte	*src = (byte *)(numi+1);
		rgbdata_t	*pal;
	
		// install palette
		switch( psprite->rendermode )
		{
		case SPR_ADDGLOW:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderGlow );
			break;
		case SPR_ADDITIVE:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderTransAdd );
			break;
                    case SPR_INDEXALPHA:
			pal = FS_LoadImage( "#decal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderTransAlpha );
			break;
		case SPR_ALPHTEST:		
			pal = FS_LoadImage( "#transparent.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderTransAlpha );
                              break;
		case SPR_NORMAL:
		default:
			pal = FS_LoadImage( "#normal.pal", src, 768 );
			R_ShaderSetRenderMode( kRenderNormal );
			break;
		}
		pframetype = (dframetype_t *)(src + 768);
		FS_FreeImage( pal ); // palette installed, no reason to keep this data
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
	mod->touchFrame = tr.registration_sequence;
	frames = NULL; // invalidate pointer
	sp_name[0] = 0;
	group_num = 0;

	for( i = 0; i < numframes; i++ )
	{
		frametype_t frametype = LittleLong( pframetype->type );
		psprite->frames[i].type = frametype;
		frame_type = frametype;
			
		switch( frametype )
		{
		case FRAME_SINGLE:
			com.strncpy( frame_prefix, "one", MAX_STRING );
			pframetype = R_SpriteLoadFrame(mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case FRAME_GROUP:
			com.strncpy( frame_prefix, "grp", MAX_STRING );
			pframetype = R_SpriteLoadGroup(mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		case FRAME_ANGLED:
			com.strncpy( frame_prefix, "ang", MAX_STRING );
			pframetype = R_SpriteLoadGroup(mod, pframetype + 1, &psprite->frames[i].frameptr, i );
			break;
		}
		if( pframetype == NULL ) break; // technically an error
	}
	mod->shaders = frames; // setup texture links
	mod->type = mod_sprite;
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
	frame = (int)ent->frame;

	if((frame >= psprite->numframes) || (frame < 0))
	{
		MsgDev( D_WARN, "R_GetSpriteFrame: no such frame %d (%s)\n", frame, ent->model->name );
		frame = 0;
	}

	if( psprite->frames[frame].type == FRAME_SINGLE )
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if( psprite->frames[frame].type == FRAME_GROUP ) 
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];
		time = RI.refdef.time;

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
	else if( psprite->frames[frame].type == FRAME_ANGLED )
	{
		// e.g. doom-style sprite monsters
		int angleframe = (int)((RI.refdef.viewangles[1] - ent->angles[1])/360*8 + 0.5 - 4) & 7;
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pspriteframe = pspritegroup->frames[angleframe];
	}
	return pspriteframe;
}