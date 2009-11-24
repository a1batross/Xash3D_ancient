//=======================================================================
//			Copyright XashXT Group 2009 ©
//			r_cin.c - plays videotextures
//=======================================================================

#include "r_local.h"

#define MAX_CINEMATICS	256

typedef struct r_cinhandle_s
{
	uint			id;
	char			*name;
	cinematics_t		*cin;
	texture_t			*image;
	struct r_cinhandle_s	*prev, *next;
} r_cinhandle_t;

static byte *r_cinMemPool;

static r_cinhandle_t	*r_cinematics;
static r_cinhandle_t	r_cinematics_headnode, *r_free_cinematics;

#define Cin_Malloc( size )	Mem_Alloc( r_cinMemPool, size )
#define Cin_Free( data )	Mem_Free( data )

/*
==================
R_ReadNextRoQFrame
==================
*/
static byte *R_ReadNextRoQFrame( cinematics_t *cin )
{
	return ri.RoQ_ReadNextFrame( cin, true );
}

static void R_ReadRoQChunk( cinematics_t *cin )
{
	ri.RoQ_ReadChunk( cin );
}

/*
==================
R_RunRoQ
==================
*/
static void R_RunRoQ( cinematics_t *cin )
{
	uint	frame;

	frame = (RI.refdef.time - cin->time) * (float)(RoQ_FRAMERATE);
	if( frame <= cin->frame ) return;

	if( frame > cin->frame + 1 )
		cin->time = RI.refdef.time - cin->frame / RoQ_FRAMERATE;

	cin->pic = cin->pic_pending;
	cin->pic_pending = R_ReadNextRoQFrame( cin );

	if( !cin->pic_pending )
	{
		FS_Seek( cin->file, cin->headerlen, SEEK_SET );
		cin->frame = 0;
		cin->pic_pending = R_ReadNextRoQFrame( cin );
		cin->time = RI.refdef.time;
	}
	cin->new_frame = true;
}

/*
==================
R_StopRoQ
==================
*/
static void R_StopRoQ( cinematics_t *cin )
{
	cin->frame = 0;
	cin->time = 0;	// done
	cin->pic = NULL;
	cin->pic_pending = NULL;

	if( cin->file )
	{
		FS_Close( cin->file );
		cin->file = 0;
	}
	if( cin->name )
	{
		Mem_Free( (char *)cin->name );
		cin->name = NULL;
	}
	if( cin->vid_buffer )
	{
		Mem_Free( cin->vid_buffer );
		cin->vid_buffer = NULL;
	}
}

/*
==================
R_OpenCinematics
==================
*/
static cinematics_t *R_OpenCinematics( char *filename )
{
	file_t		*file;
	cinematics_t	*cin = NULL;
	droqchunk_t	*chunk = &cin->chunk;

	if(( file = FS_Open( filename, "rb" )) == NULL )
		return NULL;

	cin = Cin_Malloc( sizeof( cinematics_t ));
	cin->name = filename;
	cin->file = file;
	cin->mempool = r_cinMemPool;

	// read header
	R_ReadRoQChunk( cin );

	chunk = &cin->chunk;
	if( chunk->id != RoQ_HEADER1 || chunk->size != RoQ_HEADER2 || chunk->argument != RoQ_HEADER3 )
	{
		R_StopRoQ( cin );
		Cin_Free( cin );
		return NULL;
	}

	cin->headerlen = FS_Tell( cin->file );
	cin->frame = 0;
	cin->pic = cin->pic_pending = R_ReadNextRoQFrame( cin );
	cin->time = RI.refdef.time;
	cin->new_frame = true;

	return cin;
}

/*
==================
R_ResampleCinematicFrame
==================
*/
static texture_t *R_ResampleCinematicFrame( r_cinhandle_t *handle )
{
	texture_t		*image;
	cinematics_t	*cin = handle->cin;
	static rgbdata_t	r_cin;

	if( !cin->pic )
		return NULL;

	if( !handle->image )
	{
		r_cin.width = cin->width;
		r_cin.height = cin->height;
		r_cin.type = PF_RGB_24;
		r_cin.size = cin->width * cin->height * 3;
		r_cin.flags = 0;
		r_cin.palette = NULL;
		r_cin.buffer = cin->pic;
		r_cin.numMips = r_cin.depth = 1;
		handle->image = R_LoadTexture( handle->name, &r_cin, 3, TF_CINEMATIC );
		cin->new_frame = false;
	}

	if( !cin->new_frame )
		return handle->image;

	cin->new_frame = false;

	image = handle->image;
	GL_Bind( 0, image );
	if( image->srcWidth != cin->width || image->srcHeight != cin->height )
		pglTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, image->srcWidth, image->srcHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, cin->pic );
	else pglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, image->srcWidth, image->srcHeight, GL_RGBA, GL_UNSIGNED_BYTE, cin->pic );

	image->srcWidth = cin->width;
	image->srcHeight = cin->height;

	return image;
}

//==================================================================================

/*
==================
R_CinList_f
==================
*/
void R_CinList_f( void )
{
	cinematics_t *cin;
	texture_t *image;
	r_cinhandle_t *handle, *hnode;

	Msg( "Active cintematics:" );
	hnode = &r_cinematics_headnode;
	handle = hnode->prev;
	if( handle == hnode )
	{
		Msg( " none\n" );
		return;
	}

	Msg( "\n" );
	do {
		cin = handle->cin;
		image = handle->image;
		Com_Assert( cin == NULL );

		if( image && (cin->width != image->width || cin->height != image->height) )
			Msg( "%s %i(%i)x%i(%i) f:%i\n", cin->name, cin->width, image->width, cin->height, image->height, cin->frame );
		else Msg( "%s %ix%i f:%i\n", cin->name, cin->width, cin->height, cin->frame );

		handle = handle->next;
	} while( handle != hnode );
}

/*
==================
R_InitCinematics
==================
*/
void R_InitCinematics( void )
{
	int i;

	r_cinMemPool = Mem_AllocPool( "Cinematics" );
	r_cinematics = Cin_Malloc( sizeof( r_cinhandle_t ) * MAX_CINEMATICS );

	// link cinemtics
	r_free_cinematics = r_cinematics;
	r_cinematics_headnode.id = 0;
	r_cinematics_headnode.prev = &r_cinematics_headnode;
	r_cinematics_headnode.next = &r_cinematics_headnode;

	for( i = 0; i < MAX_CINEMATICS - 1; i++ )
	{
		if( i < MAX_CINEMATICS - 1 )
			r_cinematics[i].next = &r_cinematics[i+1];
		r_cinematics[i].id = i + 1;
	}

	Cmd_AddCommand( "cinlist", R_CinList_f, "display loaded videos list" );
}

/*
==================
R_RunAllCinematics
==================
*/
void R_RunAllCinematics( void )
{
	r_cinhandle_t *handle, *hnode, *next;

	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = next )
	{
		next = handle->prev;
		R_RunRoQ( handle->cin );
	}
}

/*
==================
R_UploadCinematics
==================
*/
texture_t *R_UploadCinematics( uint id )
{
	Com_Assert( !( id > 0 && id <= MAX_CINEMATICS ));
	return R_ResampleCinematicFrame( r_cinematics + id - 1 );
}

/*
==================
R_StartCinematic
==================
*/
uint R_StartCinematics( const char *arg )
{
	char		*name = NULL;
	string		uploadName;
	size_t		name_size;
	cinematics_t	*cin = NULL;
	r_cinhandle_t	*handle, *hnode, *next;

	name_size = com.strlen( "media/" ) + com.strlen( arg ) + com.strlen( ".roq" ) + 1;
	name = Cin_Malloc( name_size );
	com.snprintf( name, name_size, "media/%s", arg );
	FS_DefaultExtension( name, ".roq" );

	// find cinematics with the same name
	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = next )
	{
		next = handle->prev;
		Com_Assert( handle->cin == NULL );

		// reuse
		if( !com.stricmp( handle->cin->name, name ) )
		{
			Cin_Free( name );
			return handle->id;
		}
	}

	// open the file, read header, etc
	cin = R_OpenCinematics( name );

	// take a free cinematic handle if possible
	if( !r_free_cinematics || !cin )
	{
		Cin_Free( name );
		return 0;
	}

	handle = r_free_cinematics;
	r_free_cinematics = handle->next;

	com.snprintf( uploadName, sizeof( uploadName ), "***r_cinematic%i***", handle->id-1 );
	name_size = com.strlen( uploadName ) + 1;
	handle->name = Cin_Malloc( name_size );
	Mem_Copy( handle->name, uploadName, name_size );
	handle->cin = cin;

	// put handle at the start of the list
	handle->prev = &r_cinematics_headnode;
	handle->next = r_cinematics_headnode.next;
	handle->next->prev = handle;
	handle->prev->next = handle;

	return handle->id;
}

/*
=================
R_FreeCinematics
=================
*/
void R_FreeCinematics( uint id )
{
	r_cinhandle_t	*handle;

	handle = r_cinematics + id - 1;
	if( !handle->cin )
		return;

	R_StopRoQ( handle->cin );
	Cin_Free( handle->cin );
	handle->cin = NULL;

	Com_Assert( handle->name == NULL );
	Cin_Free( handle->name );
	handle->name = NULL;

	// remove from linked active list
	handle->prev->next = handle->next;
	handle->next->prev = handle->prev;

	// insert into linked free list
	handle->next = r_free_cinematics;
	r_free_cinematics = handle;
}

/*
==================
R_ShutdownCinematics
==================
*/
void R_ShutdownCinematics( void )
{
	r_cinhandle_t	*handle, *hnode, *next;

	if( !r_cinMemPool )
		return;

	hnode = &r_cinematics_headnode;
	for( handle = hnode->prev; handle != hnode; handle = next )
	{
		next = handle->prev;
		R_FreeCinematics( handle->id );
	}

	Cin_Free( r_cinematics );
	Mem_FreePool( &r_cinMemPool );

	Cmd_RemoveCommand( "cinlist" );
}
