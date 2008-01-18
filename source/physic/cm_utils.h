//=======================================================================
//			Copyright XashXT Group 2007 ©
//			    cm_utils.h - misc utils
//=======================================================================
#ifndef CM_UTILS_H
#define CM_UTILS_H

_inline void CM_ConvertPositionToMeters( vec3_t out, vec3_t in )
{
	out[0] = LittleFloat(INCH2METER(in[0]));
	out[1] = LittleFloat(INCH2METER(in[2]));
	out[2] = LittleFloat(INCH2METER(in[1]));
}

_inline void CM_ConvertDimensionToMeters( vec3_t out, vec3_t in )
{
	out[0] = LittleFloat(INCH2METER(in[0]));
	out[1] = LittleFloat(INCH2METER(in[1]));
	out[2] = LittleFloat(INCH2METER(in[2]));
}

void CM_LoadBSP( const void *buffer );
void CM_FreeBSP( void );

void CM_LoadWorld( const void *buffer );
void CM_FreeWorld( void );

void CM_MakeCollisionTree( void );
void CM_LoadCollisionTree( void );
void CM_SaveCollisionTree( file_t *f, cmsave_t callback );
void CM_InitBoxHull( void );
void CM_FloodAreaConnections( void );

cmodel_t *CM_BeginRegistration ( const char *name, bool clientload, uint *checksum );
cmodel_t *CM_RegisterModel ( const char *name );
void CM_EndRegistration ( void );

void CM_SetAreaPortals ( byte *portals, size_t size );
void CM_GetAreaPortals ( byte **portals, size_t *size );
void CM_SetAreaPortalState ( int portalnum, bool open );

int CM_NumClusters( void );
int CM_NumTexinfo( void );
int CM_NumInlineModels( void );
const char *CM_EntityString( void );
const char *CM_TexName( int index );
int CM_PointContents( const vec3_t p, cmodel_t *model );
int CM_TransformedPointContents( const vec3_t p, cmodel_t *model, const vec3_t origin, const vec3_t angles );
trace_t CM_BoxTrace( const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, cmodel_t *model, int brushmask );
trace_t CM_TransformedBoxTrace( const vec3_t start, const vec3_t end, vec3_t mins, vec3_t maxs, cmodel_t *model, int brushmask, vec3_t origin, vec3_t angles );
byte *CM_ClusterPVS( int cluster );
byte *CM_ClusterPHS( int cluster );
int CM_PointLeafnum( const vec3_t p );
int CM_BoxLeafnums( const vec3_t mins, const vec3_t maxs, int *list, int listsize, int *topnode );
int CM_LeafCluster( int leafnum );
int CM_LeafArea( int leafnum );
bool CM_AreasConnected( int area1, int area2 );
int CM_WriteAreaBits( byte *buffer, int area );
void CM_ModelBounds( cmodel_t *model, vec3_t mins, vec3_t maxs );

#endif//CM_UTILS_H