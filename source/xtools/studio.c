//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        studio.c - half-life model compiler
//=======================================================================

#include "mdllib.h"
#include "matrix_lib.h"
#include "stdio.h"		// sscanf

bool		cdset;
bool		ignore_errors;

byte		*studiopool;
byte		*pData;
byte		*pStart;
string		modeloutname;

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
string		pivotname[64];	// names of the pivot points
string		defaulttexture[64];
string		sourcetexture[64];
string		mirrored[MAXSTUDIOSRCBONES];

s_mesh_t		*pmesh;
token_t		token;
dstudiohdr_t	*phdr;
dstudioseqhdr_t	*pseqhdr;
file_t		*smdfile;
char		line[2048];
int		linecount;
script_t		*studioqc;
script_t		*studioincludes[32];
int		numincludes;
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
s_animation_t	*panimation[MAXSTUDIOSEQUENCES*MAXSTUDIOBLENDS];	// each sequence can have 16 blends
s_bonecontroller_t	bonecontroller[MAXSTUDIOSRCBONES];

/*
============
WriteBoneInfo
============
*/
void WriteBoneInfo( void )
{
	int i, j;
	dstudiobone_t *pbone;
	dstudiobonecontroller_t *pbonecontroller;
	dstudioattachment_t *pattachment;
	dstudiobbox_t *pbbox;

	// save bone info
	pbone = (dstudiobone_t *)pData;
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

	pData += numbones * sizeof( dstudiobone_t );
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
			MsgDev( D_WARN, "unknown bonecontroller type %i\n", bonecontroller[i].type );
			pbone[j].bonecontroller[0] = i; //default
			break;
		}
	}

	// save bonecontroller info
	pbonecontroller = (dstudiobonecontroller_t *)pData;
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
	
	pData += numbonecontrollers * sizeof( dstudiobonecontroller_t );
	ALIGN( pData );

	// save attachment info
	pattachment = (dstudioattachment_t *)pData;
	phdr->numattachments = numattachments;
	phdr->attachmentindex = (pData - pStart);

	for (i = 0; i < numattachments; i++)
	{
		pattachment[i].bone	= attachment[i].bone;
		VectorCopy( attachment[i].org, pattachment[i].org );
	}
	
	pData += numattachments * sizeof( dstudioattachment_t );
	ALIGN( pData );

	// save bbox info
	pbbox = (dstudiobbox_t *)pData;
	phdr->numhitboxes = numhitboxes;
	phdr->hitboxindex = (pData - pStart);

	for (i = 0; i < numhitboxes; i++)
	{
		pbbox[i].bone = hitbox[i].bone;
		pbbox[i].group = hitbox[i].group;
		VectorCopy( hitbox[i].bmin, pbbox[i].bbmin );
		VectorCopy( hitbox[i].bmax, pbbox[i].bbmax );
	}

	pData += numhitboxes * sizeof( dstudiobbox_t );
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

	dstudioseqgroup_t	*pseqgroup;
	dstudioseqdesc_t	*pseqdesc;
	dstudioseqdesc_t	*pbaseseqdesc;
	dstudioevent_t	*pevent;
	dstudiopivot_t	*ppivot;
	byte		*ptransition;


	// save sequence info
	pseqdesc = (dstudioseqdesc_t *)pData;
	pbaseseqdesc = pseqdesc;
	phdr->numseq = numseq;
	phdr->seqindex = (pData - pStart);
	pData += numseq * sizeof( dstudioseqdesc_t );

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
		pevent = (dstudioevent_t *)pData;
		pseqdesc->numevents	= sequence[i]->numevents;
		pseqdesc->eventindex = (pData - pStart);
		pData += pseqdesc->numevents * sizeof( dstudioevent_t );

		for (j = 0; j < sequence[i]->numevents; j++)
		{
			pevent[j].frame = sequence[i]->event[j].frame - sequence[i]->frameoffset;
			pevent[j].event = sequence[i]->event[j].event;
			Mem_Copy( pevent[j].options, sequence[i]->event[j].options, sizeof( pevent[j].options ));
		}

		ALIGN( pData );

		// save pivots
		ppivot = (dstudiopivot_t *)pData;
		pseqdesc->numpivots	= sequence[i]->numpivots;
		pseqdesc->pivotindex = (pData - pStart);
		pData += pseqdesc->numpivots * sizeof( dstudiopivot_t );

		for (j = 0; j < sequence[i]->numpivots; j++)
		{
			VectorCopy( sequence[i]->pivot[j].org, ppivot[j].org );
			ppivot[j].start = sequence[i]->pivot[j].start - sequence[i]->frameoffset;
			ppivot[j].end = sequence[i]->pivot[j].end - sequence[i]->frameoffset;
		}

		ALIGN( pData );
	}

	// save sequence group info
	pseqgroup = (dstudioseqgroup_t *)pData;
	phdr->numseqgroups = 1;
	phdr->seqgroupindex = (pData - pStart);
	pData += sizeof( dstudioseqgroup_t );

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

	dstudioanim_t	*panim;
	dstudioanimvalue_t	*panimvalue;

	for (i = 0; i < numseq; i++) 
	{
		if (sequence[i]->seqgroup == group)
		{
			// save animations
			panim = (dstudioanim_t *)pData;
			sequence[i]->animindex = (pData - pStart);
			pData += sequence[i]->numblends * numbones * sizeof( dstudioanim_t );
			ALIGN( pData );
			
			panimvalue = (dstudioanimvalue_t *)pData;
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
	dstudiotexture_t	*ptexture;
	short		*pref;
	int		i, j;

	// save bone info
	ptexture = (dstudiotexture_t *)pData;
	phdr->numtextures = numtextures;
	phdr->textureindex = (pData - pStart);
	pData += numtextures * sizeof( dstudiotexture_t );
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
	dstudiobodyparts_t	*pbodypart;
	dstudiomodel_t	*pmodel;
	byte		*pbone;
	vec3_t		*pvert;
	vec3_t		*pnorm;
	dstudiomesh_t	*pmesh;
	s_trianglevert_t	*psrctri;
	int		i, j, k, cur;
	int		total_tris = 0;
	int		total_strips = 0;

	pbodypart = (dstudiobodyparts_t *)pData;
	phdr->numbodyparts = numbodyparts;
	phdr->bodypartindex = (pData - pStart);
	pData += numbodyparts * sizeof( dstudiobodyparts_t );

	pmodel = (dstudiomodel_t *)pData;
	pData += nummodels * sizeof( dstudiomodel_t );

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

	for( i = 0; i < nummodels; i++ ) 
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
		
		Msg( "vertices  %6s (%d vertices, %d normals)\n", memprint( pData - (char *)cur ), model[i]->numverts, model[i]->numnorms);
		cur = (int)pData;

		// save mesh info
		pmesh = (dstudiomesh_t *)pData;
		pmodel[i].nummesh = model[i]->nummesh;
		pmodel[i].meshindex = (pData - pStart);
		pData += pmodel[i].nummesh * sizeof( dstudiomesh_t );

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
			Mem_Copy( pData, pCmdSrc, numCmdBytes );
			pData += numCmdBytes;
			
			ALIGN( pData );
			total_tris += pmesh[j].numtris;
			total_strips += numcommandnodes;
		}
		Msg("mesh      %6s (%d tris, %d strips)\n", memprint( pData - ( char *)cur ), total_tris, total_strips );
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

		phdr = (dstudiohdr_t *)pStart;
		phdr->ident = IDSTUDIOHEADER;
		phdr->version = STUDIO_VERSION;

		pData = (byte *)phdr + sizeof( dstudiohdr_t );

		WriteTextures( );

		phdr->length = pData - pStart;
		Msg( "textures  %6s\n", memprint( phdr->length ));

		FS_WriteFile( texname, pStart, phdr->length );

		Mem_Set( pStart, 0, phdr->length );
		pData = pStart;
	}

	// write the model output file
	FS_DefaultExtension( modeloutname, ".mdl" );
	
	Msg ("---------------------\n");
	Msg ("writing %s:\n", modeloutname);

	phdr = (dstudiohdr_t *)pStart;
	phdr->ident = IDSTUDIOHEADER;
	phdr->version = STUDIO_VERSION;
	com.strncpy( phdr->name, modeloutname, 64 );

	VectorCopy( eyeposition, phdr->eyeposition );
	VectorCopy( bbox[0], phdr->min ); 
	VectorCopy( bbox[1], phdr->max ); 
	VectorCopy( cbox[0], phdr->bbmin ); 
	VectorCopy( cbox[1], phdr->bbmax ); 

	phdr->flags = gflags;
	pData = (byte *)phdr + sizeof( dstudiohdr_t );

	WriteBoneInfo();
	Msg("bones     %6s (%d)\n", memprint( pData - pStart - total ), numbones );
	total = pData - pStart;
	pData = WriteAnimations( pData, pStart, 0 );

	WriteSequenceInfo( );
	Msg("sequences %6s (%d frames) [%d:%02d]\n", memprint( pData - pStart - total ), totalframes, (int)totalseconds / 60, (int)totalseconds % 60 );
	total  = pData - pStart;

	WriteModel();
	Msg("models    %6s\n", memprint( pData - pStart - total ));
	total  = pData - pStart;

	if( !split_textures )
	{
		WriteTextures();
		Msg("textures  %6s\n", memprint( pData - pStart - total ));
	}

	phdr->length = pData - pStart;
	FS_WriteFile( modeloutname, pStart, phdr->length );
	Msg("total     %6s\n", memprint( phdr->length ));
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
					com.strncpy( bonetable[k].name, model[i]->node[j].name, sizeof(bonetable[k].name));
					if ((n = model[i]->node[j].parent) != -1)
						bonetable[k].parent	= findNode( model[i]->node[n].name );
					else bonetable[k].parent = -1;
					bonetable[k].bonecontroller = 0;
					bonetable[k].flags = 0;

					// set defaults
					defaultpos[k] = Kalloc( MAXSTUDIOANIMATIONS * sizeof( vec3_t ));
					defaultrot[k] = Kalloc( MAXSTUDIOANIMATIONS * sizeof( vec3_t ));

					for( n = 0; n < MAXSTUDIOANIMATIONS; n++ )
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

				if( dump_hboxes ) // remove this??
				{
					Msg( "$hbox %d \"%s\" %.2f %.2f %.2f  %.2f %.2f %.2f\n", hitbox[numhitboxes].group, bonetable[hitbox[numhitboxes].bone].name, hitbox[numhitboxes].bmin[0], hitbox[numhitboxes].bmin[1], hitbox[numhitboxes].bmin[2], hitbox[numhitboxes].bmax[0], hitbox[numhitboxes].bmax[1], hitbox[numhitboxes].bmax[2] );
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
				matrix4x4	bonetransform[MAXSTUDIOBONES];// bone transformation matrix
				matrix4x4	bonematrix; // local transformation matrix
				vec3_t	pos;

				for (j = 0; j < numbones; j++)
				{
					vec3_t origin, angle;

					// convert to degrees
					angle[0]	= sequence[i]->panim[q]->rot[j][n][0] * (180.0 / M_PI);
					angle[1]	= sequence[i]->panim[q]->rot[j][n][1] * (180.0 / M_PI);
					angle[2]	= sequence[i]->panim[q]->rot[j][n][2] * (180.0 / M_PI);
					origin[0] = sequence[i]->panim[q]->pos[j][n][0];
					origin[1] = sequence[i]->panim[q]->pos[j][n][1];
					origin[2] = sequence[i]->panim[q]->pos[j][n][2];

					Matrix4x4_CreateFromEntity( bonematrix, origin[0], origin[1], origin[2], angle[YAW], angle[ROLL], angle[PITCH], 1.0 );

					if( bonetable[j].parent == -1 ) Matrix4x4_Copy( bonetransform[j], bonematrix );
					else Matrix4x4_ConcatTransforms( bonetransform[j], bonetransform[bonetable[j].parent], bonematrix );
				}

				for( k = 0; k < nummodels; k++ )
				{
					for( j = 0; j < model[k]->numverts; j++ )
					{
						Matrix4x4_VectorTransform( bonetransform[model[k]->vert[j].bone], model[k]->vert[j].org, pos );

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
						dstudioanimvalue_t	*pcount, *pvalue;
						short		value[MAXSTUDIOANIMATIONS];
						dstudioanimvalue_t	data[MAXSTUDIOANIMATIONS];
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
						Mem_Set( data, 0, sizeof( data ) ); 
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
							sequence[i]->panim[q]->anim[j][k] = Kalloc( (pvalue - data) * sizeof( dstudioanimvalue_t ));
							memmove( sequence[i]->panim[q]->anim[j][k], data, (pvalue - data) * sizeof( dstudioanimvalue_t ));
						}
					}
				}
			}
		}
	}
}

void Grab_Skin( s_texture_t *ptexture )
{
	rgbdata_t	*pic;

	pic = FS_LoadImage( ptexture->name, error_bmp, error_bmp_size );
	if( !pic ) Sys_Break( "unable to load %s\n", ptexture->name ); // no error.bmp, missing texture...
	Image_Process( &pic, 0, 0, IMAGE_PALTO24 );

	ptexture->ppal = Kalloc( 768 );
	Mem_Copy( ptexture->ppal, pic->palette, sizeof( rgb_t ) * 256 );
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
		matrix4x4	m;
		vec3_t	p;

		// convert to degrees
		angle[0] = pmodel->skeleton[i].rot[0] * (180.0 / M_PI);
		angle[1] = pmodel->skeleton[i].rot[1] * (180.0 / M_PI);
		angle[2] = pmodel->skeleton[i].rot[2] * (180.0 / M_PI);

		parent = pmodel->node[i].parent;
		if( parent == -1 )
		{
			// scale the done pos.
			// calc rotational matrices
			Matrix4x4_CreateFromEntity( bonefixup[i].m, 0, 0, 0, angle[YAW], angle[ROLL], angle[PITCH], 1.0 );
			Matrix4x4_Transpose( bonefixup[i].im, bonefixup[i].m ); 
			VectorCopy( pmodel->skeleton[i].pos, bonefixup[i].worldorg );
		}
		else
		{
			// calc compound rotational matrices
			Matrix4x4_CreateFromEntity( m, 0, 0, 0, angle[YAW], angle[ROLL], angle[PITCH], 1.0 );
			Matrix4x4_ConcatTransforms( bonefixup[i].m, bonefixup[parent].m, m );
			Matrix4x4_Transpose( bonefixup[i].im, bonefixup[i].m ); 

			// calc true world coord.
			Matrix4x4_VectorTransform( bonefixup[parent].m, pmodel->skeleton[i].pos, p );
			VectorAdd( p, bonefixup[parent].worldorg, bonefixup[i].worldorg );
		}
	}
}

void Grab_Triangles( s_model_t *pmodel )
{
	int	i, j;
	int	tcount = 0, ncount = 0;	
	vec3_t	vmin, vmax;

	vmin[0] = vmin[1] = vmin[2] = 99999;
	vmax[0] = vmax[1] = vmax[2] = -99999;

	Build_Reference( pmodel );

	// load the base triangles
	while ( 1 ) 
	{
		if( FS_Gets( smdfile, line, sizeof( line )) != -1 )
		{		
			s_mesh_t		*pmesh;
			char		texturename[64];
			s_trianglevert_t	*ptriv;
			int		bone;
			vec3_t		vert[3];
			vec3_t		norm[3];

			linecount++;

			// check for end
			if( !com.stricmp( "end", line )) return;

			// strip off trailing smag
			com.strncpy( texturename, line, sizeof( texturename ));
			for( i = com.strlen( texturename ) - 1; i >= 0 && !isgraph( texturename[i] ); i-- );
			texturename[i+1] = '\0';

			// funky texture overrides
			for( i = 0; i < numrep; i++ )  
			{
				if( sourcetexture[i][0] == '\0' ) 
				{
					com.strncpy( texturename, defaulttexture[i], sizeof( texturename ));
					break;
				}
				if( !com.stricmp( texturename, sourcetexture[i] )) 
				{
					com.strncpy( texturename, defaulttexture[i], sizeof( texturename ));
					break;
				}
			}
			if( texturename[0] == '\0' ) // invalid name
			{
				// weird model problem, skip them
				MsgDev( D_ERROR, "Grab_Triangles: triangle with invalid texname\n" );
				FS_Gets( smdfile, line, sizeof( line ));
				FS_Gets( smdfile, line, sizeof( line ));
				FS_Gets( smdfile, line, sizeof( line ));
				linecount += 3;
				continue;
			}

			pmesh = lookup_mesh( pmodel, texturename );

			for( j = 0; j < 3; j++ ) 
			{
				if( flip_triangles ) ptriv = lookup_triangle( pmesh, pmesh->numtris ) + 2 - j;
				else ptriv = lookup_triangle( pmesh, pmesh->numtris ) + j;
				
				if( FS_Gets( smdfile, line, sizeof( line )) != -1 ) 
				{
					s_vertex_t	p;
					vec3_t		tmp;
					s_normal_t	normal;

					linecount++;
					if( sscanf( line, "%d %f %f %f %f %f %f %f %f", &bone, &p.org[0], &p.org[1], &p.org[2], &normal.org[0], &normal.org[1], &normal.org[2], &ptriv->u, &ptriv->v ) == 9 ) 
					{
						// translate triangles
						if( bone < 0 || bone >= pmodel->numbones ) 
							Sys_Break( "bogus bone index %d \n%s line %d", bone, filename, token.line );
						VectorCopy( p.org, vert[j] );
						VectorCopy( normal.org, norm[j] );

						p.bone = bone;
						normal.bone = bone;
						normal.skinref = pmesh->skinref;

						if( p.org[2] < vmin[2] ) vmin[2] = p.org[2];

						adjust_vertex( p.org );
						scale_vertex( p.org );

						// move vertex position to object space.
						VectorSubtract( p.org, bonefixup[p.bone].worldorg, tmp );
						Matrix4x4_VectorTransform( bonefixup[p.bone].im, tmp, p.org );

						// move normal to object space.
						VectorCopy( normal.org, tmp );
						Matrix4x4_VectorTransform( bonefixup[p.bone].im, tmp, normal.org );
						VectorNormalize( normal.org );

						ptriv->normindex = lookup_normal( pmodel, &normal );
						ptriv->vertindex = lookup_vertex( pmodel, &p );
                                        
						// tag bone as being used
						// pmodel->bone[bone].ref = 1;
					}
					else
					{
						Sys_Break( "%s: error on line %d: %s\n", filename, linecount, line );
					}
				}
			}
			pmodel->trimesh[tcount] = pmesh;
			pmodel->trimap[tcount] = pmesh->numtris++;
			tcount++;
		}
		else break;
	}
	if( vmin[2] != 0.0 ) MsgDev( D_NOTE, "Grab_Triangles: lowest vector at %f\n", vmin[2] );
}

void Grab_Skeleton( s_node_t *pnodes, s_bone_t *pbones )
{
	float	x, y, z, xr, yr, zr;
	char	cmd[1024];
	int	index;
          int	time = 0;
	
	while( FS_Gets( smdfile, line, sizeof( line )) != -1 ) 
	{
		linecount++;
		if( sscanf( line, "%d %f %f %f %f %f %f", &index, &x, &y, &z, &xr, &yr, &zr ) == 7 )
		{
			pbones[index].pos[0] = x;
			pbones[index].pos[1] = y;
			pbones[index].pos[2] = z;

			scale_vertex( pbones[index].pos );

			if (pnodes[index].mirrored)
				VectorScale( pbones[index].pos, -1.0, pbones[index].pos );
			
			pbones[index].rot[0] = xr;
			pbones[index].rot[1] = yr;
			pbones[index].rot[2] = zr;

			clip_rotations( pbones[index].rot ); 
		}
		else if( sscanf( line, "%s %d", cmd, &index ))
		{
			if( !com.stricmp( cmd, "time" )) 
			{
				time += index;
				if( time > 0 ) MsgDev( D_WARN, "Grab_Skeleton: Warning! An animation file is probably used as a reference\n" ); 
			}
			else if( !com.stricmp( cmd, "end" )) 
				return;
		}
	}
	Sys_Break( "Unexpected EOF %s at line %d\n", filename, linecount );
}

int Grab_Nodes( s_node_t *pnodes )
{
	int	i, index;
	char	name[2048];
	int	parent;
	int	numbones = 0;

	while( FS_Gets( smdfile, line, sizeof( line )) != -1 ) 
	{
		linecount++;
		if( sscanf( line, "%d \"%[^\"]\" %d", &index, name, &parent ) == 3 )
		{
			com.strncpy( pnodes[index].name, name, sizeof( pnodes[index].name ));
			pnodes[index].parent = parent;
			numbones = index;

			// check for mirrored bones;
			for( i = 0; i < nummirrored; i++ )
			{
				if( !com.strcmp( name, mirrored[i] ))
					pnodes[index].mirrored = 1;
			}
			if((!pnodes[index].mirrored) && parent != -1 )
				pnodes[index].mirrored = pnodes[pnodes[index].parent].mirrored;
		}
		else return numbones + 1;
	}

	Sys_Break( "Unexpected EOF %s at line %d\n", filename, token.line );
	return 0;
}

void Grab_Studio( s_model_t *pmodel )
{
	char	cmd[1024];
	int	option;

	com.strncpy( filename, pmodel->name, sizeof( filename ));

	FS_DefaultExtension( filename, ".smd" );
	smdfile = FS_Open( filename, "rb" );
	if( !smdfile ) Sys_Break( "unable to open %s\n", filename );
	Msg( "grabbing %s\n", filename );
	linecount = 0;

	while( FS_Gets( smdfile, line, sizeof( line )) != -1 )
	{
		linecount++;
		sscanf( line, "%s %d", cmd, &option );
		if( !com.stricmp( cmd, "version" ))
		{
			if( option != 1 ) Sys_Break( "Grab_Studio: %s bad version file %i\n", filename, option );
		}
		else if( !com.stricmp( cmd, "nodes" ))
		{
			pmodel->numbones = Grab_Nodes( pmodel->node );
		}
		else if( !com.stricmp( cmd, "skeleton" ))
		{
			Grab_Skeleton( pmodel->node, pmodel->skeleton );
		}
		else if( !com.stricmp( cmd, "triangles" ))
		{
			Grab_Triangles( pmodel );
		}
		else MsgDev( D_WARN, "Grab_Studio: %s unknown studio command %s at line %d\n", filename, cmd, linecount );
	}
	FS_Close( smdfile );
}

/*
==============
Cmd_Eyeposition

syntax: $eyeposition <x> <y> <z>
==============
*/
void Cmd_Eyeposition( void )
{
	// rotate points into frame of reference so model points down the positive x axis
	Com_ReadFloat( studioqc, false, &eyeposition[1] );
	Com_ReadFloat( studioqc, false, &eyeposition[0] );
	Com_ReadFloat( studioqc, false, &eyeposition[2] );

	eyeposition[0] = -eyeposition[0];	// stupid Quake bug
}

/*
==============
Cmd_Modelname

syntax: $modelname "outname"
==============
*/
void Cmd_Modelname( void )
{
	Com_ReadString( studioqc, false, modeloutname );
}

void Option_Studio( void )
{
	if(!Com_ReadToken( studioqc, SC_ALLOW_PATHNAMES, &token )) return;

	model[nummodels] = Kalloc( sizeof( s_model_t ));
	bodypart[numbodyparts].pmodel[bodypart[numbodyparts].nummodels] = model[nummodels];
	com.strncpy( model[nummodels]->name, token.string, sizeof(model[nummodels]->name));

	flip_triangles = 1;
	scale_up = default_scale;

	while(Com_ReadToken( studioqc, 0, &token ))
	{
		if( !com.stricmp( token.string, "reverse" ))
			flip_triangles = 0;
		else if( !com.stricmp( token.string, "scale" ))
			Com_ReadFloat( studioqc, false, &scale_up );
	}

	Grab_Studio( model[nummodels] );

	scale_up = default_scale;		// reset to default
	bodypart[numbodyparts].nummodels++;
	nummodels++;
}


int Option_Blank( void )
{
	model[nummodels] = Kalloc( sizeof( s_model_t ));
	bodypart[numbodyparts].pmodel[bodypart[numbodyparts].nummodels] = model[nummodels];

	com.strncpy( model[nummodels]->name, "blank", sizeof(model[nummodels]->name));

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
	int	is_started = 0;

	if(!Com_ReadToken( studioqc, 0, &token )) return;

	if (numbodyparts == 0) bodypart[numbodyparts].base = 1;
	else bodypart[numbodyparts].base = bodypart[numbodyparts-1].base * bodypart[numbodyparts-1].nummodels;
	com.strncpy( bodypart[numbodyparts].name, token.string, sizeof( bodypart[numbodyparts].name ));

	while( 1 )
	{
		if( !Com_ReadToken( studioqc, SC_ALLOW_NEWLINES, &token )) return;
		if( !com.stricmp( token.string, "{" )) is_started = 1;
		else if( !com.stricmp( token.string, "}" )) break;
		else if( !com.stricmp( token.string, "studio" )) Option_Studio();
		else if( !com.stricmp( token.string, "blank" )) Option_Blank();
	}
	numbodyparts++;
}

/*
==============
Cmd_Modelname

syntax: $body "name" "mainref.smd"
==============
*/
void Cmd_Body( void )
{
	int	is_started = 0;

	if( !Com_ReadToken( studioqc, 0, &token )) return;
	if( numbodyparts == 0 ) bodypart[numbodyparts].base = 1;
	else bodypart[numbodyparts].base = bodypart[numbodyparts-1].base * bodypart[numbodyparts-1].nummodels;
	com.strncpy( bodypart[numbodyparts].name, token.string, sizeof( bodypart[numbodyparts].name ));

	Option_Studio();
	numbodyparts++;
}

void Grab_Animation( s_animation_t *panim )
{
	vec3_t	pos;
	vec3_t	rot;
	int	index;
	char	cmd[2048];
	int	t = -99999999;
	int	start = 99999;
	float	cz, sz;
	int	end = 0;

	for( index = 0; index < panim->numbones; index++ ) 
	{
		panim->pos[index] = Kalloc( MAXSTUDIOANIMATIONS * sizeof( vec3_t ));
		panim->rot[index] = Kalloc( MAXSTUDIOANIMATIONS * sizeof( vec3_t ));
	}

	cz = com.cos( zrotation );
	sz = com.sin( zrotation );

	while( FS_Gets( smdfile, line, sizeof( line )) != -1 ) 
	{
		linecount++;
		if( sscanf( line, "%d %f %f %f %f %f %f", &index, &pos[0], &pos[1], &pos[2], &rot[0], &rot[1], &rot[2] ) == 7 )
		{
			if( t >= panim->startframe && t <= panim->endframe )
			{
				if( panim->node[index].parent == -1 )
				{
					adjust_vertex( pos );
					panim->pos[index][t][0] = cz * pos[0] - sz * pos[1];
					panim->pos[index][t][1] = sz * pos[0] + cz * pos[1];
					panim->pos[index][t][2] = pos[2];
					rot[2] += zrotation; // rotate model
				}
				else VectorCopy( pos, panim->pos[index][t] );
				if( t > end ) end = t;
				if( t < start ) start = t;

				if( panim->node[index].mirrored )
					VectorScale( panim->pos[index][t], -1.0, panim->pos[index][t] );

				scale_vertex( panim->pos[index][t] );
				clip_rotations( rot );
				VectorCopy( rot, panim->rot[index][t] );
			}
		}
		else if( sscanf( line, "%s %d", cmd, &index ))
		{
			if( !com.stricmp( cmd, "time" )) 
			{
				t = index;
			}
			else if( !com.stricmp( cmd, "end" )) 
			{
				panim->startframe = start;
				panim->endframe = end;
				return;
			}
			else Sys_Break( "%s: error on line %d: %s\n", filename, linecount, line );
		}
		else Sys_Break( "%s: error on line %d: %s\n", filename, linecount, line );
	}
	Sys_Break( "unexpected EOF: %s.smd line %i\n", panim->name, token.line );
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

		memmove( ppos, &panim->pos[j][panim->startframe], size );
		memmove( prot, &panim->rot[j][panim->startframe], size );

		Mem_Free( panim->pos[j] );
		Mem_Free( panim->rot[j] );

		panim->pos[j] = ppos;
		panim->rot[j] = prot;
	}
}

void Option_Animation( const char *name, s_animation_t *panim )
{
	char	cmd[2048];
	int	option;
			
	com.strncpy( panim->name, name, sizeof( panim->name ));
	com.strncpy( filename, panim->name, sizeof( filename ));

	FS_DefaultExtension( filename, ".smd" );
	smdfile = FS_Open( filename, "rb" );
	if( !smdfile ) Sys_Break( "unable to open %s\n", filename );
	linecount = 0;

	while( FS_Gets( smdfile, line, sizeof( line )) != -1 )
	{
		linecount++;
		sscanf( line, "%s %d", cmd, &option );

		if( !com.stricmp( cmd, "version" ))
		{
			if( option != 1 ) MsgDev( D_ERROR, "Grab_Animation: %s bad version file\n", filename );
		}
		else if( !com.stricmp( cmd, "nodes" ))
		{
			panim->numbones = Grab_Nodes( panim->node );
		}
		else if( !com.stricmp( cmd, "skeleton" ))
		{
			Grab_Animation( panim );
			Shift_Animation( panim );
		}
		else 
		{
			MsgDev( D_WARN, "Option_Animation: unknown studio command : %s\n", cmd );
			while( FS_Gets( smdfile, line, sizeof( line )) != -1 )
			{
				linecount++;
				if( !com.strncmp( line, "end", 3 ))
					break;
			}
		}
	}
	FS_Close( smdfile );
}

int Option_Motion( s_sequence_t *psequence )
{
	string	motion;

	while( Com_ReadString( studioqc, false, motion ))
		psequence->motiontype |= lookupControl( motion );
	return 0;
}

int Option_Event( s_sequence_t *psequence )
{
	if( psequence->numevents + 1 >= MAXSTUDIOEVENTS )
	{
		MsgDev( D_ERROR, "Option_Event: MAXSTUDIOEVENTS limit excedeed.\n");
		return 0;
	}

	Com_ReadLong( studioqc, false, &psequence->event[psequence->numevents].event );
	Com_ReadLong( studioqc, false, &psequence->event[psequence->numevents].frame );
	psequence->numevents++;

	// option token
	if( Com_ReadToken( studioqc, 0, &token ))
	{
		if( !com.stricmp( token.string, "}" )) return 1; // opps, hit the end

		// found an option
		com.strncpy( psequence->event[psequence->numevents-1].options, token.string, 64 );
	}
	return 0;
}

int Option_Fps( s_sequence_t *psequence )
{
	Com_ReadFloat( studioqc, false, &psequence->fps );
	return 0;
}

int Option_AddPivot( s_sequence_t *psequence )
{
	if( psequence->numpivots + 1 >= MAXSTUDIOPIVOTS )
	{
		MsgDev( D_ERROR, "MAXSTUDIOPIVOTS limit excedeed.\n" );
		return 0;
	}
	
	Com_ReadLong( studioqc, false, &psequence->pivot[psequence->numpivots].index );
	Com_ReadLong( studioqc, false, &psequence->pivot[psequence->numpivots].start );
	Com_ReadLong( studioqc, false, &psequence->pivot[psequence->numpivots].end );
	psequence->numpivots++;

	return 0;
}

/*
==============
Cmd_Origin

syntax: $origin <x> <y> <z> (z rotate)
==============
*/
void Cmd_Origin( void )
{
	Com_ReadFloat( studioqc, false, &defaultadjust[0] );
	Com_ReadFloat( studioqc, false, &defaultadjust[1] );
	Com_ReadFloat( studioqc, false, &defaultadjust[2] );

	if( Com_ReadToken( studioqc, 0, &token ))
		defaultzrotation = DEG2RAD( com.atof( token.string ) + 90 );
}


void Option_Origin( void )
{
	Com_ReadFloat( studioqc, false, &adjust[0] );
	Com_ReadFloat( studioqc, false, &adjust[1] );
	Com_ReadFloat( studioqc, false, &adjust[2] );
}

void Option_Rotate( void )
{
	Com_ReadToken( studioqc, 0, &token );
	zrotation = DEG2RAD( com.atof( token.string ) + 90 );
}

/*
==============
Cmd_ScaleUp

syntax: $scale <value>
==============
*/
void Cmd_ScaleUp( void )
{
	Com_ReadFloat( studioqc, false, &scale_up );
	default_scale = scale_up;
}

/*
==============
Cmd_Rotate

syntax: $rotate <value>
==============
*/
void Cmd_Rotate( void )
{
	if(!Com_ReadToken( studioqc, 0, &token )) return;
	zrotation = DEG2RAD( com.atof( token.string ) + 90 );
}

void Option_ScaleUp (void)
{
	Com_ReadFloat( studioqc, false, &scale_up );
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
	LX				// movement flag (may be X, Y, Z, LX, LY, LZ, XR, YR, etc )
	ACT_WALK | ACT_32 (actweight)		// act name or act number and actweight (optionally)	
}
==============
*/
static int Cmd_Sequence( void )
{
	int	i, depth = 0;
	string	smdfilename[MAXSTUDIOBLENDS];
	int	end = MAXSTUDIOANIMATIONS - 1;
	int	numblends = 0;
	int	start = 0;

	if( !Com_ReadToken( studioqc, 0, &token )) return 0;

	// allocate new sequence
	if( numseq >= MAXSTUDIOSEQUENCES ) Sys_Break( "Too many sequences in model\n" );
	sequence[numseq] = Mem_Alloc( studiopool, sizeof(s_sequence_t));
	com.strncpy( sequence[numseq]->name, token.string, sizeof( sequence[numseq]->name ));
	
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
			if(!Com_ReadToken( studioqc, SC_ALLOW_NEWLINES|SC_ALLOW_PATHNAMES, &token ))
				break;
		}
		else if(!Com_ReadToken( studioqc, SC_ALLOW_PATHNAMES, &token )) break;
		
		if( !com.stricmp( token.string, "{" )) depth++;
		else if( !com.stricmp( token.string, "}" )) depth--;
		else if( !com.stricmp( token.string, "event" )) depth -= Option_Event( sequence[numseq] );
		else if( !com.stricmp( token.string, "pivot" )) Option_AddPivot( sequence[numseq] );
		else if( !com.stricmp( token.string, "fps" )) Option_Fps( sequence[numseq] );
		else if( !com.stricmp( token.string, "origin" )) Option_Origin();
		else if( !com.stricmp( token.string, "rotate" )) Option_Rotate();
		else if( !com.stricmp( token.string, "scale" )) Option_ScaleUp();
		else if( !com.stricmp( token.string, "loop" ) || !com.stricmp( token.string, "looping" ))
			sequence[numseq]->flags |= STUDIO_LOOPING;
		else if( !com.stricmp( token.string, "frame" ) || !com.stricmp( token.string, "frames" ))
		{
			Com_ReadLong( studioqc, false, &start );
			Com_ReadLong( studioqc, false, &end );
		}
		else if( !com.stricmp( token.string, "blend" ))
		{
			Com_ReadToken( studioqc, 0, &token );
			sequence[numseq]->blendtype[0] = lookupControl( token.string );
			Com_ReadFloat( studioqc, false, &sequence[numseq]->blendstart[0] );
			Com_ReadFloat( studioqc, false, &sequence[numseq]->blendend[0] );
		}
		else if( !com.stricmp( token.string, "node" ))
		{
			Com_ReadLong( studioqc, false, &sequence[numseq]->entrynode );
			sequence[numseq]->exitnode = sequence[numseq]->entrynode;
		}
		else if( !com.stricmp( token.string, "transition" ))
		{
			Com_ReadLong( studioqc, false, &sequence[numseq]->entrynode );
			Com_ReadLong( studioqc, false, &sequence[numseq]->exitnode );
		}
		else if( !com.stricmp( token.string, "rtransition" ))
		{
			Com_ReadLong( studioqc, false, &sequence[numseq]->entrynode );
			Com_ReadLong( studioqc, false, &sequence[numseq]->exitnode );
			sequence[numseq]->nodeflags |= 1;
		}
		else if( lookupControl( token.string ) != -1 )
		{
			sequence[numseq]->motiontype |= lookupControl( token.string );
		}
		else if( !com.stricmp( token.string, "animation" ))
		{
			Com_ReadString( studioqc, false, smdfilename[numblends] );
			numblends++;
		}
		else if( i = lookupActivity( token.string ))
		{
			sequence[numseq]->activity = i;
			sequence[numseq]->actweight = 1; // default weight
			Com_ReadLong( studioqc, false, &sequence[numseq]->actweight );
		}
		else
		{
			com.strncpy( smdfilename[numblends], token.string, sizeof( smdfilename[numblends] ));
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
		if( numani >= ( MAXSTUDIOSEQUENCES * MAXSTUDIOBLENDS ))
			Sys_Break( "Too many blends in model\n" );
		panimation[numani] = Kalloc( sizeof( s_animation_t ));
		sequence[numseq]->panim[i] = panimation[numani];
		sequence[numseq]->panim[i]->startframe = start;
		sequence[numseq]->panim[i]->endframe = end;
		sequence[numseq]->panim[i]->flags = 0;
		if( i == 0 ) Msg( "grabbing %s\n", filename );
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
int Cmd_Root( void )
{
	if( Com_ReadToken( studioqc, 0, &token ))
	{
		com.strncpy( pivotname[0], token.string, sizeof( pivotname ));
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
int Cmd_Pivot( void )
{
	int	index;

	if( Com_ReadLong( studioqc, false, &index ))
	{
		if( Com_ReadToken( studioqc, 0, &token ))
		{
			com.strncpy( pivotname[index], token.string, sizeof( pivotname[index] ));
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
	if( Com_ReadToken( studioqc, 0, &token ))
	{
		// mouth is hardcoded at four controller
		if( !com.stricmp( token.string, "mouth" ))
			bonecontroller[numbonecontrollers].index = 4;
		else bonecontroller[numbonecontrollers].index = com.atoi( token.string );

		if( Com_ReadToken( studioqc, 0, &token ))
		{
			com.strncpy( bonecontroller[numbonecontrollers].name, token.string, sizeof(bonecontroller[numbonecontrollers].name ));

			Com_ReadToken( studioqc, 0, &token );
			if((bonecontroller[numbonecontrollers].type = lookupControl( token.string )) == -1 ) 
			{
				MsgDev( D_ERROR, "Cmd_Controller: unknown bonecontroller type '%s'\n", token.string );
				Com_SkipRestOfLine( studioqc );
				return 0;
			}

			Com_ReadFloat( studioqc, false, &bonecontroller[numbonecontrollers].start );
			Com_ReadFloat( studioqc, false, &bonecontroller[numbonecontrollers].end );

			if( bonecontroller[numbonecontrollers].type & (STUDIO_XR|STUDIO_YR|STUDIO_ZR))
			{
				// check for infinity loop
				if(((int)(bonecontroller[numbonecontrollers].start + 360) % 360) == ((int)(bonecontroller[numbonecontrollers].end + 360) % 360))
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
void Cmd_BBox( void )
{
	Com_ReadFloat( studioqc, false, &bbox[0][0] );
	Com_ReadFloat( studioqc, false, &bbox[0][1] );
	Com_ReadFloat( studioqc, false, &bbox[0][2] );
	Com_ReadFloat( studioqc, false, &bbox[1][0] );
	Com_ReadFloat( studioqc, false, &bbox[1][1] );
	Com_ReadFloat( studioqc, false, &bbox[1][2] );
}

/*
==============
Cmd_CBox

syntax: $cbox <mins0> <mins1> <mins2> <maxs0> <maxs1> <maxs2>
==============
*/
void Cmd_CBox( void )
{
	Com_ReadFloat( studioqc, false, &cbox[0][0] );
	Com_ReadFloat( studioqc, false, &cbox[0][1] );
	Com_ReadFloat( studioqc, false, &cbox[0][2] );
	Com_ReadFloat( studioqc, false, &cbox[1][0] );
	Com_ReadFloat( studioqc, false, &cbox[1][1] );
	Com_ReadFloat( studioqc, false, &cbox[1][2] );
}

/*
==============
Cmd_Mirror

syntax: $mirrorbone "name"
==============
*/
void Cmd_Mirror( void )
{
	Com_ReadString( studioqc, false, mirrored[nummirrored] );
	nummirrored++;
}

/*
==============
Cmd_Gamma

syntax: $gamma <value>
==============
*/
void Cmd_Gamma( void )
{
	Com_ReadFloat( studioqc, false, &gamma );
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
	int	i, depth = 0;
	int	index = 0, group = 0;

	if( numtextures == 0 ) 
	{
		MsgDev( D_ERROR, "Cmd_TextureGroup: texturegroups must follow model loading\n");
		return 0;
	}
          
	if( !Com_ReadToken( studioqc, 0, &token )) return 0;
	if( numskinref == 0 ) numskinref = numtextures;

	while( 1 )
	{
		if( !Com_ReadToken( studioqc, SC_ALLOW_NEWLINES, &token ))
		{
                              if( depth ) MsgDev( D_ERROR, "missing }\n" );
			break;
                    }

		if( !com.stricmp( token.string, "{" )) depth++;
		else if( !com.stricmp( token.string, "}" ))
		{
			depth--;
			if( depth == 0 ) break;
			group++;
			index = 0;
		}
		else if( depth == 2 )
		{
			i = lookup_texture( token.string );
			texturegroup[numtexturegroups][group][index] = i;
			if( group != 0 ) texture[i]->parent = texturegroup[numtexturegroups][0][index];
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
	Com_ReadLong( studioqc, false, &hitgroup[numhitgroups].group );
	Com_ReadString( studioqc, false, hitgroup[numhitgroups].name );
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
	Com_ReadLong( studioqc, false, &hitbox[numhitboxes].group );
	Com_ReadString( studioqc, false, hitbox[numhitboxes].name );
	Com_ReadFloat( studioqc, false, &hitbox[numhitboxes].bmin[0] );
	Com_ReadFloat( studioqc, false, &hitbox[numhitboxes].bmin[1] );
	Com_ReadFloat( studioqc, false, &hitbox[numhitboxes].bmin[2] );
	Com_ReadFloat( studioqc, false, &hitbox[numhitboxes].bmax[0] );
	Com_ReadFloat( studioqc, false, &hitbox[numhitboxes].bmax[1] );
	Com_ReadFloat( studioqc, false, &hitbox[numhitboxes].bmax[2] );
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
	Com_ReadLong( studioqc, false, &attachment[numattachments].index );		// index
	Com_ReadString( studioqc, false, attachment[numattachments].bonename );	// bone name
	Com_ReadFloat( studioqc, false, &attachment[numattachments].org[0] );		// position
	Com_ReadFloat( studioqc, false, &attachment[numattachments].org[1] );
	Com_ReadFloat( studioqc, false, &attachment[numattachments].org[2] );

	// skip old stuff
	Com_SkipRestOfLine( studioqc );

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
	Com_ReadString( studioqc, false, renamedbone[numrenamedbones].from );
	Com_ReadString( studioqc, false, renamedbone[numrenamedbones].to );
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
	char	tex_name[64];

	Com_ReadString( studioqc, false, tex_name );
	Com_ReadToken( studioqc, 0, &token );

	if( !com.stricmp( token.string, "additive" ))
		texture[lookup_texture(tex_name)]->flags |= STUDIO_NF_ADDITIVE;
	else if( !com.stricmp( token.string, "masked" ))
		texture[lookup_texture(tex_name)]->flags |= STUDIO_NF_TRANSPARENT;
	else if( !com.stricmp( token.string, "blended" ))
		texture[lookup_texture(tex_name)]->flags |= STUDIO_NF_BLENDED;
	else MsgDev( D_WARN, "Cmd_TexRenderMode: texture '%s' have unknown render mode '%s'!\n", tex_name, token.string );

}

/*
==============
Cmd_Replace

syntax: $replacetexture "oldname.bmp" "newname.bmp"
==============
*/
void Cmd_Replace( void )
{
	Com_ReadString( studioqc, false, sourcetexture[numrep] );
	Com_ReadString( studioqc, false, defaulttexture[numrep]);
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
	if( !cdset )
	{
		string	path;

		cdset = true;
		Com_ReadString( studioqc, false, path );		
		FS_AddGameHierarchy( path, FS_NOWRITE_PATH );
	}
	else MsgDev( D_WARN, "$cd already set\n" );
}

/*
==============
Cmd_CdSet

syntax: $cdtexture "path"
==============
*/
void Cmd_CdTextureSet( void )
{
	if( cdtextureset < 16 ) 
	{
		string	path;

		cdtextureset++;
		Com_ReadString( studioqc, false, path );
		FS_AddGameHierarchy( path, FS_NOWRITE_PATH );
	}
	else MsgDev( D_WARN, "$cdtexture already set\n" );
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

/*
==============
Cmd_DebugBuild

syntax: $include "filename"
==============
*/
void Cmd_Include( void )
{
	script_t	*include;

	Com_ReadToken( studioqc, SC_ALLOW_PATHNAMES, &token );
	include = Com_OpenScript( token.string, NULL, 0 );
	studioqc = studioincludes[numincludes++] = include; // new script
}

void ResetModelInfo( void )
{
	default_scale = 1.0;
	defaultzrotation = M_PI / 2;

	numrep = 0;
	gamma = 1.8f;
	flip_triangles = 1;
	normal_blend = com.cos( DEG2RAD( 2.0f ));
	dump_hboxes = 0;	// FIXME: make an option

	com.strcpy( gs_filename, "model" );
	com.strcpy( sequencegroup.label, "default" );
	FS_ClearSearchPath( ); // clear all $cd and $cdtexture

	// set default model parms
	FS_FileBase( gs_filename, modeloutname );	// kill path and ext
	FS_DefaultExtension( modeloutname, ".mdl" );	// set new ext
}

/*
==============
Cmd_StudioUnknown

syntax: "blabla"
==============
*/
void Cmd_StudioUnknown( const char *token )
{
	MsgDev( D_WARN, "Cmd_StudioUnknown: skip command \"%s\"\n", token );
	Com_SkipRestOfLine( studioqc );
}

bool ParseModelScript( void )
{
	ResetModelInfo();

	while( 1 )
	{
		if( Com_EndOfScript( studioqc ))
		{
			Com_CloseScript( studioincludes[numincludes] );
			studioincludes[numincludes] = NULL;
			studioqc = studioincludes[--numincludes];
			if( Com_EndOfScript( studioqc )) break;			
		}
		if( !Com_ReadToken( studioqc, SC_ALLOW_NEWLINES|SC_PARSE_GENERIC, &token ))
			break;

		if( !com.stricmp( token.string, "$modelname" )) Cmd_Modelname ();
		else if( !com.stricmp( token.string, "$cd")) Cmd_CdSet();
		else if( !com.stricmp( token.string, "$debug")) Cmd_DebugBuild();
		else if( !com.stricmp( token.string, "$include")) Cmd_Include();
		else if( !com.stricmp( token.string, "$cdtexture")) Cmd_CdTextureSet();
		else if( !com.stricmp( token.string, "$scale")) Cmd_ScaleUp ();
		else if( !com.stricmp( token.string, "$rotate")) Cmd_Rotate();
		else if( !com.stricmp( token.string, "$root")) Cmd_Root ();
		else if( !com.stricmp( token.string, "$pivot")) Cmd_Pivot ();
		else if( !com.stricmp( token.string, "$controller")) Cmd_Controller ();
		else if( !com.stricmp( token.string, "$body")) Cmd_Body();
		else if( !com.stricmp( token.string, "$bodygroup")) Cmd_Bodygroup();
		else if( !com.stricmp( token.string, "$sequence")) Cmd_Sequence ();
		else if( !com.stricmp( token.string, "$eyeposition")) Cmd_Eyeposition ();
		else if( !com.stricmp( token.string, "$origin")) Cmd_Origin ();
		else if( !com.stricmp( token.string, "$bbox")) Cmd_BBox ();
		else if( !com.stricmp( token.string, "$cbox")) Cmd_CBox ();
		else if( !com.stricmp( token.string, "$mirrorbone")) Cmd_Mirror ();
		else if( !com.stricmp( token.string, "$gamma")) Cmd_Gamma ();
		else if( !com.stricmp( token.string, "$texturegroup")) Cmd_TextureGroup ();
		else if( !com.stricmp( token.string, "$hgroup")) Cmd_Hitgroup ();
		else if( !com.stricmp( token.string, "$hbox")) Cmd_Hitbox ();
		else if( !com.stricmp( token.string, "$attachment")) Cmd_Attachment ();
		else if( !com.stricmp( token.string, "$externaltextures")) split_textures = 1;
		else if( !com.stricmp( token.string, "$cliptotextures")) clip_texcoords = 1;
		else if( !com.stricmp( token.string, "$renamebone")) Cmd_Renamebone ();
		else if( !com.stricmp( token.string, "$texrendermode")) Cmd_TexRenderMode();
		else if( !com.stricmp( token.string, "$replacetexture")) Cmd_Replace();
		else if( !Com_ValidScript( token.string, QC_STUDIOMDL )) return false;
		else Cmd_StudioUnknown( token.string );
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
	stripcount = numcommands = numani = allverts = alltris = totalseconds = 0;
	nummodels = split_textures = 0;

	Mem_Set( studioincludes, 0, sizeof( studioincludes ));
	Mem_Set( numtexturelayers, 0, sizeof(int) * 32 );
	Mem_Set( numtexturereps, 0, sizeof(int) * 32 );
	Mem_Set( bodypart, 0, sizeof(s_bodypart_t) * MAXSTUDIOBODYPARTS );
	Mem_EmptyPool( studiopool );	// free all memory
}

bool CompileCurrentModel( const char *name )
{
	cdset = false;
	cdtextureset = numincludes = 0;
	
	if( name ) com.strcpy( gs_filename, name );
	FS_DefaultExtension( gs_filename, ".qc" );
	if( !FS_FileExists( gs_filename ))
	{
		// try to create qc-file
		CreateModelScript();
	}

	studioqc = Com_OpenScript( gs_filename, NULL, 0 );
	studioincludes[numincludes++] = studioqc; // member base script
	if( studioqc )
	{
		if( !ParseModelScript())
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