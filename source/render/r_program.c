//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        r_program.c - cg and glsl programs
//=======================================================================


#include "r_local.h"

#define PROGRAMS_HASHSIZE	128

static program_t	*r_programsHash[PROGRAMS_HASHSIZE];
static program_t	*r_programs[MAX_PROGRAMS];
static int	r_numPrograms;

program_t		*r_defaultVertexProgram;
program_t		*r_defaultFragmentProgram;


/*
=================
R_ProgramList_f
=================
*/
void R_ProgramList_f( void )
{
	program_t	*program;
	int      	i;

	Msg( "\n" );
	Msg( "-----------------------------------\n" );

	for( i = 0; i < r_numPrograms; i++ )
	{
		program = r_programs[i];

		Msg( "%3i: ", i );

		switch( program->target )
		{
		case GL_VERTEX_PROGRAM_ARB:
			Msg("VP ");
			break;
		case GL_FRAGMENT_PROGRAM_ARB:
			Msg("FP ");
			break;
		default:
			Msg("?? ");
			break;
		}
		Msg( "%s\n", program->name );
	}

	Msg( "-----------------------------------\n" );
	Msg( "%i total programs\n", r_numPrograms );
	Msg( "\n" );
}

/*
=================
R_UploadProgram
=================
*/
static bool R_UploadProgram( const char *name, uint target, const char *string, int length, uint *progNum )
{
	const char	*errString;
	int		errPosition;

	pglGenProgramsARB( 1, progNum );
	pglBindProgramARB( target, *progNum );
	pglProgramStringARB( target, GL_PROGRAM_FORMAT_ASCII_ARB, length, string );

	errString = pglGetString( GL_PROGRAM_ERROR_STRING_ARB );
	pglGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errPosition );

	if( errPosition == -1 ) return true;

	switch( target )
	{
	case GL_VERTEX_PROGRAM_ARB:
		MsgDev( D_ERROR, "in vertex program '%s' at char %i: %s\n", name, errPosition, errString );
		break;
	case GL_FRAGMENT_PROGRAM_ARB:
		MsgDev( D_ERROR, "in fragment program '%s' at char %i: %s\n", name, errPosition, errString );
		break;
	}
	pglDeleteProgramsARB( 1, progNum );

	return false;
}

/*
=================
R_LoadProgram
=================
*/
program_t *R_LoadProgram( const char *name, uint target, const char *string, int length )
{
	program_t		*program;
	uint		progNum;
	uint		hashKey;

	if(!R_UploadProgram( name, target, string, length, &progNum ))
	{
		switch( target )
		{
		case GL_VERTEX_PROGRAM_ARB:
			return r_defaultVertexProgram;
		case GL_FRAGMENT_PROGRAM_ARB:
			return r_defaultFragmentProgram;
		}
	}

	if( r_numPrograms == MAX_PROGRAMS )
		Host_Error( "R_LoadProgram: MAX_PROGRAMS limit exceeded\n" );

	r_programs[r_numPrograms++] = program = Mem_Alloc( r_temppool, sizeof(program_t));

	// fill it in
	com.strncpy( program->name, name, sizeof(program->name));
	program->target = target;
	program->progNum = progNum;

	// add to hash table
	hashKey = Com_HashKey( name, PROGRAMS_HASHSIZE );

	program->nextHash = r_programsHash[hashKey];
	r_programsHash[hashKey] = program;

	return program;
}

/*
=================
R_FindProgram
=================
*/
program_t *R_FindProgram( const char *name, uint target )
{
	program_t		*program;
	char		*string;
	int		length;
	uint		hashKey;

	if( !name || !name[0] )
	{
		MsgDev( D_ERROR, "R_FindProgram: NULL name\n" );
		return NULL;
	}
	// see if already loaded
	hashKey = Com_HashKey( name, PROGRAMS_HASHSIZE );

	for( program = r_programsHash[hashKey]; program; program = program->nextHash )
	{
		if(!com.stricmp( program->name, name ))
		{
			if( program->target == target )
				return program;
		}
	}

	// Load it from disk
	string = FS_LoadFile( name, &length );
	if( string )
	{
		program = R_LoadProgram( name, target, string, length );
		Mem_Free( string );
		return program;
	}

	// not found
	return NULL;
}

/*
=================
R_InitPrograms
=================
*/
void R_InitPrograms( void )
{
	const char *progVP = 
		"!!ARBvp1.0"
		"PARAM mvpMatrix[4] = { state.matrix.mvp };"
		"PARAM texMatrix[4] = { state.matrix.texture[0] };"
		"DP4 result.position.x, mvpMatrix[0], vertex.position;"
		"DP4 result.position.y, mvpMatrix[1], vertex.position;"
		"DP4 result.position.z, mvpMatrix[2], vertex.position;"
		"DP4 result.position.w, mvpMatrix[3], vertex.position;"
		"MOV result.color, vertex.color;"
		"DP4 result.texcoord[0].x, texMatrix[0], vertex.texcoord[0];"
		"DP4 result.texcoord[0].y, texMatrix[1], vertex.texcoord[0];"
		"DP4 result.texcoord[0].z, texMatrix[2], vertex.texcoord[0];"
		"DP4 result.texcoord[0].w, texMatrix[3], vertex.texcoord[0];"
		"END";

	const char *progFP = 
		"!!ARBfp1.0"
		"TEMP tmp;"
		"TEX tmp, fragment.texcoord[0], texture[0], 2D;"
		"MUL result.color, tmp, fragment.color;"
		"END";
	size_t sizeVP = com.strlen( progVP );
	size_t sizeFP = com.strlen( progFP );

	if( GL_Support( R_VERTEX_PROGRAM_EXT ))
		r_defaultVertexProgram = R_LoadProgram("<defaultVP>", GL_VERTEX_PROGRAM_ARB, progVP, sizeVP );

	if( GL_Support( R_FRAGMENT_PROGRAM_EXT ))
		r_defaultFragmentProgram = R_LoadProgram("<defaultFP>", GL_FRAGMENT_PROGRAM_ARB, progFP, sizeFP );
}

/*
=================
R_ShutdownPrograms
=================
*/
void R_ShutdownPrograms( void )
{
	program_t		*program;
	int		i;

	for( i = 0; i < r_numPrograms; i++ )
	{
		program = r_programs[i];
		pglDeleteProgramsARB( 1, &program->progNum );
	}

	Mem_Set( r_programsHash, 0, sizeof(r_programsHash));
	Mem_Set( r_programs, 0, sizeof(r_programs));
	r_numPrograms = 0;
}
