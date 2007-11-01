//=======================================================================
//			Copyright XashXT Group 2007 ©
//			 roq_main.c - ROQ video maker
//=======================================================================

#include "roqlib.h"

void ROQ_Blit(byte *source, byte *dest, dword scanWidth, dword sourceSkip, dword destSkip, dword rows)
{
	while(rows)
	{
		Mem_Copy(dest, source, scanWidth);
		source += sourceSkip;
		dest += destSkip;
		rows--;
	}
}

// Doubles the size of one RGB source into the destination
void ROQ_DoubleSize(byte *source, byte *dest, dword dim)
{
	dword	x,y;
	dword	skip;

	skip = dim * 6;

	for(y = 0; y < dim; y++)
	{
		for(x = 0; x < dim; x++)
		{
			Mem_Copy(dest, source, 3);
			Mem_Copy(dest+3, source, 3);
			Mem_Copy(dest+skip, source, 3);
			Mem_Copy(dest+skip+3, source, 3);
			dest += 6;
			source += 3;
		}
		dest += skip;
	}
}

bool PrepareROQVideo ( const char *dir, const char *name, byte params )
{
	Sys_Error("not implemented (yet)\n");
	return false;
}

bool MakeROQ ( void )
{
	return false;
}