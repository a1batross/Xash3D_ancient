/***
*
*	Copyright (c) 2001-2006, Chain Studios. All rights reserved.
*
*	This product contains software technology that is a part of Half-Life FX (R)
*	from Chain Studios ("HLFX Technology"). All rights reserved.
*
*	Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Chain Studios.
*
****/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "hud.h"
#include "r_main.h"
#include "r_util.h"
#include <string.h> 

vec3_t vec3_origin( 0, 0, 0 );

double sqrt(double x);

HSPRITE m_hHudFont;

int *GetRect( void )
{
	RECT wrect;
	static int extent[4];

	if(GetWindowRect( GetActiveWindow(), &wrect ))
          {
		if(!wrect.left)
		{
			extent[0] = wrect.left;	//+4
			extent[1] = wrect.top;	//+30
			extent[2] = wrect.right;	//-4
			extent[3] = wrect.bottom;	//-4
		}
		else
		{
			extent[0] = wrect.left + 4;	//+4
			extent[1] = wrect.top + 30;	//+30
			extent[2] = wrect.right - 4;	//-4
			extent[3] = wrect.bottom - 4;	//-4
		}
	}
	return extent;	
}

void ScaleColors( int &r, int &g, int &b, int a )
{
	float x = (float)a / 255;
	r = (int)(r * x);
	g = (int)(g * x);
	b = (int)(b * x);
}

unsigned int nextpow2(unsigned int x)
{
	unsigned int y = 1;
	while (x > y) y = y << 1;
	return y;
}

float Length(const float *v)
{
	int		i;
	float	length;
	
	length = 0;
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}

float Distance(const vec3_t v1, const vec3_t v2)
{
	vec3_t d;
	VectorSubtract(v2,v1,d);
	return Length(d);
}

void VectorAngles( const float *forward, float *angles )
{
	float	tmp, yaw, pitch;
	
	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt (forward[0]*forward[0] + forward[1]*forward[1]);
		pitch = (atan2(forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}
	
	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

float VectorNormalize (float *v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;

}

void VectorInverse ( float *v )
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorScale (const float *in, float scale, float *out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}

void VectorMA (const float *veca, float scale, const float *vecb, float *vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}

HSPRITE LoadSprite(const char *pszName)
{
	int i;
	char sz[256]; 

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;

	sprintf(sz, pszName, i);

	return SPR_Load(sz);
}

float TransformColor ( float color )
{
	float trns_clr;
	if(color >= 0 ) trns_clr = color / 255.0f;
	else trns_clr = 1.0;//default value
	return trns_clr;
}

#define NOISE_SIZE		64

inline	float fade ( float t )
{
	return t * t * t * (t * (t * 6 - 15) + 10); 
}

inline	float lerp ( float t, float a, float b )
{ 
	return a + t * (b - a); 
}

inline	float grad ( int hash, float x, float y, float z )
{
	int		h = hash & 15;                      // CONVERT LO 4 BITS OF HASH CODE
    float	u = h<8 ? x : y,              		// INTO 12 GRADIENT DIRECTIONS.
            v = h<4 ? y : h==12 || h==14 ? x : z;
            
	return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}

int	permutation [256] = 
{ 
	151,160,137,91,90,15,
	131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
	190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
	88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
	77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
	102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
	135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
	5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
	223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
	129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
	251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
	49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
	138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

int	p[512];

void InitImprovedNoise()
{
	for ( int i = 0; i < 256; i++ ) 
		p [ 256 + i ] = p [ i ] = permutation [i];
}

float noise( float vx, float vy, float vz ) 
{

	float	fx = floor ( vx );							// get floor values of coords
  	float	fy = floor ( vy );
   	float	fz = floor ( vz );
   		
    int X = ((int) fx) & 255,   						// FIND UNIT CUBE THAT
       	Y = ((int) fy) & 255,       					// CONTAINS POINT.
       	Z = ((int) fz) & 255;
          
    float x = vx - fx;             					// FIND RELATIVE X,Y,Z
    float y = vy - fy;             					// OF POINT IN CUBE.
    float z = vz - fz;
      	
   	float	u = fade ( x ),                          	// COMPUTE FADE CURVES
           	v = fade ( y ),                      		// FOR EACH OF X,Y,Z.
           	w = fade ( z );
            
    int		a = p [X  ]+Y, aa = p [a]+Z, ab = p [a+1]+Z,	// HASH COORDINATES OF
      		b = p [X+1]+Y, ba = p [b]+Z, bb = p [b+1]+Z;	// THE 8 CUBE CORNERS,

	return lerp(w, lerp(v, lerp(u, grad(p[aa  ], x  , y  , z   ),  // AND ADD
                                   grad(p[ba  ], x-1, y  , z   )), // BLENDED
                           lerp(u, grad(p[ab  ], x  , y-1, z   ),  // RESULTS
                                   grad(p[bb  ], x-1, y-1, z   ))),// FROM  8
                   lerp(v, lerp(u, grad(p[aa+1], x  , y  , z-1 ),  // CORNERS
                                   grad(p[ba+1], x-1, y  , z-1 )), // OF CUBE
                           lerp(u, grad(p[ab+1], x  , y-1, z-1 ),
                                   grad(p[bb+1], x-1, y-1, z-1 ))));
}


void CreateNoiseTexture3D (float scale)
{
	int i;

	GLfloat *img = new GLfloat[NOISE_SIZE * NOISE_SIZE * NOISE_SIZE * 3];
	GLfloat *p = img;

	InitImprovedNoise();

	for (i=0; i < NOISE_SIZE*NOISE_SIZE*NOISE_SIZE*3 ; i++)
	{
		float x = i + sin(i);
		float y = i + cos(i);
		*p++ = noise(x, y, i);
	}

	g_uiNoiseTex = pTexId++;
	glBindTexture(GL_TEXTURE_3D, g_uiNoiseTex);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage3DEXT(GL_TEXTURE_3D, 0, 3, NOISE_SIZE, NOISE_SIZE, NOISE_SIZE, 0, 
					GL_RGB, GL_FLOAT, img);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT );

	delete [] img;
}

//
// R_SphereInFrustum
//
// Purpose: sphere visibility test
//
extern float frustumPlanes[6][4];

float R_SphereInFrustum( vec3_t o, float radius )
{
   int p;
   float d;

   for( p = 0; p < 6; p++ )
   {
      d = frustumPlanes[p][0] * o[0] + frustumPlanes[p][1] * o[1] + frustumPlanes[p][2] * o[2] + frustumPlanes[p][3];
      if( d <= -radius )
         return 0;
   }
   return d + radius;
}

//
// UTIL_GetClientEntityWithServerIndex
//
// Purpose: searches for the server entity on client
// Assumes:	colormap has server entity index
//
cl_entity_t *UTIL_GetClientEntityWithServerIndex( int sv_index )
{
	cl_entity_t *e;

	for (int ic=1;ic<MAX_EDICTS;ic++)
	{
		e = gEngfuncs.GetEntityByIndex( ic );
		if (!e)
			break;

		if (!e->model)
			continue;

		if (e->curstate.colormap == sv_index)
			return e;
	}

	return NULL;
}

/*
====================
AngleMatrix

====================
*/
void AngleMatrix (const float *angles, float (*matrix)[4] )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[1][0] = cp*sy;
	matrix[2][0] = -sp;
	matrix[0][1] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[2][1] = sr*cp;
	matrix[0][2] = (cr*sp*cy+-sr*-sy);
	matrix[1][2] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

/*
====================
VectorCompare

====================
*/
int VectorCompare (const float *v1, const float *v2)
{
	int		i;
	
	for (i=0 ; i<3 ; i++)
		if (v1[i] != v2[i]) return 0;
			
	return 1;
}

/*
====================
CrossProduct

====================
*/
void CrossProduct (const float *v1, const float *v2, float *cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

/*
====================
VectorTransform

====================
*/
void VectorTransform (const float *in1, float in2[3][4], float *out)
{
	out[0] = DotProduct(in1, in2[0]) + in2[0][3];
	out[1] = DotProduct(in1, in2[1]) + in2[1][3];
	out[2] = DotProduct(in1, in2[2]) + in2[2][3];
}

/*
================
ConcatTransforms

================
*/
void ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
				in1[0][2] * in2[2][3] + in1[0][3];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
				in1[1][2] * in2[2][3] + in1[1][3];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
				in1[2][2] * in2[2][3] + in1[2][3];
}

// angles index are not the same as ROLL, PITCH, YAW

/*
====================
AngleQuaternion

====================
*/
void AngleQuaternion( float *angles, vec4_t quaternion )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;

	// FIXME: rescale the inputs to 1/2 angle
	angle = angles[2] * 0.5;
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[1] * 0.5;
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[0] * 0.5;
	sr = sin(angle);
	cr = cos(angle);

	quaternion[0] = sr*cp*cy-cr*sp*sy; // X
	quaternion[1] = cr*sp*cy+sr*cp*sy; // Y
	quaternion[2] = cr*cp*sy-sr*sp*cy; // Z
	quaternion[3] = cr*cp*cy+sr*sp*sy; // W
}

/*
====================
QuaternionSlerp

====================
*/
void QuaternionSlerp( vec4_t p, vec4_t q, float t, vec4_t qt )
{
	int i;
	float	omega, cosom, sinom, sclp, sclq;

	// decide if one of the quaternions is backwards
	float a = 0;
	float b = 0;

	for (i = 0; i < 4; i++)
	{
		a += (p[i]-q[i])*(p[i]-q[i]);
		b += (p[i]+q[i])*(p[i]+q[i]);
	}
	if (a > b)
	{
		for (i = 0; i < 4; i++)
		{
			q[i] = -q[i];
		}
	}

	cosom = p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];

	if ((1.0 + cosom) > 0.000001)
	{
		if ((1.0 - cosom) > 0.000001)
		{
			omega = acos( cosom );
			sinom = sin( omega );
			sclp = sin( (1.0 - t)*omega) / sinom;
			sclq = sin( t*omega ) / sinom;
		}
		else
		{
			sclp = 1.0 - t;
			sclq = t;
		}
		for (i = 0; i < 4; i++) {
			qt[i] = sclp * p[i] + sclq * q[i];
		}
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = sin( (1.0 - t) * (0.5 * M_PI));
		sclq = sin( t * (0.5 * M_PI));
		for (i = 0; i < 3; i++)
		{
			qt[i] = sclp * p[i] + sclq * qt[i];
		}
	}
}

/*
====================
QuaternionMatrix

====================
*/
void QuaternionMatrix( vec4_t quaternion, float (*matrix)[4] )
{
	matrix[0][0] = 1.0 - 2.0 * quaternion[1] * quaternion[1] - 2.0 * quaternion[2] * quaternion[2];
	matrix[1][0] = 2.0 * quaternion[0] * quaternion[1] + 2.0 * quaternion[3] * quaternion[2];
	matrix[2][0] = 2.0 * quaternion[0] * quaternion[2] - 2.0 * quaternion[3] * quaternion[1];

	matrix[0][1] = 2.0 * quaternion[0] * quaternion[1] - 2.0 * quaternion[3] * quaternion[2];
	matrix[1][1] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[2] * quaternion[2];
	matrix[2][1] = 2.0 * quaternion[1] * quaternion[2] + 2.0 * quaternion[3] * quaternion[0];

	matrix[0][2] = 2.0 * quaternion[0] * quaternion[2] + 2.0 * quaternion[3] * quaternion[1];
	matrix[1][2] = 2.0 * quaternion[1] * quaternion[2] - 2.0 * quaternion[3] * quaternion[0];
	matrix[2][2] = 1.0 - 2.0 * quaternion[0] * quaternion[0] - 2.0 * quaternion[1] * quaternion[1];
}

/*
====================
MatrixCopy

====================
*/
void MatrixCopy( float in[3][4], float out[3][4] )
{
	memcpy( out, in, sizeof( float ) * 3 * 4 );
}

/*
====================
VectorIRotate

====================
*/
void VectorIRotate (const vec3_t in1, const float in2[3][4], vec3_t out)
{
	out[0] = in1[0]*in2[0][0] + in1[1]*in2[1][0] + in1[2]*in2[2][0];
	out[1] = in1[0]*in2[0][1] + in1[1]*in2[1][1] + in1[2]*in2[2][1];
	out[2] = in1[0]*in2[0][2] + in1[1]*in2[1][2] + in1[2]*in2[2][2];
}

/*
====================
VectorRotateByMatrix
VectorTransformByMatrix
====================
*/
void VectorRotateByMatrix (const vec3_t &in1, const float *in2, vec3_t &out)
{
     out[0] = in1[0]*in2[0] + in1[1]*in2[4] + in1[2]*in2[8];
     out[1] = in1[0]*in2[1] + in1[1]*in2[5] + in1[2]*in2[9];
     out[2] = in1[0]*in2[2] + in1[1]*in2[6] + in1[2]*in2[10];
}

void VectorTransformByMatrix (const vec3_t &in1, const float *in2, vec3_t &out)
{
     out[0] = in1[0]*in2[0] + in1[1]*in2[4] + in1[2]*in2[8] + in2[12];
     out[1] = in1[0]*in2[1] + in1[1]*in2[5] + in1[2]*in2[9] + in2[13];
     out[2] = in1[0]*in2[2] + in1[1]*in2[6] + in1[2]*in2[10] + in2[14];
}

void VectorRotateByMatrix (const float *in1, const float *in2, float *out)
{
     out[0] = in1[0]*in2[0] + in1[1]*in2[4] + in1[2]*in2[8];
     out[1] = in1[0]*in2[1] + in1[1]*in2[5] + in1[2]*in2[9];
     out[2] = in1[0]*in2[2] + in1[1]*in2[6] + in1[2]*in2[10];
}

void VectorTransformByMatrix (const float *in1, const float *in2, float *out)
{
     out[0] = in1[0]*in2[0] + in1[1]*in2[4] + in1[2]*in2[8] + in2[12];
     out[1] = in1[0]*in2[1] + in1[1]*in2[5] + in1[2]*in2[9] + in2[13];
     out[2] = in1[0]*in2[2] + in1[1]*in2[6] + in1[2]*in2[10] + in2[14];
}

/*
====================
ObjectToWorldMatrix

====================
*/
void ObjectToWorldMatrix(cl_entity_t *e, float *result)
{
	glPushMatrix();
	glLoadIdentity();
     
	glTranslatef(e->origin[0], e->origin[1], e->origin[2]);
	glRotatef ( e->angles[1],  0, 0, 1);
	glRotatef ( e->angles[0],  0, 1, 0);
	glRotatef ( e->angles[2],  1, 0, 0);

	glGetFloatv (GL_MODELVIEW_MATRIX, result);
	glPopMatrix();
}

/*
====================
SPRITE_GetList

====================
*/

char *ParseHudSprite( char *pfile, char *psz, client_sprite_t *result )
{
	char token[256];
	client_sprite_t *p = new client_sprite_t;
	int x = 0, y = 0, width = 0, height = 0;
	int section = 0;
	
	memset( p, 0, sizeof(client_sprite_t) );
          
	while( pfile )
	{
		pfile = gEngfuncs.COM_ParseFile( pfile, token );	

		if( !stricmp( token, psz ))
		{
			pfile = gEngfuncs.COM_ParseFile( pfile, token );	
			if( !stricmp( token, "{" )) section = 1;
		}
		if(section)//parse section
		{
			if( !stricmp( token, "}" )) break;//end section
			
			if ( !stricmp( token, "file" )) 
			{                                          
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				strcpy(p->szSprite, token );
				if( !gEngfuncs.COM_LoadFile( p->szSprite, 5, NULL )) return pfile;
				else
				{
					gEngfuncs.COM_FreeFile( p->szSprite);
					//fill structure at default
					HSPRITE m_hSprite = SPR_Load(p->szSprite);
					x = y = 0;
					width = SPR_Width( m_hSprite, 0 );
					height = SPR_Height( m_hSprite, 0 );
				}
			}
			else if ( !stricmp( token, "name" )) 
			{                                          
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				strcpy(p->szName, token );
			}
			else if ( !stricmp( token, "x" )) 
			{                                          
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				x = atoi(token);
			}
			else if ( !stricmp( token, "y" )) 
			{                                          
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				y = atoi(token);
			}
			else if ( !stricmp( token, "width" )) 
			{                                          
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				width = atoi(token);
			}
			else if ( !stricmp( token, "height" )) 
			{                                          
				pfile = gEngfuncs.COM_ParseFile(pfile, token);
				height = atoi(token);
			}
		}
	}

	if(!section) return pfile;//data not found

	//calculate sprite position
	p->rc.left = x;
	p->rc.right = x + width; 
	p->rc.top = y;
	p->rc.bottom = y + height;

	//write resolution for backward compatibility
	if (ScreenWidth < 640) p->iRes = 320;
	else p->iRes = 640;

	memcpy( result, p, sizeof(client_sprite_t));
	return pfile;
}

client_sprite_t *SPR_GetList( char *psz, int *piCount )
{
	int iSprCount = 0;
	char *pfile = (char *)gEngfuncs.COM_LoadFile( psz, 5, NULL);
	if (!pfile)
	{
		*piCount = iSprCount;
		return NULL;
	}
	
	char token[256];
	char *plist = pfile;

	while ( pfile ) //calculate count of sprites
	{
		pfile = gEngfuncs.COM_ParseFile(pfile, token);
		if ( !stricmp( token, "hudsprite" )) iSprCount++;
	}

          client_sprite_t *phud = new client_sprite_t[iSprCount];
          
	for(int i = 0; i < iSprCount; i++ ) //parse structures
	{
		plist = ParseHudSprite( plist, "hudsprite", &phud[i] );
	}
          
          if(!iSprCount)Msg("SPR_GetList: %s don't have sprites\n", psz );
          gEngfuncs.COM_FreeFile( pfile );
          
          *piCount = iSprCount;
	return phud;
}

/*
====================
Sys LoadGameDLL

====================
*/
bool Sys_LoadLibrary (const char* dllname, dllhandle_t* handle, const dllfunction_t *fcts)
{
	const dllfunction_t *gamefunc;
	char dllpath[128];
	dllhandle_t dllhandle = 0;

	if (handle == NULL) return false;

	// Initializations
	for (gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++)
		*gamefunc->funcvariable = NULL;

	// Try every possible name
	

	sprintf(dllpath, "%s/cl_dlls/%s", gEngfuncs.pfnGetGameDirectory(), dllname);
	dllhandle = LoadLibrary (dllpath);
        
	// No DLL found
	if (! dllhandle) return false;

	// Get the function adresses
	for (gamefunc = fcts; gamefunc && gamefunc->name != NULL; gamefunc++)
		if (!(*gamefunc->funcvariable = (void *) Sys_GetProcAddress (dllhandle, gamefunc->name)))
		{
			Sys_UnloadLibrary (&dllhandle);
			return false;
		}
          
	Msg("%s loaded succesfully!\n", dllname);
	*handle = dllhandle;
	return true;
}


void Sys_UnloadLibrary (dllhandle_t* handle)
{
	if (handle == NULL || *handle == NULL)
		return;

	FreeLibrary (*handle);
	*handle = NULL;
}

void* Sys_GetProcAddress (dllhandle_t handle, const char* name)
{
	return (void *)GetProcAddress (handle, name);
}

