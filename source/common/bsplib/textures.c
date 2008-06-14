
#include "bsplib.h"

int		nummiptex;
extern int	g_mapversion;
textureref_t	textureref[MAX_MAP_TEXTURES];

//==========================================================================


int FindMiptex( char *name )
{
	int		i;
	shader_t		*si;
	rgbdata_t		*tex;
	
	for (i = 0; i < nummiptex; i++ )
	{
		if (!stricmp(name, textureref[i].texname))
			return i;
	}
	if( nummiptex == MAX_MAP_TEXTURES ) Sys_Break("MAX_MAP_TEXTURES limit exceeds\n");

	// register texture
	strcpy (textureref[i].texname, name );
	tex = Image->LoadImage( textureref[i].texname, NULL, 0 );
	if( tex )
	{
		textureref[i].size[0] = tex->width;		
		textureref[i].size[1] = tex->height;
		Image->FreeImage( tex );
	}
	else textureref[i].size[0] = textureref[i].size[1] = -1; // technically an error

	si = FindShader( name );
	if(si)
	{
		textureref[i].value = LittleLong (si->intensity);
		textureref[i].flags = LittleLong (si->surfaceFlags);
		textureref[i].contents = LittleLong (si->contents);			
		com.strcpy(textureref[i].animname, si->nextframe);
	}
	nummiptex++;

	if (textureref[i].animname[0]) 
	{
		// MsgDev(D_INFO, "FindMiptex: animation chain \"%s->%s\"\n", textureref[i].name, textureref[i].animname );
		FindMiptex(textureref[i].texname);
	}
	return i;
}


/*
==================
textureAxisFromPlane
==================
*/
vec3_t	baseaxis[18] =
{
{0,0,1}, {1,0,0}, {0,-1,0},			// floor
{0,0,-1}, {1,0,0}, {0,-1,0},			// ceiling
{1,0,0}, {0,1,0}, {0,0,-1},			// west wall
{-1,0,0}, {0,1,0}, {0,0,-1},			// east wall
{0,1,0}, {1,0,0}, {0,0,-1},			// south wall
{0,-1,0}, {1,0,0}, {0,0,-1}			// north wall
};

void TextureAxisFromPlane(plane_t *pln, vec3_t xv, vec3_t yv)
{
	int		bestaxis;
	vec_t	dot,best;
	int		i;
	
	best = 0;
	bestaxis = 0;
	
	for (i=0 ; i<6 ; i++)
	{
		dot = DotProduct (pln->normal, baseaxis[i*3]);
		if (dot > best)
		{
			best = dot;
			bestaxis = i;
		}
	}
	
	VectorCopy (baseaxis[bestaxis*3+1], xv);
	VectorCopy (baseaxis[bestaxis*3+2], yv);
}

int TexinfoForBrushTexture (plane_t *plane, brush_texture_t *bt, vec3_t origin)
{
	vec3_t		vecs[2];
	int		sv, tv;
	vec_t		ang, sinv, cosv;
	vec_t		ns, nt;
	dsurfdesc_t	tx, *tc;
	int		i, j, k;
	brush_texture_t	anim;
	int		mt;

	if (!bt->name[0]) return 0;

	memset (&tx, 0, sizeof(tx));
	tx.texid = GetIndexFromTable( bt->name );

	if (bt->txcommand)
	{
		Mem_Copy(tx.vecs, bt->vects.quark.vects, sizeof(tx.vecs));

		if (origin[0] || origin[1] || origin[2])
		{
			tx.vecs[0][3] += DotProduct(origin, tx.vecs[0]);
			tx.vecs[1][3] += DotProduct(origin, tx.vecs[1]);
		}
		tx.flags = bt->flags;
		tx.value = bt->value;
		tx.size[0] = bt->size[0];
		tx.size[1] = bt->size[1];
	}
	else
	{
		if (g_mapversion < VALVE_FORMAT) TextureAxisFromPlane(plane, vecs[0], vecs[1]);
		if (!bt->vects.valve.scale[0]) bt->vects.valve.scale[0] = 1;
		if (!bt->vects.valve.scale[1]) bt->vects.valve.scale[1] = 1;

		if (g_mapversion < VALVE_FORMAT)
		{
			// rotate axis
			if (bt->vects.valve.rotate == 0)
			{
				sinv = 0;
				cosv = 1;
			}
			else if (bt->vects.valve.rotate == 90)
			{
				sinv = 1;
				cosv = 0;
			}
			else if (bt->vects.valve.rotate == 180)
			{
				sinv = 0;
				cosv = -1;
			}
			else if (bt->vects.valve.rotate == 270)
			{
				sinv = -1;
				cosv = 0;
			}
			else
			{
				ang = bt->vects.valve.rotate / 180 * M_PI;
				sinv = sin(ang);
				cosv = cos(ang);
			}
			if (vecs[0][0]) sv = 0;
			else if (vecs[0][1]) sv = 1;
			else sv = 2;

			if (vecs[1][0]) tv = 0;
			else if (vecs[1][1]) tv = 1;
			else tv = 2;
			
			for (i = 0; i < 2; i++)
			{
				ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
				nt = sinv * vecs[i][sv] + cosv * vecs[i][tv];
				vecs[i][sv] = ns;
				vecs[i][tv] = nt;
			}

			for (i = 0; i < 2; i++)
			{
				for (j = 0; j < 3; j++)
				{
					tx.vecs[i][j] = vecs[i][j] / bt->vects.valve.scale[i];
				}
			}
		}
		else
		{
			vec_t           scale;

			scale = 1 / bt->vects.valve.scale[0];
			VectorScale(bt->vects.valve.UAxis, scale, tx.vecs[0]);
			scale = 1 / bt->vects.valve.scale[1];
			VectorScale(bt->vects.valve.VAxis, scale, tx.vecs[1]);
		}

		tx.vecs[0][3] = bt->vects.valve.shift[0] + DotProduct(origin, tx.vecs[0]);
		tx.vecs[1][3] = bt->vects.valve.shift[1] + DotProduct(origin, tx.vecs[1]);
		tx.flags = bt->flags;
		tx.value = bt->value;
		tx.size[0] = bt->size[0];
		tx.size[1] = bt->size[1];
	}

	// find the texinfo
	tc = texinfo;
	for (i = 0; i < numtexinfo; i++, tc++)
	{
		if (tc->flags != tx.flags) continue;
		if (tc->value != tx.value) continue;
		for (j = 0; j < 2; j++)
		{
			if( tc->texid != tx.texid ) goto skip;
			for( k = 0; k < 4; k++ )
			{
				if (tc->vecs[j][k] != tx.vecs[j][k])
					goto skip;
			}
		}
		return i;
skip:;
	}
	*tc = tx;
	numtexinfo++;

	// load the next animation
	mt = FindMiptex( bt->name );

	if (textureref[mt].animname[0])
	{
		anim = *bt;
		strcpy (anim.name, textureref[mt].animname);
		tc->animid = TexinfoForBrushTexture (plane, &anim, origin);
	}
	else tc->animid = -1;

	return i;
}
