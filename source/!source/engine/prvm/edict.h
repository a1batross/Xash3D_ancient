/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef PROGS_H
#define PROGS_H

#define	MAX_ENT_CLUSTERS	16

#define	SVF_NOCLIENT	0x00000001	// don't send entity to clients, even if it has effects
#define	SVF_DEADMONSTER	0x00000002	// treat as CONTENTS_DEADMONSTER for collision
#define	SVF_MONSTER	0x00000004	// treat as CONTENTS_MONSTER for collision

typedef struct link_s
{
	struct link_s	*prev, *next;
	int		entitynumber;
} link_t;

typedef struct server_edict_s
{
	// these fields must match with edict_state_t pos. don't move it!
	bool		free;
	float		freetime;

	bool		move;

	int		linkcount;

	link_t		area;			// linked to a division node or leaf
	int		num_clusters;		// if -1, use headnode instead
	int		clusternums[MAX_ENT_CLUSTERS];
	int		headnode;			// unused if num_clusters != -1
	int		areanum, areanum2;

	int		flags;			// SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
	vec3_t		mins, maxs;
	vec3_t		absmin, absmax, size;

	
	
	vec3_t		moved_from;		// used to keep track of where objects were before they were
	vec3_t		moved_fromangles;		// moved, in case they need to be moved back

	// trace info
	solid_t			solid;
	int			clipmask;
	struct server_edict_s	*owner;

	entity_state_t	state;
	player_state_t	*client;

} server_edict_t;

#endif