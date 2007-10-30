//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 utils.c - physics callbacks
//=======================================================================

#include "physic.h"

#include <math.h>
#include <gl/gl.h>

long _ftol( double );
long _ftol2( double dblSource ) 
{
	return _ftol( dblSource );
}

int		numsurfaces;
int		numverts, numedges;
int		*map_surfedges;
csurface_t	*map_surfaces;
dvertex_t		*map_vertices;
dedge_t		*map_edges;
byte		*pmod_base;
NewtonBody	*m_level;
NewtonCollision	*collision;

void* Palloc (int size )
{
	return Mem_Alloc( physpool, size );
}

void Pfree (void *ptr, int size )
{
	Mem_Free( ptr );
}

_inline void ConvertDimensionToPhysic( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);
	v[0] = INCH2METER(tmp[0]);
	v[1] = INCH2METER(tmp[1]);
	v[2] = INCH2METER(tmp[2]);
}

_inline void ConvertDimensionToServer( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);
	v[0] = METER2INCH(tmp[0]);
	v[1] = METER2INCH(tmp[1]);
	v[2] = METER2INCH(tmp[2]);
}

_inline void ConvertPositionToPhysic( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);
	v[0] = INCH2METER(tmp[0]);
	v[1] = INCH2METER(tmp[2]);
	v[2] = INCH2METER(tmp[1]);
}

_inline void ConvertPositionToServer( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);

	v[2] = METER2INCH(tmp[1]);
	v[1] = METER2INCH(tmp[2]);
	v[0] = METER2INCH(tmp[0]);
}

_inline void ConvertDirectionToPhysic( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);
	v[0] = tmp[0];
	v[1] = tmp[2];
	v[2] = tmp[1];
}

_inline void ConvertDirectionToServer( vec3_t v )
{
	vec3_t	tmp;

	VectorCopy(v, tmp);
	v[0] = tmp[0];
	v[1] = tmp[2];
	v[2] = tmp[1];
}

/*
================
GL_BuildPolygonFromSurface
================
*/

void Phys_BuildPolygonFromSurface(csurface_t *fa)
{
	int		i, lindex;
	dedge_t		*cur_edge;
	float		*vec;
	vec3_t		face[256];// max 256 edges on a face

	// reconstruct the polygon
	for(i = 0; i < fa->numedges; i++ )
	{
		lindex = map_surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			cur_edge = &map_edges[lindex];
			vec = map_vertices[cur_edge->v[0]].point;
		}
		else
		{
			cur_edge = &map_edges[-lindex];
			vec = map_vertices[cur_edge->v[1]].point;
		}
		// newton requires reverse vertex order
		VectorCopy (vec, face[i]);
		ConvertPositionToPhysic( face[i] );// inches to meters
	}
	// add newton polygon
	NewtonTreeCollisionAddFace(collision, fa->numedges, (float *)face[0], sizeof(vec3_t), 1);
}

void Phys_LoadVerts( lump_t *l )
{
	dvertex_t		*in, *out;
	int		i, count;

	in = (void *)(pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( physpool, count * sizeof(*out));

	map_vertices = out;
	numverts = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->point[0] = LittleFloat (in->point[0]);
		out->point[1] = LittleFloat (in->point[1]);
		out->point[2] = LittleFloat (in->point[2]);
	}
}

void Phys_LoadFaces( lump_t *l )
{
	dface_t		*in;
	csurface_t 	*out;
	int		side, count, surfnum;

	in = (void *)(pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( physpool, count * sizeof(*out));	

	map_surfaces = out;
	numsurfaces = count;

	for ( surfnum = 0; surfnum < count; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;

		side = LittleShort(in->side);
		if (side) out->flags |= SURF_PLANEBACK;			
		Phys_BuildPolygonFromSurface( out );
	}
}

void Phys_LoadEdges (lump_t *l)
{
	dedge_t	*in, *out;
	int 	i, count;

	in = (void *)(pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size\n");
	count = l->filelen / sizeof(*in);
	out = Mem_Alloc( physpool, count * sizeof(*out));

	map_edges = out;
	numedges = count;

	for ( i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (word)LittleShort(in->v[0]);
		out->v[1] = (word)LittleShort(in->v[1]);
	}
}

/*
=================
Mod_LoadSurfedges
=================
*/
void Phys_LoadSurfedges (lump_t *l)
{	
	int	i, count;
	int	*in, *out;
	
	in = (void *)(pmod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) Host_Error("MOD_LoadBmodel: funny lump size\n");
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES) Host_Error("MOD_LoadBmodel: funny lump size\n");

	map_surfedges = out = Mem_Alloc( physpool, count * sizeof(*out));	
	for ( i = 0; i < count; i++) out[i] = LittleLong (in[i]);
}

void Phys_LoadBSP( uint *buffer )
{
	dheader_t		header;
	matrix4x4		m_matrix;
	vec3_t		boxP0, boxP1;
	vec3_t		extra = {10.0f, 10.0f, 10.0f}; 

	header = *(dheader_t *)buffer;
	pmod_base = (byte *)buffer;

	// create the collsion tree geometry
	collision = NewtonCreateTreeCollision( gWorld, NULL );
	NewtonTreeCollisionBeginBuild( collision );

	Phys_LoadVerts(&header.lumps[LUMP_VERTEXES]);
	Phys_LoadEdges(&header.lumps[LUMP_EDGES]);
	Phys_LoadSurfedges(&header.lumps[LUMP_SURFEDGES]);
	Phys_LoadFaces(&header.lumps[LUMP_FACES]);

	NewtonTreeCollisionEndBuild(collision, 1);
	m_level = NewtonCreateBody(gWorld, collision);	// create the level ridif body
	NewtonReleaseCollision (gWorld, collision);
	NewtonBodyGetMatrix (m_level, &m_matrix[0][0]);	// set the global position of this body 
	NewtonCollisionCalculateAABB (collision, &m_matrix[0][0], &boxP0[0], &boxP1[0]); 
	VectorSubtract( boxP0, extra, boxP0 );
	VectorAdd( boxP1, extra, boxP1 );
	NewtonSetWorldSize( gWorld, &boxP0[0], &boxP1[0] ); 

	Msg("physic map generated\n");
	Mem_Free( map_surfedges );
	Mem_Free( map_edges );
	Mem_Free( map_surfaces );
	Mem_Free( map_vertices );
}

void Phys_FreeBSP( void )
{
	if(m_level) 
	{
		Msg("physic map released\n");
		NewtonDestroyBody( gWorld, m_level );
		m_level = NULL;
	}
}

void Phys_Frame( float time )
{
	NewtonUpdate(gWorld, time );
}

void Phys_ApplyForce(const NewtonBody* body) 
{ 
	float	mass; 
	vec3_t	size, force, torque; 

	NewtonBodyGetMassMatrix (body, &mass, &size[0], &size[1], &size[2]); 

	VectorSet( torque, 0.0f, 0.0f, 0.0f );
	VectorSet( force, 0.0f, -9.8f * mass, 0.0f );

	NewtonBodyAddForce (body, force); 
	NewtonBodyAddTorque (body, torque); 
}

void Phys_ApplyTransform( const NewtonBody* body, const float* matrix )
{
	sv_edict_t	*edict = (sv_edict_t *)NewtonBodyGetUserData( body );
	matrix4x4		translate;// obj matrix
	vec3_t		origin, angles;

	Mem_Copy(translate, (float *)matrix, sizeof(matrix4x4));
	ConvertDirectionToServer( translate[0]);
	ConvertDirectionToServer( translate[1]);
	ConvertDirectionToServer( translate[2]);
	ConvertPositionToServer( translate[3] );

	MatrixAngles( translate, origin, angles );
	pi.Transform( edict, origin, angles );
}

void Phys_CreateBOX( sv_edict_t *ed, vec3_t mins, vec3_t maxs, vec3_t org, vec3_t ang, NewtonCollision **newcol, NewtonBody **newbody )
{
	NewtonCollision	*col;
	NewtonBody	*body;
	matrix4x4		trans, offset;
	vec3_t		origin, angles, size, center;

	MatrixLoadIdentity( trans );
	MatrixLoadIdentity( offset );

	VectorCopy( org, origin );
	ConvertPositionToPhysic( origin );
	VectorCopy( ang, angles );
	ConvertDirectionToPhysic( angles );
	AngleVectors( angles, trans[0], trans[1], trans[2] );
	VectorSubtract (maxs, mins, size );
	VectorAdd(mins, maxs, center );
	ConvertDimensionToPhysic( size );
	ConvertDimensionToPhysic( center );
	VectorScale( center, 0.5, center );
	VectorNegate( center, center );

	VectorCopy(origin, trans[3] );
	VectorCopy(center, offset[3] );
          
	col = NewtonCreateBox( gWorld, size[0], size[1], size[2], &offset[0][0] );
	body = NewtonCreateBody( gWorld, col );
	NewtonBodySetUserData( body, ed );
	NewtonBodySetMassMatrix( body, 10.0f, size[0], size[1], size[2] ); // 10 kg

	// Setup generic callback to engine.dll
	NewtonBodySetTransformCallback(body, Phys_ApplyTransform );
	NewtonBodySetForceAndTorqueCallback(body, Phys_ApplyForce );
	NewtonBodySetMatrix(body, &trans[0][0]);// origin

	if(newcol) *newcol = col;
	if(body) *newbody = body;
}

void Phys_RemoveBOX( NewtonBody *body )
{
	if(body) 
	{
		Msg("remove box\n");
		NewtonDestroyBody( gWorld, body );
	}
} 

void DebugShowGeometryCollision(const NewtonBody* body, int vertexCount, const float* faceVertec, int id) 
{    
	int	i = vertexCount - 1;
	vec3_t	p0, p1;

	VectorSet(p0, faceVertec[i * 3 + 0], faceVertec[i * 3 + 1], faceVertec[i * 3 + 2]);
	ConvertPositionToServer( p0 );

	for (i = 0; i < vertexCount; i ++)
	{
		VectorSet(p1, faceVertec[i * 3 + 0], faceVertec[i * 3 + 1], faceVertec[i * 3 + 2]);
		ConvertPositionToServer( p1 );
		glVertex3fv(p0);
		glVertex3fv(p1);
 		VectorCopy(p1, p0);
 	}
} 

void DebugShowBodyCollision (const NewtonBody* body) 
{ 
	NewtonBodyForEachPolygonDo (body, DebugShowGeometryCollision); 
} 

// show all collision geometry 
void DebugShowCollision ( void ) 
{ 
	//return;

	glDisable(GL_TEXTURE_2D); 
	glColor3f (1, 0.7f, 0);
	glBegin(GL_LINES); 
	NewtonWorldForEachBodyDo (gWorld, DebugShowBodyCollision); 
	glEnd(); 
	glEnable (GL_TEXTURE_2D);
}