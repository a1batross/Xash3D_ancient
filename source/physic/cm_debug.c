//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    cm_debug.c - draw collision hulls outlines
//=======================================================================

#include "physic.h"

void DebugShowGeometryCollision( const NewtonBody* body, int vertexCount, const float* faceVertec, int id ) 
{    
	// callback to render.dll
	if( ph.debug_line ) ph.debug_line( 0, vertexCount, faceVertec );
} 

void DebugShowBodyCollision( const NewtonBody* body ) 
{ 
	NewtonBodyForEachPolygonDo(body, DebugShowGeometryCollision ); 
} 

void DebugShowCollision( cmdraw_t callback  ) 
{ 
	// called from render.dll
	ph.debug_line = callback; // member draw function
	NewtonWorldForEachBodyDo( gWorld, DebugShowBodyCollision ); 
}