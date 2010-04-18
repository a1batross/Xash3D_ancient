class CWallTorch : public CBaseEntity
{
public:
	void Precache( void )
	{
		// if sound is missing just reset soundindex
		pev->impulse = PRECACHE_SOUND( "ambience/fire1.wav" );
		UTIL_PrecacheModel( "models/props/torch1.mdl" );
	}
	void Spawn( void )
	{
		Precache ();

		if( !pev->impulse )
		{
			UTIL_Remove( this );
			return;
		}

//		SetObjectClass( ED_NORMAL );
		pev->flags |= FL_PHS_FILTER;	// allow phs filter instead pvs

		// setup attenuation radius
		pev->armorvalue = 384.0f * ATTN_STATIC;

		pev->soundindex = pev->impulse;
		UTIL_SetModel( ENT( pev ), "models/props/torch1.mdl" );
		UTIL_SetSize(pev, g_vecZero, g_vecZero);
		SetBits( pFlags, PF_POINTENTITY );
		pev->animtime = gpGlobals->time + 0.2;	// enable animation
		pev->framerate = 0.5f;
	}
};

LINK_ENTITY_TO_CLASS( light_torch_small_walltorch, CWallTorch );