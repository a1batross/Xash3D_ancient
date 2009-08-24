//=======================================================================
//			Copyright XashXT Group 2008 ©
//		   r_alias.c - Quake1 models loading & drawing
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "quatlib.h"
#include "byteorder.h"

static mesh_t alias_mesh;

static byte	alias_mins[3];
static byte	alias_maxs[3];
static float	alias_radius;
maliashdr_t	*pheader;

daliastexcoord_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes. a frame may be an animating sequence of poses
daliastrivertx_t	*poseverts[MAXALIASFRAMES];
int		posenum;

/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes - Ed
=================
*/

typedef struct
{
	short	x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE	0x1000
#define FLOODFILL_FIFO_MASK	(FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy )				\
{							\
	if( pos[off] == fillcolor )				\
	{						\
		pos[off] = 255;				\
		fifo[inpt].x = x + ( dx );			\
		fifo[inpt].y = y + ( dy );			\
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;	\
	}						\
	else if( pos[off] != 255 ) fdc = pos[off];		\
}

/*
=================
Mod_AliasLoadFrame
=================
*/
void *Mod_AliasLoadFrame( void *pin, maliasframedesc_t *frame )
{
	daliastrivertx_t	*pinframe;
	daliasframe_t	*pdaliasframe;
	int		i;	

	pdaliasframe = (daliasframe_t *)pin;

	com.strncpy( frame->name, pdaliasframe->name, sizeof( frame->name ));
	frame->firstpose = posenum;
	frame->numposes = 1;

	for( i = 0; i < 3; i++ )
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmin.v[i] = pdaliasframe->bboxmax.v[i];

		alias_mins[i] = min( alias_mins[i], frame->bboxmin.v[i] );
		alias_maxs[i] = max( alias_maxs[i], frame->bboxmax.v[i] );
	}

	pinframe = (daliastrivertx_t *)(pdaliasframe + 1);

	poseverts[posenum++] = pinframe;
	pinframe += pheader->numverts;

	return (void *)pinframe;
}

/*
=================
Mod_AliasLoadGroup
=================
*/
void *Mod_AliasLoadGroup( void *pin,  maliasframedesc_t *frame )
{
	daliasgroup_t	*pingroup;
	int		i, numframes;
	daliasinterval_t	*pin_intervals;
	void		*ptemp;
	
	pingroup = (daliasgroup_t *)pin;
	numframes = LittleLong( pingroup->numframes );

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for( i = 0; i < 3; i++ )
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmin.v[i] = pingroup->bboxmax.v[i];

		alias_mins[i] = min( alias_mins[i], frame->bboxmin.v[i] );
		alias_maxs[i] = max( alias_maxs[i], frame->bboxmax.v[i] );
	}

	pin_intervals = (daliasinterval_t *)(pingroup + 1);
	frame->interval = LittleFloat( pin_intervals->interval );
	pin_intervals += numframes;
	ptemp = (void *)pin_intervals;

	for( i = 0; i < numframes; i++ )
	{
		poseverts[posenum++] = (daliastrivertx_t *)((daliasframe_t *)ptemp + 1);
		ptemp = (daliastrivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}
	return ptemp;
}

void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte		fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t	fifo[FLOODFILL_FIFO_SIZE];
	int		inpt = 0, outpt = 0;
	int		filledcolor = -1;
	rgbdata_t		*pal = FS_LoadImage( "#normal.pal", NULL, 768 );	// null buffer force to get Q1 palette
	uint		*d_8to24table = (uint *)pal->palette;
	int		i;

	if( filledcolor == -1 )
	{
		filledcolor = 0;
		// attempt to find opaque black
		for( i = 0; i < 256; ++i )
		{
			if( d_8to24table[i] == (255<<0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if(( fillcolor == filledcolor ) || ( fillcolor == 255 )) return;

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while( outpt != inpt )
	{
		int	x = fifo[outpt].x, y = fifo[outpt].y;
		int	fdc = filledcolor;
		byte	*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if( x > 0 ) FLOODFILL_STEP( -1, -1, 0 );
		if( x < skinwidth - 1 )  FLOODFILL_STEP( 1, 1, 0 );
		if( y > 0 ) FLOODFILL_STEP( -skinwidth, 0, -1 );
		if( y < skinheight - 1 ) FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
	FS_FreeImage( pal );
}

/*
===============
Mod_AliasLoadSkins
===============
*/
void *Mod_AliasLoadSkins( ref_model_t *mod, int numskins, daliasskintype_t *pskintype )
{
	int			i, j, k, s;
	string			shadername;
	byte			*skin;
	byte			*texels;
	int			groupskins;
	daliasskingroup_t		*pinskingroup;
	daliasskininterval_t	*pinskinintervals;
	
	skin = (byte *)(pskintype + 1);

	if( numskins < 1 || numskins > MAX_SKINS )
		Host_Error( "Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins );

	s = pheader->skinwidth * pheader->skinheight;

	for( i = 0; i < numskins; i++ )
	{
		if( pskintype->type == SKIN_SINGLE )
		{
			Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );

			// save 8 bit texels for the player model to remap
			texels = Mod_Malloc( mod, s );
			pheader->texels[i] = texels - (byte *)pheader;
			Mem_Copy( texels, (byte *)(pskintype + 1), s );
			com.sprintf( shadername, "%s_%i", mod->name, i );
			pheader->skins[i][0] = pheader->skins[i][1] = pheader->skins[i][2] =
			pheader->skins[i][3] = R_LoadShader( shadername, SHADER_ALIAS, 0, 0, -1 );
			pskintype = (daliasskintype_t *)((byte *)(pskintype + 1) + s );
		}
		else
		{
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong( pinskingroup->numskins );
			pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

			pskintype = (void *)(pinskinintervals + groupskins);

			for( j = 0; j < groupskins; j++ )
			{
				Mod_FloodFillSkin( skin, pheader->skinwidth, pheader->skinheight );
				if( j == 0 )
				{
					texels = Mod_Malloc( mod, s );
					pheader->texels[i] = texels - (byte *)pheader;
					Mem_Copy( texels, (byte *)(pskintype), s );
				}
				com.sprintf( shadername, "%s_%i_%i", mod->name, i, j );
				pheader->skins[i][j&3] = R_LoadShader( shadername, SHADER_ALIAS, 0, 0, -1 );
				pskintype = (daliasskintype_t *)((byte *)(pskintype) + s );
			}
			for( k = j; j < 4; j++ )
				pheader->skins[i][j&3] = pheader->skins[i][j - k]; 
		}
	}
	return (void *)pskintype;
}

/*
=================
Mod_QAliasLoadModel
=================
*/
void Mod_QAliasLoadModel( ref_model_t *mod, ref_model_t *parent, const void *buffer )
{
	int		i, j;
	daliashdr_t	*pinmodel;
	daliastexcoord_t	*pinstverts;
	daliastriangle_t	*pintriangles;
	int		version, numframes;
	int		size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;

	pinmodel = (daliashdr_t *)buffer;

	version = LittleLong( pinmodel->version );
	if( version != QALIAS_VERSION )
		Host_Error( "%s has wrong version number (%i should be %i)\n", mod->name, version, QALIAS_VERSION );

	// allocate space for a working header, plus all the data except the frames,
	// skin and group info
	size = sizeof( maliashdr_t ) + (LittleLong( pinmodel->numframes ) - 1) * sizeof( pheader->frames[0] );
	mod->extradata = pheader = Mod_Malloc( mod, size );
	mod->type = mod_alias;
	
	// endian-adjust and copy the data, starting with the alias model header
	pheader->flags = LittleLong( pinmodel->flags );
	pheader->boundingradius = LittleFloat( pinmodel->boundingradius );
	pheader->numskins = LittleLong( pinmodel->numskins );
	pheader->skinwidth = LittleLong( pinmodel->skinwidth );
	pheader->skinheight = LittleLong( pinmodel->skinheight );

	if( pheader->skinheight > MAX_LBM_HEIGHT || pheader->skinwidth > MAX_LBM_WIDTH )
		Host_Error( "model %s has a skin taller than %dx%d\n", mod->name, MAX_LBM_WIDTH, MAX_LBM_HEIGHT );

	pheader->numverts = LittleLong( pinmodel->numverts );

	if( pheader->numverts <= 0 ) Host_Error( "model %s has no vertices\n", mod->name );
	if( pheader->numverts > MAXALIASVERTS ) Host_Error( "model %s has too many vertices\n", mod->name );
	pheader->numtris = LittleLong( pinmodel->numtris );
	if( pheader->numtris <= 0 ) Host_Error( "model %s has no triangles\n", mod->name );

	pheader->numframes = LittleLong( pinmodel->numframes );
	numframes = pheader->numframes;
	if( numframes < 1 ) Host_Error( "Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes );

	pheader->size = LittleFloat( pinmodel->size ) * (1.0f / 11.0f);
	pheader->synctype = LittleLong( pinmodel->synctype );
	pheader->numframes = pheader->numframes;

	for( i = 0; i < 3; i++ )
	{
		pheader->scale[i] = LittleFloat( pinmodel->scale[i] );
		pheader->scale_origin[i] = LittleFloat( pinmodel->scale_origin[i] );
		pheader->eyeposition[i] = LittleFloat( pinmodel->eyeposition[i] );
	}

	// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_AliasLoadSkins( mod, pheader->numskins, pskintype );

	// load base s and t vertices
	pinstverts = (daliastexcoord_t *)pskintype;

	for( i = 0; i < pheader->numverts; i++ )
	{
		stverts[i].onseam = LittleLong( pinstverts[i].onseam );
		stverts[i].s = LittleLong( pinstverts[i].s );
		stverts[i].t = LittleLong( pinstverts[i].t );
	}

	// load triangle lists
	pintriangles = (daliastriangle_t *)&pinstverts[pheader->numverts];

	for( i = 0; i < pheader->numtris; i++)
	{
		triangles[i].facesfront = LittleLong( pintriangles[i].facesfront );

		for( j = 0; j < 3; j++ )
			triangles[i].vertindex[j] = LittleLong( pintriangles[i].vertindex[j] );
	}

	// load the frames
	posenum = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	alias_mins[0] = alias_mins[1] = alias_mins[2] = 0xFF;
	alias_maxs[0] = alias_maxs[1] = alias_maxs[2] = 0x00;

	for( i = 0; i < numframes; i++ )
	{
		frametype_t	frametype;

		frametype = LittleLong( pframetype->type );

		switch( frametype )
		{
		case FRAME_SINGLE:
			pframetype = (daliasframetype_t *)Mod_AliasLoadFrame( pframetype+1, &pheader->frames[i] );
			break;
		case FRAME_GROUP:
			pframetype = (daliasframetype_t *)Mod_AliasLoadGroup( pframetype+1, &pheader->frames[i] );
			break;
		}
		if( pframetype == NULL ) break; // technically an error
	}

	pheader->numposes = posenum;

	for( i = 0; i < 3; i++ )
	{
		mod->mins[i] = alias_mins[i] * pheader->scale[i] + pheader->scale_origin[i];
		mod->maxs[i] = alias_maxs[i] * pheader->scale[i] + pheader->scale_origin[i];
	}
	mod->radius = RadiusFromBounds( mod->mins, mod->maxs );
	mod->touchFrame = tr.registration_sequence; // register model
}