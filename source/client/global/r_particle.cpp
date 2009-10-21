//=======================================================================
//			Copyright XashXT Group 2008 ©
//		r_particle.cpp - famous Laurie Cheers Aurora system 
//=======================================================================

#include "extdll.h"
#include "utils.h"
#include "triangle_api.h"
#include "hud.h"
#include "r_particle.h"

ParticleSystemManager* g_pParticleSystems = NULL;

void CreateAurora( int idx, char *file )
{
	ParticleSystem *pSystem = new ParticleSystem( idx, file );
	g_pParticleSystems->AddSystem( pSystem );
}


ParticleSystemManager::ParticleSystemManager( void )
{
	m_pFirstSystem = NULL;
}

ParticleSystemManager::~ParticleSystemManager( void )
{
	ClearSystems();
}

void ParticleSystemManager::AddSystem( ParticleSystem* pNewSystem )
{
	pNewSystem->m_pNextSystem = m_pFirstSystem;
	m_pFirstSystem = pNewSystem;
}

ParticleSystem *ParticleSystemManager::FindSystem( edict_t* pEntity )
{
	for( ParticleSystem *pSys = m_pFirstSystem; pSys; pSys = pSys->m_pNextSystem )
	{
		if( pEntity->serialnumber == pSys->m_iEntIndex )
			return pSys;
	}
	return NULL;
}

// blended particles don't use the z-buffer, so we need to sort them before drawing.
// for efficiency, only the systems are sorted - individual particles just get drawn in order of creation.
// (this should actually make things look better - no ugly popping when one particle passes through another.)
void ParticleSystemManager::SortSystems( void )
{
	ParticleSystem* pSystem;
	ParticleSystem* pLast;
	ParticleSystem* pBeforeCompare, *pCompare;
	
	if (!m_pFirstSystem) return;

	// calculate how far away each system is from the viewer
	for( pSystem = m_pFirstSystem; pSystem; pSystem = pSystem->m_pNextSystem )
		pSystem->CalculateDistance();

	// do an insertion sort on the systems
	pLast = m_pFirstSystem;
	pSystem = pLast->m_pNextSystem;
	while (pSystem)
	{
		if (pLast->m_fViewerDist < pSystem->m_fViewerDist)
		{
			// pSystem is in the wrong place! First, let's unlink it from the list
			pLast->m_pNextSystem = pSystem->m_pNextSystem;

			// then find somewhere to insert it
			if (m_pFirstSystem == pLast || m_pFirstSystem->m_fViewerDist < pSystem->m_fViewerDist)
			{
				// pSystem comes before the first system, insert it there
				pSystem->m_pNextSystem = m_pFirstSystem;
				m_pFirstSystem = pSystem;
			}
			else
			{
				// insert pSystem somewhere within the sorted part of the list
				pBeforeCompare = m_pFirstSystem;
				pCompare = pBeforeCompare->m_pNextSystem;
				while (pCompare != pLast)
				{
					if (pCompare->m_fViewerDist < pSystem->m_fViewerDist)
					{
						// pSystem comes before pCompare. We've found where it belongs.
						break;
					}

					pBeforeCompare = pCompare;
					pCompare = pBeforeCompare->m_pNextSystem;
				}

				// we've found where pSystem belongs. Insert it between pBeforeCompare and pCompare.
				pBeforeCompare->m_pNextSystem = pSystem;
				pSystem->m_pNextSystem = pCompare;
			}
		}
		else
		{
			//pSystem is in the right place, move on
			pLast = pSystem;
		}
		pSystem = pLast->m_pNextSystem;
	}
}

void ParticleSystemManager::UpdateSystems( void )
{
	static float fOldTime, fTime;
	fOldTime = fTime;
	fTime = GetClientTime();
	float frametime = fTime - fOldTime;
	
	ParticleSystem* pSystem;
	ParticleSystem* pLast = NULL;
	ParticleSystem*pLastSorted = NULL;

//	SortSystems();

	pSystem = m_pFirstSystem;
	while( pSystem )
	{
		if( pSystem->UpdateSystem( frametime ))
		{
			pSystem->DrawSystem();
			pLast = pSystem;
			pSystem = pSystem->m_pNextSystem;
		}
		else // delete this system
		{
			if (pLast)
			{
				pLast->m_pNextSystem = pSystem->m_pNextSystem;
				delete pSystem;
				pSystem = pLast->m_pNextSystem;
			}
			else // deleting the first system
			{
				m_pFirstSystem = pSystem->m_pNextSystem;
				delete pSystem;
				pSystem = m_pFirstSystem;
			}
		}
	}
	g_engfuncs.pTriAPI->RenderMode(kRenderNormal);
}

void ParticleSystemManager::ClearSystems( void )
{
	ParticleSystem* pSystem = m_pFirstSystem;
	ParticleSystem* pTemp;

	while( pSystem )
	{
		pTemp = pSystem->m_pNextSystem;
		delete pSystem;
		pSystem = pTemp;
	}

	m_pFirstSystem = NULL;
}

float ParticleSystem::c_fCosTable[360 + 90];
bool ParticleSystem::c_bCosTableInit = false;

ParticleType::ParticleType( ParticleType *pNext )
{
	m_pSprayType = m_pOverlayType = NULL;
	m_StartAngle = RandomRange(45);
	m_hSprite = 0;
	m_pNext = pNext;
	m_szName[0] = 0;
	m_StartRed = m_StartGreen = m_StartBlue = m_StartAlpha = RandomRange(1);
	m_EndRed = m_EndGreen = m_EndBlue = m_EndAlpha = RandomRange(1);
	m_iRenderMode = kRenderTransAdd;
	m_iDrawCond = 0;
	m_bEndFrame = false;

	m_bIsDefined = false;
	m_iCollision = 0;
}

particle* ParticleType::CreateParticle(ParticleSystem *pSys)//particle *pPart)
{
	if (!pSys) return NULL;

	particle *pPart = pSys->ActivateParticle();
	if (!pPart) return NULL;

	pPart->age = 0.0;
	pPart->age_death = m_Life.GetInstance();

	InitParticle(pPart, pSys);

	return pPart;
}

void ParticleType::InitParticle(particle *pPart, ParticleSystem *pSys)
{
	float fLifeRecip = 1/pPart->age_death;

	particle *pOverlay = NULL;
	if (m_pOverlayType)
	{
		// create an overlay for this particle
		pOverlay = pSys->ActivateParticle();
		if (pOverlay)
		{
			pOverlay->age = pPart->age;
			pOverlay->age_death = pPart->age_death;
			m_pOverlayType->InitParticle(pOverlay, pSys);
		}
	}

	pPart->m_pOverlay = pOverlay;

	pPart->pType = this;
	pPart->velocity[0] = pPart->velocity[1] = pPart->velocity[2] = 0;
	pPart->accel[0] = pPart->accel[1] = 0;
	pPart->accel[2] = m_Gravity.GetInstance();
	pPart->m_iEntIndex = 0;

	if (m_pSprayType)
	{
		pPart->age_spray = 1/m_SprayRate.GetInstance();
	}
	else
	{
		pPart->age_spray = 0.0f;
	}

	pPart->m_fSize = m_StartSize.GetInstance();

	if (m_EndSize.IsDefined())
		pPart->m_fSizeStep = m_EndSize.GetOffset(pPart->m_fSize) * fLifeRecip;
	else
		pPart->m_fSizeStep = m_SizeDelta.GetInstance();
	//pPart->m_fSizeStep = m_EndSize.GetOffset(pPart->m_fSize) * fLifeRecip;

	pPart->frame = m_StartFrame.GetInstance();
	if (m_EndFrame.IsDefined())
		pPart->m_fFrameStep = m_EndFrame.GetOffset(pPart->frame) * fLifeRecip;
	else	pPart->m_fFrameStep = m_FrameRate.GetInstance();

	pPart->m_fAlpha = m_StartAlpha.GetInstance();
	pPart->m_fAlphaStep = m_EndAlpha.GetOffset(pPart->m_fAlpha) * fLifeRecip;
	pPart->m_fRed = m_StartRed.GetInstance();
	pPart->m_fRedStep = m_EndRed.GetOffset(pPart->m_fRed) * fLifeRecip;
	pPart->m_fGreen = m_StartGreen.GetInstance();
	pPart->m_fGreenStep = m_EndGreen.GetOffset(pPart->m_fGreen) * fLifeRecip;
	pPart->m_fBlue = m_StartBlue.GetInstance();
	pPart->m_fBlueStep = m_EndBlue.GetOffset(pPart->m_fBlue) * fLifeRecip;

	pPart->m_fAngle = m_StartAngle.GetInstance();
	pPart->m_fAngleStep = m_AngleDelta.GetInstance();

	pPart->m_fDrag = m_Drag.GetInstance();

	float fWindStrength = m_WindStrength.GetInstance();
	float fWindYaw = m_WindYaw.GetInstance();
	pPart->m_vecWind.x = fWindStrength*ParticleSystem::CosLookup(fWindYaw);
	pPart->m_vecWind.y = fWindStrength*ParticleSystem::SinLookup(fWindYaw);
	pPart->m_vecWind.z = 0;
}

//============================================


RandomRange::RandomRange( char *szToken )
{
	char *cOneDot = NULL;
	m_bDefined = true;

	for( char *c = szToken; *c; c++ )
	{
		if (*c == '.')
		{
			if (cOneDot != NULL)
			{
				// found two dots in a row - it's a range

				*cOneDot = 0; // null terminate the first number
				m_fMin = atof( szToken ); // parse the first number
				*cOneDot = '.'; // change it back, just in case
				c++;
				m_fMax = atof( c ); // parse the second number
				return;
			}
			else
			{
				cOneDot = c;
			}
		}
		else
		{
			cOneDot = NULL;
		}
	}

	// no range, just record the number
	m_fMin = m_fMax = atof( szToken );
}

//============================================

ParticleSystem::ParticleSystem( int iEntIndex, char *szFilename )
{
	int iParticles = 100; // default

	m_iEntIndex = iEntIndex;
	m_pNextSystem = NULL;
	m_pFirstType = NULL;

	if ( !c_bCosTableInit )
	{
		for ( int i = 0; i < 360 + 90; i++ )
		{
			c_fCosTable[i] = cos( i * M_PI / 180.0 );
		}
		c_bCosTableInit = true;
	}

	const char *memFile;
	const char *szFile = (char *)LOAD_FILE( szFilename, NULL );
	char *szToken;

	if( !szFile )
	{
		ALERT( at_error, "particle %s not found.\n", szFilename );
		return;
	}
	else
	{
		memFile = szFile;		
		szToken = COM_ParseToken( &szFile );

		while ( szToken )
		{
			if ( !stricmp( szToken, "particles" ) )
			{
				szToken = COM_ParseToken( &szFile );
				iParticles = atof(szToken);
			}
			else if ( !stricmp( szToken, "maintype" ) )
			{
				szToken = COM_ParseToken( &szFile );
				m_pMainType = AddPlaceholderType(szToken);
			}
			else if ( !stricmp( szToken, "attachment" ) )
			{
				szToken = COM_ParseToken( &szFile );
				m_iEntAttachment = atof(szToken);
			}
			else if ( !stricmp( szToken, "killcondition" ) )
			{
				szToken = COM_ParseToken( &szFile );
				if ( !stricmp( szToken, "empty" ) )
				{
					m_iKillCondition = CONTENTS_NONE;
				}
				else if ( !stricmp( szToken, "water" ) )
				{
					m_iKillCondition = CONTENTS_WATER;
				}
				else if ( !stricmp( szToken, "solid" ) )
				{
					m_iKillCondition = CONTENTS_SOLID;
				}
			}
			else if ( !stricmp( szToken, "{" ) )
			{
				// parse new type
				this->ParseType( &szFile ); // parses the type, moves the file pointer
			}

			szToken = COM_ParseToken( &szFile );
		}
	}
		
	FREE_FILE( (void *)memFile );
	AllocateParticles( iParticles );
}

void ParticleSystem::AllocateParticles( int iParticles )
{
	m_pAllParticles = new particle[iParticles];
	m_pFreeParticle = m_pAllParticles;
	m_pActiveParticle = NULL;
	m_pMainParticle = NULL;

	// initialise the linked list
	particle *pLast = m_pAllParticles;
	particle *pParticle = pLast+1;

	for( int i = 1;  i < iParticles;  i++ )
	{
		pLast->nextpart = pParticle;

		pLast = pParticle;
		pParticle++;
	}
	pLast->nextpart = NULL;
}

ParticleSystem::~ParticleSystem( void )
{
	delete[] m_pAllParticles;

	ParticleType *pType = m_pFirstType;
	ParticleType *pNext;
	while (pType)
	{
		pNext = pType->m_pNext;
		delete pType;
		pType = pNext;
	}
}



// returns the ParticleType with the given name, if there is one
ParticleType *ParticleSystem::GetType( const char *szName )
{
	for (ParticleType *pType = m_pFirstType; pType; pType = pType->m_pNext)
	{
		if (!stricmp(pType->m_szName, szName))
			return pType;
	}
	return NULL;
}

ParticleType *ParticleSystem::AddPlaceholderType( const char *szName )
{
	m_pFirstType = new ParticleType( m_pFirstType );
	strncpy(m_pFirstType->m_szName, szName, sizeof(m_pFirstType->m_szName) );
	return m_pFirstType;
}

// creates a new particletype from the given file
// NB: this changes the value of szFile.
ParticleType *ParticleSystem::ParseType( const char **szFile )
{
	ParticleType *pType = new ParticleType();

	// parse the .aur file
	char *szToken;

	szToken = COM_ParseToken( szFile );
	while ( stricmp( szToken, "}" ) )
	{
		if (!szFile)
			break;

		if ( !stricmp( szToken, "name" ) )
		{
			szToken = COM_ParseToken( szFile );
			strncpy(pType->m_szName, szToken, sizeof(pType->m_szName) );

			ParticleType *pTemp = GetType(szToken);
			if (pTemp)
			{
				// there's already a type with this name
				if (pTemp->m_bIsDefined)
					ALERT( at_warning, "Particle type %s is defined more than once!\n", szToken);

				// copy all our data into the existing type, throw away the type we were making
				*pTemp = *pType;
				delete pType;
				pType = pTemp;
				pType->m_bIsDefined = true; // record the fact that it's defined, so we won't need to add it to the list
			}
		}
		else if ( !stricmp( szToken, "gravity" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_Gravity = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "windyaw" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_WindYaw = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "windstrength" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_WindStrength = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "sprite" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_hSprite = TEX_Load( szToken );
		}
		else if ( !stricmp( szToken, "startalpha" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_StartAlpha = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "endalpha" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_EndAlpha = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "startred" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_StartRed = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "endred" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_EndRed = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "startgreen" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_StartGreen = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "endgreen" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_EndGreen = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "startblue" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_StartBlue = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "endblue" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_EndBlue = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "startsize" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_StartSize = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "sizedelta" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_SizeDelta = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "endsize" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_EndSize = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "startangle" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_StartAngle = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "angledelta" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_AngleDelta = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "startframe" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_StartFrame = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "endframe" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_EndFrame = RandomRange( szToken );
			pType->m_bEndFrame = true;
		}
		else if ( !stricmp( szToken, "framerate" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_FrameRate = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "lifetime" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_Life = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "spraytype" ) )
		{
			szToken = COM_ParseToken( szFile );
			ParticleType *pTemp = GetType(szToken);

			if (pTemp) pType->m_pSprayType = pTemp;
			else pType->m_pSprayType = AddPlaceholderType(szToken);
		}
		else if ( !stricmp( szToken, "overlaytype" ) )
		{
			szToken = COM_ParseToken( szFile );
			ParticleType *pTemp = GetType(szToken);

			if (pTemp) pType->m_pOverlayType = pTemp;
			else pType->m_pOverlayType = AddPlaceholderType(szToken);
		}
		else if ( !stricmp( szToken, "sprayrate" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_SprayRate = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "sprayforce" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_SprayForce = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "spraypitch" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_SprayPitch = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "sprayyaw" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_SprayYaw = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "drag" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_Drag = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "bounce" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_Bounce = RandomRange( szToken );
			if (pType->m_Bounce.m_fMin != 0 || pType->m_Bounce.m_fMax != 0)
				pType->m_bBouncing = true;
		}
		else if ( !stricmp( szToken, "bouncefriction" ) )
		{
			szToken = COM_ParseToken( szFile );
			pType->m_BounceFriction = RandomRange( szToken );
		}
		else if ( !stricmp( szToken, "rendermode" ) )
		{
			szToken = COM_ParseToken( szFile );
			if ( !stricmp( szToken, "additive" ) )
			{
				pType->m_iRenderMode = kRenderTransAdd;
			}
			else if ( !stricmp( szToken, "solid" ) )
			{
				pType->m_iRenderMode = kRenderTransAlpha;
			}
			else if ( !stricmp( szToken, "texture" ) )
			{
				pType->m_iRenderMode = kRenderTransTexture;
			}
			else if ( !stricmp( szToken, "color" ) )
			{
				pType->m_iRenderMode = kRenderTransColor;
			}
		}
		else if ( !stricmp( szToken, "drawcondition" ) )
		{
			szToken = COM_ParseToken( szFile );
			if ( !stricmp( szToken, "empty" ) )
			{
				pType->m_iDrawCond = CONTENTS_NONE;
			}
			else if ( !stricmp( szToken, "water" ) )
			{
				pType->m_iDrawCond = CONTENTS_WATER;
			}
			else if ( !stricmp( szToken, "solid" ) )
			{
				pType->m_iDrawCond = CONTENTS_SOLID;
			}
		}
		else if ( !stricmp( szToken, "collision" ) )
		{
			szToken = COM_ParseToken( szFile );
			if ( !stricmp( szToken, "none" ) )
			{
				pType->m_iCollision = COLLISION_NONE;
			}
			else if ( !stricmp( szToken, "die" ) )
			{
				pType->m_iCollision = COLLISION_DIE;
			}
			else if ( !stricmp( szToken, "bounce" ) )
			{
				pType->m_iCollision = COLLISION_BOUNCE;
			}
		}
		// get the next token
		szToken = COM_ParseToken( szFile );
	}

	if (!pType->m_bIsDefined)
	{
		// if this is a newly-defined type, we need to add it to the list
		pType->m_pNext = m_pFirstType;
		m_pFirstType = pType;
		pType->m_bIsDefined = true;
	}

	return pType;
}

particle *ParticleSystem::ActivateParticle()
{
	particle* pActivated = m_pFreeParticle;
	if (pActivated)
	{
		m_pFreeParticle = pActivated->nextpart;
		pActivated->nextpart = m_pActiveParticle;
		m_pActiveParticle = pActivated;
	}
	return pActivated;
}

extern Vector v_origin;

void ParticleSystem::CalculateDistance()
{
	if (!m_pActiveParticle)
		return;

	Vector offset = v_origin - m_pActiveParticle->origin; // just pick one
	m_fViewerDist = offset[0]*offset[0] + offset[1]*offset[1] + offset[2]*offset[2];
}


bool ParticleSystem::UpdateSystem( float frametime )
{
	// the entity emitting this system
	edict_t *source = GetEntityByIndex( m_iEntIndex );

	if( !source ) return false;
	
	// Don't update if the system is outside the player's PVS.
	enable = (source->v.renderfx == kRenderFxAurora);

	// check for contents to remove
	if( POINT_CONTENTS( source->v.origin ) & m_iKillCondition )
          {
          	enable = 0;
          }
	if( m_pMainParticle == NULL )
	{
		if ( enable )
		{
			ParticleType *pType = m_pMainType;
			if ( pType )
			{
				m_pMainParticle = pType->CreateParticle(this);//m_pMainParticle);
				if ( m_pMainParticle )
				{
					m_pMainParticle->m_iEntIndex = m_iEntIndex;
					m_pMainParticle->age_death = -1; // never die
				}
			}
		}
	}
	else if ( !enable )
	{
		m_pMainParticle->age_death = 0; // die now
		m_pMainParticle = NULL;
	}

	particle* pParticle = m_pActiveParticle;
	particle* pLast = NULL;

	while( pParticle )
	{
		if( UpdateParticle( pParticle, frametime ) )
		{
			pLast = pParticle;
			pParticle = pParticle->nextpart;
		}
		else // deactivate it
		{
			if (pLast)
			{
				pLast->nextpart = pParticle->nextpart;
				pParticle->nextpart = m_pFreeParticle;
				m_pFreeParticle = pParticle;
				pParticle = pLast->nextpart;
			}
			else // deactivate the first particle in the list
			{
				m_pActiveParticle = pParticle->nextpart;
				pParticle->nextpart = m_pFreeParticle;
				m_pFreeParticle = pParticle;
				pParticle = m_pActiveParticle;
			}
		}
	}

	return true;

}

void ParticleSystem::DrawSystem( void )
{
	Vector normal, forward, right, up;

	GetViewAngles( normal );
	AngleVectors( normal, forward, right, up );

	particle* pParticle = m_pActiveParticle;
	for( pParticle = m_pActiveParticle; pParticle; pParticle = pParticle->nextpart )
	{
		DrawParticle( pParticle, right, up );
	}
}

bool ParticleSystem::ParticleIsVisible( particle* part )
{
	vec3_t	normal, forward, right, up;

	GetViewAngles( normal );
	AngleVectors( normal, forward, right, up );

	Vector	vec = ( part->origin - v_origin );
	Vector	vecDir = vec.Normalize( );
	float	distance = vec.Length();

	if ( DotProduct ( vecDir, forward ) < 0.0f )
		return false;
	return true;
}

bool ParticleSystem::UpdateParticle( particle *part, float frametime )
{
	if( frametime == 0 ) return true;
	part->age += frametime;

	edict_t *source = GetEntityByIndex( m_iEntIndex );

	// is this particle bound to an entity?
	if( part->m_iEntIndex )
	{
		if ( enable )
		{
			if( m_iEntAttachment )
			{
				Vector	pos = Vector( 0, 0, 0 );

				GET_ATTACHMENT( source, m_iEntAttachment, pos, NULL );
				part->velocity = (pos - part->origin ) / frametime;
				part->origin = pos;
			}
			else
			{
				part->velocity = ( source->v.origin - part->origin ) / frametime;
				part->origin = source->v.origin;
			}
		}
		else
		{
			// entity is switched off, die
			return false;
		}
	}
	else
	{
		ALERT( at_console, "UpdateOther()\n" );
		// not tied to an entity, check whether it's time to die
		if( part->age_death >= 0 && part->age > part->age_death )
			return false;

		// apply acceleration and velocity
		Vector vecOldPos = part->origin;
		if( part->m_fDrag )
			part->velocity = part->velocity + (part->velocity - part->m_vecWind) * (-part->m_fDrag * frametime);
		part->velocity = part->velocity + part->accel * frametime;
		part->origin = part->origin + part->velocity * frametime;

		if( part->pType->m_bBouncing )
		{
			Vector vecTarget = part->origin + part->velocity * frametime;
			TraceResult tr;
			TRACE_LINE( part->origin, part->origin + vecTarget * 1024, true, source, &tr );

			if( tr.flFraction < 1.0f )
			{
				part->origin = tr.vecEndPos;
				float bounceforce = DotProduct( tr.vecPlaneNormal, part->velocity );
				float newspeed = (1.0f - part->pType->m_BounceFriction.GetInstance());
				part->velocity = part->velocity * newspeed;
				part->velocity = part->velocity + tr.vecPlaneNormal * ( -bounceforce * ( newspeed + part->pType->m_Bounce.GetInstance( )));
			}
		}
	}


	// spray children
	if ( part->age_spray && part->age > part->age_spray )
	{
		part->age_spray = part->age + 1/part->pType->m_SprayRate.GetInstance();

		// particle *pChild = ActivateParticle();
		if (part->pType->m_pSprayType)
		{
			particle *pChild = part->pType->m_pSprayType->CreateParticle(this);
			if (pChild)
			{
				pChild->origin = part->origin;
				float fSprayForce = part->pType->m_SprayForce.GetInstance();
				pChild->velocity = part->velocity;
				if (fSprayForce)
				{
					float fSprayPitch = part->pType->m_SprayPitch.GetInstance() - source->v.angles.x;
					float fSprayYaw = part->pType->m_SprayYaw.GetInstance() - source->v.angles.y;
					float fSprayRoll = source->v.angles.z;
					float fForceCosPitch = fSprayForce*CosLookup(fSprayPitch);
					pChild->velocity.x += CosLookup(fSprayYaw) * fForceCosPitch;
					pChild->velocity.y += SinLookup(fSprayYaw) * fForceCosPitch + SinLookup(fSprayYaw) * fSprayForce * SinLookup(fSprayRoll);
					pChild->velocity.z -= SinLookup(fSprayPitch) * fSprayForce * CosLookup(fSprayRoll);
				}
			}
		}
	}

	part->m_fSize += part->m_fSizeStep * frametime;
	part->m_fAlpha += part->m_fAlphaStep * frametime;
	part->m_fRed += part->m_fRedStep * frametime;
	part->m_fGreen += part->m_fGreenStep * frametime;
	part->m_fBlue += part->m_fBlueStep * frametime;
	part->frame += part->m_fFrameStep * frametime;
	if ( part->m_fAngleStep )
	{
		part->m_fAngle += part->m_fAngleStep * frametime;
		while ( part->m_fAngle < 0 ) part->m_fAngle += 360;
		while ( part->m_fAngle > 360 ) part->m_fAngle -= 360;
	}
	return true;
}

void ParticleSystem::DrawParticle( particle *part, Vector &right, Vector &up )
{
	float fSize = part->m_fSize;
	Vector point1, point2, point3, point4;
	Vector origin = part->origin;

	// nothing to draw?
	if ( fSize == 0 ) return;

	// frustrum visible check
	if( !ParticleIsVisible( part )) return;

	float fCosSize = CosLookup( part->m_fAngle ) * fSize;
	float fSinSize = SinLookup( part->m_fAngle ) * fSize;

	ALERT( at_console, "particle pos %g, %g, %g\n", origin.x, origin.y, origin.z );

	// calculate the four corners of the sprite
	point1 = origin + up * fSinSize;
	point1 = point1 + right * -fCosSize;
	
	point2 = origin + up * fCosSize;
	point2 = point2 + right * fSinSize;

	point3 = origin + up * -fSinSize;	
	point3 = point3 + right * fCosSize;

	point4 = origin + up * -fCosSize;
	point4 = point4 + right * -fSinSize;

	int iContents = 0;

	g_engfuncs.pTriAPI->Enable( TRI_SHADER );

	for ( particle *pDraw = part; pDraw; pDraw = pDraw->m_pOverlay )
	{
		if( pDraw->pType->m_hSprite == 0 )
			continue;

		if ( pDraw->pType->m_iDrawCond )
		{
			if ( iContents == 0 )
				iContents = POINT_CONTENTS( origin );

			if ( iContents != pDraw->pType->m_iDrawCond )
				continue;
		}
                    
		int numFrames = SPR_Frames( pDraw->pType->m_hSprite );

		// ALERT( at_console, "UpdParticle %d: age %f, life %f, R:%f G:%f, B, %f \n", pDraw->pType->m_hSprite, part->age, part->age_death, pDraw->m_fRed, pDraw->m_fGreen, pDraw->m_fBlue);

		// if we've reached the end of the sprite's frames, loop back
		while ( pDraw->frame > numFrames )
			pDraw->frame -= numFrames;

		while ( pDraw->frame < 0 )
			pDraw->frame += numFrames;

		g_engfuncs.pTriAPI->RenderMode( pDraw->pType->m_iRenderMode );
		g_engfuncs.pTriAPI->Color4f( pDraw->m_fRed, pDraw->m_fGreen, pDraw->m_fBlue, pDraw->m_fAlpha );
		g_engfuncs.pTriAPI->Bind( pDraw->pType->m_hSprite, int( pDraw->frame ));
		
		g_engfuncs.pTriAPI->Begin( TRI_QUADS );
		g_engfuncs.pTriAPI->TexCoord2f ( 0, 0 );
		g_engfuncs.pTriAPI->Vertex3fv( point1 );

		g_engfuncs.pTriAPI->TexCoord2f ( 1, 0 );
		g_engfuncs.pTriAPI->Vertex3fv ( point2 );

		g_engfuncs.pTriAPI->TexCoord2f ( 1, 1 );
		g_engfuncs.pTriAPI->Vertex3fv ( point3 );

		g_engfuncs.pTriAPI->TexCoord2f ( 0, 1 );
		g_engfuncs.pTriAPI->Vertex3fv ( point4 );
		g_engfuncs.pTriAPI->End();
	}
	g_engfuncs.pTriAPI->Disable( TRI_SHADER );
}