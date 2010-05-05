//=======================================================================
//			Copyright (C) Shambler Team 2005
//			rain.cpp - TriAPI weather effects
//			based on original code from BUzer  
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "effects_api.h"
#include "triangle_api.h"
#include "ref_params.h"
#include "hud.h"
#include "r_weather.h"

extern ref_params_t		*gpViewParams;

void WaterLandingEffect( cl_drip *drip );
void ParseRainFile( void );

cl_drip	FirstChainDrip;
cl_rainfx	FirstChainFX;

double	rain_curtime;    // current time
double	rain_oldtime;    // last time we have updated drips
double	rain_timedelta;  // difference between old time and current time
double	rain_nextspawntime;  // when the next drip should be spawned
cvar_t	*cl_debugrain;
int	dripcounter = 0;
int	fxcounter = 0;
HSPRITE	hsprWaterRing;
HSPRITE	hsprDripTexture;

/*
=================================
ProcessRain

Must think every frame.
=================================
*/
void ProcessRain( void )
{
	rain_oldtime = rain_curtime; // save old time
	rain_curtime = GetClientTime();
	rain_timedelta = rain_curtime - rain_oldtime;

	// first frame
	if ( rain_oldtime == 0 )
	{
		// fix first frame bug with nextspawntime
		rain_nextspawntime = GetClientTime();
		ParseRainFile();
		return;
	}

	if ( gHUD.Rain.dripsPerSecond == 0 && FirstChainDrip.p_Next == NULL )
	{
		// keep nextspawntime correct
		rain_nextspawntime = rain_curtime;
		return;
	}

	if ( gpViewParams->paused ) return; // not in pause

	double timeBetweenDrips = 1 / (double)gHUD.Rain.dripsPerSecond;

	cl_drip* curDrip = FirstChainDrip.p_Next;
	cl_drip* nextDrip = NULL;

	edict_t *player = GetLocalPlayer();

	if( !player ) return; // not in game

	// save debug info
	float debug_lifetime = 0;
	int debug_howmany = 0;
	int debug_attempted = 0;
	int debug_dropped = 0;

	while ( curDrip != NULL ) // go through list
	{
		nextDrip = curDrip->p_Next; // save pointer to next drip

		if ( gHUD.Rain.weatherMode == 0 )
			curDrip->origin.z -= rain_timedelta * DRIPSPEED; // rain
		else curDrip->origin.z -= rain_timedelta * SNOWSPEED; // snow

		curDrip->origin.x += rain_timedelta * curDrip->xDelta;
		curDrip->origin.y += rain_timedelta * curDrip->yDelta;
		
		// remove drip if its origin lower than minHeight
		if ( curDrip->origin.z < curDrip->minHeight ) 
		{
			if ( curDrip->landInWater/* && gHUD.Rain.weatherMode == 0*/ )
				WaterLandingEffect( curDrip ); // create water rings

			if ( cl_debugrain->value )
			{
				debug_lifetime += (rain_curtime - curDrip->birthTime);
				debug_howmany++;
			}
			
			curDrip->p_Prev->p_Next = curDrip->p_Next; // link chain
			if ( nextDrip != NULL )
				nextDrip->p_Prev = curDrip->p_Prev; 
			delete curDrip;
					
			dripcounter--;
		}

		curDrip = nextDrip; // restore pointer, so we can continue moving through chain
	}

	int maxDelta; // maximum height randomize distance
	float falltime;
	if ( gHUD.Rain.weatherMode == 0 )
	{
		maxDelta = DRIPSPEED * rain_timedelta;		// for rain
		falltime = (gHUD.Rain.globalHeight + 4096) / DRIPSPEED;
	}
	else
	{
		maxDelta = SNOWSPEED * rain_timedelta;		// for snow
		falltime = (gHUD.Rain.globalHeight + 4096) / SNOWSPEED;
	}

	while ( rain_nextspawntime < rain_curtime )
	{
		rain_nextspawntime += timeBetweenDrips;		
		if ( cl_debugrain->value ) debug_attempted++;
				
		if ( dripcounter < MAXDRIPS ) // check for overflow
		{
			float deathHeight;
			Vector vecStart, vecEnd;

			vecStart[0] = RANDOM_FLOAT( player->v.origin.x - gHUD.Rain.distFromPlayer, player->v.origin.x + gHUD.Rain.distFromPlayer );
			vecStart[1] = RANDOM_FLOAT( player->v.origin.y - gHUD.Rain.distFromPlayer, player->v.origin.y + gHUD.Rain.distFromPlayer );
			vecStart[2] = gHUD.Rain.globalHeight;

			float xDelta = gHUD.Rain.windX + RANDOM_FLOAT( gHUD.Rain.randX * -1, gHUD.Rain.randX );
			float yDelta = gHUD.Rain.windY + RANDOM_FLOAT( gHUD.Rain.randY * -1, gHUD.Rain.randY );

			// find a point at bottom of map
			vecEnd[0] = falltime * xDelta;
			vecEnd[1] = falltime * yDelta;
			vecEnd[2] = -4096;

			TraceResult tr;
			UTIL_TraceLine( vecStart, vecStart + vecEnd, ignore_monsters, player, &tr );

			if ( tr.fStartSolid || tr.fAllSolid )
			{
				if ( cl_debugrain->value > 1 )
					debug_dropped++;
				continue; // drip cannot be placed
			}
			
			// falling to water?
			int contents = POINT_CONTENTS( tr.vecEndPos );

			if ( contents == CONTENTS_WATER )
			{
				// NOTE: in Xash3D PM_WaterEntity always return a valid water volume or NULL
				// so not needs to run additional checks here
				edict_t *pWater = g_engfuncs.pfnWaterEntity( tr.vecEndPos );
				if ( pWater )
				{
					deathHeight = pWater->v.maxs[2];
				}
				else
				{
					// hit the world
					deathHeight = tr.vecEndPos[2];
				}
			}
			else
			{
				deathHeight = tr.vecEndPos[2];
			}

			// just in case..
			if ( deathHeight > vecStart[2] )
			{
				ALERT( at_error, "rain: can't create drip in water\n" );
				continue;
			}


			cl_drip *newClDrip = new cl_drip;
			if ( !newClDrip )
			{
				gHUD.Rain.dripsPerSecond = 0; // disable rain
				ALERT( at_error, "rain: failed to allocate object!\n");
				return;
			}
			
			vecStart[2] -= RANDOM_FLOAT( 0, maxDelta ); // randomize a bit
			
			newClDrip->alpha = RANDOM_FLOAT( 0.12, 0.2 );
			newClDrip->origin = vecStart;
			
			newClDrip->xDelta = xDelta;
			newClDrip->yDelta = yDelta;
	
			newClDrip->birthTime = rain_curtime; // store time when it was spawned
			newClDrip->minHeight = deathHeight;

			if ( contents == CONTENTS_WATER )
				newClDrip->landInWater = 1;
			else newClDrip->landInWater = 0;

			// add to first place in chain
			newClDrip->p_Next = FirstChainDrip.p_Next;
			newClDrip->p_Prev = &FirstChainDrip;
			if ( newClDrip->p_Next != NULL )
				newClDrip->p_Next->p_Prev = newClDrip;
			FirstChainDrip.p_Next = newClDrip;

			dripcounter++;
		}
		else
		{
			if ( cl_debugrain->value )
				ALERT( at_error, "rain: drip limit overflow! %i > %i\n", dripcounter, MAXDRIPS );
			return;
		}
	}

	if ( cl_debugrain->value ) // print debug info
	{
		ALERT( at_console, "Rain info: Drips exist: %i\n", dripcounter );
		ALERT( at_console, "Rain info: FX's exist: %i\n", fxcounter );
		ALERT( at_console, "Rain info: Attempted/Dropped: %i, %i\n", debug_attempted, debug_dropped );
		if ( debug_howmany )
		{
			float ave = debug_lifetime / (float)debug_howmany;
			ALERT( at_console, "Rain info: Average drip life time: %f\n", ave);
		}
		else ALERT( at_console, "Rain info: Average drip life time: --\n" );
	}
	return;
}

/*
=================================
WaterLandingEffect
=================================
*/
void WaterLandingEffect( cl_drip *drip )
{
	if ( fxcounter >= MAXFX )
	{
		ALERT( at_console, "Rain error: FX limit overflow!\n" );
		return;
	}	
	
	cl_rainfx *newFX = new cl_rainfx;
	if ( !newFX )
	{
		ALERT( at_console, "Rain error: failed to allocate FX object!\n");
		return;
	}
			
	newFX->alpha = RANDOM_FLOAT( 0.6, 0.9 );
	newFX->origin = drip->origin;
	newFX->origin[2] = drip->minHeight; // correct position
			
	newFX->birthTime = GetClientTime();
	newFX->life = RANDOM_FLOAT( 0.7f, 1.0f );

	// add to first place in chain
	newFX->p_Next = FirstChainFX.p_Next;
	newFX->p_Prev = &FirstChainFX;
	if ( newFX->p_Next != NULL )
		newFX->p_Next->p_Prev = newFX;
	FirstChainFX.p_Next = newFX;
			
	fxcounter++;
}

/*
=================================
ProcessFXObjects

Remove all fx objects with out time to live
Call every frame before ProcessRain
=================================
*/
void ProcessFXObjects( void )
{
	float curtime = GetClientTime();
	
	cl_rainfx* curFX = FirstChainFX.p_Next;
	cl_rainfx* nextFX = NULL;	

	while ( curFX != NULL ) // go through FX objects list
	{
		nextFX = curFX->p_Next; // save pointer to next
		
		// delete current?
		if (( curFX->birthTime + curFX->life ) < curtime )
		{
			curFX->p_Prev->p_Next = curFX->p_Next; // link chain
			if ( nextFX != NULL )
				nextFX->p_Prev = curFX->p_Prev; 
			delete curFX;					
			fxcounter--;
		}
		curFX = nextFX; // restore pointer
	}
}

/*
=================================
ResetRain
clear memory, delete all objects
=================================
*/
void ResetRain( void )
{
	// delete all drips
	cl_drip* delDrip = FirstChainDrip.p_Next;
	FirstChainDrip.p_Next = NULL;
	
	while ( delDrip != NULL )
	{
		cl_drip* nextDrip = delDrip->p_Next; // save pointer to next drip in chain
		delete delDrip;
		delDrip = nextDrip; // restore pointer
		dripcounter--;
	}

	// delete all FX objects
	cl_rainfx* delFX = FirstChainFX.p_Next;
	FirstChainFX.p_Next = NULL;
	
	while ( delFX != NULL )
	{
		cl_rainfx* nextFX = delFX->p_Next;
		delete delFX;
		delFX = nextFX;
		fxcounter--;
	}

	InitRain();
	return;
}

/*
=================================
InitRain
initialze system
=================================
*/
void InitRain( void )
{
	cl_debugrain = CVAR_REGISTER( "cl_debugrain", "0", 0, "display rain debug info (trace missing, drops etc)" );

	gHUD.Rain.dripsPerSecond = 0;
	gHUD.Rain.distFromPlayer = 0;
	gHUD.Rain.windX = 0;
	gHUD.Rain.windY = 0;
	gHUD.Rain.randX = 0;
	gHUD.Rain.randY = 0;
	gHUD.Rain.weatherMode = 0;
	gHUD.Rain.globalHeight = 0;

	FirstChainDrip.birthTime = 0;
	FirstChainDrip.minHeight = 0;
	FirstChainDrip.origin[0]=0;
	FirstChainDrip.origin[1]=0;
	FirstChainDrip.origin[2]=0;
	FirstChainDrip.alpha = 0;
	FirstChainDrip.xDelta = 0;
	FirstChainDrip.yDelta = 0;
	FirstChainDrip.landInWater = 0;
	FirstChainDrip.p_Next = NULL;
	FirstChainDrip.p_Prev = NULL;

	FirstChainFX.alpha = 0;
	FirstChainFX.birthTime = 0;
	FirstChainFX.life = 0;
	FirstChainFX.origin[0] = 0;
	FirstChainFX.origin[1] = 0;
	FirstChainFX.origin[2] = 0;
	FirstChainFX.p_Next = NULL;
	FirstChainFX.p_Prev = NULL;
	
	rain_oldtime = 0;
	rain_curtime = 0;
	rain_nextspawntime = 0;

	// engine will be free unused shaders after each change map
	// so reload it again
	hsprWaterRing = 0;
	hsprDripTexture = 0;

	return;
}

/*
===========================
ParseRainFile

List of settings:
	drips 		- max raindrips\snowflakes
	distance 	- radius of rain\snow
	windx 		- wind shift X
	windy 		- wind shift Y
	randx 		- random shift X
	randy 		- random shift Y
	mode 		- rain = 0\snow =1
	height		- max height to create raindrips\snowflakes
===========================
*/
void ParseRainFile( void )
{
	if ( gHUD.Rain.distFromPlayer != 0 || gHUD.Rain.dripsPerSecond != 0 || gHUD.Rain.globalHeight != 0 )
		return;

	char mapname[256], filepath[256];
	const char *token = NULL;
	const char *pfile;
	char *afile;

	strcpy( mapname, STRING( gpGlobals->mapname ));
	if ( strlen( mapname ) == 0 )
	{
		ALERT( at_error, "rain: unable to read map name\n" );
		return;
	}

//	Xash3D always set bare mapname without path and extension
//	mapname[strlen(mapname)-4] = 0;

	sprintf( filepath, "scripts/weather/%s.txt", mapname ); 

	afile = (char *)LOAD_FILE( filepath, NULL );
	pfile = afile;

	if ( !pfile )
	{
		if (cl_debugrain->value > 1 )
			ALERT( at_error, "rain: couldn't open rain settings file %s\n", mapname );
		return;
	}

	while ( true )
	{
		token = COM_ParseToken( &pfile );
		if( !token ) break;

		if ( !stricmp( token, "drips" )) // dripsPerSecond
		{
			token = COM_ParseToken( &pfile );
			gHUD.Rain.dripsPerSecond = atoi( token );
		}
		else if ( !stricmp( token, "distance" )) // distFromPlayer
		{
			token = COM_ParseToken( &pfile );
			gHUD.Rain.distFromPlayer = atof( token );
		}
		else if ( !stricmp( token, "windx" )) // windX
		{
			token = COM_ParseToken( &pfile );
			gHUD.Rain.windX = atof( token );
		}
		else if ( !stricmp( token, "windy" )) // windY
		{
			token = COM_ParseToken( &pfile );
			gHUD.Rain.windY = atof( token );		
		}
		else if ( !stricmp( token, "randx" )) // randX
		{
			token = COM_ParseToken( &pfile );
			gHUD.Rain.randX = atof( token );
		}
		else if ( !stricmp( token, "randy" )) // randY
		{
			token = COM_ParseToken( &pfile );
			gHUD.Rain.randY = atof( token );
		}
		else if ( !stricmp( token, "mode" )) // weatherMode
		{
			token = COM_ParseToken( &pfile );
			gHUD.Rain.weatherMode = atoi( token );
		}
		else if ( !stricmp( token, "height" )) // globalHeight
		{
			token = COM_ParseToken( &pfile );
			gHUD.Rain.globalHeight = atof( token );
		}
		else ALERT( at_error, "rain: unknown token %s in file %s\n", token, mapname );
	}
	FREE_FILE( afile );
}

//-----------------------------------------------------

void SetPoint( float x, float y, float z, float (*matrix)[4] )
{
	Vector point = Vector( x, y, z ), result;

	result.x = DotProduct( point, matrix[0] ) + matrix[0][3];
	result.y = DotProduct( point, matrix[1] ) + matrix[1][3];
	result.z = DotProduct( point, matrix[2] ) + matrix[2][3];

	g_engfuncs.pTriAPI->Vertex3f( result[0], result[1], result[2] );
}

/*
=================================
DrawRain

draw raindrips and snowflakes
=================================
*/
void DrawRain( void )
{
	if ( FirstChainDrip.p_Next == NULL )
		return; // no drips to draw

	if( hsprDripTexture == 0 )
	{
		if ( gHUD.Rain.weatherMode == 0 )
		{
			// load rain sprite
			int modelIndex = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/hi_rain.spr" );
			hsprDripTexture = g_engfuncs.pTriAPI->GetSpriteTexture( modelIndex, 0 );
		}
		else
		{
			// load snow sprite
			int modelIndex = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/snowflake.spr" );
			hsprDripTexture = g_engfuncs.pTriAPI->GetSpriteTexture( modelIndex, 0 );
          	}
	}
	if( hsprDripTexture <= 0 ) return;

	float visibleHeight = gHUD.Rain.globalHeight - SNOWFADEDIST;

	// usual triapi stuff
	g_engfuncs.pTriAPI->Enable( TRI_SHADER );
	g_engfuncs.pTriAPI->Bind( hsprDripTexture, 0 );
	g_engfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	g_engfuncs.pTriAPI->CullFace( TRI_NONE );
	
	// go through drips list
	cl_drip *Drip = FirstChainDrip.p_Next;
	edict_t *player = GetLocalPlayer();
	if( !player ) return; // not in game yet

	if ( gHUD.Rain.weatherMode == 0 ) // draw rain
	{
		while ( Drip != NULL )
		{
			cl_drip* nextdDrip = Drip->p_Next;
					
			Vector2D toPlayer; 
			toPlayer.x = player->v.origin[0] - Drip->origin[0];
			toPlayer.y = player->v.origin[1] - Drip->origin[1];
			toPlayer = toPlayer.Normalize();
	
			toPlayer.x *= DRIP_SPRITE_HALFWIDTH;
			toPlayer.y *= DRIP_SPRITE_HALFWIDTH;

			float shiftX = (Drip->xDelta / DRIPSPEED) * DRIP_SPRITE_HALFHEIGHT;
			float shiftY = (Drip->yDelta / DRIPSPEED) * DRIP_SPRITE_HALFHEIGHT;

			g_engfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, Drip->alpha );
			g_engfuncs.pTriAPI->Begin( TRI_TRIANGLES );

				g_engfuncs.pTriAPI->TexCoord2f( 0, 0 );
				g_engfuncs.pTriAPI->Vertex3f( Drip->origin[0]-toPlayer.y - shiftX, Drip->origin[1]+toPlayer.x - shiftY,Drip->origin[2] + DRIP_SPRITE_HALFHEIGHT );

				g_engfuncs.pTriAPI->TexCoord2f( 0.5, 1 );
				g_engfuncs.pTriAPI->Vertex3f( Drip->origin[0] + shiftX, Drip->origin[1] + shiftY, Drip->origin[2]-DRIP_SPRITE_HALFHEIGHT );

				g_engfuncs.pTriAPI->TexCoord2f( 1, 0 );
				g_engfuncs.pTriAPI->Vertex3f( Drip->origin[0]+toPlayer.y - shiftX, Drip->origin[1]-toPlayer.x - shiftY, Drip->origin[2]+DRIP_SPRITE_HALFHEIGHT);
	
			g_engfuncs.pTriAPI->End();
			Drip = nextdDrip;
		}
	}

	else	// draw snow
	{ 
		Vector normal;

		GetViewAngles( (float *)normal );
	
		float matrix[3][4];
		AngleMatrix( normal, matrix ); // calc view matrix

		while ( Drip != NULL )
		{
			cl_drip* nextdDrip = Drip->p_Next;

			matrix[0][3] = Drip->origin[0]; // write origin to matrix
			matrix[1][3] = Drip->origin[1];
			matrix[2][3] = Drip->origin[2];

			// apply start fading effect
			float alpha = (Drip->origin[2] <= visibleHeight) ? Drip->alpha : ((gHUD.Rain.globalHeight - Drip->origin[2]) / (float)SNOWFADEDIST) * Drip->alpha;
					
			g_engfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, alpha );
			g_engfuncs.pTriAPI->Begin( TRI_QUADS );

				g_engfuncs.pTriAPI->TexCoord2f( 0, 0 );
				SetPoint( 0, SNOW_SPRITE_HALFSIZE, SNOW_SPRITE_HALFSIZE, matrix );

				g_engfuncs.pTriAPI->TexCoord2f( 0, 1 );
				SetPoint( 0, SNOW_SPRITE_HALFSIZE, -SNOW_SPRITE_HALFSIZE, matrix );

				g_engfuncs.pTriAPI->TexCoord2f( 1, 1 );
				SetPoint( 0, -SNOW_SPRITE_HALFSIZE, -SNOW_SPRITE_HALFSIZE, matrix );

				g_engfuncs.pTriAPI->TexCoord2f( 1, 0 );
				SetPoint( 0, -SNOW_SPRITE_HALFSIZE, SNOW_SPRITE_HALFSIZE, matrix );
				
			g_engfuncs.pTriAPI->End();
			Drip = nextdDrip;
		}
	}
	g_engfuncs.pTriAPI->Disable( TRI_SHADER );
}

/*
=================================
DrawFXObjects
=================================
*/
void DrawFXObjects( void )
{
	if ( FirstChainFX.p_Next == NULL ) 
		return; // no objects to draw

	float curtime = GetClientTime();

	// usual triapi stuff
	if( hsprWaterRing == 0 )	// in case what we don't want search it if not found
	{
		// load water ring sprite
		int modelIndex = g_engfuncs.pEventAPI->EV_FindModelIndex( "sprites/waterring.spr" );
		hsprWaterRing = g_engfuncs.pTriAPI->GetSpriteTexture( modelIndex, 0 );
	}

	if( hsprWaterRing <= 0 ) return; // don't waste time

	g_engfuncs.pTriAPI->Enable( TRI_SHADER );
	g_engfuncs.pTriAPI->Bind( hsprWaterRing, 0 );
	g_engfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	g_engfuncs.pTriAPI->CullFace( TRI_NONE );
	
	// go through objects list
	cl_rainfx* curFX = FirstChainFX.p_Next;
	while ( curFX != NULL )
	{
		cl_rainfx* nextFX = curFX->p_Next;
					
		// fadeout
		float alpha = ((curFX->birthTime + curFX->life - curtime) / curFX->life) * curFX->alpha;
		float size = (curtime - curFX->birthTime) * MAXRINGHALFSIZE;
		float color[3];

		// UNDONE: calc lighting right
		g_engfuncs.pEfxAPI->R_LightForPoint( (const float *)curFX->origin, color );
//		ALERT( at_console, "color %g %g %g\n", color[0], color[1], color[2] );

		g_engfuncs.pTriAPI->Color4f( 0.4 + color[0], 0.4 + color[1], 0.4 + color[2], alpha );
		g_engfuncs.pTriAPI->Begin( TRI_QUADS );

			g_engfuncs.pTriAPI->TexCoord2f( 0, 0 );
			g_engfuncs.pTriAPI->Vertex3f( curFX->origin[0] - size, curFX->origin[1] - size, curFX->origin[2] );

			g_engfuncs.pTriAPI->TexCoord2f( 0, 1 );
			g_engfuncs.pTriAPI->Vertex3f( curFX->origin[0] - size, curFX->origin[1] + size, curFX->origin[2] );

			g_engfuncs.pTriAPI->TexCoord2f( 1, 1 );
			g_engfuncs.pTriAPI->Vertex3f( curFX->origin[0] + size, curFX->origin[1] + size, curFX->origin[2] );

			g_engfuncs.pTriAPI->TexCoord2f( 1, 0 );
			g_engfuncs.pTriAPI->Vertex3f( curFX->origin[0] + size, curFX->origin[1] - size, curFX->origin[2] );
	
		g_engfuncs.pTriAPI->End();
		curFX = nextFX;
	}
	g_engfuncs.pTriAPI->Disable( TRI_SHADER );
}

/*
=================================
DrawAll
=================================
*/

void R_DrawWeather( void )
{
	ProcessFXObjects();
	ProcessRain();
	DrawRain();
	DrawFXObjects();
}