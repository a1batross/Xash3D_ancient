/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== lights.cpp ========================================================

  spawn and think functions for editor-placed lights

*/

#include "extdll.h"
#include "utils.h"
#include "cbase.h"

class CLight : public CBaseLogic
{
public:
	void	KeyValue( KeyValueData* pkvd ); 
	void	Spawn( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void	Think( void );

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	int	GetStyle( void ) { return m_iszCurrentStyle; };
	void	SetStyle( int iszPattern );
	void	SetCorrectStyle( void );

private:
	int		m_iOnStyle; // style to use while on
	int		m_iOffStyle; // style to use while off
	int		m_iTurnOnStyle; // style to use while turning on
	int		m_iTurnOffStyle; // style to use while turning off
	int		m_iTurnOnTime; // time taken to turn on
	int		m_iTurnOffTime; // time taken to turn off
	int		m_iszPattern; // custom style to use while on
	int		m_iszCurrentStyle; // current style string
};
LINK_ENTITY_TO_CLASS( light, CLight );
LINK_ENTITY_TO_CLASS( light_spot, CLight );

TYPEDESCRIPTION	CLight::m_SaveData[] = 
{
	DEFINE_FIELD( CLight, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CLight, m_iszPattern, FIELD_STRING ),
	DEFINE_FIELD( CLight, m_iszCurrentStyle, FIELD_STRING ),
	DEFINE_FIELD( CLight, m_iOnStyle, FIELD_INTEGER ),
	DEFINE_FIELD( CLight, m_iOffStyle, FIELD_INTEGER ),
	DEFINE_FIELD( CLight, m_iTurnOnStyle, FIELD_INTEGER ),
	DEFINE_FIELD( CLight, m_iTurnOffStyle, FIELD_INTEGER ),
	DEFINE_FIELD( CLight, m_iTurnOnTime, FIELD_INTEGER ),
	DEFINE_FIELD( CLight, m_iTurnOffTime, FIELD_INTEGER ),
};
IMPLEMENT_SAVERESTORE( CLight, CBaseLogic );

//
// Cache user-entity-field values until spawn is called.
//
void CLight :: KeyValue( KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "m_iOnStyle"))
	{
		m_iOnStyle = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iOffStyle"))
	{
		m_iOffStyle = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOnStyle"))
	{
		m_iTurnOnStyle = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOffStyle"))
	{
		m_iTurnOffStyle = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOnTime"))
	{
		m_iTurnOnTime = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "m_iTurnOffTime"))
	{
		m_iTurnOffTime = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		pev->angles.x = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "pattern"))
	{
		m_iszPattern = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else	CBaseEntity::KeyValue( pkvd );
}

void CLight :: SetStyle ( int iszPattern )
{
	if (m_iStyle < 32) // if it's using a global style, don't change it
		return;
	m_iszCurrentStyle = iszPattern;
	LIGHT_STYLE(m_iStyle, (char *)STRING( iszPattern ));
}

void CLight :: SetCorrectStyle ( void )
{
	if (m_iStyle >= 32)
	{
		switch (m_iState)
		{
		case STATE_ON:
			if (m_iszPattern) // custom styles have priority over standard ones
				SetStyle( m_iszPattern );
			else if (m_iOnStyle)
				SetStyle(GetStdLightStyle(m_iOnStyle));
			else	SetStyle(MAKE_STRING("m"));
			break;
		case STATE_OFF:
			if (m_iOffStyle)
				SetStyle(GetStdLightStyle(m_iOffStyle));
			else	SetStyle(MAKE_STRING("a"));
			break;
		case STATE_TURN_ON:
			if (m_iTurnOnStyle)
				SetStyle(GetStdLightStyle(m_iTurnOnStyle));
			else	SetStyle(MAKE_STRING("a"));
			break;
		case STATE_TURN_OFF:
			if (m_iTurnOffStyle)
				SetStyle(GetStdLightStyle(m_iTurnOffStyle));
			else	SetStyle(MAKE_STRING("m"));
			break;
		}
	}
	else	m_iszCurrentStyle = GetStdLightStyle( m_iStyle );
}

void CLight :: Think( void )
{
	switch (GetState())
	{
	case STATE_TURN_ON:
		m_iState = STATE_ON;
		UTIL_FireTargets( pev->target, this, this, USE_ON );
		break;
	case STATE_TURN_OFF:
		m_iState = STATE_OFF;
		UTIL_FireTargets(pev->target, this, this, USE_OFF );
		break;
	}
	SetCorrectStyle();
}

void CLight :: Spawn( void )
{
	if (FStringNull(pev->targetname))
	{       	// inert light
		REMOVE_ENTITY(ENT(pev));
		return;
	}
	
	if (FBitSet(pev->spawnflags,SF_LIGHT_START_OFF))
		m_iState = STATE_OFF;
	else	m_iState = STATE_ON;
	SetCorrectStyle();
}

void CLight :: Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (m_iStyle >= 32)
	{
		if (useType == USE_TOGGLE)
		{
			if(m_iState == STATE_ON || m_iState == STATE_TURN_ON) useType = USE_OFF;
			else useType = USE_ON;
		}
		if (useType == USE_ON)
		{
			if (m_iTurnOnTime)
			{
				m_iState = STATE_TURN_ON;
				SetNextThink( m_iTurnOnTime );
			}
			else	m_iState = STATE_ON;
		}
		else if(useType == USE_OFF)
          	{
			if (m_iTurnOffTime)
			{
				m_iState = STATE_TURN_OFF;
				SetNextThink( m_iTurnOffTime );
			}
			else	m_iState = STATE_OFF;
          	}
		SetCorrectStyle();
	}
}

class CEnvLight : public CLight
{
public:
	void	KeyValue( KeyValueData* pkvd ); 
	void	Spawn( void );
};
LINK_ENTITY_TO_CLASS( light_environment, CEnvLight );

void CEnvLight::KeyValue( KeyValueData* pkvd )
{
	if (FStrEq(pkvd->szKeyName, "_light"))
	{
		int r, g, b, v, j;
		char szColor[64];
		j = sscanf( pkvd->szValue, "%d %d %d %d\n", &r, &g, &b, &v );
		if (j == 1)
		{
			g = b = r;
		}
		else if (j == 4)
		{
			r = r * (v / 255.0);
			g = g * (v / 255.0);
			b = b * (v / 255.0);
		}

		// simulate qrad direct, ambient,and gamma adjustments, as well as engine scaling
		r = pow( r / 114.0, 0.6 ) * 264;
		g = pow( g / 114.0, 0.6 ) * 264;
		b = pow( b / 114.0, 0.6 ) * 264;

		pkvd->fHandled = TRUE;
		sprintf( szColor, "%d", r );
		CVAR_SET_STRING( "sv_skycolor_r", szColor );
		sprintf( szColor, "%d", g );
		CVAR_SET_STRING( "sv_skycolor_g", szColor );
		sprintf( szColor, "%d", b );
		CVAR_SET_STRING( "sv_skycolor_b", szColor );
	}
	else if (FStrEq(pkvd->szKeyName, "pitch"))
	{
		pev->angles.x = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
}


void CEnvLight :: Spawn( void )
{
	char szVector[64];
	UTIL_MakeAimVectors( pev->angles );

	sprintf( szVector, "%f", gpGlobals->v_forward.x );
	CVAR_SET_STRING( "sv_skyvec_x", szVector );
	sprintf( szVector, "%f", gpGlobals->v_forward.y );
	CVAR_SET_STRING( "sv_skyvec_y", szVector );
	sprintf( szVector, "%f", gpGlobals->v_forward.z );
	CVAR_SET_STRING( "sv_skyvec_z", szVector );

	CLight::Spawn( );
}