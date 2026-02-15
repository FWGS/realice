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
#include "tr_local.h"

// tr_shader.c -- this file deals with the parsing and definition of shaders

typedef struct shadertext_s
{
	char     name[64];
	char     *text;
	shader_t *shader;
	struct shadertext_s *next;
} shadertext_t;

static char *s_shaderText;

// the shader is parsed into these global variables, then copied into
// dynamically allocated memory if it is valid.
static shaderStage_t foggedStages[MAX_SHADER_STAGES];
static shaderStage_t unfoggedStages[MAX_SHADER_STAGES];
static shaderStage_t alphaFoggedStages[MAX_SHADER_STAGES];
static texModInfo_t  texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS];
static shader_t      shader;
static qboolean      shader_force32bit;
static qboolean      fogOnly;
static qboolean      shader_noPicMip;
static qboolean      shader_noMipMaps;


#define FILE_HASH_SIZE 1024
static shadertext_t *currentShader = NULL;
static shadertext_t *hashTable[FILE_HASH_SIZE];

/*
================
return a hash value for the filename
================
*/
static long generateHashValue( const char *fname )
{
	int  i;
	long hash;
	char letter;

	hash = 0;
	i = 0;
	while( fname[i] != '\0' )
	{
		letter = tolower( fname[i] );
		if( letter == '.' )
			break;                                          // don't include extension
		if( letter == '\\' )
			letter = '/';                           // damn path names
		if( letter == PATH_SEP )
			letter = '/';                           // damn path names
		hash += (long)( letter ) * ( i + 119 );
		i++;
	}
	hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ));
	hash &= ( FILE_HASH_SIZE - 1 );
	return hash;
}

static shadertext_t *AddShaderTextToHash( const char *name, int hash )
{
	shadertext_t *shader;

	shader = (shadertext_t *)ri.Malloc( sizeof( shadertext_t ));

	Com_Memset( shader, 0, sizeof( shadertext_t ));
	strncpy( shader->name, name, sizeof( shader->name ));
	shader->next = hashTable[hash];

	hashTable[hash] = shader;
	return shader;
}

static shadertext_t *FindShaderText( const char *name )
{
	long hash = generateHashValue( name );
	shadertext_t *st = hashTable[hash];

	for( ; st; st = st->next )
	{
		if( !Q_stricmp( st->name, name ))
			return st;
	}

	return AddShaderTextToHash( name, hash );
}

/*
===============
ParseVector
===============
*/
static qboolean ParseVector( char **text, int count, float *v )
{
	char *token;
	int  i;

	for( i = 0; i < count; i++ )
	{
		token = COM_ParseExt( text, qfalse );
		if( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name );
			return qfalse;
		}
		v[i] = atof( token );
	}

	return qtrue;
}

/*
===============
NameToAFunc
===============
*/
static unsigned NameToAFunc( const char *funcname )
{
	if( !Q_stricmp( funcname, "GT0" ))
	{
		return GLS_ATEST_GT_0;
	}
	else if( !Q_stricmp( funcname, "LT128" ))
	{
		return GLS_ATEST_LT_80;
	}
	else if( !Q_stricmp( funcname, "GE128" ))
	{
		return GLS_ATEST_GE_80;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname, shader.name );
	return 0;
}

/*
===============
NameToSrcBlendMode
===============
*/
static int NameToSrcBlendMode( const char *name )
{
	if( !Q_stricmp( name, "GL_ONE" ))
	{
		return GLS_SRCBLEND_ONE;
	}
	else if( !Q_stricmp( name, "GL_ZERO" ))
	{
		return GLS_SRCBLEND_ZERO;
	}
	else if( !Q_stricmp( name, "GL_DST_COLOR" ))
	{
		return GLS_SRCBLEND_DST_COLOR;
	}
	else if( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ))
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if( !Q_stricmp( name, "GL_SRC_ALPHA" ))
	{
		return GLS_SRCBLEND_SRC_ALPHA;
	}
	else if( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ))
	{
		return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if( !Q_stricmp( name, "GL_DST_ALPHA" ))
	{
		return GLS_SRCBLEND_DST_ALPHA;
	}
	else if( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ))
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ))
	{
		return GLS_SRCBLEND_ALPHA_SATURATE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_SRCBLEND_ONE;
}

/*
===============
NameToDstBlendMode
===============
*/
static int NameToDstBlendMode( const char *name )
{
	if( !Q_stricmp( name, "GL_ONE" ))
	{
		return GLS_DSTBLEND_ONE;
	}
	else if( !Q_stricmp( name, "GL_ZERO" ))
	{
		return GLS_DSTBLEND_ZERO;
	}
	else if( !Q_stricmp( name, "GL_SRC_ALPHA" ))
	{
		return GLS_DSTBLEND_SRC_ALPHA;
	}
	else if( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ))
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if( !Q_stricmp( name, "GL_DST_ALPHA" ))
	{
		return GLS_DSTBLEND_DST_ALPHA;
	}
	else if( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ))
	{
		return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if( !Q_stricmp( name, "GL_SRC_COLOR" ))
	{
		return GLS_DSTBLEND_SRC_COLOR;
	}
	else if( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ))
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_DSTBLEND_ONE;
}

/*
===============
NameToGenFunc
===============
*/
static genFunc_t NameToGenFunc( const char *funcname )
{
	if( !Q_stricmp( funcname, "sin" ))
	{
		return GF_SIN;
	}
	else if( !Q_stricmp( funcname, "square" ))
	{
		return GF_SQUARE;
	}
	else if( !Q_stricmp( funcname, "triangle" ))
	{
		return GF_TRIANGLE;
	}
	else if( !Q_stricmp( funcname, "sawtooth" ))
	{
		return GF_SAWTOOTH;
	}
	else if( !Q_stricmp( funcname, "inversesawtooth" ))
	{
		return GF_INVERSE_SAWTOOTH;
	}
	else if( !Q_stricmp( funcname, "noise" ))
	{
		return GF_NOISE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, shader.name );
	return GF_SIN;
}

// NOXREF
static inline float waveform_atof( const char *token )
{
	if( !Q_stricmp( token, "fromEntity" ))
		return 1234567;
	return atof( token );
}

/*
===================
ParseWaveForm
===================
*/
static void ParseWaveForm( char **text, waveForm_t *wave )
{
	char *token;

	token = COM_ParseExt( text, qfalse );
	if( token[0] == 0 )
	{
		ri.Printf( PRINT_ERROR, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->func = NameToGenFunc( token );

	// BASE, AMP, PHASE, FREQ
	token = COM_ParseExt( text, qfalse );
	if( token[0] == 0 )
	{
		ri.Printf( PRINT_ERROR, "WARNING: missing BASE waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->base = waveform_atof( token );

	token = COM_ParseExt( text, qfalse );
	if( token[0] == 0 )
	{
		ri.Printf( PRINT_ERROR, "WARNING: missing AMPLITUDE waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->amplitude = waveform_atof( token );

	token = COM_ParseExt( text, qfalse );
	if( token[0] == 0 )
	{
		ri.Printf( PRINT_ERROR, "WARNING: missing PHASE waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->phase = waveform_atof( token );

	token = COM_ParseExt( text, qfalse );
	if( token[0] == 0 )
	{
		ri.Printf( PRINT_ERROR, "WARNING: missing FREQUENCY waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->frequency = waveform_atof( token );
}

/*
===================
ParseTexMod
===================
*/
static void ParseTexMod( char *_text, shaderStage_t *stage, int cntBundle )
{
	const char   *token;
	char         **text = &_text;
	texModInfo_t *tmi;

	if( stage->bundle[cntBundle].numTexMods == TR_MAX_TEXMODS )
	{
		ri.Error( ERR_DROP, "ERROR: too many tcMod stages in shader '%s'\n", shader.name );
		return;
	}

	tmi = &stage->bundle[cntBundle].texMods[stage->bundle[cntBundle].numTexMods];
	stage->bundle[cntBundle].numTexMods++;

	token = COM_ParseExt( text, qfalse );

	if( !Q_stricmp( token, "turb" )) // turb
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = atof( token );
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = atof( token );
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = atof( token );
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = atof( token );

		tmi->type = TMOD_TURBULENT;
	}
	else if( !Q_stricmp( token, "scale" )) // scale
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[1] = atof( token );
		tmi->type = TMOD_SCALE;
	}
	else if( !Q_stricmp( token, "scroll" )) // scroll
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[0] = waveform_atof( token );
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[1] = waveform_atof( token );
		tmi->type = TMOD_SCROLL;
	}
	else if( !Q_stricmp( token, "stretch" )) // stretch
	{
		ParseWaveForm( text, &tmi->wave );
		tmi->type = TMOD_STRETCH;
	}
	else if( !Q_stricmp( token, "transform" )) // transform
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][1] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][1] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[1] = atof( token );

		tmi->type = TMOD_TRANSFORM;
	}
	else if( !Q_stricmp( token, "rotate" )) // rotate
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->rotateSpeed = waveform_atof( token );
		tmi->type = TMOD_ROTATE;
	}
	else if( !Q_stricmp( token, "offset" ))
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing offset parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[0] = waveform_atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing offset parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[1] = waveform_atof( token );

		tmi->type = TMOD_OFFSET;
	}
	else if( !Q_stricmp( token, "entityTranslate" )) // entityTranslate
	{
		tmi->type = TMOD_ENTITY_TRANSLATE;
	}
	else if( !Q_stricmp( token, "parallax" ))
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing rate parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->parallax[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing rate parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->parallax[1] = atof( token );

		tmi->type = TMOD_PARALLAX;
	}
	else if( !Q_stricmp( token, "macro" ))
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[0] = 1.0f / atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[1] = 1.0f / atof( token );
		shader.needsNormal = qtrue;
		tmi->type = TMOD_MACRO;
	}
	else
	{
		ri.Printf( PRINT_WARNING, "WARNING: unknown tcMod '%s' in shader '%s'\n", token, shader.name );
	}
}

/*
===================
ParseStage
===================
*/
static qboolean ParseStage( shaderStage_t *stage, char **text )
{
	char     *token;
	int      cntBundle = 0;
	int      depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0;
	qboolean depthMaskExplicit = qfalse;

	stage->active = qtrue;
	stage->noMipMaps = shader_noMipMaps;
	stage->noPicMip = shader_noPicMip;
	stage->force32bit = shader_force32bit;

	while( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no matching '}' found\n" );
			return qfalse;
		}

		if( token[0] == '}' )
		{
			break;
		}
		else if( !Q_stricmp( token, "nomipmaps" ))
		{
			stage->noMipMaps = qtrue;
		}
		else if( !Q_stricmp( token, "nopicmip" ))
		{
			stage->noPicMip = qtrue;
		}
		else if( !Q_stricmp( token, "force32bit" ))
		{
			stage->force32bit = qtrue;
		}
		else if( !Q_stricmp( token, "map" )) // map <name>
		{
			token = COM_ParseExt( text, qfalse );
			if( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'map' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			if( !Q_stricmp( token, "$whiteimage" ))
			{
				stage->bundle[cntBundle].image[0] = tr.whiteImage;
				continue;
			}
			else if( !Q_stricmp( token, "$lightmap" ))
			{
				stage->bundle[cntBundle].isLightmap = qtrue;
				if( shader.lightmapIndex < 0 )
				{
					stage->bundle[cntBundle].image[0] = tr.whiteImage;
				}
				else
				{
					stage->bundle[cntBundle].image[0] = tr.lightmaps[shader.lightmapIndex];
				}
				continue;
			}
			else
			{
				stage->bundle[cntBundle].image[0] = R_FindImageFile( token, !stage->noMipMaps, !stage->noPicMip, stage->force32bit, GL_REPEAT );
				if( !stage->bundle[cntBundle].image[0] )
				{
					ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
					return qfalse;
				}
			}
		}
		else if( !Q_stricmp( token, "clampmap" )) // clampmap <name>
		{
			token = COM_ParseExt( text, qfalse );
			if( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			stage->bundle[cntBundle].image[0] = R_FindImageFile( token, !stage->noMipMaps, !stage->noPicMip, stage->force32bit, GL_CLAMP );
			if( !stage->bundle[cntBundle].image[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
				return qfalse;
			}
		}
		else if( !Q_stricmp( token, "animMap" ) || !Q_stricmp( token, "animMapOnce" )) // animMap <frequency> <image1> .... <imageN>
		{
			shader.flags |= 1;

			if( !Q_stricmp( token, "animMapOnce" ))
				stage->bundle[cntBundle].flags |= 1;

			token = COM_ParseExt( text, qfalse );
			if( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'animMmap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}
			stage->bundle[cntBundle].imageAnimationSpeed = atof( token );

			// parse up to MAX_IMAGE_ANIMATIONS animations
			while( 1 )
			{
				int num;

				token = COM_ParseExt( text, qfalse );
				if( !token[0] )
				{
					break;
				}

				num = stage->bundle[cntBundle].numImageAnimations;
				if( num < MAX_IMAGE_ANIMATIONS )
				{
					stage->bundle[cntBundle].image[num] = R_FindImageFile( token, !stage->noMipMaps, !stage->noPicMip, stage->force32bit, GL_REPEAT );
					if( !stage->bundle[cntBundle].image[num] )
					{
						ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
						return qfalse;
					}
					stage->bundle[cntBundle].numImageAnimations++;
				}
				else
				{
					ri.Printf( PRINT_WARNING, "WARNING: mapanim - too many animated textures specified for shader '%s'\n", shader.name );
				}
			}
		}
		else if( !Q_stricmp( token, "alphaFunc" )) // alphafunc <func>
		{
			token = COM_ParseExt( text, qfalse );
			if( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			atestBits = NameToAFunc( token );
		}
		else if( !Q_stricmp( token, "depthfunc" )) // depthFunc <func>
		{
			token = COM_ParseExt( text, qfalse );

			if( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			if( !Q_stricmp( token, "lequal" ))
				depthFuncBits = 0;
			else if( !Q_stricmp( token, "equal" ))
				depthFuncBits = GLS_DEPTHFUNC_EQUAL;
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		else if( !Q_stricmp( token, "detail" )) // detail
		{
			stage->isDetail = qtrue;
		}
		else if( !Q_stricmp( token, "blendfunc" )) // blendfunc <srcFactor> <dstFactor> or blendfunc <add|filter|blend>
		{
			token = COM_ParseExt( text, qfalse );
			if( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
				continue;
			}

			// check for "simple" blends first
			if( !Q_stricmp( token, "add" ))
			{
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			}
			else if( !Q_stricmp( token, "filter" ))
			{
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			}
			else if( !Q_stricmp( token, "blend" ))
			{
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			}
			else
			{
				// complex double blends
				blendSrcBits = NameToSrcBlendMode( token );

				token = COM_ParseExt( text, qfalse );
				if( token[0] == 0 )
				{
					ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
					continue;
				}
				blendDstBits = NameToDstBlendMode( token );
			}

			// clear depth mask for blended surfaces
			if( !depthMaskExplicit )
			{
				depthMaskBits = 0;
			}
		}
		//
		// rgbGen
		//
		else if( !Q_stricmp( token, "rgbGen" ))
		{
			token = COM_ParseExt( text, qfalse );
			if( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for rgbGen in shader '%s'\n", shader.name );
				continue;
			}

			if( !Q_stricmp( token, "wave" ))
			{
				ParseWaveForm( text, &stage->rgbWave );
				stage->rgbGen = CGEN_WAVEFORM;
			}
			else if( !Q_stricmp( token, "colorwave" ))
			{
				vec3_t color;

				ParseVector( text, 3, color );
				VectorScale( color, 255, stage->constantColor );
				ParseWaveForm( text, &stage->rgbWave );
				stage->rgbGen = CGEN_MULTIPLY_BY_WAVEFORM;
			}
			else if( !Q_stricmp( token, "identity" ))
				stage->rgbGen = CGEN_IDENTITY;
			else if( !Q_stricmp( token, "identityLighting" ))
				stage->rgbGen = CGEN_IDENTITY_LIGHTING;
			else if( !Q_stricmp( token, "global" ))
				stage->rgbGen = CGEN_GLOBAL_COLOR;
			else if( !Q_stricmp( token, "entity" ) || !Q_stricmp( token, "fromentity" ))
				stage->rgbGen = CGEN_ENTITY;
			else if( !Q_stricmp( token, "oneMinusEntity" ))
				stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
			else if( !Q_stricmp( token, "vertex" ) || !Q_stricmp( token, "fromclient" ))
			{
				stage->rgbGen = CGEN_VERTEX;
				if( stage->alphaGen == 0 )
					stage->alphaGen = AGEN_VERTEX;
			}
			else if( !Q_stricmp( token, "exactVertex" ))
				stage->rgbGen = CGEN_EXACT_VERTEX;
			else if( !Q_stricmp( token, "lightingDiffuse" ))
				stage->rgbGen = CGEN_LIGHTING_DIFFUSE;
			else if( !Q_stricmp( token, "oneMinusVertex" ))
				stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
			else if( !Q_stricmp( token, "const" ) || !Q_stricmp( token, "constant" ))
			{
				vec3_t color;
				ParseVector( text, 3, color );
				VectorScale( color, 255, stage->constantColor );
				stage->rgbGen = CGEN_CONSTANT;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// alphaGen
		//
		else if( !Q_stricmp( token, "alphaGen" ))
		{
			token = COM_ParseExt( text, qfalse );
			if( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for alphaGen in shader '%s'\n", shader.name );
				continue;
			}

			if( !Q_stricmp( token, "wave" ))
			{
				ParseWaveForm( text, &stage->alphaWave );
				stage->alphaGen = AGEN_WAVEFORM;
			}
			else if( !Q_stricmp( token, "identity" ))
				stage->alphaGen = AGEN_IDENTITY;
			else if( !Q_stricmp( token, "global" ))
				stage->alphaGen = AGEN_GLOBAL_ALPHA;
			else if( !Q_stricmp( token, "entity" ) || !Q_stricmp( token, "fromentity" ))
				stage->alphaGen = AGEN_ENTITY;
			else if( !Q_stricmp( token, "oneMinusEntity" ))
				stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
			else if( !Q_stricmp( token, "vertex" ) || !Q_stricmp( token, "fromclient" ))
				stage->alphaGen = AGEN_VERTEX;
			else if( !Q_stricmp( token, "lightingSpecular" ))
			{
				shader.needsNormal = qtrue;
				stage->alphaMax = 255.0;
				stage->specOrigin[0] = -960.0;
				stage->specOrigin[1] = 1980.0;
				stage->specOrigin[2] = 96.0;
				stage->alphaGen = AGEN_LIGHTING_SPECULAR;

				token = COM_ParseExt( text, qfalse );
				if( token[0] )
				{
					stage->alphaMax = atof( token ) * 255.0f;
					ParseVector( text, 3, stage->specOrigin );
				}
			}
			else if( !Q_stricmp( token, "oneMinusVertex" ))
				stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
			else if( !Q_stricmp( token, "portal" ))
			{
				stage->alphaGen = AGEN_PORTAL;
				token = COM_ParseExt( text, qfalse );
				if( token[0] == 0 )
				{
					shader.portalRange = 256;
					ri.Printf( PRINT_WARNING, "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", shader.name );
				}
				else
				{
					shader.portalRange = atof( token );
				}
			}
			else if( !Q_stricmp( token, "dot" ))
			{
				stage->alphaGen = AGEN_DOT;
				shader.needsNormal = qtrue;
				stage->alphaMin = 0.0f;
				stage->alphaMax = 1.0f;

				token = COM_ParseExt( text, qfalse );
				if( token[0] )
				{
					stage->alphaMin = atof( token );
					token = COM_ParseExt( text, qfalse );
					if( token[0] )
						stage->alphaMax = atof( token );
				}
			}
			else if( !Q_stricmp( token, "oneMinusDot" ))
			{
				stage->alphaGen = AGEN_ONE_MINUS_DOT;
				shader.needsNormal = qtrue;
				stage->alphaMin = 0.0f;
				stage->alphaMax = 1.0f;

				token = COM_ParseExt( text, qfalse );
				if( token[0] )
				{
					stage->alphaMin = atof( token );
					token = COM_ParseExt( text, qfalse );
					if( token[0] )
						stage->alphaMax = atof( token );
				}
			}
			else if( !Q_stricmp( token, "const" ) || !Q_stricmp( token, "constant" ))
			{
				token = COM_ParseExt( text, qfalse );
				if( token[0] == 0 )
				{
					ri.Printf( PRINT_WARNING, "WARNING: no alphaGen constant specified in shader '%s'\n", shader.name );
					continue;
				}

				uint value = atof( token ) * 255;
				if( value < 256 )
				{
					stage->alphaGen = AGEN_CONST;
					stage->alphaConst = value;
				}
				else
				{
					ri.Printf( PRINT_WARNING, "WARNING: alphaGen constant %d out of [0..1] range in shader '%s'\n", value, shader.name );
				}

			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		else if( !Q_stricmp( token, "texgen" ) || !Q_stricmp( token, "tcGen" )) // tcGen <function>
		{
			token = COM_ParseExt( text, qfalse );
			if( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", shader.name );
				continue;
			}

			if( !Q_stricmp( token, "environment" ))
			{
				shader.needsNormal = qtrue;
				stage->bundle[cntBundle].tcGen = TCGEN_ENVIRONMENT_MAPPED;
			}
			else if( !Q_stricmp( token, "lightmap" ))
			{
				stage->bundle[cntBundle].tcGen = TCGEN_LIGHTMAP;
			}
			else if( !Q_stricmp( token, "texture" ) || !Q_stricmp( token, "base" ))
			{
				stage->bundle[cntBundle].tcGen = TCGEN_TEXTURE;
			}
			else if( !Q_stricmp( token, "vector" ))
			{
				ParseVector( text, 3, stage->bundle[0].tcGenVectors[0] );
				ParseVector( text, 3, stage->bundle[0].tcGenVectors[1] );

				stage->bundle[cntBundle].tcGen = TCGEN_VECTOR;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown texgen parm in shader '%s'\n", shader.name );
			}
		}
		else if( !Q_stricmp( token, "tcMod" )) // tcMod <type> <...>
		{
			char buffer[1024] = "";

			while( 1 )
			{
				token = COM_ParseExt( text, qfalse );
				if( token[0] == 0 )
					break;
				strcat( buffer, token );
				strcat( buffer, " " );
			}

			ParseTexMod( buffer, stage, cntBundle );

			continue;
		}
		else if( !Q_stricmp( token, "depthwrite" ) || !Q_stricmp( token, "depthmask" ))
		{
			depthMaskBits = GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = qtrue;
		}
		else if( !Q_stricmp( token, "noDepthTest" ))
		{
			depthFuncBits = GLS_DEPTHTEST_DISABLE;
		}
		else if( !Q_stricmp( token, "nextBundle" ))
		{
			if( !qglActiveTextureARB )
			{
				ri.Printf( PRINT_WARNING, "WARNING: nextBundle on a card without multitexturing in shader '%s'\n", shader.name );
				return qfalse;
			}

			// ???
			stage->multitextureEnv = stage->multitextureEnv & 0xfffffffcU | 1;

			cntBundle++;
			if( cntBundle >= NUM_TEXTURE_BUNDLES )
			{
				ri.Printf( PRINT_WARNING, "WARNING: too many nextBundle commands in shader '%s'\n", shader.name );
				return qfalse;
			}
		}
		else if( !Q_stricmp( token, "frameFromEntity" ))
		{
			stage->bundle[cntBundle].frameFromEntity = qtrue;
		}
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	//
	// if cgen isn't explicitly specified, use either identity or identitylighting
	//
	if( stage->rgbGen == CGEN_BAD )
	{
		if( cntBundle || stage->bundle[0].isLightmap )
		{
			stage->rgbGen = CGEN_IDENTITY;
		}
		else if( blendSrcBits == 0 || blendSrcBits == GLS_SRCBLEND_ONE || blendSrcBits == GLS_SRCBLEND_SRC_ALPHA )
		{
			stage->rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		else
		{
			stage->rgbGen = CGEN_IDENTITY;
		}
	}


	//
	// implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
	//
	if( blendSrcBits == GLS_SRCBLEND_ONE && blendDstBits == GLS_DSTBLEND_ZERO )
	{
		blendSrcBits = 0;
		blendDstBits = 0;
		depthMaskBits = GLS_DEPTHMASK_TRUE;
	}

	if( depthFuncBits == GLS_DEPTHTEST_DISABLE )
		depthMaskBits = 0;

	// decide which agens we can skip

	// was alphaGen == AGEN_IDENTITY which turned into alphaGen == AGEN_SKIP in decompile
	// which doesn't make sense
	// changed to alphaGen == AGEN_IDENTITY
	if( stage->alphaGen == AGEN_IDENTITY && ( stage->rgbGen == CGEN_IDENTITY || stage->rgbGen == CGEN_LIGHTING_DIFFUSE ))
	{
		stage->alphaGen = AGEN_SKIP;
	}

	//
	// compute state bits
	//
	stage->stateBits = depthMaskBits
			   | blendSrcBits | blendDstBits
			   | atestBits
			   | depthFuncBits;

	return qtrue;
}

/*
===============
ParseDeform

deformVertexes wave <spread> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes normal <frequency> <amplitude>
deformVertexes move <vector> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes bulge <bulgeWidth> <bulgeHeight> <bulgeSpeed>
deformVertexes projectionShadow
deformVertexes autoSprite
deformVertexes autoSprite2
deformVertexes text[0-7]
===============
*/
static void ParseDeform( char **text )
{
	deformStage_t *ds;
	char *token;

	token = COM_ParseExt( text, qfalse );
	if( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", shader.name );
		return;
	}

	if( shader.numDeforms == MAX_SHADER_DEFORMS )
	{
		ri.Printf( PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader.name );
		return;
	}

	ds = &shader.deforms[ shader.numDeforms ];
	shader.numDeforms++;

	if( !Q_stricmp( token, "projectionShadow" ))
	{
		ds->deformation = DEFORM_PROJECTION_SHADOW;
		return;
	}

	if( !Q_stricmp( token, "autosprite" ))
	{
		ds->deformation = DEFORM_AUTOSPRITE;
		return;
	}

	if( !Q_stricmp( token, "autosprite2" ))
	{
		ds->deformation = DEFORM_AUTOSPRITE2;
		return;
	}

	if( !Q_stricmpn( token, "text", 4 ))
	{
		int n;

		n = token[4] - '0';
		if( n < 0 || n > 7 )
			n = 0;

		ds->deformation = DEFORM_TEXT0 + n;
		shader.needsNormal = qtrue;
		return;
	}

	if( !Q_stricmp( token, "bulge" ))
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeWidth = atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeHeight = atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeSpeed = atof( token );

		ds->deformation = DEFORM_BULGE;
		shader.needsNormal = qtrue;
		return;
	}

	if( !Q_stricmp( token, "wave" ))
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}

		if( atof( token ) != 0 )
		{
			ds->deformationSpread = 1.0f / atof( token );
		}
		else
		{
			ds->deformationSpread = 100.0f;
			ri.Printf( PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_WAVE;
		shader.needsNormal = qtrue;
		return;
	}

	if( !Q_stricmp( token, "normal" ))
	{
		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.amplitude = atof( token );

		token = COM_ParseExt( text, qfalse );
		if( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.frequency = atof( token );

		ds->deformation = DEFORM_NORMALS;
		shader.needsNormal = qtrue;
		return;
	}

	if( !Q_stricmp( token, "move" ))
	{
		ParseVector( text, 3, ds->moveVector );
		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_MOVE;
		return;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader.name );
}

/*
===============
ParseSkyParms

skyParms <outerbox> <cloudheight> <innerbox>
===============
*/
static void ParseSkyParms( char **text )
{
	static const char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
	char *token;
	char pathname[MAX_QPATH];
	int  i;

	// outerbox
	token = COM_ParseExt( text, qfalse );
	if( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}

	if( strcmp( token, "-" ))
	{
		for( i = 0; i < 6; i++ )
		{
			Com_sprintf( pathname, sizeof( pathname ), "%s_%s.tga", token, suf[i] );
			shader.sky.outerbox[i] = R_FindImageFile( pathname, qtrue, qtrue, shader_force32bit, GL_CLAMP );
			if( !shader.sky.outerbox[i] )
				shader.sky.outerbox[i] = tr.defaultImage;
		}
	}

	// cloudheight
	token = COM_ParseExt( text, qfalse );
	if( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	shader.sky.cloudHeight = atof( token );
	if( !shader.sky.cloudHeight )
		shader.sky.cloudHeight = 512;
	R_InitSkyTexCoords( shader.sky.cloudHeight );

	// innerbox
	token = COM_ParseExt( text, qfalse );
	if( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if( strcmp( token, "-" ))
	{
		for( i = 0; i < 6; i++ )
		{
			Com_sprintf( pathname, sizeof( pathname ), "%s_%s.tga", token, suf[i] );
			shader.sky.innerbox[i] = R_FindImageFile( pathname, qtrue, qtrue, shader_force32bit, GL_CLAMP );
			if( !shader.sky.innerbox[i] )
				shader.sky.innerbox[i] = tr.defaultImage;
		}
	}

	shader.isSky = qtrue;
}

/*
=================
ParseSort
=================
*/
void ParseSort( char **text )
{
	char *token;

	token = COM_ParseExt( text, qfalse );
	if( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", shader.name );
		return;
	}

	if( !Q_stricmp( token, "portal" ))
		shader.sort = SS_PORTAL;
	else if( !Q_stricmp( token, "sky" ))
		shader.sort = SS_ENVIRONMENT;
	else if( !Q_stricmp( token, "opaque" ))
		shader.sort = SS_OPAQUE;
	else if( !Q_stricmp( token, "decal" ))
		shader.sort = SS_DECAL;
	else if( !Q_stricmp( token, "seeThrough" ))
		shader.sort = SS_SEE_THROUGH;
	else if( !Q_stricmp( token, "banner" ))
		shader.sort = SS_BANNER;
	else if( !Q_stricmp( token, "additive" ))
		shader.sort = SS_BLEND1;
	else if( !Q_stricmp( token, "nearest" ))
		shader.sort = SS_NEAREST;
	else if( !Q_stricmp( token, "underwater" ))
		shader.sort = SS_UNDERWATER;
	else
		shader.sort = atof( token );
}

// this table is also present in q3map

typedef struct
{
	char *name;
	int  clearSolid, surfaceFlags, contents;
} infoParm_t;

infoParm_t infoParms[] = {
	// server relevant contents
	{"water", 1, 0, CONTENTS_WATER },
	{"slime", 1, 0, CONTENTS_SLIME }, // mildly damaging
	{"lava", 1, 0, CONTENTS_LAVA },   // very damaging
	{"playerclip", 1, 0, CONTENTS_PLAYERCLIP },
	{"monsterclip", 1, 0, CONTENTS_MONSTERCLIP },
	{"cameraclip", 1, 0, CONTENTS_CAMERACLIP },
	{"weaponclip", 1, 0, CONTENTS_WEAPONCLIP },
	{"nodrop", 1, 0, CONTENTS_NODROP }, // don't drop items or leave bodies (death fog, lava, etc)
	{"nonsolid", 1, SURF_NONSOLID, 0},  // clears the solid flag

	// utility relevant attributes
	{"origin", 1, 0, CONTENTS_ORIGIN },         // center of rotating brushes
	{"trans", 0, 0, CONTENTS_TRANSLUCENT },     // don't eat contained surfaces
	{"detail", 0, 0, CONTENTS_DETAIL },         // don't include in structural bsp
	{"structural", 0, 0, CONTENTS_STRUCTURAL }, // force into structural bsp even if trnas
	{"areaportal", 1, 0, CONTENTS_AREAPORTAL }, // divides areas

	{"fog", 1, 0, CONTENTS_FOG},              // carves surfaces entering
	{"sky", 0, SURF_SKY, 0 },                 // emit light from an environment map
	{"alphashadow", 0, SURF_ALPHASHADOW, 0 }, // test light on a per-pixel basis

	// server attributes
	{"slick", 0, SURF_SLICK, 0 },
	{"noimpact", 0, SURF_NOIMPACT, 0 }, // don't make impact explosions or marks
	{"nomarks", 0, SURF_NOMARKS, 0 },   // don't make impact marks, but still explode
	{"ladder", 0, SURF_LADDER, 0 },
	{"nodamage", 0, SURF_NODAMAGE, 0 },
	{"nosteps", 0, SURF_NOSTEPS, 0 },

	{"wood", 0, SURF_TYPE_WOOD, 0 },
	{"metal", 0, SURF_TYPE_METAL, 0 },
	{"rock", 0, SURF_TYPE_ROCK, 0 },
	{"dirt", 0, SURF_TYPE_DIRT, 0 },
	{"grill", 0, SURF_TYPE_GRILL, 0 },
	{"organic", 0, SURF_TYPE_ORGANIC, 0 },

	// drawsurf attributes
	{"nodraw", 0, SURF_NODRAW, 0 }, // don't generate a drawsurface (or a lightmap)
	{"castshadow", 0, SURF_CASTSHADOW, 0 },
	{"nolightmap", 0, SURF_NOLIGHTMAP, 0 }, // don't generate a lightmap
	{"nodlight", 0, SURF_NODLIGHT, 0 },     // don't ever add dynamic lights
	{"hint", 0, SURF_HINT, 0},
};


/*
===============
ParseSurfaceParm

surfaceparm <name>
===============
*/
static void ParseSurfaceParm( char **text, uint *surfaceFlags, uint *contentFlags )
{
	char      *token;
	const int numInfoParms = sizeof( infoParms ) / sizeof( infoParms[0] );

	token = COM_ParseExt( text, qfalse );
	for( int i = 0; i < numInfoParms; i++ )
	{
		if( !Q_stricmp( token, infoParms[i].name ))
		{
			*surfaceFlags |= infoParms[i].surfaceFlags;
			*contentFlags |= infoParms[i].contents;

			if( infoParms[i].clearSolid )
			{
				*contentFlags &= ~CONTENTS_SOLID;
			}

			break;
		}
	}
}

/*
=================
ParseShader

The current text pointer is at the explicit text definition of the
shader.  Parse it into the global shader variable.  Later functions
will optimize it.
=================
*/
static qboolean ParseShader( char **text )
{
	char *token;
	int  s;
	int  depth;

	s = 0;
	depth = 0;

	token = COM_ParseExt( text, qtrue );
	if( token[0] != '{' )
	{
		ri.Printf( PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, shader.name );
		return qfalse;
	}

	while( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", shader.name );
			return qfalse;
		}

		if( token[0] == '}' ) // end of shader definition
		{
			break;
		}
		else if( token[0] == '{' ) // stage definition
		{
			if( !ParseStage( &unfoggedStages[s], text ))
			{
				return qfalse;
			}
			unfoggedStages[s].active = qtrue;

			if( s > 0 )
			{
				if( !FBitSet( unfoggedStages[s].stateBits, GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ))
					ri.Printf( PRINT_WARNING, "WARNING: shader '%s' has opaque maps defined after stage 0!!!\n", shader.name );

				if( FBitSet( unfoggedStages[s].stateBits, GLS_DEPTHMASK_TRUE ))
					ri.Printf( PRINT_WARNING, "WARNING: shader '%s' has depthmask enabled after stage 0!!!\n", shader.name );
			}

			s++;
			continue;
		}
		else if( !Q_stricmpn( token, "qer", 3 )) // skip stuff that only the QuakeEdRadient needs
		{
			SkipRestOfLine( text );
			continue;
		}
		else if( !Q_stricmp( token, "surfaceParm" )
			 || !Q_stricmp( token, "surfaceLight" )
			 || !Q_stricmp( token, "surfaceColor" )
			 || !Q_stricmp( token, "surfaceAngle" )
			 || !Q_stricmp( token, "surfaceDensity" ))
		{
			if( !Q_stricmp( token, "surfaceParm" ))
			{
				token = COM_ParseExt( text, qfalse );
				ParseSurfaceParm( text, &shader.surfaceFlags, &shader.contentFlags );
			}
			else
			{
				SkipRestOfLine( text );
			}
			continue;
		}
		else if( !Q_stricmp( token, "q3map_sun" )) // sun parms
		{
			float a, b;

			token = COM_ParseExt( text, qfalse );
			tr.sunLight[0] = atof( token );
			token = COM_ParseExt( text, qfalse );
			tr.sunLight[1] = atof( token );
			token = COM_ParseExt( text, qfalse );
			tr.sunLight[2] = atof( token );

			VectorNormalize( tr.sunLight );

			token = COM_ParseExt( text, qfalse );
			a = atof( token );
			VectorScale( tr.sunLight, a, tr.sunLight );

			token = COM_ParseExt( text, qfalse );
			a = atof( token );
			a = a / 180 * M_PI;

			token = COM_ParseExt( text, qfalse );
			b = atof( token );
			b = b / 180 * M_PI;

			tr.sunDirection[0] = cos( a ) * cos( b );
			tr.sunDirection[1] = sin( a ) * cos( b );
			tr.sunDirection[2] = sin( b );
		}
		else if( !Q_stricmpn( token, "q3map", 5 )) // skip stuff that only the q3map needs
		{
			SkipRestOfLine( text );
			continue;
		}
		else if( !Q_stricmp( token, "deformVertexes" ))
		{
			ParseDeform( text );
			continue;
		}
		else if( !Q_stricmp( token, "tesssize" ))
		{
			SkipRestOfLine( text );
			continue;
		}
		else if( !Q_stricmp( token, "subdivisions" ))
		{
			token = COM_ParseExt( text, qfalse );
			if( token[0] == 0 )
			{
				ri.Printf( PRINT_ERROR, "WARNING: missing subdivisions parms in shader '%s'\n", shader.name );
				continue;
			}

			shader.subdivisions = atof( token );
		}
		else if( !Q_stricmp( token, "nomipmaps" )) // no mip maps
		{
			shader_noMipMaps = qtrue;
			continue;
		}
		else if( !Q_stricmp( token, "nopicmip" )) // no picmip adjustment
		{
			shader_noPicMip = qtrue;
			continue;
		}
		else if( !Q_stricmp( token, "force32bit" )) // force loading as 32-bit texture
		{
			shader_force32bit = qtrue;
			continue;
		}
		else if( !Q_stricmp( token, "polygonOffset" ))
		{
			shader.polygonOffset = qtrue;
			continue;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if( !Q_stricmp( token, "entityMergable" ))
		{
			shader.entityMergable = qtrue;
			continue;
		}
		else if( !Q_stricmp( token, "fogParms" )) // fogParms
		{
			if( !ParseVector( text, 3, shader.fogParms.color ))
			{
				return qfalse;
			}

			token = COM_ParseExt( text, qfalse );
			if( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing depth for opaque parm for 'fogParms' keyword in shader '%s'\n", shader.name );
				continue;
			}
			shader.fogParms.depthForOpaque = atof( token );

			// skip any old gradient directions
			SkipRestOfLine( text );
			continue;
		}
		else if( !Q_stricmp( token, "foggen" ))
		{
			ParseWaveForm( text, &shader.fogGenWaveForm );
			continue;
		}
		else if( !Q_stricmp( token, "portal" )) // portal
		{
			shader.sort = SS_PORTAL;
			continue;
		}
		else if( !Q_stricmp( token, "skyparms" )) // skyparms <cloudheight> <outerbox> <innerbox>
		{
			ParseSkyParms( text );
			continue;
		}
		else if( !Q_stricmp( token, "portalsky" ))
		{
			shader.sort = SS_PORTALSKY;
			shader.isPortalSky = qtrue;
			continue;
		}
		else if( !Q_stricmp( token, "light" )) // light <value> determines flaring in q3map, not needed here
		{
			token = COM_ParseExt( text, qfalse );
			continue;
		}
		else if( !Q_stricmp( token, "spritegen" ))
		{
			token = COM_ParseExt( text, qfalse );
			if( token[0] == 0 )
			{
				ri.Printf( PRINT_ERROR, "WARNING: missing spritegen parm in shader '%s'\n", shader.name );
				continue;
			}

			if( !Q_stricmp( token, "parallel" ))
				shader.sprite.type = SPRITE_PARALLEL;
			else if( !Q_stricmp( token, "parallel_oriented" ))
				shader.sprite.type = SPRITE_PARALLEL_ORIENTED;
			else if( !Q_stricmp( token, "parallel_upright" ))
				shader.sprite.type = SPRITE_PARALLEL_UPRIGHT;
			else if( !Q_stricmp( token, "oriented" ))
				shader.sprite.type = SPRITE_ORIENTED;
			else
				// a1ba: added by me
				ri.Printf( PRINT_ERROR, "WARNING: unknown spritegen parm '%s' in shader '%s'\n", token, shader.name );

			shader.sprite.scale = 1.0f;
			continue;
		}
		else if( !Q_stricmp( token, "spritescale" ))
		{
			token = COM_ParseExt( text, qfalse );
			if( token[0] == 0 )
			{
				ri.Printf( PRINT_ERROR, "WARNING: missing spritescale parm in shader '%s'\n", shader.name );
				continue;
			}

			shader.sprite.scale = atof( token );
			continue;
		}
		else if( !Q_stricmp( token, "cull" )) // cull <face>
		{
			token = COM_ParseExt( text, qfalse );
			if( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", shader.name );
				continue;
			}

			if( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twosided" ) || !Q_stricmp( token, "disable" ))
			{
				shader.cullType = CT_TWO_SIDED;
			}
			else if( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ))
			{
				shader.cullType = CT_BACK_SIDED;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token, shader.name );
			}
			continue;
		}
		else if( !Q_stricmp( token, "fogonly" ))
		{
			fogOnly = qtrue;
			unfoggedStages[0].bundle[0].image[0] = tr.whiteImage;
			unfoggedStages[0].active = qtrue;
			unfoggedStages[0].stateBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA;
			continue;
		}
		else if( !Q_stricmp( token, "sort" )) // sort
		{
			ParseSort( text );
			continue;
		}
		else if( !Q_stricmp( token, "if" ))
		{
			// not accurate, decompiled code is garbage
			qboolean expression_result = qfalse;
			int      skip_depth = 1;

			depth++;

			token = COM_ParseExt( text, qtrue );

			if( !Q_stricmp( token, "mtex" ))
			{
				expression_result = qfalse; // qglActiveTextureARB != 0;
			}
			else if( !Q_stricmp( token, "no_mtex" ))
			{
				expression_result = qtrue; // qglActiveTextureARB == 0;
			}
			else if( !Q_stricmp( token, "1" ))
			{
				expression_result = qtrue;
			}
			else if( !Q_stricmp( token, "0" ))
			{
				expression_result = qfalse;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: invalid if argument '%s' in shader '%s', not passing\n", token, shader.name );
				expression_result = qfalse;
			}

			if( !expression_result )
			{
				while( skip_depth > 0 )
				{
					SkipRestOfLine( text );
					token = COM_ParseExt( text, qtrue );

					if( token[0] == 0 )
					{
						ri.Printf( PRINT_WARNING, "WARNING: no matching endif in shader '%s'\n", shader.name );
						break;
					}
					else if( !Q_stricmp( token, "if" ))
					{
						skip_depth++;
					}
					else if( !Q_stricmp( token, "endif" ))
					{
						skip_depth--;
					}
				}
			}

			continue;
		}
		else if( !Q_stricmp( token, "endif" ))
		{
			depth--;

			if( depth < 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: no matching endif in shader '%s'\n", shader.name );
			}
			continue;
		}
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown general shader parameter '%s' in '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	//
	// ignore shaders that don't have any stages, unless it is a sky or fog
	//
	if( s == 0 && !shader.isSky && !fogOnly
	    && !FBitSet( shader.contentFlags, CONTENTS_FOG ) && !FBitSet( shader.surfaceFlags, SURF_NODRAW ))
	{
		return qfalse;
	}

	shader.explicitlyDefined = qtrue;

	if( s != 0 && fogOnly )
		ri.Printf( PRINT_ERROR, "WARNING: shader '%s' is marked as fogonly in addition to having regular stages\n", shader.name );

	return qtrue;
}

/*
========================================================================================

SHADER OPTIMIZATION AND FOGGING

========================================================================================
*/

/*
===================
ComputeStageIteratorFunc

See if we can use on of the simple fastpath stage functions,
otherwise set to the generic stage function
===================
*/
static void ComputeStageIteratorFunc( void )
{
	shader.optimalStageIteratorFunc = RB_StageIteratorGeneric;

	//
	// see if this should go into the sky path
	//
	if( shader.isSky )
	{
		shader.optimalStageIteratorFunc = RB_StageIteratorSky;
		goto done;
	}

	if( r_ignoreFastPath->integer )
	{
		return;
	}

	//
	// see if this can go into the vertex lit fast path
	//
	if( shader.numUnfoggedPasses == 1 )
	{
		if( unfoggedStages[0].rgbGen == CGEN_LIGHTING_DIFFUSE )
		{
			if( unfoggedStages[0].alphaGen == AGEN_IDENTITY )
			{
				if( unfoggedStages[0].bundle[0].tcGen == TCGEN_TEXTURE )
				{
					if( !shader.polygonOffset )
					{
						if( !unfoggedStages[0].multitextureEnv )
						{
							if( !shader.numDeforms )
							{
								shader.optimalStageIteratorFunc = RB_StageIteratorVertexLitTexture;
								goto done;
							}
						}
					}
				}
			}
		}
	}

	//
	// see if this can go into an optimized LM, multitextured path
	//
	if( shader.numUnfoggedPasses == 1 )
	{
		if(( unfoggedStages[0].rgbGen == CGEN_IDENTITY ) && ( unfoggedStages[0].alphaGen == AGEN_IDENTITY ))
		{
			if( unfoggedStages[0].bundle[0].tcGen == TCGEN_TEXTURE
			    && unfoggedStages[0].bundle[1].tcGen == TCGEN_LIGHTMAP )
			{
				if( !shader.polygonOffset )
				{
					if( !shader.numDeforms )
					{
						if( unfoggedStages[0].multitextureEnv )
						{
							shader.optimalStageIteratorFunc = RB_StageIteratorLightmappedMultitexture;
							goto done;
						}
					}
				}
			}
		}
	}

done:
	return;
}

typedef struct
{
	int stateBits;
	int multitextureEnv;
} collapse_t;

static collapse_t collapse[] =
{
	{ GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO, MT_ENV_MODULATE },
	{ GLS_SRCBLEND_ZERO | GLS_DSTBLEND_SRC_COLOR, MT_ENV_MODULATE },
	{ GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE, MT_ENV_ADD },
	{ 0, MT_ENV_NONE },
};

/*
================
CollapseMultitexture

Attempt to combine two stages into a single multitexture stage
FIXME: I think modulated add + modulated add collapses incorrectly
=================
*/
static qboolean CollapseMultitexture( int *stagecounter )
{
	int    iUseCollapse;
	int    stagenum;
	int    abits, bbits;
	shaderStage_t *stage;
	textureBundle_t tmpBundle;
	vec4_t color_a;
	vec4_t color_b;

	for( stagenum = 0; stagenum < *stagecounter - 1; stagenum++ )
	{
		stage = &unfoggedStages[stagenum];

		if( !stage[0].active || !stage[1].active )
			continue;

		if( stage->multitextureEnv )
			continue;

		// on voodoo2, don't combine different tmus
		if( glConfig.driverType == GLDRV_VOODOO )
		{
			if( stage[0].bundle[0].image[0]->TMU != stage[1].bundle[0].image[0]->TMU )
				continue;
		}

		abits = stage[0].stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS );
		bbits = stage[1].stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS );

		if( abits == GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO )
			abits = 0;

		// make sure that both stages have identical state other than blend modes
		if( !(((( stage->stateBits & ~( GLS_DEPTHMASK_TRUE | GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS )) == ( stage[1].stateBits & ~( GLS_DEPTHMASK_TRUE | GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS )))
		       && (( abits == bbits || ( abits == 0 )))) && ( qtrue )))
			continue;

		// search for a valid multitexture blend function
		for( iUseCollapse = 0; collapse[iUseCollapse].stateBits != 0; iUseCollapse++ )
		{
			if( bbits == collapse[iUseCollapse].stateBits )
				break;
		}

		// nothing found
		if( collapse[iUseCollapse].stateBits == 0 )
			continue;

		// GL_ADD is a separate extension
		if( collapse[iUseCollapse].multitextureEnv == MT_ENV_ADD && !glConfig.textureEnvAddAvailable )
			continue;

		// make sure waveforms have identical parameters
		if(( stage[0].rgbGen != stage[1].rgbGen ) || ( stage[0].alphaGen != stage[1].alphaGen ))
			continue;

		if( stage[0].rgbGen - CGEN_WAVEFORM < 2 )
		{
			if( memcmp( &stage[0].rgbWave, &stage[1].rgbWave, sizeof( stage[0].rgbWave )))
				continue;
		}

		if( stage[0].alphaGen == AGEN_WAVEFORM )
		{
			if( memcmp( &stage[0].alphaWave, &stage[1].alphaWave, sizeof( stage[0].alphaWave )))
				continue;
		}

		// set the new blend state bits
		stage->multitextureEnv = collapse[iUseCollapse].multitextureEnv;

		for( int i = 0; i < 4; i++ )
			color_a[i] = ((float)stage[0].constantColor[i] ) / 255.0f;

		for( int i = 0; i < 4; i++ )
			color_b[i] = ((float)stage[1].constantColor[i] ) / 255.0f;

		if( stage->multitextureEnv == MT_ENV_MODULATE )
		{
			for( int i = 0; i < 4; i++ )
				color_a[i] *= color_b[i];
		}
		else if( stage->multitextureEnv == MT_ENV_ADD )
		{
			for( int i = 0; i < 4; i++ )
			{
				float val = color_a[i] + color_b[i];
				color_a[i] = Q_min( val, 1.0f );
			}
		}
		else
		{
			ri.Printf( 3, "Unknown MT mode collapsing multitexture in '%s'\n", shader.name );
		}

		for( int i = 0; i < 4; i++ )
			stage->constantColor[i] = color_a[i] * 255.0f;

		// something something 3dfx
		if( stage[0].bundle[0].isLightmap )
		{
			tmpBundle = stage[0].bundle[0];
			stage[0].bundle[0] = stage[1].bundle[0];
			stage[0].bundle[1] = tmpBundle;
		}
		else
			stage[0].bundle[1] = stage[1].bundle[0];

		//
		// move down subsequent shaders
		//
		if( stagenum + 2 < MAX_SHADER_STAGES )
			memmove( &stage[1], &stage[2], sizeof( stage[0] ) * ( MAX_SHADER_STAGES - 2 - stagenum ));

		Com_Memset( &unfoggedStages[MAX_SHADER_STAGES - 1], 0, sizeof( unfoggedStages[0] ));
		( *stagecounter )--;
	}

	return qtrue;
}

/*
==============
SortNewShader

Positions the most recently created shader in the tr.sortedShaders[]
array so that the shader->sort key is sorted reletive to the other
shaders.

Sets shader->sortedIndex
==============
*/
static void SortNewShader( void )
{
	int      i;
	float    sort;
	shader_t *newShader;

	newShader = tr.shaders[ tr.numShaders - 1 ];
	sort = newShader->sort;

	for( i = tr.numShaders - 2; i >= 0; i-- )
	{
		if( tr.sortedShaders[i]->sort <= sort )
		{
			break;
		}
		tr.sortedShaders[i + 1] = tr.sortedShaders[i];
		tr.sortedShaders[i + 1]->sortedIndex++;
	}

	newShader->sortedIndex = i + 1;
	tr.sortedShaders[i + 1] = newShader;
}

/*
====================
GeneratePermanentShader
====================
*/
static shader_t *GeneratePermanentShader( void )
{
	shader_t *newShader;
	int      i, b;
	int      size, hash;

	if( tr.numShaders == MAX_SHADERS )
	{
		ri.Printf( PRINT_WARNING, "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n" );
		currentShader->shader = tr.defaultShader;
		return tr.defaultShader;
	}

	newShader = ri.Hunk_Alloc( sizeof( shader_t ), h_low );

	*newShader = shader;

	tr.shaders[ tr.numShaders ] = newShader;
	newShader->index = tr.numShaders;

	tr.sortedShaders[ tr.numShaders ] = newShader;
	newShader->sortedIndex = tr.numShaders;

	tr.numShaders++;

	for( i = 0; i < newShader->numUnfoggedPasses; i++ )
	{
		if( !unfoggedStages[i].active )
			break;

		newShader->stages[i] = ri.Hunk_Alloc( sizeof( unfoggedStages[i] ), h_low );
		*newShader->stages[i] = unfoggedStages[i];

		for( b = 0; b < NUM_TEXTURE_BUNDLES; b++ )
		{
			size = newShader->stages[i]->bundle[b].numTexMods * sizeof( texModInfo_t );
			if( size != 0 )
			{
				newShader->stages[i]->bundle[b].texMods = ri.Hunk_Alloc( size, h_low );
				Com_Memcpy( newShader->stages[i]->bundle[b].texMods, unfoggedStages[i].bundle[b].texMods, size );
			}
		}
	}
	newShader->numUnfoggedPasses = i;

	for( i = 0; i < newShader->numFoggedPasses; i++ )
	{
		if( !foggedStages[i].active )
			break;

		newShader->foggedStages[i] = ri.Hunk_Alloc( sizeof( foggedStages[i] ), h_low );
		*newShader->foggedStages[i] = foggedStages[i];

		for( b = 0; b < NUM_TEXTURE_BUNDLES; b++ )
		{
			size = newShader->foggedStages[i]->bundle[b].numTexMods * sizeof( texModInfo_t );
			if( size != 0 )
			{
				newShader->foggedStages[i]->bundle[b].texMods = ri.Hunk_Alloc( size, h_low );
				Com_Memcpy( newShader->foggedStages[i]->bundle[b].texMods, foggedStages[i].bundle[b].texMods, size );
			}
		}
	}
	newShader->numFoggedPasses = i;

	for( i = 0; i < newShader->numAlphaFoggedPasses; i++ )
	{
		if( !alphaFoggedStages[i].active )
			break;

		newShader->alphaFoggedStages[i] = ri.Hunk_Alloc( sizeof( newShader->alphaFoggedStages[i] ), h_low );
		*newShader->alphaFoggedStages[i] = alphaFoggedStages[i];

		for( b = 0; b < NUM_TEXTURE_BUNDLES; b++ )
		{
			size = newShader->alphaFoggedStages[i]->bundle[b].numTexMods * sizeof( texModInfo_t );
			if( size != 0 )
			{
				newShader->alphaFoggedStages[i]->bundle[b].texMods = ri.Hunk_Alloc( size, h_low );
				Com_Memcpy( newShader->alphaFoggedStages[i]->bundle[b].texMods, alphaFoggedStages[i].bundle[b].texMods, size );
			}
		}
	}
	newShader->numAlphaFoggedPasses = i;

	SortNewShader();

	return newShader;
}

/*
=================
VertexLightingCollapse

If vertex lighting is enabled, only render a single
pass, trying to guess which is the correct one to best aproximate
what it is supposed to look like.
=================
*/
static void VertexLightingCollapse( void )
{
	int stage;
	shaderStage_t *bestStage;
	int bestImageRank;
	int rank;

	// if we aren't opaque, just use the first pass
	if( shader.sort == SS_OPAQUE )
	{
		// pick the best texture for the single pass
		bestStage = &unfoggedStages[0];
		bestImageRank = -999999;

		for( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
		{
			shaderStage_t *pStage = &unfoggedStages[stage];

			if( !pStage->active )
				break;

			rank = 0;

			if( pStage->bundle[0].isLightmap )
				rank -= 100;

			if( pStage->bundle[0].tcGen != TCGEN_TEXTURE )
				rank -= 5;

			if( pStage->bundle[0].numTexMods )
				rank -= 5;

			if( pStage->rgbGen != CGEN_IDENTITY )
				rank -= 3;

			if( rank > bestImageRank )
			{
				bestImageRank = rank;
				bestStage = pStage;
			}
		}

		unfoggedStages[0].bundle[0] = bestStage->bundle[0];
		unfoggedStages[0].stateBits &= ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
		unfoggedStages[0].stateBits |= GLS_DEPTHMASK_TRUE;

		if( shader.lightmapIndex == LIGHTMAP_NONE )
			unfoggedStages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		else
			unfoggedStages[0].rgbGen = CGEN_EXACT_VERTEX;

		unfoggedStages[0].alphaGen = AGEN_SKIP;
	}
	else
	{
		// don't use a lightmap (tesla coils)
		if( unfoggedStages[0].bundle[0].isLightmap )
			unfoggedStages[0] = unfoggedStages[1];

		// if we were in a cross-fade cgen, hack it to normal
		if( unfoggedStages[0].rgbGen == CGEN_ONE_MINUS_ENTITY || unfoggedStages[1].rgbGen == CGEN_ONE_MINUS_ENTITY )
			unfoggedStages[0].rgbGen = CGEN_IDENTITY_LIGHTING;

		if(( unfoggedStages[0].rgbGen == CGEN_WAVEFORM && unfoggedStages[0].rgbWave.func == GF_SAWTOOTH )
		   && ( unfoggedStages[1].rgbGen == CGEN_WAVEFORM && unfoggedStages[1].rgbWave.func == GF_INVERSE_SAWTOOTH ))
		{
			unfoggedStages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}

		if(( unfoggedStages[0].rgbGen == CGEN_WAVEFORM && unfoggedStages[0].rgbWave.func == GF_INVERSE_SAWTOOTH )
		   && ( unfoggedStages[1].rgbGen == CGEN_WAVEFORM && unfoggedStages[1].rgbWave.func == GF_SAWTOOTH ))
		{
			unfoggedStages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
	}

	for( stage = 1; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = &unfoggedStages[stage];

		if( !pStage->active )
			break;

		Com_Memset( pStage, 0, sizeof( *pStage ));
	}
}

/*
=========================
FinishShader

Returns a freshly allocated shader with all the needed info
from the current global working shader
=========================
*/
static shader_t *FinishShader( void )
{
	int      stage;
	qboolean hasLightmapStage;
	qboolean vertexLightmap;

	hasLightmapStage = qfalse;
	vertexLightmap = qfalse;

	if( currentShader == NULL )
		currentShader = FindShaderText( shader.name );

	if( shader.defaultShader )
	{
		currentShader->shader = tr.defaultShader;
		currentShader = NULL;
		return tr.defaultShader;
	}

	//
	// set sky stuff appropriate
	//
	if( shader.isPortalSky )
		shader.sort = SS_PORTALSKY;

	if( shader.isSky )
		shader.sort = SS_ENVIRONMENT;

	//
	// set polygon offset
	//
	if( shader.polygonOffset && !shader.sort )
		shader.sort = SS_DECAL;

	if( fogOnly )
	{
		foggedStages[0] = unfoggedStages[0];
		shader.sort = SS_FOG;
		unfoggedStages[0].active = qfalse;
	}

	shader.needsLGrid = 0;

	//
	// set appropriate stage information
	//
	for( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = &unfoggedStages[stage];

		if( !pStage->active )
		{
			break;
		}

		// check for a missing texture
		if( !pStage->bundle[0].image[0] )
		{
			ri.Printf( PRINT_WARNING, "Shader %s has a stage with no image\n", shader.name );
			pStage->active = qfalse;
			continue;
		}

		if( pStage->rgbGen == CGEN_LIGHTING_DIFFUSE )
			shader.needsLGrid = qtrue;

		//
		// default texture coordinate generation
		//
		for( int bundle = 0; bundle < NUM_TEXTURE_BUNDLES; bundle++ )
		{
			if( pStage->bundle[bundle].isLightmap )
			{
				if( pStage->bundle[bundle].tcGen == TCGEN_BAD )
					pStage->bundle[bundle].tcGen = TCGEN_LIGHTMAP;
				hasLightmapStage = qtrue;
			}
			else
			{
				if( pStage->bundle[bundle].tcGen == TCGEN_BAD )
					pStage->bundle[bundle].tcGen = TCGEN_TEXTURE;
			}
		}

		//
		// determine sort order and fog color adjustment
		//
		if( FBitSet( pStage->stateBits, GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) && FBitSet( unfoggedStages[0].stateBits, GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ))
		{
			int blendSrcBits = pStage->stateBits & GLS_SRCBLEND_BITS;
			int blendDstBits = pStage->stateBits & GLS_DSTBLEND_BITS;

			if( !fogOnly )
			{
				// fog color adjustment only works for blend modes that have a contribution
				// that aproaches 0 as the modulate values aproach 0 --
				// GL_ONE, GL_ONE
				// GL_ZERO, GL_ONE_MINUS_SRC_COLOR
				// GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA

				// modulate, additive
				if((( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE ))
				   || (( blendSrcBits == GLS_SRCBLEND_ZERO ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR )))
				{
					pStage->adjustColorsForFog = ACFF_MODULATE_RGB;
				}
				// strict blend
				else if(( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ))
				{
					pStage->adjustColorsForFog = ACFF_MODULATE_ALPHA;
				}
				// premultiplied alpha
				else if(( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ))
				{
					pStage->adjustColorsForFog = ACFF_MODULATE_RGBA;
				}
				else
				{
					// we can't adjust this one correctly, so it won't be exactly correct in fog
				}
			}

			// don't screw with sort order if this is a portal or environment
			if( !shader.sort )
			{
				// see through item, like a grill or grate
				if( pStage->stateBits & GLS_DEPTHMASK_TRUE )
					shader.sort = SS_SEE_THROUGH;
				else
					shader.sort = SS_BLEND0;
			}
		}
	}

	// there are times when you will need to manually apply a sort to
	// opaque alpha tested shaders that have later blend passes
	if( !shader.sort )
		shader.sort = SS_OPAQUE;

	//
	// if we are in r_vertexLight mode, never use a lightmap texture
	//
	if( stage > 1 )
	{
		if( r_vertexLight->integer || glConfig.hardwareType == GLHW_PERMEDIA2 )
		{
			VertexLightingCollapse();
			stage = 1;
			hasLightmapStage = qfalse;
		}

		//
		// look for multitexture potential
		//
		if( stage > 1 && qglActiveTextureARB )
		{
			CollapseMultitexture( &stage );
		}
	}

	if( shader.lightmapIndex >= 0 && !hasLightmapStage )
	{
		ri.Printf( PRINT_DEVELOPER, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name );
		shader.lightmapIndex = LIGHTMAP_NONE;
	}

	if( qglTextureEnvCombineExists && unfoggedStages[0].active && unfoggedStages[1].active )
	{
		if( unfoggedStages[0].rgbGen == CGEN_IDENTITY && unfoggedStages[0].alphaGen == CGEN_IDENTITY )
		{
			if( !FBitSet( unfoggedStages[0].stateBits, GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS )
			    || FBitSet( unfoggedStages[0].stateBits, GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO ))
			{
				unfoggedStages[0].rgbGen = CGEN_0xE;
				unfoggedStages[0].alphaGen = AGEN_0x11;
			}
		}
	}

	// double check?
	if( shader.lightmapIndex >= 0 && !hasLightmapStage )
	{
		ri.Printf( PRINT_DEVELOPER, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name );
		shader.lightmapIndex = LIGHTMAP_NONE;
	}

	if( !fogOnly )
	{
		memcpy( foggedStages, unfoggedStages, sizeof( foggedStages ));
		memcpy( alphaFoggedStages, unfoggedStages, sizeof( alphaFoggedStages ));
	}

	if( stage >= MAX_SHADER_STAGES )
	{
		ri.Error( 1, "WARNING: overflowed shader stages in shader '%s'\n", shader.name );
	}

	uint depthBits = 0;

	if( stage > 0 )
		depthBits = unfoggedStages[stage - 1].stateBits & GLS_DEPTH_BITS;

	if( foggedStages[stage].unknown == qfalse )
	{
		foggedStages[stage].active = qtrue;
		foggedStages[stage].stateBits = depthBits | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA;
		foggedStages[stage].rgbGen = CGEN_0xB;
		foggedStages[stage].alphaGen = AGEN_IDENTITY;
		foggedStages[stage].bundle[0].image[0] = tr.fogImage;
		foggedStages[stage].unknown = qtrue;
		foggedStages[stage].bundle[0].tcGen = TCGEN_FOG;
	}

	if( alphaFoggedStages[stage].unknown == qfalse )
	{
		alphaFoggedStages[stage].active = qtrue;
		alphaFoggedStages[stage].rgbGen = CGEN_0x10;
		alphaFoggedStages[stage].alphaGen = AGEN_0x12;
		alphaFoggedStages[stage].stateBits = depthBits | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA;
		alphaFoggedStages[stage].unknown = qtrue;
		alphaFoggedStages[stage].bundle[0].image[0] = tr.whiteImage;
		alphaFoggedStages[stage].bundle[0].tcGen = TCGEN_IDENTITY;
	}

	for( int i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		unfoggedStages[i].adjustColorsForFog = ACFF_NONE;
		if( unfoggedStages[i].rgbGen == CGEN_0xE )
			unfoggedStages[i].rgbGen == CGEN_IDENTITY;

		if( unfoggedStages[i].alphaGen == AGEN_0x11 )
			unfoggedStages[i].alphaGen = AGEN_IDENTITY;
	}

	shader.numAlphaFoggedPasses = 0;
	shader.numUnfoggedPasses = 0;
	shader.numFoggedPasses = 0;

	for( int i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		if( foggedStages[i].active )
			shader.numFoggedPasses++;

		if( unfoggedStages[i].active )
			shader.numUnfoggedPasses++;

		if( alphaFoggedStages[i].active )
			shader.numAlphaFoggedPasses++;
	}

	for( int i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		if( foggedStages[i].rgbGen == CGEN_0xE )
		{
			if( !foggedStages[i].bundle[1].isLightmap )
				foggedStages[i].stateBits |= GLS_CLAMP_EDGE;
			else
				foggedStages[i].stateBits |= GLS_MULTITEXTURE_ENV;
		}

		if( alphaFoggedStages[i].rgbGen == CGEN_0xE )
		{
			if( !alphaFoggedStages[i].bundle[1].isLightmap )
				alphaFoggedStages[i].stateBits |= GLS_CLAMP_EDGE;
			else
				alphaFoggedStages[i].stateBits |= GLS_MULTITEXTURE_ENV;
		}
	}

	if( !fogOnly )
	{
		if( foggedStages[0].rgbGen == CGEN_0xE || ( !FBitSet( foggedStages[0].stateBits, GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) && foggedStages[0].adjustColorsForFog ))
			shader.numFoggedPasses--;

		if( alphaFoggedStages[0].rgbGen == CGEN_0xE || ( !FBitSet( alphaFoggedStages[0].stateBits, GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) && alphaFoggedStages[0].adjustColorsForFog ))
			shader.numAlphaFoggedPasses--;
	}

	for( int i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		if( alphaFoggedStages[i].active )
		{
			alphaFoggedStages[i].hasNormalMap = qtrue;
			shader.stagesWithAlphaFog |= 4;
		}

		if( shader.numFoggedPasses == shader.numUnfoggedPasses && foggedStages[i].active )
		{
			foggedStages[i].hasNormalMap = qtrue;
			shader.stagesWithAlphaFog |= 2;
		}
	}

	// determine which stage iterator function is appropriate
	ComputeStageIteratorFunc();

	shader.needsLSpherical = 0;

	if( shader.numUnfoggedPasses && unfoggedStages[0].rgbGen == CGEN_LIGHTING_DIFFUSE && unfoggedStages[0].alphaGen == AGEN_IDENTITY )
		shader.needsLSpherical = 1;

	return GeneratePermanentShader();
}

// ========================================================================================
/*
===============
R_FindShader

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

In the interest of not requiring an explicit shader text entry to
be defined for every single image used in the game, three default
shader behaviors can be auto-created for any image:

If lightmapIndex == LIGHTMAP_NONE, then the image will have
dynamic diffuse lighting applied to it, as apropriate for most
entity skin surfaces.

If lightmapIndex == LIGHTMAP_2D, then the image will be used
for 2D rendering unless an explicit shader is found

If lightmapIndex == LIGHTMAP_BY_VERTEX, then the image will use
the vertex rgba modulate values, as apropriate for misc_model
pre-lit surfaces.

Other lightmapIndex values will have a lightmap stage created
and src*dest blending applied with the texture, as apropriate for
most world construction surfaces.

===============
*/
shader_t *R_FindShader( const char *name, int lightmapIndex, qboolean mipRawImage, qboolean allowPicmip, qboolean repeat )
{
	char     strippedName[MAX_QPATH];
	char     fileName[MAX_QPATH];
	int      i, hash;
	char     *shaderText;
	image_t  *image;
	shader_t *sh;

	if( name[0] == 0 )
	{
		return tr.defaultShader;
	}

	// use (fullbright) vertex lighting if the bsp file doesn't have
	// lightmaps
	if( lightmapIndex >= 0 && lightmapIndex >= tr.numLightmaps )
	{
		lightmapIndex = LIGHTMAP_BY_VERTEX;
	}

	COM_StripExtension( name, strippedName );

	hash = generateHashValue( strippedName );

	for( currentShader = hashTable[hash]; currentShader; currentShader = currentShader->next )
	{
		if( !Q_stricmp( currentShader->name, strippedName ))
		{
			break;
		}
	}

	if( currentShader )
	{
		for( sh = currentShader->shader; sh; sh = sh->next )
		{
			if( sh->lightmapIndex == lightmapIndex || sh == tr.defaultShader )
			{
				return sh;
			}
		}
	}
	else
	{
		currentShader = AddShaderTextToHash( strippedName, hash );
	}

	// make sure the render thread is stopped, because we are probably
	// going to have to upload an image
	R_SyncRenderThread();

	// clear the global shader
	Com_Memset( &shader, 0, sizeof( shader ));
	Com_Memset( &foggedStages, 0, sizeof( foggedStages ));
	Com_Memset( &unfoggedStages, 0, sizeof( unfoggedStages ));
	Com_Memset( &alphaFoggedStages, 0, sizeof( alphaFoggedStages ));

	shader.sprite.scale = 1.0;
	fogOnly = qfalse;
	shader_noPicMip = qfalse;
	shader_noMipMaps = qfalse;
	shader_force32bit = qfalse;
	Q_strncpyz( shader.name, strippedName, sizeof( shader.name ));

	shader.lightmapIndex = lightmapIndex;
	for( i = 0; i < MAX_SHADER_STAGES; i++ )
	{
		for( int j = 0; j < NUM_TEXTURE_BUNDLES; j++ ) // FIXME: are we sure about that?
			unfoggedStages[i].bundle[j].texMods = texMods[i];
	}

	// FIXME: set these "need" values apropriately
	shader.needsNormal = qtrue;
	shader.needsST1 = qtrue;
	shader.needsST2 = qtrue;
	shader.needsColor = qtrue;

	//
	// attempt to define shader from an explicit parameter file
	//
	shaderText = currentShader->text;
	if( shaderText )
	{
		// enable this when building a pak file to get a global list
		// of all explicit shaders
		if( r_printShaders->integer )
		{
			ri.Printf( PRINT_ALL, "*SHADER* %s\n", name );
		}

		if( !ParseShader( &shaderText ))
		{
			// had errors, so use default shader
			shader.defaultShader = qtrue;
		}

		if( shader.lightmapIndex == LIGHTMAP_BY_VERTEX && FBitSet( shader.surfaceFlags, SURF_HINT ))
		{
			unfoggedStages[0].rgbGen = CGEN_EXACT_VERTEX;
		}

		sh = FinishShader();
		return sh;
	}


	//
	// if not defined in the in-memory shader descriptions,
	// look for a single TGA, BMP, or PCX
	//
	Q_strncpyz( fileName, name, sizeof( fileName ));
	COM_DefaultExtension( fileName, sizeof( fileName ), ".tga" );
	image = R_FindImageFile( fileName, mipRawImage, allowPicmip, qfalse, repeat ? GL_REPEAT : GL_CLAMP );
	if( !image )
	{
		ri.Printf( PRINT_DEVELOPER, "Couldn't find image for shader %s\n", name );
		shader.defaultShader = qtrue;
		return FinishShader();
	}

	//
	// create the default shading commands
	//
	if( shader.lightmapIndex == LIGHTMAP_NONE )
	{
		// dynamic colors at vertexes
		unfoggedStages[0].bundle[0].image[0] = image;
		unfoggedStages[0].active = qtrue;
		unfoggedStages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		unfoggedStages[0].stateBits = GLS_DEFAULT;
	}
	else if( shader.lightmapIndex == LIGHTMAP_BY_VERTEX )
	{
		// explicit colors at vertexes
		unfoggedStages[0].bundle[0].image[0] = image;
		unfoggedStages[0].active = qtrue;
		unfoggedStages[0].rgbGen = CGEN_EXACT_VERTEX;
		unfoggedStages[0].alphaGen = AGEN_SKIP;
		unfoggedStages[0].stateBits = GLS_DEFAULT;
	}
	else if( shader.lightmapIndex == LIGHTMAP_2D )
	{
		// GUI elements
		unfoggedStages[0].bundle[0].image[0] = image;
		unfoggedStages[0].active = qtrue;
		unfoggedStages[0].rgbGen = CGEN_VERTEX;
		unfoggedStages[0].alphaGen = AGEN_VERTEX;
		unfoggedStages[0].stateBits = GLS_DEPTHTEST_DISABLE
					      | GLS_SRCBLEND_SRC_ALPHA
					      | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if( shader.lightmapIndex == LIGHTMAP_WHITEIMAGE )
	{
		// fullbright level
		unfoggedStages[0].bundle[0].image[0] = tr.whiteImage;
		unfoggedStages[0].active = qtrue;
		unfoggedStages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		unfoggedStages[0].stateBits = GLS_DEFAULT;

		unfoggedStages[1].bundle[0].image[0] = image;
		unfoggedStages[1].active = qtrue;
		unfoggedStages[1].rgbGen = CGEN_IDENTITY;
		unfoggedStages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}
	else
	{
		// two pass lightmap
		unfoggedStages[0].bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
		unfoggedStages[0].bundle[0].isLightmap = qtrue;
		unfoggedStages[0].active = qtrue;
		unfoggedStages[0].rgbGen = CGEN_IDENTITY; // lightmaps are scaled on creation
		// for identitylight
		unfoggedStages[0].stateBits = GLS_DEFAULT;

		unfoggedStages[1].bundle[0].image[0] = image;
		unfoggedStages[1].active = qtrue;
		unfoggedStages[1].rgbGen = CGEN_IDENTITY;
		unfoggedStages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}

	return FinishShader();
}

/*
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShader( const char *name )
{
	shader_t *sh;

	if( strlen( name ) >= MAX_QPATH )
	{
		Com_Printf( "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, LIGHTMAP_2D, qtrue, qtrue, qtrue );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if( sh->defaultShader )
	{
		return 0;
	}

	return sh->index;
}

/*
====================
RE_RegisterShaderNoMip

For menu graphics that should never be picmiped
====================
*/
qhandle_t RE_RegisterShaderNoMip( const char *name )
{
	shader_t *sh;

	if( strlen( name ) >= MAX_QPATH )
	{
		Com_Printf( "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, LIGHTMAP_2D, qfalse, qfalse, qfalse );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if( sh->defaultShader )
	{
		return 0;
	}

	return sh->index;
}

/*
====================
R_GetShaderByHandle

When a handle is passed in by another module, this range checks
it and returns a valid (possibly default) shader_t to be used internally.
====================
*/
shader_t *R_GetShaderByHandle( qhandle_t hShader )
{
	if( hShader < 0 )
	{
		ri.Printf( PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader ); // bk: FIXME name
		return tr.defaultShader;
	}
	if( hShader >= tr.numShaders )
	{
		ri.Printf( PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
		return tr.defaultShader;
	}
	return tr.shaders[hShader];
}

/*
===============
R_ShaderList_f

Dump information on all valid shaders to the console
A second parameter will cause it to print in sorted order
===============
*/
void R_ShaderList_f( void )
{
	int i;
	int count;
	shader_t *shader;

	ri.Printf( PRINT_ALL, "-----------------------\n" );

	count = 0;
	for( i = 0; i < tr.numShaders; i++ )
	{
		if( ri.Cmd_Argc() > 1 )
		{
			shader = tr.sortedShaders[i];
		}
		else
		{
			shader = tr.shaders[i];
		}

		ri.Printf( PRINT_ALL, "%i ", shader->numUnfoggedPasses );

		if( shader->lightmapIndex >= 0 )
		{
			ri.Printf( PRINT_ALL, "L " );
		}
		else
		{
			ri.Printf( PRINT_ALL, "  " );
		}

		qboolean have_add = qfalse;
		qboolean have_modulate = qfalse;
		for( int stage = 0; shader->stages[stage] && shader->stages[stage]->active; stage++ )
		{
			if( shader->stages[stage]->multitextureEnv & MT_ENV_ADD )
				have_add = qtrue;
			if( shader->stages[stage]->multitextureEnv & MT_ENV_MODULATE )
				have_modulate = qtrue;
		}

		if( have_add || have_modulate )
			ri.Printf( PRINT_ALL, "MT(%s%s) ", have_add ? "+" : " ", have_modulate ? "*" : " " );
		else
			ri.Printf( PRINT_ALL, "       " );

		if( shader->explicitlyDefined )
		{
			ri.Printf( PRINT_ALL, "E " );
		}
		else
		{
			ri.Printf( PRINT_ALL, "  " );
		}

		if( shader->optimalStageIteratorFunc == RB_StageIteratorGeneric )
		{
			ri.Printf( PRINT_ALL, "gen " );
		}
		else if( shader->optimalStageIteratorFunc == RB_StageIteratorSky )
		{
			ri.Printf( PRINT_ALL, "sky " );
		}
		else if( shader->optimalStageIteratorFunc == RB_StageIteratorLightmappedMultitexture )
		{
			ri.Printf( PRINT_ALL, "lmmt" );
		}
		else if( shader->optimalStageIteratorFunc == RB_StageIteratorVertexLitTexture )
		{
			ri.Printf( PRINT_ALL, "vlt " );
		}
		else
		{
			ri.Printf( PRINT_ALL, "    " );
		}

		if( shader->defaultShader )
		{
			ri.Printf( PRINT_ALL, ": %s (DEFAULTED)\n", shader->name );
		}
		else
		{
			ri.Printf( PRINT_ALL, ": %s\n", shader->name );
		}
		count++;
	}
	ri.Printf( PRINT_ALL, "%i total shaders\n", count );
	ri.Printf( PRINT_ALL, "------------------\n" );
}

/*
====================
ScanAndLoadShaderFiles

Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
#define MAX_SHADER_FILES 4096
static void ScanAndLoadShaderFiles( void )
{
	char **shaderFiles;
	char *buffers[MAX_SHADER_FILES];
	char *p;
	int  numShaders;
	int  i;
	char *oldp, *token, *hashMem;
	int  hash, size;

	long sum = 0;
	// scan for shader files
	shaderFiles = ri.FS_ListFiles( "scripts", ".shader", &numShaders );

	if( !shaderFiles || !numShaders )
	{
		ri.Printf( PRINT_WARNING, "WARNING: no shader files found\n" );
		return;
	}

	if( numShaders > MAX_SHADER_FILES )
		numShaders = MAX_SHADER_FILES;

	// load and parse shader files
	for( i = 0; i < numShaders; i++ )
	{
		char filename[MAX_QPATH];

		Com_sprintf( filename, sizeof( filename ), "scripts/%s", shaderFiles[i] );
		ri.Printf( PRINT_ALL, "...loading '%s'\n", filename );
		sum += ri.FS_ReadFile( filename, (void **)&buffers[i] );
		if( !buffers[i] )
			ri.Error( ERR_DROP, "Couldn't load %s", filename );
	}

	// build single large buffer
	s_shaderText = ri.Hunk_Alloc( sum + numShaders * 2, h_low );

	// free in reverse order, so the temp files are all dumped
	for( i = numShaders - 1; i >= 0; i-- )
	{
		strcat( s_shaderText, "\n" );
		p = &s_shaderText[strlen( s_shaderText )];
		strcat( s_shaderText, buffers[i] );
		ri.FS_FreeFile( buffers[i] );
		buffers[i] = p;
	}

	// free up memory
	ri.FS_FreeFileList( shaderFiles );
	return;
}

static shadertext_t *AllocShaderText( const char *text )
{
	long hash = generateHashValue( text );
	return AddShaderTextToHash( text, hash );
}

static void FindShadersInShaderText( void )
{
	char *p;
	char *oldp;
	char *token;

	if( s_shaderText )
	{
		p = s_shaderText;
		// look for label
		while( qtrue )
		{
			oldp = p;
			token = COM_ParseExt( &p, qtrue );
			if( token[0] == 0 )
			{
				break;
			}

			if( *token == '{' )
			{
				p = oldp;
				SkipBracedSection( &p );
			}
			else
			{
				currentShader = AllocShaderText( token );
				currentShader->text = p;
			}
		}
	}
}

/*
====================
CreateInternalShaders
====================
*/
static void CreateInternalShaders( void )
{
	tr.numShaders = 0;

	// init the default shader
	Com_Memset( &shader, 0, sizeof( shader ));
	Com_Memset( &foggedStages, 0, sizeof( foggedStages ));
	Com_Memset( &unfoggedStages, 0, sizeof( unfoggedStages ));

	fogOnly = qfalse;

	Q_strncpyz( shader.name, "<default>", sizeof( shader.name ));
	shader.lightmapIndex = LIGHTMAP_NONE;
	unfoggedStages[0].bundle[0].image[0] = tr.defaultImage;
	unfoggedStages[0].active = qtrue;
	unfoggedStages[0].stateBits = GLS_DEFAULT;
	currentShader = FindShaderText( shader.name );
	tr.defaultShader = FinishShader();

	Q_strncpyz( shader.name, "<white>", sizeof( shader.name ));
	shader.lightmapIndex = LIGHTMAP_NONE;
	unfoggedStages[0].bundle[0].image[0] = tr.whiteImage;
	unfoggedStages[0].active = qtrue;
	unfoggedStages[0].stateBits = GLS_DEFAULT;
	currentShader = FindShaderText( shader.name );
	tr.defaultShader = FinishShader();

	// shadow shader is just a marker
	Q_strncpyz( shader.name, "<stencil shadow>", sizeof( shader.name ));
	shader.lightmapIndex = LIGHTMAP_NONE;
	shader.sort = SS_STENCIL_SHADOW;
	currentShader = FindShaderText( shader.name );
	tr.shadowShader = FinishShader();
}

static void CreateExternalShaders( void )
{
	tr.projectionShadowShader = R_FindShader( "projectionShadow", LIGHTMAP_NONE, qtrue, qtrue, qtrue );
	tr.flareShader = R_FindShader( "flareShader", LIGHTMAP_NONE, qtrue, qtrue, qtrue );
}

void InitStaticShaders( void )
{
	const char *token;
	char       *text, *buf;
	char       shadername[64];

	if( ri.FS_ReadFile( "scripts/static_shaders.txt", (void **)&buf ) == -1 )
	{
		ri.Printf( PRINT_ERROR, "Couldn't find static shaders file: scripts/staticshaders.txt\n" );
		return;
	}

	text = buf;
	while( qtrue )
	{
		token = COM_ParseExt( &text, qtrue );
		if( !token[0] )
			break;

		strncpy( shadername, token, sizeof( shadername ));

		if( !R_FindShader( shadername, LIGHTMAP_NONE, qtrue, qtrue, qtrue ))
			ri.Printf( PRINT_ERROR, "InitStaticShaders: Couldn't find shader: %s\n", shadername );
	}

	ri.FS_FreeFile( buf );
}

void R_SetupShaders( void )
{
	ri.Printf( PRINT_ALL, "Setting up Shaders\n" );
	s_shaderText = NULL;
	Com_Memset( hashTable, 0, sizeof( hashTable ));
	CreateInternalShaders();
	InitStaticShaders();
	CreateExternalShaders();
}

/*
==================
R_StartupShaders
==================
*/
void R_StartupShaders( void )
{
	ri.Printf( PRINT_ALL, "Initializing Shaders\n" );

	currentShader = NULL;
	s_shaderText = NULL;
	Com_Memset( hashTable, 0, sizeof( hashTable ));

	ScanAndLoadShaderFiles();
	FindShadersInShaderText();

	R_SetupShaders();
}

void R_ShutdownShaders( void )
{
	if( s_shaderText )
		ri.Free( s_shaderText );

	s_shaderText = NULL;

	for( int i = 0; i < FILE_HASH_SIZE; i++ )
	{
		while( hashTable[i]->shader )
		{
			shadertext_t *ptr = hashTable[i];
			hashTable[i] = ptr->next;

			ri.Free( ptr );
		}
	}
}
