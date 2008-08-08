//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        studio.c - half-life model compiler
//=======================================================================

#include "mdllib.h"

bool		cdset;
bool		ignore_errors;

byte		*studiopool;
byte		*pData;
byte		*pStart;
char		modeloutname[MAX_SYSPATH];

int		numrep;
int		flip_triangles;
int		dump_hboxes;
int		gflags;

int		cdtextureset;
int		clip_texcoords;
int		split_textures;
int		numseq;
int		nummirrored;
int		numani;
int		numxnodes;
int		numrenamedbones;
int		totalframes = 0;
int		xnode[100][100];
int		numbones;
int		numhitboxes;
int		numhitgroups;
int		numattachments;
int		numtextures;
int		numskinref;
int		numskinfamilies;
int		numbonecontrollers;
int		used[MAXSTUDIOTRIANGLES];
int		skinref[256][MAXSTUDIOSKINS]; // [skin][skinref], returns texture index
int		striptris[MAXSTUDIOTRIANGLES+2];
int		stripverts[MAXSTUDIOTRIANGLES+2];
short		commands[MAXSTUDIOTRIANGLES * 13];
int		neighbortri[MAXSTUDIOTRIANGLES][3];
int		neighboredge[MAXSTUDIOTRIANGLES][3];
int		numtexturegroups;
int		numtexturelayers[32];
int		numtexturereps[32];
int		texturegroup[32][32][32];
int		numcommandnodes;
int		nummodels;
int		numbodyparts;
int		stripcount;
int		numcommands;
int		linecount;
int		allverts;
int		alltris;

float		totalseconds = 0;
float		default_scale;
float		scale_up;
float		defaultzrotation;
float		normal_blend;
float		zrotation;
float		gamma;

vec3_t		adjust;
vec3_t		bbox[2];
vec3_t		cbox[2];
vec3_t		eyeposition;
vec3_t		defaultadjust;

char		filename[MAX_SYSPATH];
char		line[MAX_SYSPATH];
string		pivotname[64];	// names of the pivot points
string		defaulttexture[64];
string		sourcetexture[64];
string		mirrored[MAXSTUDIOSRCBONES];

s_mesh_t		*pmesh;
studiohdr_t	*phdr;
studioseqhdr_t	*pseqhdr;
s_sequencegroup_t	sequencegroup;
s_trianglevert_t	(*triangles)[3];
s_model_t		*model[MAXSTUDIOMODELS];
s_bbox_t		hitbox[MAXSTUDIOSRCBONES];
s_texture_t	*texture[MAXSTUDIOSKINS];
s_hitgroup_t	hitgroup[MAXSTUDIOSRCBONES];
s_sequence_t	*sequence[MAXSTUDIOSEQUENCES];
s_bodypart_t	bodypart[MAXSTUDIOBODYPARTS];
s_bonefixup_t	bonefixup[MAXSTUDIOSRCBONES];
s_bonetable_t	bonetable[MAXSTUDIOSRCBONES];
s_attachment_t	attachment[MAXSTUDIOSRCBONES];
s_renamebone_t	renamedbone[MAXSTUDIOSRCBONES];
s_animation_t	*panimation[MAXSTUDIOANIMATIONS];
s_bonecontroller_t	bonecontroller[MAXSTUDIOSRCBONES];

/*
============
WriteBoneInfo
============
*/
void WriteBoneInfo( void )
{
	int i, j;
	mstudiobone_t *pbone;
	mstudiobonecontroller_t *pbonecontroller;
	mstudioattachment_t *pattachment;
	mstudiobbox_t *pbbox;

	// save bone info
	pbone = (mstudiobone_t *)pData;
	phdr->numbones = numbones;
	phdr->boneindex = (pData - pStart);

	for (i = 0; i < numbones; i++) 
	{
		com.strncpy( pbone[i].name, bonetable[i].name, 32 );
		pbone[i].parent = bonetable[i].parent;
		pbone[i].flags = bonetable[i].flags;
		pbone[i].value[0] = bonetable[i].pos[0];
		pbone[i].value[1] = bonetable[i].pos[1];
		pbone[i].value[2] = bonetable[i].pos[2];
		pbone[i].value[3] = bonetable[i].rot[0];
		pbone[i].value[4] = bonetable[i].rot[1];
		pbone[i].value[5] = bonetable[i].rot[2];
		pbone[i].scale[0] = bonetable[i].posscale[0];
		pbone[i].scale[1] = bonetable[i].posscale[1];
		pbone[i].scale[2] = bonetable[i].posscale[2];
		pbone[i].scale[3] = bonetable[i].rotscale[0];
		pbone[i].scale[4] = bonetable[i].rotscale[1];
		pbone[i].scale[5] = bonetable[i].rotscale[2];
	}

	pData += numbones * sizeof( mstudiobone_t );
	ALIGN( pData );

	// map bonecontroller to bones
	for (i = 0; i < numbones; i++)
		for (j = 0; j < 6; j++)
			pbone[i].bonecontroller[j] = -1;
	
	for (i = 0; i < numbonecontrollers; i++)
	{
		j = bonecontroller[i].bone;

		switch( bonecontroller[i].type & STUDIO_TYPES )
		{
		case STUDIO_X:
			pbone[j].bonecontroller[0] = i;
			break;
		case STUDIO_Y:
			pbone[j].bonecontroller[1] = i;
			break;
		case STUDIO_Z:
			pbone[j].bonecontroller[2] = i;
			break;
		case STUDIO_XR:
			pbone[j].bonecontroller[3] = i;
			break;
		case STUDIO_YR:
			pbone[j].bonecontroller[4] = i;
			break;
		case STUDIO_ZR:
			pbone[j].bonecontroller[5] = i;
			break;
		default: 
			Msg("Warning: unknown bonecontroller type %i\n", bonecontroller[i].type );
			pbone[j].bonecontroller[0] = i; //default
			break;
		}
	}

	// save bonecontroller info
	pbonecontroller = (mstudiobonecontroller_t *)pData;
	phdr->numbonecontrollers = numbonecontrollers;
	phdr->bonecontrollerindex = (pData - pStart);

	for (i = 0; i < numbonecontrollers; i++)
	{
		pbonecontroller[i].bone = bonecontroller[i].bone;
		pbonecontroller[i].index = bonecontroller[i].index;
		pbonecontroller[i].type = bonecontroller[i].type;
		pbonecontroller[i].start = bonecontroller[i].start;
		pbonecontroller[i].end = bonecontroller[i].end;
	}
	
	pData += numbonecontrollers * sizeof( mstudiobonecontroller_t );
	ALIGN( pData );

	// save attachment info
	pattachment = (mstudioattachment_t *)pData;
	phdr->numattachments = numattachments;
	phdr->attachmentindex = (pData - pStart);

	for (i = 0; i < numattachments; i++)
	{
		pattachment[i].bone	= attachment[i].bone;
		VectorCopy( attachment[i].org, pattachment[i].org );
	}
	
	pData += numattachments * sizeof( mstudioattachment_t );
	ALIGN( pData );

	// save bbox info
	pbbox = (mstudiobbox_t *)pData;
	phdr->numhitboxes = numhitboxes;
	phdr->hitboxindex = (pData - pStart);

	for (i = 0; i < numhitboxes; i++)
	{
		pbbox[i].bone = hitbox[i].bone;
		pbbox[i].group = hitbox[i].group;
		VectorCopy( hitbox[i].bmin, pbbox[i].bbmin );
		VectorCopy( hitbox[i].bmax, pbbox[i].bbmax );
	}

	pData += numhitboxes * sizeof( mstudiobbox_t );
	ALIGN( pData );
}

/*
============
WriteSequenceInfo
============
*/
void WriteSequenceInfo( void )
{
	int i, j;

	mstudioseqgroup_t	*pseqgroup;
	mstudioseqdesc_t	*pseqdesc;
	mstudioseqdesc_t	*pbaseseqdesc;
	mstudioevent_t	*pevent;
	mstudiopivot_t	*ppivot;
	byte		*ptransition;


	// save sequence info
	pseqdesc = (mstudioseqdesc_t *)pData;
	pbaseseqdesc = pseqdesc;
	phdr->numseq = numseq;
	phdr->seqindex = (pData - pStart);
	pData += numseq * sizeof( mstudioseqdesc_t );

	for (i = 0; i < numseq; i++, pseqdesc++) 
	{
		com.strncpy( pseqdesc->label, sequence[i]->name, 32 );
		pseqdesc->numframes	= sequence[i]->numframes;
		pseqdesc->fps = sequence[i]->fps;
		pseqdesc->flags = sequence[i]->flags;

		pseqdesc->numblends = sequence[i]->numblends;
		pseqdesc->blendtype[0] = sequence[i]->blendtype[0];
		pseqdesc->blendtype[1] = sequence[i]->blendtype[1];
		pseqdesc->blendstart[0] = sequence[i]->blendstart[0];
		pseqdesc->blendend[0] = sequence[i]->blendend[0];
		pseqdesc->blendstart[1] = sequence[i]->blendstart[1];
		pseqdesc->blendend[1] = sequence[i]->blendend[1];

		pseqdesc->motiontype = sequence[i]->motiontype;
		pseqdesc->motionbone = 0;
		VectorCopy( sequence[i]->linearmovement, pseqdesc->linearmovement );

		pseqdesc->seqgroup = sequence[i]->seqgroup;
		pseqdesc->animindex	= sequence[i]->animindex;
		pseqdesc->activity = sequence[i]->activity;
		pseqdesc->actweight	= sequence[i]->actweight;

		VectorCopy( sequence[i]->bmin, pseqdesc->bbmin );
		VectorCopy( sequence[i]->bmax, pseqdesc->bbmax );

		pseqdesc->entrynode	= sequence[i]->entrynode;
		pseqdesc->exitnode = sequence[i]->exitnode;
		pseqdesc->nodeflags	= sequence[i]->nodeflags;

		totalframes += sequence[i]->numframes;
		totalseconds += sequence[i]->numframes / sequence[i]->fps;

		// save events
		pevent = (mstudioevent_t *)pData;
		pseqdesc->numevents	= sequence[i]->numevents;
		pseqdesc->eventindex = (pData - pStart);
		pData += pseqdesc->numevents * sizeof( mstudioevent_t );

		for (j = 0; j < sequence[i]->numevents; j++)
		{
			pevent[j].frame = sequence[i]->event[j].frame - sequence[i]->frameoffset;
			pevent[j].event = sequence[i]->event[j].event;
			memcpy( pevent[j].options, sequence[i]->event[j].options, sizeof( pevent[j].options ));
		}

		ALIGN( pData );

		// save pivots
		ppivot = (mstudiopivot_t *)pData;
		pseqdesc->numpivots	= sequence[i]->numpivots;
		pseqdesc->pivotindex = (pData - pStart);
		pData += pseqdesc->numpivots * sizeof( mstudiopivot_t );

		for (j = 0; j < sequence[i]->numpivots; j++)
		{
			VectorCopy( sequence[i]->pivot[j].org, ppivot[j].org );
			ppivot[j].start = sequence[i]->pivot[j].start - sequence[i]->frameoffset;
			ppivot[j].end = sequence[i]->pivot[j].end - sequence[i]->frameoffset;
		}

		ALIGN( pData );
	}

	// save sequence group info
	pseqgroup = (mstudioseqgroup_t *)pData;
	phdr->numseqgroups = 1;
	phdr->seqgroupindex = (pData - pStart);
	pData += sizeof( mstudioseqgroup_t );

	ALIGN( pData );

	com.strncpy( pseqgroup->label, sequencegroup.label, 32 );
	com.strncpy( pseqgroup->name, sequencegroup.name, 64 );

	// save transition graph
	ptransition = (byte *)pData;
	phdr->numtransitions = numxnodes;
	phdr->transitionindex = (pData - pStart);
	pData += numxnodes * numxnodes * sizeof( byte );
	ALIGN( pData );
	for (i = 0; i < numxnodes; i++)
		for(j = 0; j < numxnodes; j++)
			*ptransition++ = xnode[i][j];

}

/*
============
WriteAnimations
============
*/
byte *WriteAnimations( byte *pData, byte *pStart, int group )
{
	int i, j, k, q, n;

	mstudioanim_t	*panim;
	mstudioanimvalue_t	*panimvalue;

	for (i = 0; i < numseq; i++) 
	{
		if (sequence[i]->seqgroup == group)
		{
			// save animations
			panim = (mstudioanim_t *)pData;
			sequence[i]->animindex = (pData - pStart);
			pData += sequence[i]->numblends * numbones * sizeof( mstudioanim_t );
			ALIGN( pData );
			
			panimvalue = (mstudioanimvalue_t *)pData;
			for (q = 0; q < sequence[i]->numblends; q++)
			{
				// save animation value info
				for (j = 0; j < numbones; j++)
				{
					for (k = 0; k < 6; k++)
					{
						if (sequence[i]->panim[q]->numanim[j][k] == 0)
						{
							panim->offset[k] = 0;
						}
						else
						{
							panim->offset[k] = ((byte *)panimvalue - (byte *)panim);
							for (n = 0; n < sequence[i]->panim[q]->numanim[j][k]; n++)
							{
								panimvalue->value = sequence[i]->panim[q]->anim[j][k][n].value;
								panimvalue++;
							}
						}
					}
					if (((byte *)panimvalue - (byte *)panim) > 0xffff)
						Sys_Break("sequence \"%s\" is greater than 64K\n", sequence[i]->name );
					panim++;
				}
			}

			pData = (byte *)panimvalue;
			ALIGN( pData );
		}
	}
	return pData;
}

/*
============
WriteTextures
============
*/	
void WriteTextures( void )
{
	mstudiotexture_t	*ptexture;
	short		*pref;
	int		i, j;

	// save bone info
	ptexture = (mstudiotexture_t *)pData;
	phdr->numtextures = numtextures;
	phdr->textureindex = (pData - pStart);
	pData += numtextures * sizeof( mstudiotexture_t );
	ALIGN( pData );

	phdr->skinindex = (pData - pStart);
	phdr->numskinref = numskinref;
	phdr->numskinfamilies = numskinfamilies;
	pref = (short *)pData;

	for (i = 0; i < phdr->numskinfamilies; i++) 
	{
		for (j = 0; j < phdr->numskinref; j++) 
		{
			*pref = skinref[i][j];
			pref++;
		}
	}

	pData = (byte *)pref;
	ALIGN( pData );

	phdr->texturedataindex = (pData - pStart); // must be the end of the file!

	for (i = 0; i < numtextures; i++)
	{
		com.strncpy( ptexture[i].name, texture[i]->name, 64 );
		ptexture[i].flags = texture[i]->flags;
		ptexture[i].width = texture[i]->skinwidth;
		ptexture[i].height = texture[i]->skinheight;
		ptexture[i].index = (pData - pStart);
		Mem_Copy( pData, texture[i]->pdata, texture[i]->size );
		pData += texture[i]->size;
	}

	ALIGN( pData );
}

/*
============
WriteModel
============
*/
void WriteModel( void )
{
	mstudiobodyparts_t	*pbodypart;
	mstudiomodel_t	*pmodel;
	byte		*pbone;
	vec3_t		*pvert;
	vec3_t		*pnorm;
	mstudiomesh_t	*pmesh;
	s_trianglevert_t	*psrctri;
	int		i, j, k, cur;
	int		total_tris = 0;
	int		total_strips = 0;

	pbodypart = (mstudiobodyparts_t *)pData;
	phdr->numbodyparts = numbodyparts;
	phdr->bodypartindex = (pData - pStart);
	pData += numbodyparts * sizeof( mstudiobodyparts_t );

	pmodel = (mstudiomodel_t *)pData;
	pData += nummodels * sizeof( mstudiomodel_t );

	for (i = 0, j = 0; i < numbodyparts; i++)
	{
		com.strncpy( pbodypart[i].name, bodypart[i].name, 64 );
		pbodypart[i].nummodels = bodypart[i].nummodels;
		pbodypart[i].base = bodypart[i].base;
		pbodypart[i].modelindex = ((byte *)&pmodel[j]) - pStart;
		j += bodypart[i].nummodels;
	}
	
	ALIGN( pData );
	cur = (int)pData;

	for (i = 0; i < nummodels; i++) 
	{
		int normmap[MAXSTUDIOVERTS];
		int normimap[MAXSTUDIOVERTS];
		int n = 0;

		com.strncpy( pmodel[i].name, model[i]->name, 64 );

		// remap normals to be sorted by skin reference
		for (j = 0; j < model[i]->nummesh; j++)
		{
			for (k = 0; k < model[i]->numnorms; k++)
			{
				if (model[i]->normal[k].skinref == model[i]->pmesh[j]->skinref)
				{
					normmap[k] = n;
					normimap[n] = k;
					model[i]->pmesh[j]->numnorms++;
					n++;
				}
			}
		}
		
		// save vertice bones
		pbone = pData;
		pmodel[i].numverts	= model[i]->numverts;
		pmodel[i].vertinfoindex = (pData - pStart);
		for (j = 0; j < pmodel[i].numverts; j++) *pbone++ = model[i]->vert[j].bone;

		ALIGN( pbone );

		// save normal bones
		pmodel[i].numnorms	= model[i]->numnorms;
		pmodel[i].norminfoindex = ((byte *)pbone - pStart);
		for (j = 0; j < pmodel[i].numnorms; j++) *pbone++ = model[i]->normal[normimap[j]].bone;

		ALIGN( pbone );
		pData = pbone;

		// save group info
		pvert = (vec3_t *)pData;
		pData += model[i]->numverts * sizeof( vec3_t );
		pmodel[i].vertindex	= ((byte *)pvert - pStart); 

		ALIGN( pData );			
		pnorm = (vec3_t *)pData;

		pData += model[i]->numnorms * sizeof( vec3_t );
		pmodel[i].normindex		= ((byte *)pnorm - pStart); 
		ALIGN( pData );

		for (j = 0; j < model[i]->numverts; j++)
		{
			VectorCopy( model[i]->vert[j].org, pvert[j] );
		}

		for (j = 0; j < model[i]->numnorms; j++)
		{
			VectorCopy( model[i]->normal[normimap[j]].org, pnorm[j] );
		}
		
		Msg("vertices  %6d bytes (%d vertices, %d normals)\n", pData - cur, model[i]->numverts, model[i]->numnorms);
		cur = (int)pData;

		// save mesh info
		pmesh = (mstudiomesh_t *)pData;
		pmodel[i].nummesh = model[i]->nummesh;
		pmodel[i].meshindex = (pData - pStart);
		pData += pmodel[i].nummesh * sizeof( mstudiomesh_t );

		ALIGN( pData );

		total_tris = 0;
		total_strips = 0;

		for (j = 0; j < model[i]->nummesh; j++)
		{
			int numCmdBytes;
			byte *pCmdSrc;

			pmesh[j].numtris	= model[i]->pmesh[j]->numtris;
			pmesh[j].skinref	= model[i]->pmesh[j]->skinref;
			pmesh[j].numnorms	= model[i]->pmesh[j]->numnorms;

			psrctri = (s_trianglevert_t *)(model[i]->pmesh[j]->triangle);
			for (k = 0; k < pmesh[j].numtris * 3; k++) 
			{
				psrctri->normindex	= normmap[psrctri->normindex];
				psrctri++;
			}
			numCmdBytes = BuildTris( model[i]->pmesh[j]->triangle, model[i]->pmesh[j], &pCmdSrc );

			pmesh[j].triindex = (pData - pStart);
			memcpy( pData, pCmdSrc, numCmdBytes );
			pData += numCmdBytes;
			
			ALIGN( pData );
			total_tris += pmesh[j].numtris;
			total_strips += numcommandnodes;
		}
		Msg("mesh      %6d bytes (%d tris, %d strips)\n", pData - cur, total_tris, total_strips);
		cur = (int)pData;
	}	
}


/*
============
WriteMDLFile
============
*/
void WriteMDLFile( void )
{
	int	total = 0;

	pStart = Kalloc( FILEBUFFER );
	FS_StripExtension( modeloutname ); 

	if( split_textures )
	{
		// write textures out to a separate file
		string	texname;

		com.snprintf( texname, MAX_STRING, "%sT.mdl", modeloutname );
		Msg( "writing %s:\n", texname );

		phdr = (studiohdr_t *)pStart;
		phdr->ident = IDSTUDIOHEADER;
		phdr->version = STUDIO_VERSION;

		pData = (byte *)phdr + sizeof( studiohdr_t );

		WriteTextures( );

		phdr->length = pData - pStart;
		Msg( "textures  %6d bytes\n", phdr->length );

		FS_WriteFile( texname, pStart, phdr->length );

		memset( pStart, 0, phdr->length );
		pData = pStart;
	}

	// write the model output file
	FS_DefaultExtension( modeloutname, ".mdl" );
	
	Msg ("---------------------\n");
	Msg ("writing %s:\n", modeloutname);

	phdr = (studiohdr_t *)pStart;
	phdr->ident = IDSTUDIOHEADER;
	phdr->version = STUDIO_VERSION;
	com.strncpy( phdr->name, modeloutname, 64 );

	VectorCopy( eyeposition, phdr->eyeposition );
	VectorCopy( bbox[0], phdr->min ); 
	VectorCopy( bbox[1], phdr->max ); 
	VectorCopy( cbox[0], phdr->bbmin ); 
	VectorCopy( cbox[1], phdr->bbmax ); 

	phdr->flags = gflags;
	pData = (byte *)phdr + sizeof( studiohdr_t );

	WriteBoneInfo();
	Msg("bones     %6d bytes (%d)\n", pData - pStart - total, numbones );
	total = pData - pStart;
	pData = WriteAnimations( pData, pStart, 0 );

	WriteSequenceInfo( );
	Msg("sequences %6d bytes (%d frames) [%d:%02d]\n", pData - pStart - total, totalframes, (int)totalseconds / 60, (int)totalseconds % 60 );
	total  = pData - pStart;

	WriteModel();
	Msg("models    %6d bytes\n", pData - pStart - total );
	total  = pData - pStart;

	if( !split_textures )
	{
		WriteTextures();
		Msg("textures  %6d bytes\n", pData - pStart - total );
	}

	phdr->length = pData - pStart;
	FS_WriteFile( modeloutname, pStart, phdr->length );
	Msg("total     %6d\n", phdr->length );
}

/*
============
SimplifyModel
============
*/
void SimplifyModel( void )
{
	int	i, j, k;
	int	n, m, q;
	vec3_t	*defaultpos[MAXSTUDIOSRCBONES];
	vec3_t	*defaultrot[MAXSTUDIOSRCBONES];
	int	iError = 0;

	OptimizeAnimations();
	ExtractMotion();
	MakeTransitions();

	// find used bones
	for (i = 0; i < nummodels; i++)
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++) model[i]->boneref[k] = 0;
		for (j = 0; j < model[i]->numverts; j++) model[i]->boneref[model[i]->vert[j].bone] = 1;
		for (k = 0; k < MAXSTUDIOSRCBONES; k++)
		{
			// tag parent bones as used if child has been used
			if (model[i]->boneref[k])
			{
				n = model[i]->node[k].parent;
				while (n != -1 && !model[i]->boneref[n])
				{
					model[i]->boneref[n] = 1;
					n = model[i]->node[n].parent;
				}
			}
		}
	}

	// rename model bones if needed
	for( i = 0; i < nummodels; i++ )
	{
		for (j = 0; j < model[i]->numbones; j++)
		{
			for (k = 0; k < numrenamedbones; k++)
			{
				if (!com.strcmp( model[i]->node[j].name, renamedbone[k].from))
				{
					com.strncpy( model[i]->node[j].name, renamedbone[k].to, 64 );
					break;
				}
			}
		}
	}

	// union of all used bones
	numbones = 0;
	for( i = 0; i < nummodels; i++ )
	{
		for( k = 0; k < MAXSTUDIOSRCBONES; k++ ) model[i]->boneimap[k] = -1;
		for( j = 0; j < model[i]->numbones; j++ )
		{
			if (model[i]->boneref[j])
			{
				k = findNode( model[i]->node[j].name );
				if( k == -1 )
				{
					// create new bone
					k = numbones;
					strncpy( bonetable[k].name, model[i]->node[j].name, sizeof(bonetable[k].name));
					if ((n = model[i]->node[j].parent) != -1)
						bonetable[k].parent	= findNode( model[i]->node[n].name );
					else bonetable[k].parent = -1;
					bonetable[k].bonecontroller = 0;
					bonetable[k].flags = 0;

					// set defaults
					defaultpos[k] = Kalloc( MAXSTUDIOANIMATIONS * sizeof( vec3_t ));
					defaultrot[k] = Kalloc( MAXSTUDIOANIMATIONS * sizeof( vec3_t ));

					for (n = 0; n < MAXSTUDIOANIMATIONS; n++)
					{
						VectorCopy( model[i]->skeleton[j].pos, defaultpos[k][n] );
						VectorCopy( model[i]->skeleton[j].rot, defaultrot[k][n] );
					}

					VectorCopy( model[i]->skeleton[j].pos, bonetable[k].pos );
					VectorCopy( model[i]->skeleton[j].rot, bonetable[k].rot );
					numbones++;
				}
				else
				{
					// double check parent assignments
					n = model[i]->node[j].parent;
					if (n != -1) n = findNode( model[i]->node[n].name );
	 				m = bonetable[k].parent;

					if (n != m)
					{
						MsgDev( D_ERROR, "SimplifyModel: illegal parent bone replacement in model \"%s\"\n\t\"%s\" has \"%s\", previously was \"%s\"\n", model[i]->name, model[i]->node[j].name, (n != -1) ? bonetable[n].name : "ROOT", (m != -1) ? bonetable[m].name : "ROOT" );
						iError++;
					}
				}
				
				model[i]->bonemap[j] = k;
				model[i]->boneimap[k] = j;
			}
		}
	}

	// handle errors
	if( iError && !ignore_errors ) Sys_Break("Unexpected errors, compilation aborted\n");
	if( numbones >= MAXSTUDIOBONES ) Sys_Break( "Too many bones in model: used %d, max %d\n", numbones, MAXSTUDIOBONES );

	// rename sequence bones if needed
	for( i = 0; i < numseq; i++ )
	{
		for( j = 0; j < sequence[i]->panim[0]->numbones; j++ )
		{
			for (k = 0; k < numrenamedbones; k++)
			{
				if (!com.strcmp( sequence[i]->panim[0]->node[j].name, renamedbone[k].from))
				{
					com.strncpy( sequence[i]->panim[0]->node[j].name, renamedbone[k].to, 64 );
					break;
				}
			}
		}
	}

	// map each sequences bone list to master list
	for( i = 0; i < numseq; i++ )
	{
		for (k = 0; k < MAXSTUDIOSRCBONES; k++) sequence[i]->panim[0]->boneimap[k] = -1;
		for (j = 0; j < sequence[i]->panim[0]->numbones; j++)
		{
			k = findNode( sequence[i]->panim[0]->node[j].name );
			
			if (k == -1) sequence[i]->panim[0]->bonemap[j] = -1;
			else
			{
				char *szAnim = "ROOT";
				char *szNode = "ROOT";

				// whoa, check parent connections!
				if (sequence[i]->panim[0]->node[j].parent != -1) szAnim = sequence[i]->panim[0]->node[sequence[i]->panim[0]->node[j].parent].name;
				if (bonetable[k].parent != -1) szNode = bonetable[bonetable[k].parent].name;

				if (strcmp(szAnim, szNode))
				{
					MsgDev( D_ERROR, "SimplifyModel: illegal parent bone replacement in sequence \"%s\"\n\t\"%s\" has \"%s\", reference has \"%s\"\n", sequence[i]->name, sequence[i]->panim[0]->node[j].name, szAnim, szNode );
					iError++;
				}
				sequence[i]->panim[0]->bonemap[j] = k;
				sequence[i]->panim[0]->boneimap[k] = j;
			}
		}
	}
	
	//handle errors
	if( iError && !ignore_errors ) Sys_Break("unexpected errors, compilation aborted\n" );

	// link bonecontrollers
	for (i = 0; i < numbonecontrollers; i++)
	{
		for (j = 0; j < numbones; j++)
		{
			if (com.stricmp( bonecontroller[i].name, bonetable[j].name) == 0)
				break;
		}
		if (j >= numbones)
		{
			MsgDev( D_WARN, "SimplifyModel: unknown bonecontroller link '%s'\n", bonecontroller[i].name );
			j = numbones - 1;	
		}
		bonecontroller[i].bone = j;
	}

	// link attachments
	for (i = 0; i < numattachments; i++)
	{
		for (j = 0; j < numbones; j++)
		{
			if (stricmp( attachment[i].bonename, bonetable[j].name) == 0)
				break;
		}
		if (j >= numbones)
		{
			MsgDev( D_WARN, "SimplifyModel: unknown attachment link '%s'\n", attachment[i].bonename );
			j = numbones - 1;
		}
		attachment[i].bone = j;
	}

	// relink model
	for (i = 0; i < nummodels; i++)
	{
		for (j = 0; j < model[i]->numverts; j++) model[i]->vert[j].bone = model[i]->bonemap[model[i]->vert[j].bone];
		for (j = 0; j < model[i]->numnorms; j++) model[i]->normal[j].bone = model[i]->bonemap[model[i]->normal[j].bone];
	}

	// set hitgroups
	for (k = 0; k < numbones; k++) bonetable[k].group = -9999;
	for (j = 0; j < numhitgroups; j++)
	{
		for (k = 0; k < numbones; k++)
		{
			if (strcmpi( bonetable[k].name, hitgroup[j].name) == 0)
			{
				bonetable[k].group = hitgroup[j].group;
				break;
			}
		}
		if (k >= numbones)
		{
			MsgDev( D_WARN, "SimplifyModel: cannot find bone %s for hitgroup %d\n", hitgroup[j].name, hitgroup[j].group );
			continue;
		}
	}

	for (k = 0; k < numbones; k++)
	{
		if (bonetable[k].group == -9999)
		{
			if (bonetable[k].parent != -1)
				bonetable[k].group = bonetable[bonetable[k].parent].group;
			else bonetable[k].group = 0;
		}
	}

	if (numhitboxes == 0)
	{
		// find intersection box volume for each bone
		for (k = 0; k < numbones; k++)
		{
			for (j = 0; j < 3; j++)
			{
				bonetable[k].bmin[j] = 0.0;
				bonetable[k].bmax[j] = 0.0;
			}
		}
		// try all the connect vertices
		for (i = 0; i < nummodels; i++)
		{
			vec3_t	p;
			for (j = 0; j < model[i]->numverts; j++)
			{
				VectorCopy( model[i]->vert[j].org, p );
				k = model[i]->vert[j].bone;

				if (p[0] < bonetable[k].bmin[0]) bonetable[k].bmin[0] = p[0];
				if (p[1] < bonetable[k].bmin[1]) bonetable[k].bmin[1] = p[1];
				if (p[2] < bonetable[k].bmin[2]) bonetable[k].bmin[2] = p[2];
				if (p[0] > bonetable[k].bmax[0]) bonetable[k].bmax[0] = p[0];
				if (p[1] > bonetable[k].bmax[1]) bonetable[k].bmax[1] = p[1];
				if (p[2] > bonetable[k].bmax[2]) bonetable[k].bmax[2] = p[2];
			}
		}

		// add in all your children as well
		for (k = 0; k < numbones; k++)
		{
			if ((j = bonetable[k].parent) != -1)
			{
				if (bonetable[k].pos[0] < bonetable[j].bmin[0]) bonetable[j].bmin[0] = bonetable[k].pos[0];
				if (bonetable[k].pos[1] < bonetable[j].bmin[1]) bonetable[j].bmin[1] = bonetable[k].pos[1];
				if (bonetable[k].pos[2] < bonetable[j].bmin[2]) bonetable[j].bmin[2] = bonetable[k].pos[2];
				if (bonetable[k].pos[0] > bonetable[j].bmax[0]) bonetable[j].bmax[0] = bonetable[k].pos[0];
				if (bonetable[k].pos[1] > bonetable[j].bmax[1]) bonetable[j].bmax[1] = bonetable[k].pos[1];
				if (bonetable[k].pos[2] > bonetable[j].bmax[2]) bonetable[j].bmax[2] = bonetable[k].pos[2];
			}
		}

		for (k = 0; k < numbones; k++)
		{
			if (bonetable[k].bmin[0] < bonetable[k].bmax[0] - 1 && bonetable[k].bmin[1] < bonetable[k].bmax[1] - 1 && bonetable[k].bmin[2] < bonetable[k].bmax[2] - 1)
			{
				hitbox[numhitboxes].bone = k;
				hitbox[numhitboxes].group = bonetable[k].group;
				VectorCopy( bonetable[k].bmin, hitbox[numhitboxes].bmin );
				VectorCopy( bonetable[k].bmax, hitbox[numhitboxes].bmax );

				if( dump_hboxes )//remove this??
				{
					Msg("$hbox %d \"%s\" %.2f %.2f %.2f  %.2f %.2f %.2f\n", hitbox[numhitboxes].group, bonetable[hitbox[numhitboxes].bone].name, hitbox[numhitboxes].bmin[0], hitbox[numhitboxes].bmin[1], hitbox[numhitboxes].bmin[2], hitbox[numhitboxes].bmax[0], hitbox[numhitboxes].bmax[1], hitbox[numhitboxes].bmax[2] );
				}
				numhitboxes++;
			}
		}
	}
	else
	{
		for (j = 0; j < numhitboxes; j++)
		{
			for (k = 0; k < numbones; k++)
			{
				if (strcmpi( bonetable[k].name, hitbox[j].name) == 0)
				{
					hitbox[j].bone = k;
					break;
				}
			}
			if (k >= numbones) 
			{
				MsgDev( D_WARN, "SimplifyModel: cannot find bone %s for bbox\n", hitbox[j].name );
				continue;
			}
		}
	}

	// relink animations
	for (i = 0; i < numseq; i++)
	{
		vec3_t *origpos[MAXSTUDIOSRCBONES];
		vec3_t *origrot[MAXSTUDIOSRCBONES];

		for (q = 0; q < sequence[i]->numblends; q++)
		{
			// save pointers to original animations
			for (j = 0; j < sequence[i]->panim[q]->numbones; j++)
			{
				origpos[j] = sequence[i]->panim[q]->pos[j];
				origrot[j] = sequence[i]->panim[q]->rot[j];
			}

			for (j = 0; j < numbones; j++)
			{
				if ((k = sequence[i]->panim[0]->boneimap[j]) >= 0)
				{
					// link to original animations
					sequence[i]->panim[q]->pos[j] = origpos[k];
					sequence[i]->panim[q]->rot[j] = origrot[k];
				}
				else
				{
					// link to dummy animations
					sequence[i]->panim[q]->pos[j] = defaultpos[j];
					sequence[i]->panim[q]->rot[j] = defaultrot[j];
				}
			}
		}
	}

	// find scales for all bones
	for (j = 0; j < numbones; j++)
	{
		for (k = 0; k < 6; k++)
		{
			float minv, maxv, scale;

			if (k < 3) 
			{
				minv = -128.0f;
				maxv = 128.0f;
			}
			else
			{
				minv = -M_PI / 8.0f;
				maxv = M_PI / 8.0f;
			}

			for( i = 0; i < numseq; i++ )
			{
				for( q = 0; q < sequence[i]->numblends; q++ )
				{
					for( n = 0; n < sequence[i]->numframes; n++ )
					{
						float v;
						switch( k )
						{
						case 0: 
						case 1: 
						case 2: 
							v = ( sequence[i]->panim[q]->pos[j][n][k] - bonetable[j].pos[k] ); 
							break;
						case 3:
						case 4:
						case 5:
							v = ( sequence[i]->panim[q]->rot[j][n][k-3] - bonetable[j].rot[k-3] ); 
							if (v >= M_PI) v -= M_PI * 2;
							if (v < -M_PI) v += M_PI * 2;
							break;
						}
						if (v < minv) minv = v;
						if (v > maxv) maxv = v;
					}
				}
			}

			if (minv < maxv)
			{
				if (-minv> maxv) scale = minv / -32768.0;
				else scale = maxv / 32767;
			}
			else scale = 1.0 / 32.0;

			switch(k)
			{
			case 0: 
			case 1: 
			case 2: 
				bonetable[j].posscale[k] = scale;
				break;
			case 3:
			case 4:
			case 5:
				bonetable[j].rotscale[k-3] = scale;
				break;
			}
		}
	}

	// find bounding box for each sequence
	for (i = 0; i < numseq; i++)
	{
		vec3_t bmin, bmax;

		// find intersection box volume for each bone
		for (j = 0; j < 3; j++)
		{
			bmin[j] = 9999.0;
			bmax[j] = -9999.0;
		}

		for (q = 0; q < sequence[i]->numblends; q++)
		{
			for (n = 0; n < sequence[i]->numframes; n++)
			{
				matrix3x4 bonetransform[MAXSTUDIOBONES];// bone transformation matrix
				matrix3x4 bonematrix; // local transformation matrix
				vec3_t pos;

				for (j = 0; j < numbones; j++)
				{
					vec3_t angle;

					// convert to degrees
					angle[0]	= sequence[i]->panim[q]->rot[j][n][0] * (180.0 / M_PI);
					angle[1]	= sequence[i]->panim[q]->rot[j][n][1] * (180.0 / M_PI);
					angle[2]	= sequence[i]->panim[q]->rot[j][n][2] * (180.0 / M_PI);

					AngleMatrixFLU( angle, bonematrix );

					bonematrix[0][3] = sequence[i]->panim[q]->pos[j][n][0];
					bonematrix[1][3] = sequence[i]->panim[q]->pos[j][n][1];
					bonematrix[2][3] = sequence[i]->panim[q]->pos[j][n][2];

					if (bonetable[j].parent == -1) MatrixCopy( bonematrix, bonetransform[j] );
					else R_ConcatTransforms (bonetransform[bonetable[j].parent], bonematrix, bonetransform[j]);
				}

				for (k = 0; k < nummodels; k++)
				{
					for (j = 0; j < model[k]->numverts; j++)
					{
						VectorTransform( model[k]->vert[j].org, bonetransform[model[k]->vert[j].bone], pos );

						if (pos[0] < bmin[0]) bmin[0] = pos[0];
						if (pos[1] < bmin[1]) bmin[1] = pos[1];
						if (pos[2] < bmin[2]) bmin[2] = pos[2];
						if (pos[0] > bmax[0]) bmax[0] = pos[0];
						if (pos[1] > bmax[1]) bmax[1] = pos[1];
						if (pos[2] > bmax[2]) bmax[2] = pos[2];
					}
				}
			}
		}

		VectorCopy( bmin, sequence[i]->bmin );
		VectorCopy( bmax, sequence[i]->bmax );
	}

	// reduce animations
	{
		int total = 0;
		int changes = 0;
		int p;
		
		for (i = 0; i < numseq; i++)
		{
			for (q = 0; q < sequence[i]->numblends; q++)
			{
				for (j = 0; j < numbones; j++)
				{
					for (k = 0; k < 6; k++)
					{
						mstudioanimvalue_t	*pcount, *pvalue;
						short value[MAXSTUDIOANIMATIONS];
						mstudioanimvalue_t data[MAXSTUDIOANIMATIONS];
						float v;
						
						for (n = 0; n < sequence[i]->numframes; n++)
						{
							switch(k)
							{
							case 0: 
							case 1: 
							case 2: 
								value[n] = ( sequence[i]->panim[q]->pos[j][n][k] - bonetable[j].pos[k] ) / bonetable[j].posscale[k]; 
								break;
							case 3:
							case 4:
							case 5:
								v = ( sequence[i]->panim[q]->rot[j][n][k-3] - bonetable[j].rot[k-3] ); 
								if (v >= M_PI) v -= M_PI * 2;
								if (v < -M_PI) v += M_PI * 2;
								value[n] = v / bonetable[j].rotscale[k-3]; 
								break;
							}
						}
						if( n == 0 ) Sys_Break("no animation frames: \"%s\"\n", sequence[i]->name );

						sequence[i]->panim[q]->numanim[j][k] = 0;
						memset( data, 0, sizeof( data ) ); 
						pcount = data; 
						pvalue = pcount + 1;

						pcount->num.valid = 1;
						pcount->num.total = 1;
						pvalue->value = value[0];
						pvalue++;

						for (m = 1, p = 0; m < n; m++)
						{
							if (abs(value[p] - value[m]) > 1600)
							{
								changes++;
								p = m;
							}
						}

						// this compression algorithm needs work
						for( m = 1; m < n; m++ )
						{
							if( pcount->num.total == 255 )
							{
								// too many, force a new entry
								pcount = pvalue;
								pvalue = pcount + 1;
								pcount->num.valid++;
								pvalue->value = value[m];
								pvalue++;
							} 

							// insert value if they're not equal, 
							// or if we're not on a run and the run is less than 3 units
							else if ((value[m] != value[m-1]) || ((pcount->num.total == pcount->num.valid) && ((m < n - 1) && value[m] != value[m+1])))
							{
								total++;
								if (pcount->num.total != pcount->num.valid)
								{
									pcount = pvalue;
									pvalue = pcount + 1;
								}
								pcount->num.valid++;
								pvalue->value = value[m];
								pvalue++;
							}
							pcount->num.total++;
						}

						sequence[i]->panim[q]->numanim[j][k] = pvalue - data;
						if( sequence[i]->panim[q]->numanim[j][k] == 2 && value[0] == 0)
						{
							sequence[i]->panim[q]->numanim[j][k] = 0;
						}
						else
						{
							sequence[i]->panim[q]->anim[j][k] = Kalloc( (pvalue - data) * sizeof( mstudioanimvalue_t ));
							memmove( sequence[i]->panim[q]->anim[j][k], data, (pvalue - data) * sizeof( mstudioanimvalue_t ));
						}
					}
				}
			}
		}
	}
}

void Grab_Skin( s_texture_t *ptexture )
{
	string	filename;
	rgbdata_t	*pic;

	com.strncpy( filename, ptexture->name, MAX_STRING );

	pic = FS_LoadImage( filename, error_bmp, error_bmp_size );
	if( !pic ) Sys_Break( "unable to load %s\n", filename ); // no error.bmp, missing texture...
	Image_ConvertPalette( pic );

	ptexture->ppal = Kalloc( 768 );
	Mem_Copy( ptexture->ppal, pic->palette, sizeof(rgb_t) * 256 );
	ptexture->ppicture = Kalloc( pic->size );
	Mem_Copy( ptexture->ppicture, pic->buffer, pic->size );
	ptexture->srcwidth = pic->width;
	ptexture->srcheight	= pic->height;
	FS_FreeImage( pic ); // free old image
}

void SetSkinValues( void )
{
	int	i, j;
	int	index;

	for (i = 0; i < numtextures; i++)
	{
		Grab_Skin ( texture[i] );
		texture[i]->max_s = -9999999;
		texture[i]->min_s = 9999999;
		texture[i]->max_t = -9999999;
		texture[i]->min_t = 9999999;
	}

	for (i = 0; i < nummodels; i++)
		for (j = 0; j < model[i]->nummesh; j++) 
			TextureCoordRanges( model[i]->pmesh[j], texture[model[i]->pmesh[j]->skinref] );

	for (i = 0; i < numtextures; i++)
	{
		if (texture[i]->max_s < texture[i]->min_s )
		{
			// must be a replacement texture
			if (texture[i]->flags & STUDIO_NF_CHROME)
			{
				texture[i]->max_s = 63;
				texture[i]->min_s = 0;
				texture[i]->max_t = 63;
				texture[i]->min_t = 0;
			}
			else
			{
				texture[i]->max_s = texture[texture[i]->parent]->max_s;
				texture[i]->min_s = texture[texture[i]->parent]->min_s;
				texture[i]->max_t = texture[texture[i]->parent]->max_t;
				texture[i]->min_t = texture[texture[i]->parent]->min_t;
			}
		}
		ResizeTexture( texture[i] );
	}

	for (i = 0; i < nummodels; i++)
		for (j = 0; j < model[i]->nummesh; j++) 
			ResetTextureCoordRanges( model[i]->pmesh[j], texture[model[i]->pmesh[j]->skinref] );

	// build texture groups
	for (i = 0; i < MAXSTUDIOSKINS; i++) for (j = 0; j < MAXSTUDIOSKINS; j++) skinref[i][j] = j;
	
	index = 0;
	for (i = 0; i < numtexturelayers[0]; i++)
	{
		for (j = 0; j < numtexturereps[0]; j++)
		{
			skinref[i][texturegroup[0][0][j]] = texturegroup[0][i][j];
		}
	}

	if (i != 0) numskinfamilies = i;
	else
	{
		numskinfamilies = 1;
		numskinref = numtextures;
	}
}

void Build_Reference( s_model_t *pmodel)
{
	int	i, parent;
	float	angle[3];

	for (i = 0; i < pmodel->numbones; i++)
	{
		float m[3][4];
		vec3_t p;

		// convert to degrees
		angle[0] = pmodel->skeleton[i].rot[0] * (180.0 / M_PI);
		angle[1] = pmodel->skeleton[i].rot[1] * (180.0 / M_PI);
		angle[2] = pmodel->skeleton[i].rot[2] * (180.0 / M_PI);

		parent = pmodel->node[i].parent;
		if (parent == -1)
		{
			// scale the done pos.
			// calc rotational matrices
			AngleMatrixFLU( angle, bonefixup[i].m );
			AngleIMatrixFLU( angle, bonefixup[i].im );
			VectorCopy( pmodel->skeleton[i].pos, bonefixup[i].worldorg );
		}
		else
		{
			// calc compound rotational matrices
			// FIXME : Hey, it's orthogical so inv(A) == transpose(A)
			AngleMatrixFLU( angle, m );
			R_ConcatTransforms( bonefixup[parent].m, m, bonefixup[i].m );
			AngleIMatrixFLU( angle, m );
			R_ConcatTransforms( m, bonefixup[parent].im, bonefixup[i].im );

			// calc true world coord.
			VectorTransform(pmodel->skeleton[i].pos, bonefixup[parent].m, p );
			VectorAdd( p, bonefixup[parent].worldorg, bonefixup[i].worldorg );
		}
	}
}

void Grab_Triangles( s_model_t *pmodel )
{
	int	i, j;
	int	tcount = 0;	
	vec3_t	vmin, vmax;

	vmin[0] = vmin[1] = vmin[2] = 99999;
	vmax[0] = vmax[1] = vmax[2] = -99999;

	Build_Reference( pmodel );

	// load the base triangles
	while ( 1 ) 
	{
		s_mesh_t		*pmesh;
		char		texturename[64];
		s_trianglevert_t	*ptriv;
		int		bone;

		vec3_t vert[3];
		vec3_t norm[3];

		if(!Com_GetToken( true )) break;
		linecount++;
		
		if(Com_MatchToken( "end" )) break;//triangles end
		else if(!stricmp( ".bmp", &com_token[strlen(com_token)-4]))
		{
			// probably is texture name
			com.strncpy( texturename, com_token, 64 );

			// funky texture overrides
			for( i = 0; i < numrep; i++ )  
			{
				if( sourcetexture[i][0] == '\0') 
				{
					com.strncpy( texturename, defaulttexture[i], 64 );
					break;
				}
				if(!com.stricmp( texturename, sourcetexture[i])) 
				{
					com.strncpy( texturename, defaulttexture[i], 64 );
					break;
				}
			}
			if( com.strlen( texturename ) < 5 )//invalid name
			{
				// weird model problem, skip them
				MsgDev( D_ERROR, "Grab_Triangles: triangle with invalid texname\n" );
				for(i = 0; i < 3; i++)
				{
					if(!Com_GetToken( true ))
						Sys_Break( "Unexpected EOF %s at line %d\n", filename, linecount );
					while(Com_TryToken());
					linecount++;
				}
				continue;
			}
		}
                    else //triangles description
                    {
			pmesh = lookup_mesh( pmodel, texturename );

			for (j = 0; j < 3; j++) 
			{
				s_vertex_t p;
				vec3_t tmp;
				s_normal_t normal;

				if (flip_triangles) ptriv = lookup_triangle( pmesh, pmesh->numtris ) + 2 - j;
				else ptriv = lookup_triangle( pmesh, pmesh->numtris ) + j;

				// grab triangle info
				bone = com.atoi(com_token);
				p.org[0] = com.atof(Com_GetToken( false ));
				p.org[1] = com.atof(Com_GetToken( false ));
				p.org[2] = com.atof(Com_GetToken( false ));
				normal.org[0] = atof(Com_GetToken( false ));
				normal.org[1] = atof(Com_GetToken( false ));
				normal.org[2] = atof(Com_GetToken( false ));
				ptriv->u = atof(Com_GetToken( false ));
				ptriv->v = atof(Com_GetToken( false ));

				// skip MilkShape additional info
				while(Com_TryToken()); 
		                                        
				//translate triangles
				if( bone < 0 || bone >= pmodel->numbones ) 
					Sys_Break("bogus bone index %d \n%s line %d", bone, filename, linecount );
				VectorCopy( p.org, vert[j] );
				VectorCopy( normal.org, norm[j] );

				p.bone = bone;
				normal.bone = bone;
				normal.skinref = pmesh->skinref;

				if (p.org[2] < vmin[2]) vmin[2] = p.org[2];

				adjust_vertex( p.org );
				scale_vertex( p.org );

				// move vertex position to object space.
				VectorSubtract( p.org, bonefixup[p.bone].worldorg, tmp );
				VectorTransform(tmp, bonefixup[p.bone].im, p.org );

				// move normal to object space.
				VectorCopy( normal.org, tmp );
				VectorTransform(tmp, bonefixup[p.bone].im, normal.org );
				VectorNormalize( normal.org );

				ptriv->normindex = lookup_normal( pmodel, &normal );
				ptriv->vertindex = lookup_vertex( pmodel, &p );
                                        
				// tag bone as being used
				// pmodel->bone[bone].ref = 1;

				if( j < 2 )
				{
					Com_GetToken( true );
					linecount++;
				}
			}

			pmodel->trimesh[tcount] = pmesh;
			pmodel->trimap[tcount] = pmesh->numtris++;
			tcount++;
		}
	}
	if( vmin[2] != 0.0 ) MsgDev( D_NOTE, "Grab_Triangles: lowest vector at %f\n", vmin[2] );
}

void Grab_Skeleton( s_node_t *pnodes, s_bone_t *pbones )
{
	int index;
          int time = 0;
	
	while ( 1 ) 
	{
		if(!Com_GetToken( true )) break;
		linecount++;
		
		if(Com_MatchToken( "end" )) return;//skeleton end
		else if(Com_MatchToken( "time" ))
		{
			//check time
			time += atoi(Com_GetToken( false ));
			if(time > 0) MsgDev( D_WARN, "Grab_Skeleton: Warning! An animation file is probably used as a reference\n"); 
			continue;
		}
                    else
                    {
			//grab skeleton info
			index = atoi( com_token);
			pbones[index].pos[0] = atof(Com_GetToken( false ));
			pbones[index].pos[1] = atof(Com_GetToken( false ));
			pbones[index].pos[2] = atof(Com_GetToken( false ));

			scale_vertex( pbones[index].pos );
			if (pnodes[index].mirrored) VectorScale( pbones[index].pos, -1.0, pbones[index].pos );
			
			pbones[index].rot[0] = atof(Com_GetToken( false ));			
			pbones[index].rot[1] = atof(Com_GetToken( false ));
			pbones[index].rot[2] = atof(Com_GetToken( false ));

			clip_rotations( pbones[index].rot ); 
		}
	}
	Sys_Break( "Unexpected EOF %s at line %d\n", filename, linecount );
}

int Grab_Nodes( s_node_t *pnodes )
{
	int index;
	char name[MAX_SYSPATH];
	int parent;
	int numbones = 0;
	int i;

	while( 1 ) 
	{
		if(!Com_GetToken( true )) break;
		linecount++;

		//end of nodes description
		if(Com_MatchToken( "end" )) return numbones + 1;
		
		index = atoi(com_token); //read bone index (we already have filled token)
		strcpy( name, Com_GetToken( false ));
		parent = atoi(Com_GetToken( false )); //read bone parent

		strncpy( pnodes[index].name, name, sizeof(pnodes[index].name));
		pnodes[index].parent = parent;
		numbones = index;

		// check for mirrored bones;
		for (i = 0; i < nummirrored; i++) if (strcmp( name, mirrored[i] ) == 0) pnodes[index].mirrored = 1;
		if ((! pnodes[index].mirrored) && parent != -1)
			pnodes[index].mirrored = pnodes[pnodes[index].parent].mirrored;
	}

	Sys_Break( "Unexpected EOF %s at line %d\n", filename, linecount );
	return 0;
}

void Grab_Studio( s_model_t *pmodel )
{
          bool	load;

	com.strncpy( filename, pmodel->name, sizeof(filename));

	FS_DefaultExtension( filename, ".smd" );
	load = Com_IncludeScript( filename, NULL, 0 );
	if(!load) Sys_Break("unable to open %s\n", filename );
	Msg("grabbing %s\n", filename);
	
	linecount = 0;

	while ( 1 )
	{
		if(!Com_GetToken( true )) break;
		linecount++;

		if (Com_MatchToken( "version" ))
		{
			int option = atoi(Com_GetToken( false ));
			if (option != 1) MsgDev( D_ERROR, "Grab_Studio: %s bad version file\n", filename );
		}
		else if (Com_MatchToken( "nodes" ))
		{
			pmodel->numbones = Grab_Nodes( pmodel->node );
		}
		else if (Com_MatchToken( "skeleton" ))
		{
			Grab_Skeleton( pmodel->node, pmodel->skeleton );
		}
		else if (Com_MatchToken( "triangles" ))
		{
			Grab_Triangles( pmodel );
		}
		else MsgDev( D_WARN, "Grab_Studio: unknown studio command %s at line %d\n", com_token, linecount );
	}
}

/*
==============
Cmd_Eyeposition

syntax: $eyeposition <x> <y> <z>
==============
*/
void Cmd_Eyeposition (void)
{
	// rotate points into frame of reference so model points down the positive x axis
	eyeposition[1] = com.atof (Com_GetToken (false));
	eyeposition[0] = -com.atof (Com_GetToken (false));
	eyeposition[2] = com.atof (Com_GetToken (false));
}

/*
==============
Cmd_Modelname

syntax: $modelname "outname"
==============
*/
void Cmd_Modelname (void)
{
	com.strcpy( modeloutname, Com_GetToken( false ));
}

void Option_Studio( void )
{
	if(!Com_GetToken (false)) return;

	model[nummodels] = Kalloc( sizeof( s_model_t ));
	bodypart[numbodyparts].pmodel[bodypart[numbodyparts].nummodels] = model[nummodels];

	strncpy( model[nummodels]->name, com_token, sizeof(model[nummodels]->name));

	flip_triangles = 1;
	scale_up = default_scale;

	while(Com_TryToken())
	{
		if (Com_MatchToken( "reverse" ))
		{
			flip_triangles = 0;
		}
		else if (Com_MatchToken( "scale" ))
		{
			scale_up = atof( Com_GetToken(false));
		}
	}

	Grab_Studio( model[nummodels] );

	bodypart[numbodyparts].nummodels++;
	nummodels++;

	scale_up = default_scale;
}


int Option_Blank( void )
{
	model[nummodels] = Kalloc( sizeof( s_model_t ));
	bodypart[numbodyparts].pmodel[bodypart[numbodyparts].nummodels] = model[nummodels];

	strncpy( model[nummodels]->name, "blank", sizeof(model[nummodels]->name));

	bodypart[numbodyparts].nummodels++;
	nummodels++;
	return 0;
}

/*
==============
Cmd_Bodygroup

syntax: $bodygroup "name"
{
	studio "bodyref.smd"
	blank
}
==============
*/
void Cmd_Bodygroup( void )
{
	int is_started = 0;

	if(!Com_TryToken()) return;

	if (numbodyparts == 0) bodypart[numbodyparts].base = 1;
	else bodypart[numbodyparts].base = bodypart[numbodyparts-1].base * bodypart[numbodyparts-1].nummodels;
	strncpy( bodypart[numbodyparts].name, com_token, sizeof(bodypart[numbodyparts].name));

	while( 1 )
	{
		if(!Com_GetToken(true)) return;

		if(Com_MatchToken( "{" )) is_started = 1;
		else if (Com_MatchToken( "}" )) break;
		else if (Com_MatchToken("studio" )) Option_Studio();
		else if (Com_MatchToken("blank" )) Option_Blank();
	}

	numbodyparts++;
	return;
}

/*
==============
Cmd_Modelname

syntax: $body "name" "mainref.smd"
==============
*/
void Cmd_Body( void )
{
	int is_started = 0;
	if (!Com_GetToken(false)) return;

	if (numbodyparts == 0) bodypart[numbodyparts].base = 1;
	else bodypart[numbodyparts].base = bodypart[numbodyparts-1].base * bodypart[numbodyparts-1].nummodels;

	strncpy(bodypart[numbodyparts].name, com_token, sizeof(bodypart[numbodyparts].name));
	Option_Studio();

	numbodyparts++;
}

void Grab_Animation( s_animation_t *panim )
{
	vec3_t pos;
	vec3_t rot;
	int index;
	int t = -99999999;
	float cz, sz;
	int start = 99999;
	int end = 0;

	for (index = 0; index < panim->numbones; index++) 
	{
		panim->pos[index] = Kalloc( MAXSTUDIOANIMATIONS * sizeof( vec3_t ));
		panim->rot[index] = Kalloc( MAXSTUDIOANIMATIONS * sizeof( vec3_t ));
	}

	cz = cos( zrotation );
	sz = sin( zrotation );

	while ( 1 ) 
	{
		if(!Com_GetToken( true )) break;
		linecount++;

		if(Com_MatchToken( "end" ))
		{
			panim->startframe = start;
			panim->endframe = end;
			return;
		}
		else if(Com_MatchToken( "time" ))
		{
			t = atoi(Com_GetToken(false));
		}
                    else
                    {
			index = atoi(com_token);
			pos[0] = atof(Com_GetToken( false ));
			pos[1] = atof(Com_GetToken( false ));
			pos[2] = atof(Com_GetToken( false ));
			rot[0] = atof(Com_GetToken( false ));
			rot[1] = atof(Com_GetToken( false ));
			rot[2] = atof(Com_GetToken( false ));

			if (t >= panim->startframe && t <= panim->endframe)
			{
				if (panim->node[index].parent == -1)
				{
					adjust_vertex( pos );
					panim->pos[index][t][0] = cz * pos[0] - sz * pos[1];
					panim->pos[index][t][1] = sz * pos[0] + cz * pos[1];
					panim->pos[index][t][2] = pos[2];
					rot[2] += zrotation; // rotate model
				}
				else
				{
					VectorCopy( pos, panim->pos[index][t] );
				}

				if (t > end) end = t;
				if (t < start) start = t;

				if (panim->node[index].mirrored)
					VectorScale( panim->pos[index][t], -1.0, panim->pos[index][t] );

				scale_vertex( panim->pos[index][t] );
				clip_rotations( rot );
				VectorCopy( rot, panim->rot[index][t] );
			}
		}
	}

	Sys_Break( "unexpected EOF: %s\n", panim->name );
}

void Shift_Animation( s_animation_t *panim )
{
	int j, size;

	size = (panim->endframe - panim->startframe + 1) * sizeof( vec3_t );

	// shift
	for( j = 0; j < panim->numbones; j++ )
	{
		vec3_t *ppos;
		vec3_t *prot;

		ppos = Kalloc( size );
		prot = Kalloc( size );
		Mem_Move( studiopool, (void *)&ppos, &panim->pos[j][panim->startframe], size );
		Mem_Move( studiopool, (void *)&prot, &panim->rot[j][panim->startframe], size );
		panim->pos[j] = ppos;
		panim->rot[j] = prot;
	}
}

void Option_Animation( char *name, s_animation_t *panim )
{
	bool	load;
	
	com.strncpy( panim->name, name, sizeof(panim->name));
	com.strncpy( filename, panim->name, sizeof(filename));

	FS_DefaultExtension(filename, ".smd" );
	load = Com_IncludeScript( filename, NULL, 0 );
	if(!load) Sys_Break("unable to open %s\n", filename );
	linecount = 0;

	while ( 1 )
	{
		if(!Com_GetToken( true )) break;
		linecount++;

		if(Com_MatchToken( "end" )) break;
		else if (Com_MatchToken( "version" ))
		{
			int option = atoi(Com_GetToken( false ));
			if (option != 1) Msg("Warning: %s bad version file\n", filename );
		}
		else if (Com_MatchToken( "nodes" ))
		{
			panim->numbones = Grab_Nodes( panim->node );
		}
		else if (Com_MatchToken( "skeleton" ))
		{
			Grab_Animation( panim );
			Shift_Animation( panim );
		}
		else 
		{
			MsgDev( D_WARN, "Option_Animation: unknown studio command : %s\n", com_token );
			while(Com_TryToken()); // skip other tokens at line
		}
	}
}

int Option_Motion ( s_sequence_t *psequence )
{
	while (Com_TryToken()) psequence->motiontype |= lookupControl( com_token);
	return 0;
}

int Option_Event( s_sequence_t *psequence )
{
	if (psequence->numevents + 1 >= MAXSTUDIOEVENTS)
	{
		MsgDev( D_ERROR, "Option_Event: MAXSTUDIOEVENTS limit excedeed.\n");
		return 0;
	}

	psequence->event[psequence->numevents].event = atoi( Com_GetToken (false));
	psequence->event[psequence->numevents].frame = atoi(Com_GetToken (false));
	psequence->numevents++;

	// option token
	if (Com_TryToken())
	{
		if (Com_MatchToken( "}" )) return 1; // opps, hit the end
		strcpy( psequence->event[psequence->numevents-1].options, com_token);// found an option
	}
	return 0;
}


int Option_Fps( s_sequence_t *psequence )
{
	psequence->fps = atof(Com_GetToken (false));
	return 0;
}

int Option_AddPivot ( s_sequence_t *psequence )
{
	if( psequence->numpivots + 1 >= MAXSTUDIOPIVOTS )
	{
		Msg("Warning: MAXSTUDIOPIVOTS limit excedeed.\n");
		return 0;
	}
	
	psequence->pivot[psequence->numpivots].index = atoi(Com_GetToken (false));
	psequence->pivot[psequence->numpivots].start = atoi(Com_GetToken (false));
	psequence->pivot[psequence->numpivots].end = atoi(Com_GetToken (false));
	psequence->numpivots++;

	return 0;
}

/*
==============
Cmd_Origin

syntax: $origin <x> <y> <z> (z rotate)
==============
*/
void Cmd_Origin (void)
{
	defaultadjust[0] = atof (Com_GetToken (false));
	defaultadjust[1] = atof (Com_GetToken (false));
	defaultadjust[2] = atof (Com_GetToken (false));

	if (Com_TryToken()) defaultzrotation = (atof( com_token) + 90) * (M_PI / 180.0);
}


void Option_Origin (void)
{
	adjust[0] = atof (Com_GetToken (false));
	adjust[1] = atof (Com_GetToken (false));
	adjust[2] = atof (Com_GetToken (false));
}

void Option_Rotate(void )
{
	zrotation = (atof(Com_GetToken (false)) + 90) * (M_PI / 180.0);
}

/*
==============
Cmd_ScaleUp

syntax: $scale <value>
==============
*/
void Cmd_ScaleUp (void)
{
	default_scale = scale_up = com.atof (Com_GetToken (false));
}

/*
==============
Cmd_Rotate

syntax: $rotate <value>
==============
*/
void Cmd_Rotate(void)
{
	if (!Com_GetToken(false)) return;
	zrotation = (atof(com_token) + 90) * (M_PI / 180.0);
}

void Option_ScaleUp (void)
{
	scale_up = atof (Com_GetToken (false));
}

/*
==============
Cmd_Sequence

syntax: $sequence "seqname"
{
	(animation) "blend1.smd" "blend2.smd"	// blendings (one smdfile - normal animation)
	event { <event> <frame> ("option") }	// event num, frame num, option string (optionally, heh...)
	pivot <x> <y> <z>			// xyz pivot point
	fps <framerate>			// 30.0 frames per second is default value
	origin <x> <y> <z>			// xyz anim offset 
	rotate <value>			// z-axis rotation value 0 - 360 deg	
	scale <value>			// 1.0 is default size
	loop				// loop this animation (flag)
	frame <start> <end>			// use range(start, end) from smd file
	blend "type" <min> <max>		// blend description types: XR, YR e.t.c and range(min-max)
	node <number>			// link animation with node
	transition <startnode> <endnode>	// move from start to end node
	rtransition <startnode> <endnode>	// rotate from start to end node
	LX				// movement flag (may be X, Y, Z, LX, LY, LZ, XR, YR, e.t.c.)
	ACT_WALK | ACT_32 (actweight)		// act name or act number and actweight (optionally)	
}
==============
*/
static int Cmd_Sequence( void )
{
	int	i, depth = 0;
	string	smdfilename[MAXSTUDIOBLENDS];
	int	numblends = 0;
	int	start = 0;
	int	end = MAXSTUDIOANIMATIONS - 1;

	if(!Com_GetToken( false )) return 0;

	// allocate new sequence
	if( numseq >= MAXSTUDIOSEQUENCES ) Sys_Break("Too many sequences in model\n" );
	sequence[numseq] = Mem_Alloc( studiopool, sizeof(s_sequence_t));
	com.strncpy( sequence[numseq]->name, com_token, sizeof(sequence[numseq]->name));
	
	VectorCopy( defaultadjust, adjust );
	scale_up = default_scale;

	// set default values
	zrotation = defaultzrotation;
	sequence[numseq]->fps = 30.0;
	sequence[numseq]->seqgroup = 0;
	sequence[numseq]->blendstart[0] = 0.0;
	sequence[numseq]->blendend[0] = 1.0f;

	while( 1 )
	{
		if( depth > 0 )
		{
			if(!Com_GetToken( true ))
				break;
		}
		else if(!Com_TryToken()) break;
		
		if (Com_MatchToken( "{" )) depth++;
		else if (Com_MatchToken( "}" )) depth--;
		else if (Com_MatchToken( "event" )) depth -= Option_Event( sequence[numseq] );
		else if (Com_MatchToken( "pivot" )) Option_AddPivot( sequence[numseq] );
		else if (Com_MatchToken( "fps" )) Option_Fps( sequence[numseq] );
		else if (Com_MatchToken( "origin" )) Option_Origin();
		else if (Com_MatchToken( "rotate" )) Option_Rotate();
		else if (Com_MatchToken( "scale" )) Option_ScaleUp();
		else if (Com_MatchToken( "loop" )) sequence[numseq]->flags |= STUDIO_LOOPING;
		else if (Com_MatchToken( "frame" ))
		{
			start = com.atoi(Com_GetToken( false ));
			end = com.atoi(Com_GetToken( false ));
		}
		else if (Com_MatchToken( "blend" ))
		{
			sequence[numseq]->blendtype[0] = lookupControl(Com_GetToken( false ));
			sequence[numseq]->blendstart[0] = com.atof(Com_GetToken( false ));
			sequence[numseq]->blendend[0] = com.atof(Com_GetToken( false ));
		}
		else if (Com_MatchToken( "node" ))
		{
			sequence[numseq]->entrynode = sequence[numseq]->exitnode = com.atoi(Com_GetToken( false ));
		}
		else if (Com_MatchToken( "transition" ))
		{
			sequence[numseq]->entrynode = com.atoi(Com_GetToken( false ));
			sequence[numseq]->exitnode = com.atoi(Com_GetToken( false ));
		}
		else if (Com_MatchToken( "rtransition" ))
		{
			sequence[numseq]->entrynode = com.atoi(Com_GetToken( false ));
			sequence[numseq]->exitnode = com.atoi(Com_GetToken( false ));
			sequence[numseq]->nodeflags |= 1;
		}
		else if (lookupControl( com_token) != -1)
		{
			sequence[numseq]->motiontype |= lookupControl( com_token);
		}
		else if (Com_MatchToken( "animation" ))
		{
			strncpy( smdfilename[numblends], Com_GetToken( false ), sizeof(smdfilename[numblends]));
			numblends++;
		}
		else if (i = lookupActivity( com_token))
		{
			sequence[numseq]->activity = i;
			sequence[numseq]->actweight = 1;//default weight
			if(Com_TryToken())
			{
				// make sure what is really actweight
				if(!Com_MatchToken("{") && !Com_MatchToken("}") && strlen(com_token) < 3 && atoi(com_token) <= 100)
					sequence[numseq]->actweight = com.atoi(com_token);
				else Com_FreeToken(); // release token
			}
		}
		else
		{
			com.strncpy( smdfilename[numblends], com_token, sizeof(smdfilename[numblends]));
			numblends++;
		}
	}

	if( numblends == 0 ) 
	{
		Mem_Free( sequence[numseq] ); // no needs more
		MsgDev( D_INFO, "Cmd_Sequence: \"%s\" has no animations. skipped...\n", sequence[numseq]->name );
		return 0;
	}
	for( i = 0; i < numblends; i++ )
	{
		panimation[numani] = Kalloc( sizeof( s_animation_t ));
		sequence[numseq]->panim[i] = panimation[numani];
		sequence[numseq]->panim[i]->startframe = start;
		sequence[numseq]->panim[i]->endframe = end;
		sequence[numseq]->panim[i]->flags = 0;
		if( i == 0 ) Msg( "grabbing %s\n", filename );
		else MsgDev( D_INFO, "adding %s[%i]\n", filename, numani );
		Option_Animation( smdfilename[i], panimation[numani] );
		numani++;
	}
	sequence[numseq]->numblends = numblends;
	numseq++;

	return 1;
}

/*
==============
Cmd_Root

syntax: $root "pivotname"
==============
*/
int Cmd_Root (void)
{
	if (Com_GetToken(false))
	{
		strncpy( pivotname[0], com_token, sizeof(pivotname));
		return 0;
	}
	return 1;
}

/*
==============
Cmd_Pivot

syntax: $pivot (<index>) ("pivotname")
==============
*/
int Cmd_Pivot (void)
{
	if (Com_GetToken (false))
	{
		int index = atoi(com_token);
		if (Com_GetToken(false))
		{
			strncpy( pivotname[index], com_token, sizeof(pivotname[index]));
			return 0;
		}
	}
	return 1;
}

/*
==============
Cmd_Controller

syntax: $controller "mouth"|<number> ("name") <type> <start> <end>
==============
*/
int Cmd_Controller( void )
{
	if (Com_GetToken (false))
	{
		//mouth is hardcoded at four controller
		if (Com_MatchToken( "mouth" )) bonecontroller[numbonecontrollers].index = 4;
		else bonecontroller[numbonecontrollers].index = com.atoi(com_token);

		if (Com_GetToken(false))
		{
			com.strncpy( bonecontroller[numbonecontrollers].name, com_token, sizeof(bonecontroller[numbonecontrollers].name));

			Com_GetToken(false);
			if ((bonecontroller[numbonecontrollers].type = lookupControl(com_token)) == -1) 
			{
				MsgDev( D_ERROR, "Cmd_Controller: unknown bonecontroller type '%s'\n", com_token );
				return 0;
			}

			bonecontroller[numbonecontrollers].start = atof(Com_GetToken(false));
			bonecontroller[numbonecontrollers].end = atof(Com_GetToken(false));

			if (bonecontroller[numbonecontrollers].type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
			{
				if (((int)(bonecontroller[numbonecontrollers].start + 360) % 360) == ((int)(bonecontroller[numbonecontrollers].end + 360) % 360))
					bonecontroller[numbonecontrollers].type |= STUDIO_RLOOP;
			}
			numbonecontrollers++;
		}
	}
	return 1;
}

/*
==============
Cmd_BBox

syntax: $bbox <mins0> <mins1> <mins2> <maxs0> <maxs1> <maxs2>
==============
*/
void Cmd_BBox (void)
{
	bbox[0][0] = atof(Com_GetToken(false));
	bbox[0][1] = atof(Com_GetToken(false));
	bbox[0][2] = atof(Com_GetToken(false));
	bbox[1][0] = atof(Com_GetToken(false));
	bbox[1][1] = atof(Com_GetToken(false));
	bbox[1][2] = atof(Com_GetToken(false));
}

/*
==============
Cmd_CBox

syntax: $cbox <mins0> <mins1> <mins2> <maxs0> <maxs1> <maxs2>
==============
*/
void Cmd_CBox (void)
{
	cbox[0][0] = atof(Com_GetToken(false));
	cbox[0][1] = atof(Com_GetToken(false));
	cbox[0][2] = atof(Com_GetToken(false));
	cbox[1][0] = atof(Com_GetToken(false));
	cbox[1][1] = atof(Com_GetToken(false));
	cbox[1][2] = atof(Com_GetToken(false));
}

/*
==============
Cmd_Mirror

syntax: $mirrorbone "name"
==============
*/
void Cmd_Mirror ( void )
{
	strncpy( mirrored[nummirrored], Com_GetToken(false), sizeof(mirrored[nummirrored]));
	nummirrored++;
}

/*
==============
Cmd_Gamma

syntax: $gamma <value>
==============
*/
void Cmd_Gamma ( void )
{
	gamma = atof(Com_GetToken(false));
}

/*
==============
Cmd_TextureGroup

syntax: $texturegroup
{
{ texture1.bmp }
{ texture2.bmp }
{ texture3.bmp }
}
==============
*/
int Cmd_TextureGroup( void )
{
	int i;
	int depth = 0;
	int index = 0;
	int group = 0;

	if (numtextures == 0) 
	{
		MsgDev( D_ERROR, "Cmd_TextureGroup: texturegroups must follow model loading\n");
		return 0;
	}
          
	if (!Com_GetToken(false)) return 0;
	if (numskinref == 0) numskinref = numtextures;

	while (1)
	{
		if(!Com_GetToken(true))
		{
                              if (depth)MsgDev( D_ERROR, "missing }\n");
			break;
                    }

		if (Com_MatchToken( "{" )) depth++;
		else if (Com_MatchToken( "}" ))
		{
			depth--;
			if (depth == 0) break;
			group++;
			index = 0;
		}
		else if (depth == 2)
		{
			i = lookup_texture( com_token);
			texturegroup[numtexturegroups][group][index] = i;
			if (group != 0) texture[i]->parent = texturegroup[numtexturegroups][0][index];
			index++;
			numtexturereps[numtexturegroups] = index;
			numtexturelayers[numtexturegroups] = group + 1;
		}
	}

	numtexturegroups++;
	return 0;
}

/*
==============
Cmd_Hitgroup

syntax: $hitgroup <index> <name>
==============
*/
int Cmd_Hitgroup( void )
{
	hitgroup[numhitgroups].group = atoi(Com_GetToken(false));
	strncpy( hitgroup[numhitgroups].name, Com_GetToken(false), sizeof(hitgroup[numhitgroups].name));
	numhitgroups++;

	return 0;
}

/*
==============
Cmd_Hitbox

syntax: $hbox <index> <name> <mins0> <mins1> <mins2> <maxs0> <maxs1> <maxs2>
==============
*/
int Cmd_Hitbox( void )
{
	hitbox[numhitboxes].group = atoi(Com_GetToken(false));
	strncpy( hitbox[numhitboxes].name, Com_GetToken(false), sizeof(hitbox[numhitboxes].name));
	hitbox[numhitboxes].bmin[0] = atof( Com_GetToken(false));
	hitbox[numhitboxes].bmin[1] = atof(Com_GetToken(false));
	hitbox[numhitboxes].bmin[2] = atof(Com_GetToken(false));
	hitbox[numhitboxes].bmax[0] = atof(Com_GetToken(false));
	hitbox[numhitboxes].bmax[1] = atof(Com_GetToken(false));
	hitbox[numhitboxes].bmax[2] = atof(Com_GetToken(false));
	numhitboxes++;

	return 0;
}

/*
==============
Cmd_Attachment

syntax: $attachment <index> <bonename> <x> <y> <z> (old stuff)
==============
*/
int Cmd_Attachment( void )
{
	// index
	attachment[numattachments].index = atoi(Com_GetToken(false));
	// bone name
	com.strncpy( attachment[numattachments].bonename, Com_GetToken(false), sizeof(attachment[numattachments].bonename));
	// position
	attachment[numattachments].org[0] = atof(Com_GetToken(false));
	attachment[numattachments].org[1] = atof(Com_GetToken(false));
	attachment[numattachments].org[2] = atof(Com_GetToken(false));

	// skip old stuff
	while(Com_TryToken());

	numattachments++;
	return 0;
}

/*
==============
Cmd_Renamebone

syntax: $renamebone <oldname> <newname>
==============
*/
void Cmd_Renamebone( void )
{
	com.strncpy( renamedbone[numrenamedbones].from, Com_GetToken(false), 32 );
	com.strncpy( renamedbone[numrenamedbones].to, Com_GetToken(false), 32 );
	numrenamedbones++;
}


/*
==============
Cmd_TexRenderMode

syntax: $texrendermode "texture.bmp" "rendermode"
==============
*/
void Cmd_TexRenderMode( void )
{
	char tex_name[64];

	strcpy(tex_name, Com_GetToken(false));
	Com_GetToken(false);

	if(Com_MatchToken( "additive" ))
	{
		texture[lookup_texture(tex_name)]->flags |= STUDIO_NF_ADDITIVE;
	}
	else if(Com_MatchToken( "masked" ))
	{
		texture[lookup_texture(tex_name)]->flags |= STUDIO_NF_TRANSPARENT;
	}
	else if(Com_MatchToken( "blended" ))
	{
		texture[lookup_texture(tex_name)]->flags |= STUDIO_NF_BLENDED;
	}
	else MsgDev( D_WARN, "Cmd_TexRenderMode: texture '%s' have unknown render mode '%s'!\n", tex_name, com_token);

}

/*
==============
Cmd_Replace

syntax: $replacetexture "oldname.bmp" "newname.bmp"
==============
*/
void Cmd_Replace( void )
{
	com.strcpy ( sourcetexture[numrep], Com_GetToken(false));
	com.strcpy ( defaulttexture[numrep], Com_GetToken(false));
	numrep++;
}

/*
==============
Cmd_CdSet

syntax: $cd "path"
==============
*/
void Cmd_CdSet( void )
{
	if(!cdset)
	{
		cdset = true;
		FS_AddGameHierarchy(Com_GetToken (false));
	}
	else  Msg("Warning: $cd already set\n");
}

/*
==============
Cmd_CdSet

syntax: $cdtexture "path"
==============
*/
void Cmd_CdTextureSet( void )
{
	if(cdtextureset < 16) 
	{
		cdtextureset++;
		FS_AddGameHierarchy( Com_GetToken (false));
	}
	else Msg("Warning: $cdtexture already set\n");
}

/*
==============
Cmd_DebugBuild

syntax: $debug
==============
*/
void Cmd_DebugBuild( void )
{
	ignore_errors = true;
}

void ResetModelInfo( void )
{
	default_scale = 1.0;
	defaultzrotation = M_PI / 2;

	numrep = 0;
	gamma = 1.8f;
	flip_triangles = 1;
	normal_blend = cos( 2.0 * (M_PI / 180.0));

	//make an option
	dump_hboxes = 0;

	strcpy( gs_filename, "model" );
	strcpy( sequencegroup.label, "default" );
	FS_ClearSearchPath( ); //clear all $cd and $cdtexture

	//set default model parms
	FS_FileBase( gs_filename, modeloutname );//kill path and ext
	FS_DefaultExtension( modeloutname, ".mdl" );//set new ext
}

/*
==============
Cmd_StudioUnknown

syntax: "blabla"
==============
*/
void Cmd_StudioUnknown( void )
{
	MsgDev( D_WARN, "Cmd_StudioUnknown: skip command \"%s\"\n", com_token);
	while(Com_TryToken());
}

bool ParseModelScript( void )
{
	ResetModelInfo();

	while( 1 )
	{
		if(!Com_GetToken (true)) break;

		if (Com_MatchToken("$modelname")) Cmd_Modelname ();
		else if (Com_MatchToken("$cd")) Cmd_CdSet();
		else if (Com_MatchToken("$debug")) Cmd_DebugBuild();
		else if (Com_MatchToken("$cdtexture")) Cmd_CdTextureSet();
		else if (Com_MatchToken("$scale")) Cmd_ScaleUp ();
		else if (Com_MatchToken("$rotate")) Cmd_Rotate();
		else if (Com_MatchToken("$root")) Cmd_Root ();
		else if (Com_MatchToken("$pivot")) Cmd_Pivot ();
		else if (Com_MatchToken("$controller")) Cmd_Controller ();
		else if (Com_MatchToken("$body")) Cmd_Body();
		else if (Com_MatchToken("$bodygroup")) Cmd_Bodygroup();
		else if (Com_MatchToken("$sequence")) Cmd_Sequence ();
		else if (Com_MatchToken("$eyeposition")) Cmd_Eyeposition ();
		else if (Com_MatchToken("$origin")) Cmd_Origin ();
		else if (Com_MatchToken("$bbox")) Cmd_BBox ();
		else if (Com_MatchToken("$cbox")) Cmd_CBox ();
		else if (Com_MatchToken("$mirrorbone")) Cmd_Mirror ();
		else if (Com_MatchToken("$gamma")) Cmd_Gamma ();
		else if (Com_MatchToken("$texturegroup")) Cmd_TextureGroup ();
		else if (Com_MatchToken("$hgroup")) Cmd_Hitgroup ();
		else if (Com_MatchToken("$hbox")) Cmd_Hitbox ();
		else if (Com_MatchToken("$attachment")) Cmd_Attachment ();
		else if (Com_MatchToken("$externaltextures")) split_textures = 1;
		else if (Com_MatchToken("$cliptotextures")) clip_texcoords = 1;
		else if (Com_MatchToken("$renamebone")) Cmd_Renamebone ();
		else if (Com_MatchToken("$texrendermode")) Cmd_TexRenderMode();
		else if (Com_MatchToken("$replacetexture")) Cmd_Replace();
		else if (!Com_ValidScript( QC_STUDIOMDL )) return false;
		else Cmd_StudioUnknown();
	}

	// process model
	SetSkinValues();
	SimplifyModel();

	return true;
}

void CreateModelScript( void )
{
	file_t	*f = FS_Open( "model.qc", "w" );
	search_t	*t = FS_Search( "*.smd", false );
	int	i;

	// header
	FS_Printf( f, "\n$modelname \"model.mdl\"" );
	FS_Printf( f, "\n$cd \".\\\"" );
	FS_Printf( f, "\n$cdtexture \".\\\"" );
	FS_Printf( f, "\n$scale 1.0" );
	FS_Printf( f, "\n$cliptotextures\n\n" );

	// body FIXME: parse for real reference
	FS_Printf( f, "\n//reference mesh(es)" );
	FS_Printf( f, "\n$body \"studio\" \"reference\"" );

	// write animations
	for( i = 0; t && i < t->numfilenames; i++ )
	{
		// FIXME: make cases for "attack_%"
		FS_Printf( f, "$sequence \"t->filenames[i]\" \"t->filenames[i]\"" );
		if(com.stristr(t->filenames[i], "walk" )) FS_Printf( f, "LX fps 30 loop ACT_WALK 1\n" );
		else if(com.stristr(t->filenames[i], "run" )) FS_Printf( f, "LX fps 30 loop ACT_RUN 1\n" );		
		else if(com.stristr(t->filenames[i], "idle")) FS_Printf( f, "fps 16 loop ACT_IDLE 50\n" );
		else FS_Printf( f, "fps 16\n" );
	}
	FS_Printf(f, "\n" ); //finished
	FS_Close( f );
}

void ClearModel( void )
{
	cdset = false;
	cdtextureset = 0;
	ignore_errors = false;

	numrep = gflags = numseq = nummirrored = 0;
	numxnodes = numrenamedbones = totalframes = numbones = numhitboxes = 0;
	numhitgroups = numattachments = numtextures = numskinref = numskinfamilies = 0;
	numbonecontrollers = numtexturegroups = numcommandnodes = numbodyparts = 0;
	stripcount = numcommands = linecount = allverts = alltris = totalseconds = 0;
	nummodels = numani = split_textures = 0;

	memset(numtexturelayers, 0, sizeof(int) * 32);
	memset(numtexturereps, 0, sizeof(int) * 32);
	memset(bodypart, 0, sizeof(s_bodypart_t) * MAXSTUDIOBODYPARTS);
	Mem_EmptyPool( studiopool );	// free all memory
}

bool CompileCurrentModel( const char *name )
{
	bool load = false;

	cdset = false;
	cdtextureset = 0;
	
	if( name ) com.strcpy( gs_filename, name );
	FS_DefaultExtension( gs_filename, ".qc" );
	if(!FS_FileExists( gs_filename ))
	{
		// try to create qc-file
		CreateModelScript();
	}

	load = Com_LoadScript( gs_filename, NULL, 0 );
	if( load )
	{
		if(!ParseModelScript())
			return false;
		WriteMDLFile();
		ClearModel();
		return true;
	}

	Msg("%s not found\n", gs_filename );
	return false;
}

bool CompileStudioModel ( byte *mempool, const char *name, byte parms )
{
	if(mempool) studiopool = mempool;
	else
	{
		Msg("Studiomdl: can't allocate memory pool.\nAbort compilation\n");
		return false;
	}
	return CompileCurrentModel( name );	
}