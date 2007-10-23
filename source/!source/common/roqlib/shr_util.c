/*
** Copyright (C) 2003 Eric Lasota/Orbiter Productions
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 2.1 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <string.h>

#include "roqlib_shared.h"

// Blitting/DoubleSize functions used in both the encoder and decoder

// Fast blitting function
void Blit(unsigned char *source, unsigned char *dest, unsigned long scanWidth, unsigned long sourceSkip, unsigned long destSkip, unsigned long rows)
{
	while(rows)
	{
		memcpy(dest, source, scanWidth);
		source += sourceSkip;
		dest += destSkip;
		rows--;
	}
}

// Doubles the size of one RGB source into the destination
void DoubleSize(unsigned char *source, unsigned char *dest, unsigned long dim)
{
	unsigned long x,y;
	unsigned long skip;

	skip = dim * 6;

	for(y=0;y<dim;y++)
	{
		for(x=0;x<dim;x++)
		{
			memcpy(dest, source, 3);
			memcpy(dest+3, source, 3);
			memcpy(dest+skip, source, 3);
			memcpy(dest+skip+3, source, 3);
			dest += 6;
			source += 3;
		}
		dest += skip;
	}
}
