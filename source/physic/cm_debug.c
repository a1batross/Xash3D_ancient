//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    cm_debug.c - draw collision hulls outlines
//=======================================================================

#include "cm_local.h"

void DebugShowGeometryCollision( const NewtonBody* body, int vertexCount, const float* faceVertec, int id ) 
{    
	rgba_t	color;

	if( body == cms.body ) Vector4Set( color, 255, 178, 0, 255 ); // world
	else Vector4Set( color, 255, 25, 25, 255 );

	ph.debug_line( color, vertexCount, faceVertec, NULL );
} 

void DebugShowBodyCollision( const NewtonBody* body ) 
{ 
	NewtonBodyForEachPolygonDo( body, DebugShowGeometryCollision ); 
} 

void DebugShowCollision( cmdraw_t callback  ) 
{ 
	if( !callback ) return;
	ph.debug_line = callback; // member draw function

	if( cm_debugdraw->integer == 1 )
	{
		// called from render.dll
		NewtonWorldForEachBodyDo( gWorld, DebugShowBodyCollision ); 
	}
	if( cm_debugdraw->integer == 2 )
	{
		CM_CollisionDrawForEachBrush();
		CM_CollisionDrawForEachSurface();
	}
}