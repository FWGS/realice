//
//
// bg_lib,c -- standard C library replacement routines used by code
// compiled for the virtual machine

#include "q_shared.h"

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// ==================================================================================


// this file is excluded from release builds because of intrinsics

// #ifndef _MSC_VER
// a1ba: do we need this on modern systems?

void *memmove( void *dest, const void *src, size_t count )
{
	int i;

	if( dest > src )
	{
		for( i = count - 1; i >= 0; i-- )
		{
			((char *)dest )[i] = ((char *)src )[i];
		}
	}
	else
	{
		for( i = 0; i < count; i++ )
		{
			((char *)dest )[i] = ((char *)src )[i];
		}
	}
	return dest;
}

static int randSeed = 0;

void srand( unsigned seed )
{
	randSeed = seed;
}

int rand( void )
{
	randSeed = ( 69069 * randSeed + 1 );
	return randSeed & 0x7fff;
}

double atof( const char *string )
{
	float sign;
	float value;
	int   c;


	// skip whitespace
	while( *string <= ' ' )
	{
		if( !*string )
		{
			return 0;
		}
		string++;
	}

	// check sign
	switch( *string )
	{
	case '+':
		string++;
		sign = 1;
		break;
	case '-':
		string++;
		sign = -1;
		break;
	default:
		sign = 1;
		break;
	}

	// read digits
	value = 0;
	c = string[0];
	if( c != '.' )
	{
		do
		{
			c = *string++;
			if( c < '0' || c > '9' )
			{
				break;
			}
			c -= '0';
			value = value * 10 + c;
		}
		while( 1 );
	}
	else
	{
		string++;
	}

	// check for decimal point
	if( c == '.' )
	{
		double fraction;

		fraction = 0.1;
		do
		{
			c = *string++;
			if( c < '0' || c > '9' )
			{
				break;
			}
			c -= '0';
			value += c * fraction;
			fraction *= 0.1;
		}
		while( 1 );

	}

	// not handling 10e10 notation...

	return value * sign;
}

double _atof( const char **stringPtr )
{
	const char *string;
	float      sign;
	float      value;
	int c = '0'; // bk001211 - uninitialized use possible

	string = *stringPtr;

	// skip whitespace
	while( *string <= ' ' )
	{
		if( !*string )
		{
			*stringPtr = string;
			return 0;
		}
		string++;
	}

	// check sign
	switch( *string )
	{
	case '+':
		string++;
		sign = 1;
		break;
	case '-':
		string++;
		sign = -1;
		break;
	default:
		sign = 1;
		break;
	}

	// read digits
	value = 0;
	if( string[0] != '.' )
	{
		do
		{
			c = *string++;
			if( c < '0' || c > '9' )
			{
				break;
			}
			c -= '0';
			value = value * 10 + c;
		}
		while( 1 );
	}

	// check for decimal point
	if( c == '.' )
	{
		double fraction;

		fraction = 0.1;
		do
		{
			c = *string++;
			if( c < '0' || c > '9' )
			{
				break;
			}
			c -= '0';
			value += c * fraction;
			fraction *= 0.1;
		}
		while( 1 );

	}

	// not handling 10e10 notation...
	*stringPtr = string;

	return value * sign;
}
