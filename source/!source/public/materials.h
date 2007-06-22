//=======================================================================
//			Copyright XashXT Group 2007 ©
//			materials.h - surface contents e.t.c.
//=======================================================================

#ifndef MATERIALS_H
#define MATERIALS_H

// 0-2 are axial planes
#define PLANE_X			0
#define PLANE_Y			1
#define PLANE_Z			2
#define PLANE_ANYX			3
#define PLANE_ANYY			4
#define PLANE_ANYZ			5

// lower bits are stronger, and will eat weaker brushes completely
#define CONTENTS_NONE		0 	// just a mask for source tabulation
#define CONTENTS_SOLID		1 	// an eye is never valid in a solid
#define CONTENTS_WINDOW		2 	// translucent, but not watery
#define CONTENTS_AUX		4
#define CONTENTS_LAVA		8
#define CONTENTS_SLIME		16
#define CONTENTS_WATER		32
#define CONTENTS_MIST		64
#define LAST_VISIBLE_CONTENTS		64
#define CONTENTS_MUD		128	// not a "real" content property - used only for watertype

// remaining contents are non-visible, and don't eat brushes
#define CONTENTS_FOG		0x4000	//future expansion
#define CONTENTS_AREAPORTAL		0x8000
#define CONTENTS_PLAYERCLIP		0x10000
#define CONTENTS_MONSTERCLIP		0x20000
#define CONTENTS_CLIP		CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP //both type clip
#define CONTENTS_CURRENT_0		0x40000	// currents can be added to any other contents, and may be mixed
#define CONTENTS_CURRENT_90		0x80000
#define CONTENTS_CURRENT_180		0x100000
#define CONTENTS_CURRENT_270		0x200000
#define CONTENTS_CURRENT_UP		0x400000
#define CONTENTS_CURRENT_DOWN		0x800000
#define CONTENTS_ORIGIN		0x1000000	// removed before bsping an entity
#define CONTENTS_MONSTER		0x2000000	// should never be on a brush, only in game
#define CONTENTS_DEADMONSTER		0x4000000
#define CONTENTS_DETAIL		0x8000000	// brushes to be added after vis leafs
#define CONTENTS_TRANSLUCENT		0x10000000// auto set if any surface has trans
#define CONTENTS_LADDER		0x20000000

//surfaces
#define SURF_NONE			0 	// just a mask for source tabulation
#define SURF_LIGHT			0x1	// value will hold the light strength
#define SURF_SLICK			0x2	// effects game physics
#define SURF_SKY			0x4	// don't draw, but add to skybox
#define SURF_WARP			0x8	// turbulent water warp
#define SURF_TRANS33		0x10
#define SURF_TRANS66		0x20
#define SURF_FLOWING		0x40	// scroll towards angle
#define SURF_NODRAW			0x80	// don't bother referencing the texture
#define SURF_HINT			0x100	// make a primary bsp splitter
#define SURF_SKIP			0x200	// completely ignore, allowing non-closed brushes
#define SURF_METAL			0x00000400  // metal floor
#define SURF_DIRT			0x00000800  // dirt, sand, rock
#define SURF_VENT			0x00001000  // ventillation duct
#define SURF_GRATE			0x00002000  // metal grating
#define SURF_TILE			0x00004000  // floor tiles
#define SURF_GRASS      		0x00008000  // grass
#define SURF_SNOW       		0x00010000  // snow
#define SURF_CARPET     		0x00020000  // carpet
#define SURF_FORCE      		0x00040000  // forcefield
#define SURF_GRAVEL     		0x00080000  // gravel
#define SURF_ICE			0x00100000  // ice
#define SURF_STANDARD   		0x00200000  // normal footsteps
#define SURF_STEPMASK		0x000FFC00

#endif//MATERIALS_H