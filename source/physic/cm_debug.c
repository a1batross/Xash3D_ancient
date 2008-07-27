//=======================================================================
//			Copyright XashXT Group 2007 ©
//		    cm_debug.c - draw collision hulls outlines
//=======================================================================

#include "cm_local.h"

void DebugShowGeometryCollision( const NewtonBody* body, int vertexCount, const float* faceVertec, int id ) 
{    
	int color;

	if( body == cm.body ) color = PackRGBA( 1, 0.7f, 0, 1 ); // world
	else color = PackRGBA( 1, 0.1f, 0.1f, 1 );

	ph.debug_line( color, vertexCount, faceVertec );
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
	}
}