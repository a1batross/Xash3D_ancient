//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_studio.c - cm inline studio
//=======================================================================

#include "engine.h"
#include "server.h"

edict_t *m_pCurrentEntity;
studiohdr_t *m_pStudioHeader;
mstudiomodel_t *m_pSubModel;
mstudiobodyparts_t *m_pBodyPart;
matrix3x4 m_pRotationMatrix;
vec3_t g_xVertsTransform[MAXSTUDIOVERTS];
matrix3x4 g_xBonesTransform[MAXSTUDIOBONES];
uint m_BodyCount;

// mesh buffer
vec3_t g_xModelVerts[MAXSTUDIOVERTS];
uint iNumVertices = 0;
vec3_t *m_pModelVerts;

void SV_GetBodyCount( void )
{
	if(m_pStudioHeader)
	{
		m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex);
		m_BodyCount = m_pBodyPart->nummodels;
	}
	else m_BodyCount = 0; // just reset it
}

int SV_StudioExtractBbox( studiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if(sequence == -1) return 0;
	
	VectorCopy( pseqdesc[ sequence ].bbmin, mins );
	VectorCopy( pseqdesc[ sequence ].bbmax, maxs );

	return 1;
}

byte *SV_GetModelPtr( edict_t *ent )
{
	cmodel_t	*cmod;
	
	cmod = CM_LoadModel( ent ); 
	if(!cmod || !cmod->extradata) 
		return NULL;

	return cmod->extradata;
}

/*
====================
StudioCalcBoneQuaterion

====================
*/
void SV_StudioCalcBoneQuaterion( mstudiobone_t *pbone, float *q )
{
	int	i;
	vec3_t	angle1;

	for (i = 0; i < 3; i++) 
		angle1[i] = pbone->value[i+3];
	AngleQuaternion( angle1, q );
}

/*
====================
StudioCalcBonePosition

====================
*/
void SV_StudioCalcBonePosition( mstudiobone_t *pbone, float *pos )
{
	int	i;

	for (i = 0; i < 3; i++) 
		pos[i] = pbone->value[i];
}

/*
====================
StudioSetUpTransform

====================
*/
void SV_StudioSetUpTransform ( void )
{
	vec3_t	mins, maxs, modelpos;

	SV_StudioExtractBbox( m_pStudioHeader, 0, mins, maxs );
	CM_RoundUpHullSize(mins, true );
	CM_RoundUpHullSize(maxs, true ); 
	VectorAdd( mins, maxs, modelpos );
	VectorScale( modelpos, 0.5, modelpos );

	// setup matrix
	m_pRotationMatrix[0][0] = 1;
	m_pRotationMatrix[1][0] = 0;
	m_pRotationMatrix[2][0] = 0;

	m_pRotationMatrix[0][1] = 0;
	m_pRotationMatrix[1][1] = 1;
	m_pRotationMatrix[2][1] = 0;

	m_pRotationMatrix[0][2] = 0;
	m_pRotationMatrix[1][2] = 0;
	m_pRotationMatrix[2][2] = -1;

	m_pRotationMatrix[0][3] = modelpos[0];
	m_pRotationMatrix[1][3] = modelpos[1];
	m_pRotationMatrix[2][3] = modelpos[2];// ? modelpos[2] : maxs[2];
}

void SV_StudioCalcRotations ( float pos[][3], vec4_t *q )
{
	int		i;
	mstudiobone_t	*pbone;

	pbone = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);
	for (i = 0; i < m_pStudioHeader->numbones; i++, pbone++ ) 
	{
		SV_StudioCalcBoneQuaterion( pbone, q[i] );
		SV_StudioCalcBonePosition( pbone, pos[i] );
	}
}

/*
====================
StudioSetupBones

====================
*/
void SV_StudioSetupBones( void )
{
	int		i;
	mstudiobone_t	*pbones;
	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix3x4		bonematrix;

	SV_StudioCalcRotations( pos, q );
	pbones = (mstudiobone_t *)((byte *)m_pStudioHeader + m_pStudioHeader->boneindex);

	for (i = 0; i < m_pStudioHeader->numbones; i++) 
	{
		QuaternionMatrix( q[i], bonematrix );

		bonematrix[0][3] = pos[i][0];
		bonematrix[1][3] = pos[i][1];
		bonematrix[2][3] = pos[i][2];

		if (pbones[i].parent == -1) 
			R_ConcatTransforms (m_pRotationMatrix, bonematrix, g_xBonesTransform[i]);
		else R_ConcatTransforms(g_xBonesTransform[pbones[i].parent], bonematrix, g_xBonesTransform[i]);
	}
}

void SV_StudioSetupModel ( int bodypart, int body )
{
	int index;

	if(bodypart > m_pStudioHeader->numbodyparts) bodypart = 0;
	m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;
	m_pSubModel = (mstudiomodel_t *)((byte *)m_pStudioHeader + m_pBodyPart->modelindex) + index;
}

void SV_StudioAddMesh( int mesh )
{
	mstudiomesh_t	*pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + mesh;
	short		*ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);
	int		i;

	while(i = *(ptricmds++))
	{
		for(i = abs(i); i > 0; i--, ptricmds += 4)
		{
			if(!ptricmds) Sys_Error("ptricmds == NULL");
			m_pModelVerts[iNumVertices][0] = INCH2METER(g_xVertsTransform[ptricmds[0]][0]);
			m_pModelVerts[iNumVertices][1] = INCH2METER(g_xVertsTransform[ptricmds[0]][1]);
			m_pModelVerts[iNumVertices][2] = INCH2METER(g_xVertsTransform[ptricmds[0]][2]);
			iNumVertices++;
		}
		if(!ptricmds) Sys_Error("ptricmds == NULL");
	}
	if(!ptricmds) Sys_Error("ptricmds == NULL");
}

void SV_StudioDrawMeshes ( void )
{
	int	i;

	for (i = 0; i < m_pSubModel->nummesh; i++) 
		SV_StudioAddMesh( i );
}



void SV_StudioGetVertices( void )
{
	int		i;
	vec3_t		*pstudioverts;
	byte		*pvertbone;

	pvertbone = ((byte *)m_pStudioHeader + m_pSubModel->vertinfoindex);
	pstudioverts = (vec3_t *)((byte *)m_pStudioHeader + m_pSubModel->vertindex);

	for (i = 0; i < m_pSubModel->numverts; i++)
	{
		VectorTransform(pstudioverts[i], g_xBonesTransform[pvertbone[i]], g_xVertsTransform[i]);
	}
	SV_StudioDrawMeshes();
}

float *SV_GetModelVerts( sv_edict_t *ent, int *numvertices )
{
	cmodel_t	*cmod;
	int	i;

	m_pCurrentEntity = PRVM_EDICT_NUM(ent->serialnumber);
	i = (int)m_pCurrentEntity->progs.sv->body;
	cmod = CM_LoadModel( m_pCurrentEntity );

	if(cmod)
	{
		Msg("get physmesh for %s(%s) with index %d(%d)\n", cmod->name, PRVM_GetString(m_pCurrentEntity->progs.sv->model), (int)m_pCurrentEntity->progs.sv->modelindex, ent->serialnumber );
		*numvertices = cmod->physmesh[i].numverts;
		return (float *)cmod->physmesh[i].verts;
	}
	return NULL;
}

bool SV_CreateMeshBuffer( edict_t *in, cmodel_t *out )
{
	//int	i, j;

	// validate args
	if(!in || !out || !out->extradata)
		return false;

	// setup global pointers
	m_pCurrentEntity = in;
	m_pStudioHeader = (studiohdr_t *)out->extradata;
	m_pModelVerts = &g_xModelVerts[0];

	SV_GetBodyCount();

	// first we need to recalculate bounding box
	SV_StudioExtractBbox( m_pStudioHeader, 0, out->mins, out->maxs );
	CM_RoundUpHullSize(out->mins, false ); // normalize mins ( ceil )
	CM_RoundUpHullSize(out->maxs, false ); // normalize maxs ( ceil ) 

	Msg("create physmesh for %s\n", out->name );
	/*for( i = 0; i < m_BodyCount; i++)
	{
		// clear count
		iNumVertices = 0;

		__try	// FIXME
		{
			SV_StudioSetUpTransform();
			SV_StudioSetupBones();

			for (j = 0; j < m_pStudioHeader->numbodyparts; j++)
			{
				SV_StudioSetupModel( j, i );
				SV_StudioGetVertices();
			}
		}
		__except(EXCEPTION_EXECUTE_HANDLER) 
		{ 
			Sys_Error("m_pSubModel == NULL" ); 
		}
		if(iNumVertices)
		{
			out->physmesh[i].verts = Mem_Alloc( out->mempool, iNumVertices * sizeof(vec3_t));
			Mem_Copy(out->physmesh[i].verts,m_pModelVerts, iNumVertices * sizeof(vec3_t));
			out->physmesh[i].numverts = iNumVertices;
		}
	}*/
	
	return true;
}