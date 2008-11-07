//=======================================================================
//			Copyright XashXT Group 2007 ©
//			const.h - shared engine constants
//=======================================================================
#ifndef CONST_H
#define CONST_H

// dlumpinfo_t->compression
#define CMP_NONE			0	// compression none
#define CMP_LZSS			1	// currently not used
#define CMP_ZLIB			2	// zip-archive compression

// dlumpinfo_t->type
#define TYPE_QPAL			64	// quake palette
#define TYPE_QTEX			65	// probably was never used
#define TYPE_QPIC			66	// quake1 and hl pic (lmp_t)
#define TYPE_MIPTEX2		67	// half-life (mip_t) previous was TYP_SOUND but never used in quake1
#define TYPE_MIPTEX			68	// quake1 (mip_t)
#define TYPE_BINDATA		69	// engine internal data
#define TYPE_STRDATA		70	// big unterminated string (stringtable marked as TYPE_BINARYDATA)

#define TYPE_RAW			71	// unrecognized raw data
#define TYPE_SCRIPT			72	// .txt scrips (xash ext)
#define TYPE_VPROGS			73	// .dat progs (xash ext)

// edict_t->spawnflags
#define SF_START_ON			0x1

// entity_state_t->renderfx
#define RF_MINLIGHT			(1<<0)	// allways have some light (viewmodel)
#define RF_PLAYERMODEL		(1<<1)	// don't draw through eyes, only mirrors
#define RF_VIEWMODEL		(1<<2)	// it's a viewmodel
#define RF_FULLBRIGHT		(1<<3)	// allways draw full intensity
#define RF_DEPTHHACK		(1<<4)	// for view weapon Z crunching
#define RF_TRANSLUCENT		(1<<5)
#define RF_IR_VISIBLE		(1<<6)	// skin is an index in image_precache
#define RF_HOLOGRAMM		(1<<7)

// entity_state_t->effects
#define EF_BRIGHTFIELD		(1<<0)	// swirling cloud of particles
#define EF_MUZZLEFLASH		(1<<1)	// single frame ELIGHT on entity attachment 0
#define EF_BRIGHTLIGHT		(1<<2)	// DLIGHT centered at entity origin
#define EF_DIMLIGHT			(1<<3)	// player flashlight
#define EF_INVLIGHT			(1<<4)	// get lighting from ceiling
#define EF_NOINTERP			(1<<5)	// don't interpolate the next frame
#define EF_LIGHT			(1<<6)	// rocket flare glow sprite
#define EF_NODRAW			(1<<7)	// don't draw entity
#define EF_TELEPORT			(1<<8)	// create teleport splash
#define EF_ROTATE			(1<<9)	// rotate bonus item

// user contents
#define CONTENTS_SOLID		(1<<0)	// an eye is never valid in a solid
#define CONTENTS_LAVA		(1<<1)	// liquid type (apply damage and some visual sfx)
#define CONTENTS_SLIME		(1<<2)	// liquid type (apply damaga and some visual sfx)
#define CONTENTS_WATER		(1<<3)	// normal translucent water		
#define CONTENTS_FOG		(1<<4)	// fog area or underwater volume
#define CONTENTS_WINDOW		(1<<5)	// get rid of this

// system contents
#define CONTENTS_AREAPORTAL		(1<<15)
#define CONTENTS_ANTIPORTAL		(1<<16)
#define CONTENTS_PLAYERCLIP		(1<<17)
#define CONTENTS_MONSTERCLIP		(1<<18)
#define CONTENTS_CLIP		CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP
#define CONTENTS_ORIGIN		(1<<19)	// removed before bsping an entity
#define CONTENTS_BODY		(1<<20)	// should never be on a brush, only in game
#define CONTENTS_CORPSE		(1<<21)	// dead bodies
#define CONTENTS_DETAIL		(1<<22)	// brushes not used for the bsp
#define CONTENTS_STRUCTURAL		(1<<23)	// brushes used for the bsp
#define CONTENTS_TRANSLUCENT		(1<<24)	// don't consume surface fragments inside
#define CONTENTS_TRIGGER		(1<<25)	// trigger volume
#define CONTENTS_NODROP		(1<<26)	// don't leave bodies or items (death fog, lava)
#define CONTENTS_LADDER		(1<<27)	// ladder in games
#define CONTENTS_LIGHTGRID		(1<<28)	// lightgrid contents

// surface flags
#define SURF_NODAMAGE		(1<<0)	// never give falling damage
#define SURF_SLICK			(1<<1)	// effects game physics
#define SURF_SKY			(1<<2)	// lighting from environment map
#define SURF_POINTLIGHT		(1<<3)	// generate lighting info at vertexes
#define SURF_NOIMPACT		(1<<4)	// don't make missile explosions
#define SURF_NOMARKS		(1<<5)	// don't leave missile marks
#define SURF_PORTAL			(1<<6)	// portal surface
#define SURF_NODRAW			(1<<7)	// don't generate a drawsurface at all
#define SURF_HINT			(1<<8)	// make a primary bsp splitter
#define SURF_SKIP			(1<<9)	// make a secondary bsp splitter
#define SURF_NOLIGHTMAP		(1<<10)	// surface doesn't need a lightmap
#define SURF_NOSTEPS		(1<<11)	// no footstep sounds
#define SURF_LIGHTFILTER		(1<<12)	// act as a light filter during q3map -light
#define SURF_ALPHASHADOW		(1<<13)	// do per-pixel light shadow casting in q3map
#define SURF_NODLIGHT		(1<<14)	// never add dynamic lights
#define SURF_ALPHA			(1<<15)	// alpha surface (e.g. grates, trees)
#define SURF_BLEND			(1<<16)	// blended surface (e.g. windows)
#define SURF_ADDITIVE		(1<<17)	// additive surface (studio skins)
#define SURF_VERTEXLIT		(SURF_POINTLIGHT|SURF_NOLIGHTMAP)

// engine physics constants
#define COLLISION_SNAPSCALE		(32.0f)
#define COLLISION_SNAP		(1.0f / COLLISION_SNAPSCALE)
#define COLLISION_SNAP2		(2.0f / COLLISION_SNAPSCALE)
#define COLLISION_PLANE_DIST_EPSILON	(2.0f / COLLISION_SNAPSCALE)

#endif//CONST_H