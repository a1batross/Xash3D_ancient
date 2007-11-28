//=======================================================================
//			Copyright XashXT Group 2007 ©
//			sv_studio.c - cm inline studio
//=======================================================================

#include "engine.h"
#include "server.h"

edict_t *m_pCurrentEntity;
mstudiomodel_t *m_pSubModel;
studiohdr_t *m_pStudioHeader;
mstudiobodyparts_t *m_pBodyPart;
matrix3x4 m_pRotationMatrix;
vec3_t g_xStudioVerts[ MAXSTUDIOVERTS ];
vec3_t g_xModelVerts[ MAXSTUDIOVERTS ];
matrix3x4 g_xBonesTransform[ MAXSTUDIOBONES ];
vec3_t *m_pStudioVerts;
uint iNumVertices = 0;

int SV_StudioExtractBbox( studiohdr_t *phdr, int sequence, float *mins, float *maxs )
{
	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)phdr + phdr->seqindex);

	if(sequence == -1) return 0;
	
	VectorCopy( pseqdesc[ sequence ].bbmin, mins );
	VectorCopy( pseqdesc[ sequence ].bbmax, maxs );

	return 1;
}

byte *SV_GetModelPtr(edict_t *ent)
{
	cmodel_t	*cmod;
	
	if(!ent || !ent->progs.sv->modelindex) return NULL;
	cmod = CM_LoadModel( ent->progs.sv->modelindex ); 
	if(!cmod || !cmod->extradata) return NULL;

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
	for (i = 0; i < 3; i++) angle1[i] = pbone->value[i+3]; // default;
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
	for (i = 0; i < 3; i++) pos[i] = pbone->value[i]; // default;
}

/*
====================
StudioSetUpTransform

====================
*/
void SV_StudioSetUpTransform ( void )
{
	vec3_t	angles, modelpos;

	//VectorCopy( m_pCurrentEntity->progs.sv->origin, modelpos );
	VectorCopy( m_pCurrentEntity->progs.sv->angles, angles );
	AngleMatrix(angles, m_pRotationMatrix);

	//m_pRotationMatrix[0][3] = modelpos[0];
	//m_pRotationMatrix[1][3] = modelpos[1];
	//m_pRotationMatrix[2][3] = modelpos[2];
}

void SV_StudioCalcRotations ( float pos[][3], vec4_t *q )
{
	int		i;
	mstudiobone_t	*pbone;

	// add in programtic controllers
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
	double		f = 1.0f;

	mstudiobone_t	*pbones;

	static float	pos[MAXSTUDIOBONES][3];
	static vec4_t	q[MAXSTUDIOBONES];
	matrix3x4		bonematrix;

	static float	pos2[MAXSTUDIOBONES][3];
	static vec4_t	q2[MAXSTUDIOBONES];
	static float	pos3[MAXSTUDIOBONES][3];
	static vec4_t	q3[MAXSTUDIOBONES];
	static float	pos4[MAXSTUDIOBONES][3];
	static vec4_t	q4[MAXSTUDIOBONES];

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

void SV_StudioSetupModel ( int bodypart )
{
	int index;

	if(bodypart > m_pStudioHeader->numbodyparts) bodypart = 0;
	m_pBodyPart = (mstudiobodyparts_t *)((byte *)m_pStudioHeader + m_pStudioHeader->bodypartindex) + bodypart;

	index = m_pCurrentEntity->progs.sv->body / m_pBodyPart->base;
	index = index % m_pBodyPart->nummodels;
	m_pSubModel = (mstudiomodel_t *)((byte *)m_pStudioHeader + m_pBodyPart->modelindex) + index;
}

void SV_StudioAddPolygon( bool order, float *vertex )
{
	VectorCopy(vertex, g_xModelVerts[iNumVertices]);

	//if(order) 
	//ConvertPositionToPhysic(g_xModelVerts[iNumVertices]);
	//else 
	ConvertDimensionToPhysic(g_xModelVerts[iNumVertices]);

	iNumVertices++;
}

void SV_StudioDrawMeshes ( void )
{
	int	i, j;
	bool	order = false;

	mstudiomesh_t *pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex);

	for (j = 0; j < m_pSubModel->nummesh; j++) 
	{
		short	*ptricmds;

		pmesh = (mstudiomesh_t *)((byte *)m_pStudioHeader + m_pSubModel->meshindex) + j;
		ptricmds = (short *)((byte *)m_pStudioHeader + pmesh->triindex);

		while(i = *(ptricmds++))
		{
			if (i < 0) 
			{
				i = -i;
				order = true;
			}
			else order = false;
			
			for(; i > 0; i--, ptricmds += 4)
			{
				SV_StudioAddPolygon( order, g_xStudioVerts[ptricmds[0]] );
			}
		}
	}
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
		VectorTransform(pstudioverts[i], g_xBonesTransform[pvertbone[i]], g_xStudioVerts[i]);
	}
	SV_StudioDrawMeshes();
}

float *SV_GetModelVerts( sv_edict_t *ent, int *numvertices )
{
	int		i;
	byte		*buffer;

	*numvertices = iNumVertices = 0;
	m_pCurrentEntity = PRVM_EDICT_NUM(ent->serialnumber);
	m_pStudioVerts = &g_xStudioVerts[0];
	buffer = SV_GetModelPtr( m_pCurrentEntity );
	if(!buffer) return NULL;
	m_pStudioHeader = (studiohdr_t *)buffer;

	SV_StudioSetUpTransform();
	SV_StudioSetupBones();

	for (i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		SV_StudioSetupModel( i );
		SV_StudioGetVertices();
	}
	if(m_pStudioVerts)
	{
		*numvertices = iNumVertices;
		return (float *)g_xModelVerts;
	}
	return NULL;
}