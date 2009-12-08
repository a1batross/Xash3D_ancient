//=======================================================================
//			Copyright XashXT Group 2007 ©
//			r_draw.c - draw 2d pictures
//=======================================================================

#include "r_local.h"
#include "mathlib.h"
#include "triangle_api.h"

static vec4_t	pic_xyz[4] = { {0,0,0,1}, {0,0,0,1}, {0,0,0,1}, {0,0,0,1} };
static vec2_t	pic_st[4];
static rgba_t	pic_colors[4];
static mesh_t	pic_mesh = { 4, pic_xyz, pic_xyz, NULL, pic_st, { 0, 0, 0, 0 }, { pic_colors, pic_colors, pic_colors, pic_colors }, 6, NULL };
meshbuffer_t	pic_mbuffer;

/*
===============
R_DrawSetColor
===============
*/
void R_DrawSetColor( const rgba_t color )
{
	if( color ) Vector4Copy( color, glState.draw_color );
	else Vector4Set( glState.draw_color, 255, 255, 255, 255 );
}

/*
===============
R_DrawStretchPic
===============
*/
void R_DrawStretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, shader_t handle )
{
	int		bcolor;
	ref_shader_t	*shader;
	static int	oldframe;

	if( handle < 0 || handle > MAX_SHADERS || !(shader = &r_shaders[handle]))
		return;

	// lower-left
	Vector2Set( pic_xyz[0], x, y );
	Vector2Set( pic_st[0], s1, t1 );
	Vector4Copy( glState.draw_color, pic_colors[0] );
	bcolor = *(int *)pic_colors[0];

	// lower-right
	Vector2Set( pic_xyz[1], x+w, y );
	Vector2Set( pic_st[1], s2, t1 );
	*(int *)pic_colors[1] = bcolor;

	// upper-right
	Vector2Set( pic_xyz[2], x+w, y+h );
	Vector2Set( pic_st[2], s2, t2 );
	*(int *)pic_colors[2] = bcolor;

	// upper-left
	Vector2Set( pic_xyz[3], x, y+h );
	Vector2Set( pic_st[3], s1, t2 );
	*(int *)pic_colors[3] = bcolor;

	if( pic_mbuffer.shaderkey != (int)shader->sortkey || -pic_mbuffer.infokey-1+4 > MAX_ARRAY_VERTS )
	{
		if( pic_mbuffer.shaderkey )
		{
			pic_mbuffer.infokey = -1;
			R_RenderMeshBuffer( &pic_mbuffer );
		}
	}

	tr.iRenderMode = glState.draw_rendermode;
	pic_mbuffer.shaderkey = shader->sortkey;
	pic_mbuffer.infokey -= 4;

	// upload video right before rendering
	if( shader->flags & SHADER_VIDEOMAP ) R_UploadCinematicShader( shader );
	R_PushMesh( &pic_mesh, MF_TRIFAN|shader->features | ( r_shownormals->integer ? MF_NORMALS : 0 ));

	if( oldframe != glState.draw_frame )
	{
		if( pic_mbuffer.shaderkey != (int)shader->sortkey )
		{
			// will be rendering on next call
			oldframe = glState.draw_frame;
			return;
		}
		if( pic_mbuffer.shaderkey )
		{
			pic_mbuffer.infokey = -1;
			R_RenderMeshBuffer( &pic_mbuffer );
		}
		oldframe = glState.draw_frame;
	}
}

/*
=============
R_DrawStretchRaw
=============
*/
void R_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, bool dirty )
{
	if( !GL_Support( R_ARB_TEXTURE_NPOT_EXT ))
	{
		int	width = 1, height = 1;
	
		// check the dimensions
		while( width < cols ) width <<= 1;
		while( height < rows ) height <<= 1;

		if( cols != width || rows != height )
			Host_Error( "R_DrawStretchRaw: size is not a power of two (%i x %i)\n", cols, rows );
	}

	if( cols > glConfig.max_2d_texture_size )
		Host_Error( "R_DrawStretchRaw: size exceeds hardware limits (%i > %i)\n", cols, glConfig.max_2d_texture_size ); 
	if( rows > glConfig.max_2d_texture_size )
		Host_Error( "R_DrawStretchRaw: size exceeds hardware limits (%i > %i)\n", rows, glConfig.max_2d_texture_size );

	GL_Bind( 0, tr.cinTexture );

	if( cols == tr.cinTexture->width && rows == tr.cinTexture->height )
	{
		if( dirty ) pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
	}
	else
	{
		tr.cinTexture->width = cols;
		tr.cinTexture->height = rows;
		if( dirty ) pglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
	}

	R_CheckForErrors();

	pglBegin( GL_QUADS );
	pglTexCoord2f( 0, 0 );
	pglVertex2f( x, y );
	pglTexCoord2f( 1, 0 );
	pglVertex2f( x + w, y );
	pglTexCoord2f( 1, 1 );
	pglVertex2f( x + w, y + h );
	pglTexCoord2f( 0, 1 );
	pglVertex2f( x, y + h );
	pglEnd();
}

void R_DrawGetParms( int *w, int *h, int *f, int frame, shader_t handle )
{
	ref_shader_t	*shader;
	int		cur = 0;

	if( !w && !h && !f ) return;

	// assume error
	if( w ) *w = 0;
	if( h ) *h = 0;
	if( f ) *f = 1;

	if( handle < 0 || handle > MAX_SHADERS || !(shader = &r_shaders[handle]))
		return;

	if( !shader->num_stages || !shader->stages[0].textures[0] )
		return;

	if( shader->stages[0].textures[0] && shader->stages[0].num_textures && frame > 0 )
		cur = bound( 0, frame, shader->stages[0].num_textures );

	if( w ) *w = (int)shader->stages[0].textures[cur]->srcWidth;
	if( h ) *h = (int)shader->stages[0].textures[cur]->srcHeight;
	if( f ) *f = (int)shader->stages[0].num_textures;
}

void R_DrawSetParms( shader_t handle, kRenderMode_t rendermode, int frame )
{
	ref_shader_t	*shader;

	if( handle < 0 || handle > MAX_SHADERS || !(shader = &r_shaders[handle]) || !shader->num_stages )
		return;

	glState.draw_rendermode = rendermode;

	if( !shader->stages[0].num_textures )
		return;

	// change frame if need
	if( shader->stages[0].flags & SHADERSTAGE_FRAMES )
	{
		// make sure what frame inbound
		glState.draw_frame = bound( 0, frame, shader->stages[0].num_textures - 1 );
	}
}

/*
=============================================================

		TRIAPI IMPLEMENTATION

=============================================================
*/
#define MAX_TRIVERTS	1024
#define MAX_TRIELEMS	MAX_TRIVERTS * 6
#define MAX_TRIANGLES	MAX_TRIELEMS / 3

static vec4_t		tri_vertex[MAX_TRIVERTS];
static vec4_t		tri_normal[MAX_TRIVERTS];
static vec2_t		tri_coords[MAX_TRIVERTS];
static rgba_t		tri_colors[MAX_TRIVERTS];
static elem_t		tri_elems[MAX_TRIELEMS];
static mesh_t		tri_mesh;
static bool		tri_caps[3];
meshbuffer_t		tri_mbuffer;
tristate_t		triState;
	
static void Tri_DrawPolygon( void )
{
	ref_shader_t	*shader;
	static int	oldframe;

	if( tri_caps[TRI_SHADER] )
		shader = &r_shaders[triState.currentShader];
	else shader = tr.fillShader;

	tri_mesh.numVertexes = triState.numVertex;
	tri_mesh.numElems = triState.numIndex;

	if( triState.hasNormals )
		tri_mesh.normalsArray = tri_normal;		
	else tri_mesh.normalsArray = NULL; // no normals

	tri_mesh.xyzArray = tri_vertex;
	tri_mesh.stArray = tri_coords;
	tri_mesh.colorsArray[0] = tri_colors;
	tri_mesh.elems = tri_elems;

	triState.numVertex = triState.numIndex = triState.numColor = 0;

	if( tri_mbuffer.shaderkey != (int)shader->sortkey || -tri_mbuffer.infokey-1+4 > MAX_ARRAY_VERTS )
	{
		if( tri_mbuffer.shaderkey )
		{
			tri_mbuffer.infokey = -1;
			R_RenderMeshBuffer( &tri_mbuffer );
		}
	}

	tr.iRenderMode = triState.currentRenderMode;
	tri_mbuffer.shaderkey = shader->sortkey;
	tri_mbuffer.infokey -= 4;

	triState.features = shader->features;
	triState.features |= MF_COLORS;
	if( r_shownormals->integer || triState.hasNormals )
		triState.features |= MF_NORMALS;

	if( triState.noCulling )
		triState.features |= MF_NOCULL;

	// upload video right before rendering
	if( shader->flags & SHADER_VIDEOMAP ) R_UploadCinematicShader( shader );
	R_PushMesh( &tri_mesh, triState.features );

	if( oldframe != glState.draw_frame )
	{
		if( tri_mbuffer.shaderkey != (int)shader->sortkey )
		{
			// will be rendering on next call
			oldframe = glState.draw_frame;
			return;
		}
		if( tri_mbuffer.shaderkey )
		{
			tri_mbuffer.infokey = -1;
			R_RenderMeshBuffer( &tri_mbuffer );
		}
		oldframe = glState.draw_frame;
	}
}

static void Tri_CheckOverflow( int numIndices, int numVertices )
{
	if( numIndices > MAX_TRIELEMS )
		Host_Error( "Tri_Overflow: %i > MAX_TRIELEMS\n", numIndices );
	if( numVertices > MAX_TRIVERTS )
		Host_Error( "Tri_Overflow: %i > MAX_TRIVERTS\n", numVertices );			

	if( triState.numIndex + numIndices <= MAX_TRIELEMS && triState.numVertex + numVertices <= MAX_TRIVERTS )
		return;

	Tri_DrawPolygon();
}

void Tri_Fog( float flFogColor[3], float flStart, float flEnd, int bOn )
{
	// FIXME: implement
}

void Tri_CullFace( int mode )
{
	if( mode == TRI_FRONT )
		triState.noCulling = false;
	else if( mode == TRI_NONE )
		triState.noCulling = true;
}

void Tri_RenderMode( const kRenderMode_t mode )
{
	triState.currentRenderMode = mode;
}

void Tri_Vertex3f( const float x, const float y, const float z )
{
	uint	oldIndex = triState.numIndex;

	switch( triState.drawMode )
	{
	case TRI_LINES:
		tri_elems[triState.numIndex++] = triState.numVertex;
		if( triState.vertexState++ == 1 )
		{
			Tri_Vertex3f( x + 1, y + 1, z + 1 );
			triState.vertexState = 0;
			triState.checkFlush = true; // flush for long sequences of quads.
		}
		break;
	case TRI_TRIANGLES:
		tri_elems[triState.numIndex++] = triState.numVertex;
		if( triState.vertexState++ == 2 )
		{
			triState.vertexState = 0;
			triState.checkFlush = true; // flush for long sequences of triangles.
		}
		break;
	case TRI_QUADS:
		if( triState.vertexState++ < 3 )
		{
			tri_elems[triState.numIndex++] = triState.numVertex;
		}
		else
		{
			// we've already done triangle (0, 1, 2), now draw (2, 3, 0)
			tri_elems[triState.numIndex++] = triState.numVertex - 1;
			tri_elems[triState.numIndex++] = triState.numVertex;
			tri_elems[triState.numIndex++] = triState.numVertex - 3;
			triState.vertexState = 0;
			triState.checkFlush = true; // flush for long sequences of quads.
		}
		break;
	case TRI_TRIANGLE_STRIP:
		if( triState.numVertex + triState.vertexState > MAX_TRIVERTS )
		{
			// This is a strip that's too big for us to buffer.
			// (We can't just flush the buffer because we have to keep
			// track of the last two vertices.
			Host_Error( "Tri_SetVertex: overflow: %i > MAX_TRIVERTS\n", triState.numVertex + triState.vertexState );
		}
		if( triState.vertexState++ < 3 )
		{
			tri_elems[triState.numIndex++] = triState.numVertex;
		}
		else
		{
			// flip triangles between clockwise and counter clockwise
			if( triState.vertexState & 1 )
			{
				// draw triangle [n-2 n-1 n]
				tri_elems[triState.numIndex++] = triState.numVertex - 2;
				tri_elems[triState.numIndex++] = triState.numVertex - 1;
				tri_elems[triState.numIndex++] = triState.numVertex;
			}
			else
			{
				// draw triangle [n-1 n-2 n]
				tri_elems[triState.numIndex++] = triState.numVertex - 1;
				tri_elems[triState.numIndex++] = triState.numVertex - 2;
				tri_elems[triState.numIndex++] = triState.numVertex;
			}
		}
		break;
	case TRI_POLYGON:
	case TRI_TRIANGLE_FAN:	// same as polygon
		if( triState.numVertex + triState.vertexState > MAX_TRIVERTS )
		{
			// This is a polygon or fan that's too big for us to buffer.
			// (We can't just flush the buffer because we have to keep
			// track of the starting vertex.
			Host_Error( "Tri_Vertex3f: overflow: %i > MAX_TRIVERTS\n", triState.numVertex + triState.vertexState );
		}
		if( triState.vertexState++ < 3 )
		{
			tri_elems[triState.numIndex++] = triState.numVertex;
		}
		else
		{
			// draw triangle [0 n-1 n]
			tri_elems[triState.numIndex++] = triState.numVertex - ( triState.vertexState - 1 );
			tri_elems[triState.numIndex++] = triState.numVertex - 1;
			tri_elems[triState.numIndex++] = triState.numVertex;
		}
		break;
	default:
		Host_Error( "Tri_SetVertex: unknown mode: %i\n", triState.drawMode );
		break;
	}

	// copy current vertex
	tri_vertex[triState.numVertex][0] = x;
	tri_vertex[triState.numVertex][1] = y;
	tri_vertex[triState.numVertex][2] = z;

	triState.numVertex++;

	// flush buffer if needed
	if( triState.checkFlush )
		Tri_CheckOverflow( triState.numIndex - oldIndex, triState.vertexState );
}

void Tri_Color4ub( const byte r, const byte g, const byte b, const byte a )
{
	tri_colors[triState.numVertex][0] = r;
	tri_colors[triState.numVertex][1] = g;
	tri_colors[triState.numVertex][2] = b;
	tri_colors[triState.numVertex][3] = a;
	triState.numColor++;
}

void Tri_Normal3f( const float x, const float y, const float z )
{
	triState.hasNormals = true; // curstate has normals
	tri_normal[triState.numVertex][0] = x;
	tri_normal[triState.numVertex][1] = y;
	tri_normal[triState.numVertex][2] = z;
}

void Tri_TexCoord2f( const float u, const float v )
{
	tri_coords[triState.numVertex][0] = u;
	tri_coords[triState.numVertex][1] = v;
}

void Tri_Bind( shader_t handle, int frame )
{
	ref_shader_t	*shader;

	if( handle < 0 || handle > MAX_SHADERS || !(shader = &r_shaders[handle]))
	{
		MsgDev( D_ERROR, "TriBind: bad shader %i\n", handle );
		return;
	}

	if( !shader->num_stages || !shader->stages[0].textures[0] )
	{
		MsgDev( D_ERROR, "TriBind: bad shader %i\n", handle );
		return;
	}

	triState.currentShader = handle;

	// FIXME: scan stages while( frame < stage->num_textures ) ?
	if( shader->stages[0].textures[0] && shader->stages[0].num_textures && frame > 0 )
		glState.draw_frame = bound( 0, frame, shader->stages[0].num_textures );
}

void Tri_Enable( int cap )
{
	if( cap < 0 || cap > TRI_MAXCAPS ) return;
	tri_caps[cap] = true;
}

void Tri_Disable( int cap )
{
	if( cap < 0 || cap > TRI_MAXCAPS ) return;
	tri_caps[cap] = false;
}

void Tri_Begin( int mode )
{
	triState.drawMode = mode;
	triState.vertexState = 0;
	triState.checkFlush = false;
	triState.hasNormals = false;
}

void Tri_End( void )
{
	if( triState.numIndex )
		Tri_DrawPolygon();
}

void Tri_RenderCallback( int fTrans )
{
	triState.fActive = true;

	if( fTrans ) GL_SetState( GLSTATE_NO_DEPTH_TEST );
	R_LoadIdentity();

	pglColor4f( 1, 1, 1, 1 );

	RI.currententity = RI.previousentity = NULL;
	RI.currentmodel = NULL;

	tri_mbuffer.infokey = -1;
	tri_mbuffer.shaderkey = 0;

	ri.DrawTriangles( fTrans );

	// fulsh remaining tris
	if( tri_mbuffer.infokey != -1 )
	{
		R_RenderMeshBuffer( &tri_mbuffer );
		tri_mbuffer.infokey = -1;
	}

	triState.fActive = false;
}
