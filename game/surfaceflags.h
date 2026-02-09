/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// This file must be identical in the quake and utils directories

// contents flags are seperate bits
// a given brush can contribute multiple content bits

// these definitions also need to be in q_shared.h!

#define CONTENTS_SOLID           BIT( 0 ) // an eye is never valid in a solid
#define CONTENTS_LAVA            BIT( 3 )
#define CONTENTS_SLIME           BIT( 4 )
#define CONTENTS_WATER           BIT( 5 )
#define CONTENTS_FOG             BIT( 6 )
#define CONTENTS_AREAPORTAL      BIT( 15 )
#define CONTENTS_PLAYERCLIP      BIT( 16 )
#define CONTENTS_MONSTERCLIP     BIT( 17 )
#define CONTENTS_CAMERACLIP      BIT( 18 ) // (fakk2)
#define CONTENTS_WEAPONCLIP      BIT( 19 ) // (fakk2) blocks projectiles and weapon attacks as well
#define CONTENTS_SHOOTABLE_ONLY  BIT( 20 ) // (fakk2) player can walk through this but can shoot it as well
#define CONTENTS_ORIGIN          BIT( 24 ) // removed before bsping an entity
#define CONTENTS_BODY            BIT( 25 ) // should never be on a brush, only in game
#define CONTENTS_CORPSE          BIT( 26 )
#define CONTENTS_DETAIL          BIT( 27 ) // brushes not used for the bsp
#define CONTENTS_STRUCTURAL      BIT( 28 ) // brushes used for the bsp
#define CONTENTS_TRANSLUCENT     BIT( 29 ) // don't consume surface fragments inside
#define CONTENTS_NODROP          BIT( 31 ) // don't leave bodies or items (death fog, lava)

#define CONTENTS_KEEP ( CONTENTS_DETAIL )

#define MASK_CLIP ( CONTENTS_PLAYERCLIP | CONTENTS_MONSTERCLIP | CONTENTS_CAMERACLIP | CONTENTS_WEAPONCLIP )


#define SURF_NODAMAGE     BIT( 0 )   // never give falling damage
#define SURF_SLICK        BIT( 1 )   // effects game physics
#define SURF_SKY          BIT( 2 )   // lighting from environment map
#define SURF_LADDER       BIT( 3 )   // ladder surface
#define SURF_NOIMPACT     BIT( 4 )   // don't make missile explosions
#define SURF_NOMARKS      BIT( 5 )   // don't leave missile marks
#define SURF_CASTSHADOW   BIT( 6 )   // used in conjunction with nodraw allows surface to be not drawn but still cast shadows
#define SURF_NODRAW       BIT( 7 )   // don't generate a drawsurface at all
#define SURF_NOLIGHTMAP   BIT( 10 )  // surface doesn't need a lightmap
#define SURF_ALPHASHADOW  BIT( 11 )  // do per-pixel shadow tests based on the texture
#define SURF_NOSTEPS      BIT( 13 )  // no footstep sounds
#define SURF_NONSOLID     BIT( 14 )  // don't collide against curves with this set
#define SURF_RICOCHET     BIT( 15 )  // ricochet bullets

#define SURF_TYPE_WOOD    BIT( 16 ) // wood surface
#define SURF_TYPE_METAL   BIT( 17 ) // metal surface
#define SURF_TYPE_ROCK    BIT( 18 ) // stone surface
#define SURF_TYPE_DIRT    BIT( 19 ) // dirt surface
#define SURF_TYPE_GRILL   BIT( 20 ) // metal grill surface
#define SURF_TYPE_ORGANIC BIT( 21 ) // oraganic (grass, loamy dirt)
#define SURF_TYPE_SQUISHY BIT( 22 ) // squishy (swamp dirt, flesh)

#define SURF_NODLIGHT BIT( 23 )                // don't dlight even if solid (solid lava, skies)
#define SURF_HINT     BIT( 24 )                // choose this plane as a partitioner

#define SURF_PATCH BIT( 29 )
#define SURF_KEEP  ( SURF_PATCH )

#define MASK_SURF_TYPE ( SURF_TYPE_WOOD | SURF_TYPE_METAL | SURF_TYPE_ROCK | SURF_TYPE_DIRT | SURF_TYPE_GRILL | SURF_TYPE_ORGANIC | SURF_TYPE_SQUISHY )
