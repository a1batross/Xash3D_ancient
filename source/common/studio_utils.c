//=======================================================================
//			Copyright XashXT Group 2007 ©
//		      studio_utils.c - common compiler funcs
//=======================================================================

#include "mdllib.h"
typedef struct { int type; char *name; } activity_map_t;	// studio activity map conversion
int		used[MAXSTUDIOTRIANGLES];		// the command list holds counts and s/t values
short		commands[MAXSTUDIOTRIANGLES * 13];	//that are valid for every frame
int		stripverts[MAXSTUDIOTRIANGLES+2];
int		striptris[MAXSTUDIOTRIANGLES+2];
int		neighbortri[MAXSTUDIOTRIANGLES][3];
int		neighboredge[MAXSTUDIOTRIANGLES][3];
s_trianglevert_t	(*triangles)[3];

enum ai_activity
{
	ACT_RESET = 0,	// Set m_Activity to this invalid value to force a reset to m_IdealActivity
	ACT_IDLE = 1,
	ACT_GUARD,
	ACT_WALK,
	ACT_RUN,
	ACT_FLY,		// Fly (and flap if appropriate)
	ACT_SWIM,
	ACT_HOP,		// vertical jump
	ACT_LEAP,		// long forward jump
	ACT_FALL,
	ACT_LAND,
	ACT_STRAFE_LEFT,
	ACT_STRAFE_RIGHT,
	ACT_ROLL_LEFT,	// tuck and roll, left
	ACT_ROLL_RIGHT,	// tuck and roll, right
	ACT_TURN_LEFT,	// turn quickly left (stationary)
	ACT_TURN_RIGHT,	// turn quickly right (stationary)
	ACT_CROUCH,	// the act of crouching down from a standing position
	ACT_CROUCHIDLE,	// holding body in crouched position (loops)
	ACT_STAND,	// the act of standing from a crouched position
	ACT_USE,
	ACT_SIGNAL1,
	ACT_SIGNAL2,
	ACT_SIGNAL3,
	ACT_TWITCH,
	ACT_COWER,
	ACT_SMALL_FLINCH,
	ACT_BIG_FLINCH,
	ACT_RANGE_ATTACK1,
	ACT_RANGE_ATTACK2,
	ACT_MELEE_ATTACK1,
	ACT_MELEE_ATTACK2,
	ACT_RELOAD,
	ACT_ARM,		// pull out gun, for instance
	ACT_DISARM,	// reholster gun
	ACT_EAT,		// monster chowing on a large food item (loop)
	ACT_DIESIMPLE,
	ACT_DIEBACKWARD,
	ACT_DIEFORWARD,
	ACT_DIEVIOLENT,
	ACT_BARNACLE_HIT,	// barnacle tongue hits a monster
	ACT_BARNACLE_PULL,	// barnacle is lifting the monster ( loop )
	ACT_BARNACLE_CHOMP,	// barnacle latches on to the monster
	ACT_BARNACLE_CHEW,	// barnacle is holding the monster in its mouth ( loop )
	ACT_SLEEP,
	ACT_INSPECT_FLOOR,	// for active idles, look at something on or near the floor
	ACT_INSPECT_WALL,	// for active idles, look at something directly ahead of you
	ACT_IDLE_ANGRY,	// alternate idle animation in which the monster is clearly agitated. (loop)
	ACT_WALK_HURT,	// limp  (loop)
	ACT_RUN_HURT,	// limp  (loop)
	ACT_HOVER,	// Idle while in flight
	ACT_GLIDE,	// Fly (don't flap)
	ACT_FLY_LEFT,	// Turn left in flight
	ACT_FLY_RIGHT,	// Turn right in flight
	ACT_DETECT_SCENT,	// this means the monster smells a scent carried by the air
	ACT_SNIFF,	// this is the act of actually sniffing an item in front of the monster
	ACT_BITE,		// some large monsters can eat small things in one bite. This plays one time, EAT loops.
	ACT_THREAT_DISPLAY,	// without attacking, monster demonstrates that it is angry. (Yell, stick out chest, etc )
	ACT_FEAR_DISPLAY,	// monster just saw something that it is afraid of
	ACT_EXCITED,	// for some reason, monster is excited. Sees something he really likes to eat, or whatever
	ACT_SPECIAL_ATTACK1,// very monster specific special attacks.
	ACT_SPECIAL_ATTACK2,	
	ACT_COMBAT_IDLE,	// agitated idle.
	ACT_WALK_SCARED,
	ACT_RUN_SCARED,
	ACT_VICTORY_DANCE,	// killed a player, do a victory dance.
	ACT_DIE_HEADSHOT,	// die, hit in head. 
	ACT_DIE_CHESTSHOT,	// die, hit in chest
	ACT_DIE_GUTSHOT,	// die, hit in gut
	ACT_DIE_BACKSHOT,	// die, hit in back
	ACT_FLINCH_HEAD,
	ACT_FLINCH_CHEST,
	ACT_FLINCH_STOMACH,
	ACT_FLINCH_LEFTARM,
	ACT_FLINCH_RIGHTARM,
	ACT_FLINCH_LEFTLEG,
	ACT_FLINCH_RIGHTLEG,
	ACT_VM_NONE,	// weapon viewmodel animations
	ACT_VM_DEPLOY,	// deploy
	ACT_VM_DEPLOY_EMPTY,// deploy empty weapon
	ACT_VM_HOLSTER,	// holster empty weapon
	ACT_VM_HOLSTER_EMPTY,
	ACT_VM_IDLE1,
	ACT_VM_IDLE2,
	ACT_VM_IDLE3,
	ACT_VM_RANGE_ATTACK1,
	ACT_VM_RANGE_ATTACK2,
	ACT_VM_RANGE_ATTACK3,
	ACT_VM_MELEE_ATTACK1,
	ACT_VM_MELEE_ATTACK2,
	ACT_VM_MELEE_ATTACK3,
	ACT_VM_SHOOT_EMPTY,
	ACT_VM_START_RELOAD,
	ACT_VM_RELOAD,
	ACT_VM_RELOAD_EMPTY,
	ACT_VM_TURNON,
	ACT_VM_TURNOFF,
	ACT_VM_PUMP,	// pumping gun
	ACT_VM_PUMP_EMPTY,
	ACT_VM_START_CHARGE,
	ACT_VM_CHARGE,
	ACT_VM_OVERLOAD,
	ACT_VM_IDLE_EMPTY,
};

static activity_map_t activity_map[] =
{
{ACT_IDLE,		"ACT_IDLE"		},
{ACT_GUARD,		"ACT_GUARD"		},
{ACT_WALK,		"ACT_WALK"		},
{ACT_RUN,			"ACT_RUN"			},
{ACT_FLY,			"ACT_FLY"			},
{ACT_SWIM,		"ACT_SWIM",		},
{ACT_HOP,			"ACT_HOP",		},
{ACT_LEAP,		"ACT_LEAP"		},
{ACT_FALL,		"ACT_FALL"		},
{ACT_LAND,		"ACT_LAND"		},
{ACT_STRAFE_LEFT,		"ACT_STRAFE_LEFT"		},
{ACT_STRAFE_RIGHT,		"ACT_STRAFE_RIGHT"		},
{ACT_ROLL_LEFT,		"ACT_ROLL_LEFT"		},
{ACT_ROLL_RIGHT,		"ACT_ROLL_RIGHT"		},
{ACT_TURN_LEFT,		"ACT_TURN_LEFT"		},
{ACT_TURN_RIGHT,		"ACT_TURN_RIGHT"		},
{ACT_CROUCH,		"ACT_CROUCH"		},
{ACT_CROUCHIDLE,		"ACT_CROUCHIDLE"		},
{ACT_STAND,		"ACT_STAND"		},
{ACT_USE,			"ACT_USE"			},
{ACT_SIGNAL1,		"ACT_SIGNAL1"		},
{ACT_SIGNAL2,		"ACT_SIGNAL2"		},
{ACT_SIGNAL3,		"ACT_SIGNAL3"		},
{ACT_TWITCH,		"ACT_TWITCH"		},
{ACT_COWER,		"ACT_COWER"		},
{ACT_SMALL_FLINCH,		"ACT_SMALL_FLINCH"		},
{ACT_BIG_FLINCH,		"ACT_BIG_FLINCH"		},
{ACT_RANGE_ATTACK1,		"ACT_RANGE_ATTACK1"		},
{ACT_RANGE_ATTACK2,		"ACT_RANGE_ATTACK2"		},
{ACT_MELEE_ATTACK1,		"ACT_MELEE_ATTACK1"		},
{ACT_MELEE_ATTACK2,		"ACT_MELEE_ATTACK2"		},
{ACT_RELOAD,		"ACT_RELOAD"		},
{ACT_ARM,			"ACT_ARM"			},
{ACT_DISARM,		"ACT_DISARM"		},
{ACT_EAT,			"ACT_EAT"			},
{ACT_DIESIMPLE,		"ACT_DIESIMPLE"		},
{ACT_DIEBACKWARD,		"ACT_DIEBACKWARD"		},
{ACT_DIEFORWARD,		"ACT_DIEFORWARD"		},
{ACT_DIEVIOLENT,		"ACT_DIEVIOLENT"		},
{ACT_BARNACLE_HIT,		"ACT_BARNACLE_HIT"		},
{ACT_BARNACLE_PULL,		"ACT_BARNACLE_PULL"		},
{ACT_BARNACLE_CHOMP,	"ACT_BARNACLE_CHOMP"	},
{ACT_BARNACLE_CHEW,		"ACT_BARNACLE_CHEW"		},
{ACT_SLEEP,		"ACT_SLEEP"		},
{ACT_INSPECT_FLOOR,		"ACT_INSPECT_FLOOR"		},
{ACT_INSPECT_WALL,		"ACT_INSPECT_WALL"		},
{ACT_IDLE_ANGRY,		"ACT_IDLE_ANGRY"		},
{ACT_WALK_HURT,		"ACT_WALK_HURT"		},
{ACT_RUN_HURT,		"ACT_RUN_HURT"		},
{ACT_HOVER,		"ACT_HOVER"		},
{ACT_GLIDE,		"ACT_GLIDE"		},
{ACT_FLY_LEFT,		"ACT_FLY_LEFT"		},
{ACT_FLY_RIGHT,		"ACT_FLY_RIGHT"		},
{ACT_DETECT_SCENT,		"ACT_DETECT_SCENT"		},
{ACT_SNIFF,		"ACT_SNIFF"		},		
{ACT_BITE,		"ACT_BITE"		},		
{ACT_THREAT_DISPLAY,	"ACT_THREAT_DISPLAY"	},
{ACT_FEAR_DISPLAY,		"ACT_FEAR_DISPLAY"		},
{ACT_EXCITED,		"ACT_EXCITED"		},	
{ACT_SPECIAL_ATTACK1,	"ACT_SPECIAL_ATTACK1"	},
{ACT_SPECIAL_ATTACK2,	"ACT_SPECIAL_ATTACK2"	},
{ACT_COMBAT_IDLE,		"ACT_COMBAT_IDLE"		},
{ACT_WALK_SCARED,		"ACT_WALK_SCARED"		},
{ACT_RUN_SCARED,		"ACT_RUN_SCARED"		},
{ACT_VICTORY_DANCE,		"ACT_VICTORY_DANCE"		},
{ACT_DIE_HEADSHOT,		"ACT_DIE_HEADSHOT"		},
{ACT_DIE_CHESTSHOT,		"ACT_DIE_CHESTSHOT"		},
{ACT_DIE_GUTSHOT,		"ACT_DIE_GUTSHOT"		},
{ACT_DIE_BACKSHOT,		"ACT_DIE_BACKSHOT"		},
{ACT_FLINCH_HEAD,		"ACT_FLINCH_HEAD"		},
{ACT_FLINCH_CHEST,		"ACT_FLINCH_CHEST"		},
{ACT_FLINCH_STOMACH,	"ACT_FLINCH_STOMACH"	},
{ACT_FLINCH_LEFTARM,	"ACT_FLINCH_LEFTARM"	},
{ACT_FLINCH_RIGHTARM,	"ACT_FLINCH_RIGHTARM"	},
{ACT_FLINCH_LEFTLEG,	"ACT_FLINCH_LEFTLEG"	},
{ACT_FLINCH_RIGHTLEG,	"ACT_FLINCH_RIGHTLEG"	},
{ACT_VM_NONE,		"ACT_VM_NONE"		},	// invalid animation
{ACT_VM_DEPLOY,		"ACT_VM_DEPLOY"		},	// deploy
{ACT_VM_DEPLOY_EMPTY,	"ACT_VM_DEPLOY_EMPTY"	},	// deploy empty weapon
{ACT_VM_HOLSTER,		"ACT_VM_HOLSTER"		},	// holster empty weapon
{ACT_VM_HOLSTER_EMPTY,	"ACT_VM_HOLSTER_EMPTY"	},
{ACT_VM_IDLE1,		"ACT_VM_IDLE1"		},
{ACT_VM_IDLE2,		"ACT_VM_IDLE2"		},
{ACT_VM_IDLE3,		"ACT_VM_IDLE3"		},
{ACT_VM_RANGE_ATTACK1,	"ACT_VM_RANGE_ATTACK1"	},
{ACT_VM_RANGE_ATTACK2,	"ACT_VM_RANGE_ATTACK2"	},
{ACT_VM_RANGE_ATTACK3,	"ACT_VM_RANGE_ATTACK3"	},
{ACT_VM_MELEE_ATTACK1,	"ACT_VM_MELEE_ATTACK1"	},
{ACT_VM_MELEE_ATTACK2,	"ACT_VM_MELEE_ATTACK2"	},
{ACT_VM_MELEE_ATTACK3,	"ACT_VM_MELEE_ATTACK3"	},
{ACT_VM_SHOOT_EMPTY,	"ACT_VM_SHOOT_EMPTY"	},
{ACT_VM_START_RELOAD,	"ACT_VM_START_RELOAD"	},
{ACT_VM_RELOAD,		"ACT_VM_RELOAD"		},
{ACT_VM_RELOAD_EMPTY,	"ACT_VM_RELOAD_EMPTY"	},
{ACT_VM_TURNON,		"ACT_VM_TURNON"		},
{ACT_VM_TURNOFF,		"ACT_VM_TURNOFF"		},
{ACT_VM_PUMP,		"ACT_VM_PUMP"		},	// user animations
{ACT_VM_PUMP_EMPTY,		"ACT_VM_PUMP_EMPTY"		},
{ACT_VM_START_CHARGE,	"ACT_VM_START_CHARGE"	},
{ACT_VM_CHARGE,		"ACT_VM_CHARGE"		},
{ACT_VM_OVERLOAD,		"ACT_VM_OVERLOAD"		},
{ACT_VM_IDLE_EMPTY,		"ACT_VM_IDLE_EMPTY"		},
{0, 			NULL			},
};

void OptimizeAnimations( void )
{
	int	i, j;
	int	n, m;
	int	type;
	int	q;
	int	iError = 0;

	// optimize animations
	for (i = 0; i < numseq; i++)
	{
		sequence[i].numframes = sequence[i].panim[0]->endframe - sequence[i].panim[0]->startframe + 1;

		// force looping animations to be looping
		if (sequence[i].flags & STUDIO_LOOPING)
		{
			for (j = 0; j < sequence[i].panim[0]->numbones; j++)
			{
				for (q = 0; q < sequence[i].numblends; q++)
				{
					vec3_t *ppos = sequence[i].panim[q]->pos[j];
					vec3_t *prot = sequence[i].panim[q]->rot[j];

					n = 0; // sequence[i].panim[q]->startframe;
					m = sequence[i].numframes - 1;
					
					type = sequence[i].motiontype;
					if (!(type & STUDIO_LX)) ppos[m][0] = ppos[n][0];
					if (!(type & STUDIO_LY)) ppos[m][1] = ppos[n][1];
					if (!(type & STUDIO_LZ)) ppos[m][2] = ppos[n][2];

					prot[m][0] = prot[n][0];
					prot[m][1] = prot[n][1];
					prot[m][2] = prot[n][2];
				}
			}
		}

		for (j = 0; j < sequence[i].numevents; j++)
		{
			if (sequence[i].event[j].frame < sequence[i].panim[0]->startframe)
			{
				Msg( "sequence %s has event (%d) before first frame (%d)\n", sequence[i].name, sequence[i].event[j].frame, sequence[i].panim[0]->startframe );
				sequence[i].event[j].frame = sequence[i].panim[0]->startframe;
				iError++;
			}
			if (sequence[i].event[j].frame > sequence[i].panim[0]->endframe)
			{
				Msg( "sequence %s has event (%d) after last frame (%d)\n", sequence[i].name, sequence[i].event[j].frame, sequence[i].panim[0]->endframe );
				sequence[i].event[j].frame = sequence[i].panim[0]->endframe;
				iError++;
			}
		}

		sequence[i].frameoffset = sequence[i].panim[0]->startframe;
	}
}

void FindNeighbor (int starttri, int startv)
{
	s_trianglevert_t	m1, m2;
	s_trianglevert_t	*last, *check;
	int		j, k;

	// used[starttri] |= (1 << startv);

	last = &triangles[starttri][0];

	m1 = last[(startv+1)%3];
	m2 = last[(startv+0)%3];

	for (j = starttri + 1, check = &triangles[starttri + 1][0]; j < pmesh->numtris; j++, check += 3)
	{
		if (used[j] == 7) continue;
		for (k = 0; k < 3; k++)
		{
			if (memcmp(&check[k],&m1,sizeof(m1))) continue;
			if (memcmp(&check[ (k+1)%3 ],&m2,sizeof(m2))) continue;

			neighbortri[starttri][startv] = j;
			neighboredge[starttri][startv] = k;

			neighbortri[j][k] = starttri;
			neighboredge[j][k] = startv;

			used[starttri] |= (1 << startv);
			used[j] |= (1 << k);
			return;
		}
	}
}

int StripLength (int starttri, int startv)
{
	int	j, k;

	used[starttri] = 2;

	stripverts[0] = (startv)%3;
	stripverts[1] = (startv+1)%3;
	stripverts[2] = (startv+2)%3;

	striptris[0] = starttri;
	striptris[1] = starttri;
	striptris[2] = starttri;
	stripcount = 3;

	while( 1 )
	{
		if (stripcount & 1)
		{
			j = neighbortri[starttri][(startv+1)%3];
			k = neighboredge[starttri][(startv+1)%3];
		}
		else
		{
			j = neighbortri[starttri][(startv+2)%3];
			k = neighboredge[starttri][(startv+2)%3];
		}
		if (j == -1 || used[j]) goto done;

		stripverts[stripcount] = (k+2)%3;
		striptris[stripcount] = j;
		stripcount++;

		used[j] = 2;
		starttri = j;
		startv = k;
	}

done:

	// clear the temp used flags
	for (j = 0; j < pmesh->numtris; j++)
		if (used[j] == 2) used[j] = 0;

	return stripcount;
}

int FanLength (int starttri, int startv)
{
	int	j, k;

	used[starttri] = 2;

	stripverts[0] = (startv)%3;
	stripverts[1] = (startv+1)%3;
	stripverts[2] = (startv+2)%3;

	striptris[0] = starttri;
	striptris[1] = starttri;
	striptris[2] = starttri;
	stripcount = 3;

	while( 1 )
	{
		j = neighbortri[starttri][(startv+2)%3];
		k = neighboredge[starttri][(startv+2)%3];

		if (j == -1 || used[j]) goto done;

		stripverts[stripcount] = (k+2)%3;
		striptris[stripcount] = j;
		stripcount++;

		used[j] = 2;
		starttri = j;
		startv = k;
	}

done:
	// clear the temp used flags
	for (j=0 ; j<pmesh->numtris ; j++)
		if (used[j] == 2) used[j] = 0;

	return stripcount;
}

int BuildTris (s_trianglevert_t (*x)[3], s_mesh_t *y, byte **ppdata )
{
	int	i, j, k, m;
	int	startv;
	int	len, bestlen, besttype;
	int	bestverts[MAXSTUDIOTRIANGLES];
	int	besttris[MAXSTUDIOTRIANGLES];
	int	peak[MAXSTUDIOTRIANGLES];
	int	type;
	int	total = 0;
	long 	t;
	int	maxlen;

	triangles = x;
	pmesh = y;


	t = time( NULL );

	for (i = 0; i < pmesh->numtris; i++)
	{
		neighbortri[i][0] = neighbortri[i][1] = neighbortri[i][2] = -1;
		used[i] = 0;
		peak[i] = pmesh->numtris;
	}

	for (i = 0; i < pmesh->numtris; i++)
	{
		for (k = 0; k < 3; k++)
		{
			if (used[i] & (1 << k)) continue;
			FindNeighbor( i, k );
		}
	}

	// build tristrips
	numcommandnodes = 0;
	numcommands = 0;
	memset (used, 0, sizeof(used));

	for (i=0 ; i<pmesh->numtris ;)
	{
		// pick an unused triangle and start the trifan
		if (used[i])
		{
			i++;
			continue;
		}

		maxlen = 9999;
		bestlen = 0;
		m = 0;
		for (k = i; k < pmesh->numtris && bestlen < 127; k++)
		{
			int localpeak = 0;

			if (used[k]) continue;
			if (peak[k] <= bestlen) continue;

			m++;
			for (type = 0 ; type < 2 ; type++)
			{
				for (startv =0 ; startv < 3 ; startv++)
				{
					if (type == 1) len = FanLength (k, startv);
					else len = StripLength (k, startv);

					if (len > 127)
					{
						// skip these, they are too long to encode
					}
					else if (len > bestlen)
					{
						besttype = type;
						bestlen = len;
						for (j = 0; j < bestlen; j++)
						{
							besttris[j] = striptris[j];
							bestverts[j] = stripverts[j];
						}
					}
					if (len > localpeak) localpeak = len;
				}
			}

			peak[k] = localpeak;
			if (localpeak == maxlen) break;
		}

		total += (bestlen - 2);
		maxlen = bestlen;

		// mark the tris on the best strip as used
		for (j = 0; j < bestlen; j++)
			used[besttris[j]] = 1;

		if (besttype == 1) commands[numcommands++] = -bestlen;
		else commands[numcommands++] = bestlen;

		for (j = 0; j < bestlen; j++)
		{
			s_trianglevert_t *tri;
			tri = &triangles[besttris[j]][bestverts[j]];

			commands[numcommands++] = tri->vertindex;
			commands[numcommands++] = tri->normindex;
			commands[numcommands++] = tri->s;
			commands[numcommands++] = tri->t;
		}
		numcommandnodes++;

		if (t != time(NULL))
		{
			Msg("%2d%%\r", (total * 100) / pmesh->numtris );
			t = time(NULL);
		}
	}

	commands[numcommands++] = 0;// end of list marker
	*ppdata = (byte *)commands;

	return numcommands * sizeof( short );
}

void ExtractMotion( void )
{ 
	int i, j, k, q;

	// extract linear motion
	for (i = 0; i < numseq; i++)
	{
		if (sequence[i].numframes > 1)
		{
			// assume 0 for now.
			int	type;
			vec3_t	*ppos;
			vec3_t	motion = {0,0,0};

			type = sequence[i].motiontype;
			ppos = sequence[i].panim[0]->pos[0];
			k = sequence[i].numframes - 1;

			if (type & STUDIO_LX) motion[0] = ppos[k][0] - ppos[0][0];
			if (type & STUDIO_LY) motion[1] = ppos[k][1] - ppos[0][1];
			if (type & STUDIO_LZ) motion[2] = ppos[k][2] - ppos[0][2];

			for (j = 0; j < sequence[i].numframes; j++)
			{	
				vec3_t adj;
				for (k = 0; k < sequence[i].panim[0]->numbones; k++)
				{
					if (sequence[i].panim[0]->node[k].parent == -1)
					{
						ppos = sequence[i].panim[0]->pos[k];

						VectorScale( motion, j * 1.0 / (sequence[i].numframes - 1), adj );
						for (q = 0; q < sequence[i].numblends; q++)
						{
							VectorSubtract( sequence[i].panim[q]->pos[k][j], adj, sequence[i].panim[q]->pos[k][j] );
						}
					}
				}
			}
			VectorCopy( motion, sequence[i].linearmovement );
		}
		else
		{
			VectorSubtract( sequence[i].linearmovement, sequence[i].linearmovement, sequence[i].linearmovement );
		}
	}

	// extract unused motion
	for (i = 0; i < numseq; i++)
	{
		int type = sequence[i].motiontype;

		for (k = 0; k < sequence[i].panim[0]->numbones; k++)
		{
			if (sequence[i].panim[0]->node[k].parent == -1)
			{
				for (q = 0; q < sequence[i].numblends; q++)
				{
					float motion[6];
					motion[0] = sequence[i].panim[q]->pos[k][0][0];
					motion[1] = sequence[i].panim[q]->pos[k][0][1];
					motion[2] = sequence[i].panim[q]->pos[k][0][2];
					motion[3] = sequence[i].panim[q]->rot[k][0][0];
					motion[4] = sequence[i].panim[q]->rot[k][0][1];
					motion[5] = sequence[i].panim[q]->rot[k][0][2];

					for (j = 0; j < sequence[i].numframes; j++)
					{	
						//if (type & STUDIO_X) sequence[i].panim[q]->pos[k][j][0] = motion[0];
						//if (type & STUDIO_Y) sequence[i].panim[q]->pos[k][j][1] = motion[1];
						//if (type & STUDIO_Z) sequence[i].panim[q]->pos[k][j][2] = motion[2];
						if (type & STUDIO_XR) sequence[i].panim[q]->rot[k][j][0] = motion[3];
						if (type & STUDIO_YR) sequence[i].panim[q]->rot[k][j][1] = motion[4];
						if (type & STUDIO_ZR) sequence[i].panim[q]->rot[k][j][2] = motion[5];
					}
				}
			}
		}
	}

	// extract auto motion
	for (i = 0; i < numseq; i++)
	{
		// assume 0 for now.
		int	type;
		vec3_t	*ppos;
		vec3_t	*prot;
		vec3_t	motion = {0,0,0};
		vec3_t	angles = {0,0,0};

		type = sequence[i].motiontype;

		for (j = 0; j < sequence[i].numframes; j++)
		{	
			ppos = sequence[i].panim[0]->pos[0];
			prot = sequence[i].panim[0]->rot[0];

			if (type & STUDIO_AX) motion[0] = ppos[j][0] - ppos[0][0];
			if (type & STUDIO_AY) motion[1] = ppos[j][1] - ppos[0][1];
			if (type & STUDIO_AZ) motion[2] = ppos[j][2] - ppos[0][2];
			if (type & STUDIO_AXR) angles[0] = prot[j][0] - prot[0][0];
			if (type & STUDIO_AYR) angles[1] = prot[j][1] - prot[0][1];
			if (type & STUDIO_AZR) angles[2] = prot[j][2] - prot[0][2];

			VectorCopy( motion, sequence[i].automovepos[j] );
			VectorCopy( angles, sequence[i].automoveangle[j] );

			for (k = 0; k < sequence[i].panim[0]->numbones; k++)
			{
				if (sequence[i].panim[0]->node[k].parent == -1)
				{
					for (q = 0; q < sequence[i].numblends; q++)
					{
						// VectorSubtract( sequence[i].panim[q]->pos[k][j], motion, sequence[i].panim[q]->pos[k][j] );
						// VectorSubtract( sequence[i].panim[q]->rot[k][j], angles, sequence[i].panim[q]->pos[k][j] );
					}
				}
			}
		}
	}
}


int findNode( char *name )
{
	int k;

	for (k = 0; k < numbones; k++)
	{
		if (strcmp( bonetable[k].name, name ) == 0)
			return k;
	}
	return -1;
}


void MakeTransitions( void )
{
	int i, j, k;
	int iHit;

	// add in direct node transitions
	for (i = 0; i < numseq; i++)
	{
		if (sequence[i].entrynode != sequence[i].exitnode)
		{
			xnode[sequence[i].entrynode-1][sequence[i].exitnode-1] = sequence[i].exitnode;
			if (sequence[i].nodeflags)
				xnode[sequence[i].exitnode-1][sequence[i].entrynode-1] = sequence[i].entrynode;
		}
		if (sequence[i].entrynode > numxnodes) numxnodes = sequence[i].entrynode;
	}

	// add multi-stage transitions 
	do 
	{
		iHit = 0;
		for (i = 1; i <= numxnodes; i++)
		{
			for (j = 1; j <= numxnodes; j++)
			{
				// if I can't go there directly
				if (i != j && xnode[i-1][j-1] == 0)
				{
					for (k = 1; k < numxnodes; k++)
					{
						// but I found someone who knows how that I can get to
						if (xnode[k-1][j-1] > 0 && xnode[i-1][k-1] > 0)
						{
							// then go to them
							xnode[i-1][j-1] = -xnode[i-1][k-1];
							iHit = 1;
							break;
						}
					}
				}
			}
		}
		// reset previous pass so the links can be used in the next pass
		for (i = 1; i <= numxnodes; i++)
			for (j = 1; j <= numxnodes; j++)
				xnode[i-1][j-1] = abs( xnode[i-1][j-1] );
	}
	while(iHit);
}

int lookup_texture( char *texturename )
{
	int i;

	for (i = 0; i < numtextures; i++)
	{
		if (com.stricmp( texture[i].name, texturename ) == 0)
			return i;
	}

	com.strncpy( texture[i].name, texturename, sizeof(texture[i].name));
	
	if(com.stristr( texturename, "chrome" ) != NULL)
		texture[i].flags = STUDIO_NF_FLATSHADE | STUDIO_NF_CHROME;
	else if(com.stristr( texturename, "bright" ) != NULL)
		texture[i].flags = STUDIO_NF_FLATSHADE | STUDIO_NF_FULLBRIGHT;
	else texture[i].flags = 0;

	numtextures++;
	return i;
}

s_mesh_t *lookup_mesh( s_model_t *pmodel, char *texturename )
{
	int i, j;

	j = lookup_texture( texturename );

	for (i = 0; i < pmodel->nummesh; i++)
	{
		if (pmodel->pmesh[i]->skinref == j)
			return pmodel->pmesh[i];
	}
	
	if (i >= MAXSTUDIOMESHES) Sys_Break( "too many meshes in model: \"%s\"\n", pmodel->name );

	pmodel->nummesh = i + 1;
	pmodel->pmesh[i] = Kalloc( sizeof( s_mesh_t ) );
	pmodel->pmesh[i]->skinref = j;

	return pmodel->pmesh[i];
}

s_trianglevert_t *lookup_triangle( s_mesh_t *pmesh, int index )
{
	if (index >= pmesh->alloctris)
	{
		int start = pmesh->alloctris;
		pmesh->alloctris = index + 256;
		pmesh->triangle = Realloc( pmesh->triangle, pmesh->alloctris * sizeof( *pmesh->triangle ));
	}
	return pmesh->triangle[index];
}

int lookup_normal( s_model_t *pmodel, s_normal_t *pnormal )
{
	int i;

	for (i = 0; i < pmodel->numnorms; i++)
	{
		if (DotProduct( pmodel->normal[i].org, pnormal->org ) > normal_blend && pmodel->normal[i].bone == pnormal->bone && pmodel->normal[i].skinref == pnormal->skinref)
			return i;
	}

	if (i >= MAXSTUDIOVERTS) Sys_Break( "too many normals in model: \"%s\"\n", pmodel->name);

	VectorCopy( pnormal->org, pmodel->normal[i].org );
	pmodel->normal[i].bone = pnormal->bone;
	pmodel->normal[i].skinref = pnormal->skinref;
	pmodel->numnorms = i + 1;
	return i;
}

int lookup_vertex( s_model_t *pmodel, s_vertex_t *pv )
{
	int i;

	// assume 2 digits of accuracy
	pv->org[0] = (int)(pv->org[0] * 100) / 100.0;
	pv->org[1] = (int)(pv->org[1] * 100) / 100.0;
	pv->org[2] = (int)(pv->org[2] * 100) / 100.0;

	for (i = 0; i < pmodel->numverts; i++)
	{
		if (VectorCompare( pmodel->vert[i].org, pv->org ) && pmodel->vert[i].bone == pv->bone)
			return i;
	}

	if (i >= MAXSTUDIOVERTS) Sys_Break( "too many vertices in model: \"%s\"\n", pmodel->name);

	VectorCopy( pv->org, pmodel->vert[i].org );
	pmodel->vert[i].bone = pv->bone;
	pmodel->numverts = i + 1;
	return i;
}

int lookupControl( char *string )
{
	if (stricmp(string,"X")==0) return STUDIO_X;
	if (stricmp(string,"Y")==0) return STUDIO_Y;
	if (stricmp(string,"Z")==0) return STUDIO_Z;
	if (stricmp(string,"XR")==0) return STUDIO_XR;
	if (stricmp(string,"YR")==0) return STUDIO_YR;
	if (stricmp(string,"ZR")==0) return STUDIO_ZR;
	if (stricmp(string,"LX")==0) return STUDIO_LX;
	if (stricmp(string,"LY")==0) return STUDIO_LY;
	if (stricmp(string,"LZ")==0) return STUDIO_LZ;
	if (stricmp(string,"AX")==0) return STUDIO_AX;
	if (stricmp(string,"AY")==0) return STUDIO_AY;
	if (stricmp(string,"AZ")==0) return STUDIO_AZ;
	if (stricmp(string,"AXR")==0) return STUDIO_AXR;
	if (stricmp(string,"AYR")==0) return STUDIO_AYR;
	if (stricmp(string,"AZR")==0) return STUDIO_AZR;
	return -1;
}

void adjust_vertex( float *org )
{
	org[0] = (org[0] - adjust[0]);
	org[1] = (org[1] - adjust[1]);
	org[2] = (org[2] - adjust[2]);
}

void scale_vertex( float *org )
{
	float tmp = org[0];
	org[0] = org[0] * scale_up;
	org[1] = org[1] * scale_up;
	org[2] = org[2] * scale_up;
}

void clip_rotations( vec3_t rot )
{
	int j;// clip everything to : -M_PI <= x < M_PI

	for (j = 0; j < 3; j++)
	{
		while (rot[j] >= M_PI) rot[j] -= M_PI*2;
		while (rot[j] < -M_PI) rot[j] += M_PI*2;
	}
}

int lookupActivity( char *szActivity )
{
	int i;

	for (i = 0; activity_map[i].name; i++)
	{
		if (!stricmp( szActivity, activity_map[i].name ))
			return activity_map[i].type;
	}

	// match ACT_#
	if (!strnicmp( szActivity, "ACT_", 4 ))
		return atoi( &szActivity[4] );
	return 0;
}

void TextureCoordRanges( s_mesh_t *pmesh, s_texture_t *ptexture  )
{
	int i, j;

	if (ptexture->flags & STUDIO_NF_CHROME)
	{
		ptexture->skintop = 0;
		ptexture->skinleft = 0;
		ptexture->skinwidth = (ptexture->srcwidth + 3) & ~3;
		ptexture->skinheight = ptexture->srcheight;

		for (i = 0; i < pmesh->numtris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				pmesh->triangle[i][j].s = 0;
				pmesh->triangle[i][j].t = 0;
			}
			ptexture->max_s = 63;
			ptexture->min_s = 0;
			ptexture->max_t = 63;
			ptexture->min_t = 0;
		}
		return;
	}

	for (i = 0; i < pmesh->numtris; i++)
	{
		for (j = 0; j < 3; j++)
		{
			if (pmesh->triangle[i][j].u > 2.0) pmesh->triangle[i][j].u = 2.0;
			if (pmesh->triangle[i][j].u < -1.0) pmesh->triangle[i][j].u = -1.0;
			if (pmesh->triangle[i][j].v > 2.0) pmesh->triangle[i][j].v = 2.0;
			if (pmesh->triangle[i][j].v < -1.0) pmesh->triangle[i][j].v = -1.0;
		}
	}

	// pack texture coords
	if (!clip_texcoords)
	{
		int k, n;

		do 
		{
			float min_u = 10;
			float max_u = -10;
			float k_max_u, n_min_u;
			k = -1;
			n = -1;
			
			for (i = 0; i < pmesh->numtris; i++) 
			{
				float local_min, local_max;
				local_min = min( pmesh->triangle[i][0].u, min( pmesh->triangle[i][1].u, pmesh->triangle[i][2].u ));
				local_max = max( pmesh->triangle[i][0].u, max( pmesh->triangle[i][1].u, pmesh->triangle[i][2].u ));
				if (local_min < min_u) { min_u = local_min; k = i; k_max_u = local_max; }
				if (local_max > max_u) { max_u = local_max; n = i; n_min_u = local_min; }
			}

			if (k_max_u + 1.0 < max_u)
			{
				for (j = 0; j < 3; j++) pmesh->triangle[k][j].u += 1.0;
			}
			else if (n_min_u - 1.0 > min_u)
			{
				for (j = 0; j < 3; j++) pmesh->triangle[n][j].u -= 1.0;
			}
			else break;
		} while (1);

		do 
		{
			float min_v = 10;
			float max_v = -10;
			float k_max_v, n_min_v;
			k = -1;
			n = -1;
			
			for (i = 0; i < pmesh->numtris ; i++) 
			{
				float local_min, local_max;
				local_min = min( pmesh->triangle[i][0].v, min( pmesh->triangle[i][1].v, pmesh->triangle[i][2].v ));
				local_max = max( pmesh->triangle[i][0].v, max( pmesh->triangle[i][1].v, pmesh->triangle[i][2].v ));
				if (local_min < min_v) { min_v = local_min; k = i; k_max_v = local_max; }
				if (local_max > max_v) { max_v = local_max; n = i; n_min_v = local_min; }
			}

			if (k_max_v + 1.0 < max_v)
			{
				for (j = 0; j < 3; j++) pmesh->triangle[k][j].v += 1.0;
			}
			else if (n_min_v - 1.0 > min_v)
			{
				for (j = 0; j < 3; j++) pmesh->triangle[n][j].v -= 1.0;
			}
			else break;
		} while (1);
	}
	else
	{
		for (i = 0; i < pmesh->numtris; i++) 
		{
			for (j = 0; j < 3; j++) 
			{
				if (pmesh->triangle[i][j].u < 0) pmesh->triangle[i][j].u = 0;
				if (pmesh->triangle[i][j].u > 1) pmesh->triangle[i][j].u = 1;
				if (pmesh->triangle[i][j].v < 0) pmesh->triangle[i][j].v = 0;
				if (pmesh->triangle[i][j].v > 1) pmesh->triangle[i][j].v = 1;
			}
		}
	}

	// convert to pixel coordinates
	for (i = 0; i < pmesh->numtris ; i++)
	{
		for (j = 0; j < 3; j++)
		{
			// FIXME: losing texture coord resultion!
			pmesh->triangle[i][j].s = pmesh->triangle[i][j].u * (ptexture->srcwidth - 1);
			pmesh->triangle[i][j].t = pmesh->triangle[i][j].v * (ptexture->srcheight - 1);
		}
	}

	// find the range
	if (!clip_texcoords)
	{
		for (i = 0; i < pmesh->numtris; i++)
		{
			for (j = 0; j < 3; j++)
			{
				ptexture->max_s = max( pmesh->triangle[i][j].s, ptexture->max_s );
				ptexture->min_s = min( pmesh->triangle[i][j].s, ptexture->min_s );
				ptexture->max_t = max( pmesh->triangle[i][j].t, ptexture->max_t );
				ptexture->min_t = min( pmesh->triangle[i][j].t, ptexture->min_t );
			}
		}
	}
	else
	{
		ptexture->max_s = ptexture->srcwidth-1;
		ptexture->min_s = 0;
		ptexture->max_t = ptexture->srcheight-1;
		ptexture->min_t = 0;
	}
}

void ResetTextureCoordRanges( s_mesh_t *pmesh, s_texture_t *ptexture  )
{
	int i, j;

	// adjust top, left edge
	for (i=0 ; i<pmesh->numtris ; i++)
	{
		for (j = 0; j < 3; j++)
		{
			pmesh->triangle[i][j].s -= ptexture->min_s;
			pmesh->triangle[i][j].t = (ptexture->max_t - ptexture->min_t) - (pmesh->triangle[i][j].t - ptexture->min_t);
		}
	}
}

void ResizeTexture( s_texture_t *ptexture )
{
	int	i, j, s, t;
	byte	*pdest;
	int	srcadjwidth;

	// make the width a multiple of 4; some hardware requires this, and it ensures
	// dword alignment for each scan

	ptexture->skintop = ptexture->min_t;
	ptexture->skinleft = ptexture->min_s;
	ptexture->skinwidth = (int)((ptexture->max_s - ptexture->min_s) + 1 + 3) & ~3;
	ptexture->skinheight = (int)(ptexture->max_t - ptexture->min_t) + 1;
	ptexture->size = ptexture->skinwidth * ptexture->skinheight + 256 * 3;

	Msg("BMP %s [%d %d] (%.0f%%)  %6d bytes\n", ptexture->name,  ptexture->skinwidth, ptexture->skinheight, ((ptexture->skinwidth * ptexture->skinheight) / (float)(ptexture->srcwidth * ptexture->srcheight)) * 100.0f, ptexture->size );
	
	if( ptexture->size > 1536 * 1536)
	{
		Msg("%g %g %g %g\n", ptexture->min_s, ptexture->max_s, ptexture->min_t, ptexture->max_t );
		Sys_Break("texture too large\n");
	}

	pdest = Kalloc( ptexture->size );
	ptexture->pdata = pdest;

	// data is saved as a multiple of 4
	srcadjwidth = (ptexture->srcwidth + 3) & ~3;

	// move the picture data to the model area, replicating missing data, deleting unused data.
	for (i = 0, t = ptexture->srcheight - ptexture->skinheight - ptexture->skintop + 10 * ptexture->srcheight; i < ptexture->skinheight; i++, t++)
	{
		while (t >= ptexture->srcheight) t -= ptexture->srcheight;
		while (t < 0) t += ptexture->srcheight;

		for (j = 0, s = ptexture->skinleft + 10 * ptexture->srcwidth; j < ptexture->skinwidth; j++, s++)
		{
			while (s >= ptexture->srcwidth) s -= ptexture->srcwidth;
			*(pdest++) = *(ptexture->ppicture + s + t * srcadjwidth);
		}
	}

	// TODO: process the texture and flag it if fullbright or transparent are used.
	// TODO: only save as many palette entries as are actually used.

	if (gamma != 1.8)
	{
		// gamma correct the monster textures to a gamma of 1.8
		float g;
		byte *psrc = (byte *)ptexture->ppal;
		g = gamma / 1.8;
		
		for (i = 0; i < 768; i++) pdest[i] = pow( psrc[i] / 255.0, g ) * 255;
	}
	else memcpy( pdest, ptexture->ppal, 256 * sizeof( rgb_t ) );

	Mem_Free( ptexture->ppicture );
	Mem_Free( ptexture->ppal );
}