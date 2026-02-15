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
#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "../cgame/tr_types.h"

#define REF_API_VERSION 8

typedef struct refexport_s
{
	void          (*Shutdown)( qboolean destroyWindow );
	void          (*BeginRegistration)( glconfig_t *config );
	qhandle_t     (*RegisterModel)( const char *name );
	qhandle_t     (*RegisterSkin)( const char *name );
	qhandle_t     (*RegisterShader)( const char *name );
	qhandle_t     (*RegisterShaderNoMip)( const char *name );
	qhandle_t     (*RefreshShaderNoMip)( const char *name );
	void          (*EndRegistration)( void );
	void          (*SetWorldVisData)( const byte *vis );
	void          (*LoadWorld)( const char *name );
	void          (*ClearScene)( void );
	void          (*AddRefEntityToScene)( const refEntity_t *re );
	void          (*AddRefSpriteToScene)( const refEntity_t *re );
	void          (*AddPolyToScene)( qhandle_t hShader, int numVerts, const polyVert_t *verts, int num );
	void          (*AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void          (*RenderScene)( const refdef_t *fd );
	refEntity_t  *(*GetRenderEntity)( int entityNumber );
	void          (*SavePerformanceCounter)( void );
	void          (*SetColor)( const float *rgba );
	void          (*Set2DWindow)( int x, int y, int w, int h, float left, float right, float bottom, float top, float n, float f );
	void          (*DrawStretchPic)( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader ); // 0 = white
	void          (*DrawTilePic)( float x, float y, float w, float h, qhandle_t hShader );
	void          (*DrawTilePicOffset)( float x, float y, float w, float h, qhandle_t hShader, int offsetX, int offsetY );
	void          (*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, const byte *data );
	void          (*DebugLine)( vec3_t start, vec3_t end, float r, float g, float b, float alpha );
	void          (*DrawBox)( float x, float y, float width, float height );
	void          (*AddBox)( float x, float y, float width, float height );
	void          (*BeginFrame)( stereoFrame_t stereoFrame );
	void          (*Scissor)( int x, int y, int width, int height );
	void          (*DrawLineLoop)( vec2_t *points, int count, int stipple_factor, int stipple_mask );
	void          (*EndFrame)( int *frontEndMsec, int *backEndMsec );
	int           (*MarkFragments)( int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );
	int           (*LerpTag)( orientation_t *tag, qhandle_t model, int startFrame, int endFrame, float frac, const char *tagName );
	void          (*ModelBounds)( qhandle_t model, vec3_t mins, vec3_t maxs );
	float         (*ModelRadius)( qhandle_t model );
	int           (*TIKI_GetHandle)( qhandle_t handle );
	void          (*TIKI_FlushAll)( void );
	void          (*DrawString)( fontheader_t *font, const char *text, float x, float y, int maxlen );
	float         (*GetFontHeight)( fontheader_t *font );
	float         (*GetFontStringWidth)( fontheader_t *font, const char *s );
	fontheader_t *(*LoadFont)( const char *name );
	void          (*SwipeBegin)( float thistime, float life, qhandle_t shader );
	void          (*SwipePoint)( vec3_t point1, vec3_t point2, float time );
	void          (*SwipeEnd)( void );
	void          (*SetRenderTime)( int t );
	float         (*NoiseGet4f)( float x, float y, float z, float t );
	qboolean      (*SetMode)( int mode, glconfig_t *glConfig );
	qboolean      (*SetFullscreen)( qboolean fullscreen, glconfig_t *glConfig );
	int           (*GetShaderWidth)( qhandle_t handle );
	int           (*GetShaderHeight)( qhandle_t handle );
	const char   *(*GetGraphicsInfo)( void );
} refexport_t;

typedef struct refimport_s
{
	void (*Printf)( int printLevel, const char *fmt, ... );
	void (*Error)( int errorLevel, const char *fmt, ... );
	int (*Milliseconds)( void );
	void (*Hunk_Clear)( void );
	void *(*Hunk_Alloc)( int size );
	void *(*Hunk_AllocateTempMemory)( int size );
	void (*Hunk_FreeTempMemory)( void *block );
	void *(*Malloc)( int bytes );
	void (*Free)( void *buf );
	cvar_t *(*Cvar_Get)( const char *name, const char *value, int flags );
	void (*Cvar_Set)( const char *name, const char *value );
	void (*Cmd_AddCommand)( const char *name, void (*cmd)( void ));
	void (*Cmd_RemoveCommand)( const char *name );
	int (*Cmd_Argc)( void );
	char *(*Cmd_Argv)( int i );
	void (*Cmd_ExecuteText)( int exec_when, const char *text );
	void (*CM_DrawDebugSurface)( void ( *drawPoly )( int color, int numPoints, float *points ));
	int (*FS_FOpenFileRead)( const char *filename, fileHandle_t *file, qboolean uniqueFILE );
	int (*FS_Read)( void *buffer, int len, fileHandle_t f );
	void (*FS_FCloseFile)( fileHandle_t f );
	int (*FS_Seek)( fileHandle_t f, long offset, int origin );
	int (*FS_FileIsInPAK)( const char *name, int *pCheckSum );
	int (*FS_ReadFile)( const char *name, void **buf );
	void (*FS_FreeFile)( void *buf );
	char ** (*FS_ListFiles)( const char *name, const char *extension, int *numfilesfound );
	void (*FS_FreeFileList)( char **filelist );
	void (*FS_WriteFile)( const char *qpath, const void *buffer, int size );
	void *(TIKI_GetAnim)( int tikihandle );
	dtiki_t *(TIKI_GetTiki)( int tikihandle );
	void (*TIKI_FreeTiki)( int tikihandle );
	int (*TIKI_RegisterTiki)( const char *path );
	void (*TIKI_CalculateBounds)( init tikihandle, float scale, vec3_t mins, vec3_t maxs );
	float (*TIKI_GlobalRadius)( int );
	void (*CM_BoxTrace)( trace_t *results, float *start, float *end, float *mins, float *maxs, clipHandle_t model, int brushmask, int capsule );
	const char *(CM_EntityString)( void );
	void (*CL_RefSetPerformanceCounters)( int total_tris, int total_verts, int total_texels, int world_tris, int world_verts, int character_lights );

	void *DebugLines;
	int *numDebugLines;
} refimport_t;

refexport_t *GetRefAPI( int apiVersion, refimport_t *rimp );

#endif // __TR_PUBLIC_H
