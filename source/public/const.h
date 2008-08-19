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

#endif//CONST_H