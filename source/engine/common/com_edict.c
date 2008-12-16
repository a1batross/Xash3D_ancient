//=======================================================================
//			Copyright XashXT Group 2008 ©
//		       com_edict.c - generic edict manager
//=======================================================================

#include "common.h"

/*
=============
ED_NewString

FIXME: hashtable ?
=============
*/
char *ED_NewString( const char *string, byte *mempool )
{
	char	*data, *data_p;
	int	i, l;
	
	l = com.strlen( string ) + 1;
	data = Mem_Alloc( mempool, l );
	data_p = data;

	for( i = 0; i < l; i++ )
	{
		if( string[i] == '\\' && i < l - 1 )
		{
			i++;
			if( string[i] == 'n' )
				*data_p++ = '\n';
			else *data_p++ = '\\';
		}
		else *data_p++ = string[i];
	}
	return data;
}
