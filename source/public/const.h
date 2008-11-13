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
#define TYPE_BINDATA		69	// engine internal data (map lumps, save lumps etc)
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
#define RF_TRANSLUCENT		(1<<5)	// FIXME: remove or replace
#define RF_IR_VISIBLE		(1<<6)	// skin is an index in image_precache
#define RF_HOLOGRAMM		(1<<7)	// studio hologramm effect (like hl1)
#define RF_OCCLUSIONTEST		(1<<8)	// do occlusion test for this entity
#define RF_PLANARSHADOW	 	(1<<9)	// force shadow to planar
#define RF_NOSHADOW			(1<<10)	// disable shadow at all

// FIXME: player_state_t->renderfx
#define RDF_UNDERWATER		(1<<0)	// warp the screen as apropriate
#define RDF_NOWORLDMODEL		(1<<1)	// used for player configuration screen
#define RDF_BLOOM			(1<<2)	// light blooms
#define RDF_OLDAREABITS		(1<<3)	// forces R_MarkLeaves() if not set
#define RDF_PAIN			(1<<4)	// motion blur effects
#define RDF_IRGOGGLES		(1<<5)	// infra red goggles effect
#define RDF_PORTALINVIEW		(1<<6)	// cull entities using vis too because areabits are merged serverside
#define RDF_SKYPORTALINVIEW		(1<<7)	// draw skyportal instead of regular sky
#define RDF_NOFOVADJUSTMENT		(1<<8)	// do not adjust fov for widescreen

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

// bsp contents
typedef enum
{
	CONTENTS_NONE		= 0, 	// just a mask for source tabulation
	CONTENTS_SOLID		= BIT(0),	// an eye is never valid in a solid
	CONTENTS_WINDOW		= BIT(1),	// translucent, but not watery
	CONTENTS_AUX		= BIT(2),
	CONTENTS_LAVA		= BIT(3),
	CONTENTS_SLIME		= BIT(4),
	CONTENTS_WATER		= BIT(5),

	// space for new user contents

	CONTENTS_MIST		= BIT(12),// g-cont. what difference between fog and mist ?
	LAST_VISIBLE_CONTENTS	= BIT(12),// mask (LAST_VISIBLE_CONTENTS-1)
	CONTENTS_FOG		= BIT(13),// future expansion
	CONTENTS_AREAPORTAL		= BIT(14),// func_areaportal volume
	CONTENTS_PLAYERCLIP		= BIT(15),// clip affect only by player or bot
	CONTENTS_MONSTERCLIP	= BIT(16),// clip affect only by monster or npc
	CONTENTS_CLIP		= (CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP), // both type clip
	CONTENTS_ORIGIN		= BIT(17),// removed before bsping an entity
	CONTENTS_BODY		= BIT(18),// should never be on a brush, only in game
	CONTENTS_CORPSE		= BIT(19),// deadbody
	CONTENTS_DETAIL		= BIT(20),// brushes to be added after vis leafs
	CONTENTS_TRANSLUCENT	= BIT(21),// auto set if any surface has trans
	CONTENTS_LADDER		= BIT(22),// like water but ladder : )
	CONTENTS_TRIGGER		= BIT(23),// trigger volume

	// content masks
	MASK_SOLID		= (CONTENTS_SOLID|CONTENTS_WINDOW),
	MASK_PLAYERSOLID		= (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_BODY),
	MASK_MONSTERSOLID		= (CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_WINDOW|CONTENTS_BODY),
	MASK_DEADSOLID		= (CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WINDOW|CONTENTS_WINDOW),
	MASK_WATER		= (CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME),
	MASK_OPAQUE		= (CONTENTS_SOLID|CONTENTS_SLIME|CONTENTS_LAVA),
	MASK_SHOT			= (CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_WINDOW|CONTENTS_CORPSE)
} contentType_t;

typedef enum
{
	SURF_NONE			= 0,	// just a mask for source tabulation
	SURF_LIGHT		= BIT(0),	// value will hold the light strength
	SURF_SLICK		= BIT(1),	// effects game physics
	SURF_SKY			= BIT(2),	// don't draw, but add to skybox
	SURF_WARP			= BIT(3),	// turbulent water warp
	SURF_TRANS		= BIT(4),	// translucent
	SURF_BLEND		= BIT(5),	// same as blend
	SURF_ALPHA		= BIT(6),	// alphatest
	SURF_ADDITIVE		= BIT(7),	// additive surface
	SURF_NODRAW		= BIT(8),	// don't bother referencing the texture
	SURF_HINT			= BIT(9),	// make a primary bsp splitter
	SURF_SKIP			= BIT(10),// completely ignore, allowing non-closed brushes
	SURF_NULL			= BIT(11),// remove face after compile
	SURF_NOLIGHTMAP		= BIT(12),// don't place lightmap for this surface
	SURF_MIRROR		= BIT(12),// remove face after compile
	SURF_CHROME		= BIT(13),// chrome surface effect
	SURF_GLOW			= BIT(14),// sprites glow
} surfaceType_t;

// engine physics constants
#define COLLISION_SNAPSCALE		(32.0f)
#define COLLISION_SNAP		(1.0f / COLLISION_SNAPSCALE)
#define COLLISION_SNAP2		(2.0f / COLLISION_SNAPSCALE)
#define COLLISION_PLANE_DIST_EPSILON	(2.0f / COLLISION_SNAPSCALE)

#endif//CONST_H