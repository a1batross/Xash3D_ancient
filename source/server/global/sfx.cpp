//=======================================================================
//			Copyright (C) Shambler Team 2004
//		    sfx.cpp - different type special effects
//			e.g. explodes, sparks, smoke e.t.c			    
//=======================================================================

#include "sfx.h"
#include "client.h"

void SFX_Explode( short model, Vector origin, float scale, int flags )
{
	MESSAGE_BEGIN( MSG_PAS, gmsg.TempEntity, origin );
		WRITE_BYTE( TE_EXPLOSION );	// This makes a dynamic light and the explosion sprites/sound
		WRITE_COORD( origin.x );	// Send to PAS because of the sound
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z );
		WRITE_SHORT( model );
		WRITE_BYTE( (byte)(scale - 30) * 0.7); // scale * 10
		WRITE_BYTE( 15  ); // framerate
		WRITE_BYTE( flags );
	MESSAGE_END();
}

void SFX_Trail( int entindex, short model, Vector color, float life )
{
	MESSAGE_BEGIN( MSG_BROADCAST, gmsg.TempEntity );
		WRITE_BYTE( TE_BEAMFOLLOW );
		WRITE_SHORT( entindex );	// entity
		WRITE_SHORT( model );	// model
		WRITE_BYTE( life );		// life
		WRITE_BYTE( 5 );		// width
		WRITE_BYTE( color.x );	// r, g, b
		WRITE_BYTE( color.y );	// r, g, b
		WRITE_BYTE( color.z );	// r, g, b
		WRITE_BYTE( 200 );		// brightness
	MESSAGE_END();// move PHS/PVS data sending into here (SEND_ALL, SEND_PVS, SEND_PHS)

}

void SFX_MakeGibs( int shards, Vector pos, Vector size, Vector velocity, float time, int flags)
{
	MESSAGE_BEGIN( MSG_PVS, gmsg.TempEntity, pos );
		WRITE_BYTE( TE_BREAKMODEL);
		WRITE_COORD( pos.x ); // position
		WRITE_COORD( pos.y );
		WRITE_COORD( pos.z );
		WRITE_COORD( size.x); // size
		WRITE_COORD( size.y);
		WRITE_COORD( size.z);
		WRITE_COORD( velocity.x ); // velocity
		WRITE_COORD( velocity.y );
		WRITE_COORD( velocity.z );
		WRITE_BYTE( 10 );
		WRITE_SHORT( shards );     // model id#
		WRITE_BYTE( 0 );	       // let client decide
		WRITE_BYTE( time );	       // life time
		WRITE_BYTE( flags );
	MESSAGE_END();
}

void SFX_EjectBrass ( const Vector &vecOrigin, const Vector &vecVelocity, float rotation, int model, int soundtype )
{
	MESSAGE_BEGIN( MSG_PVS, gmsg.TempEntity, vecOrigin );
		WRITE_BYTE( TE_MODEL);
		WRITE_COORD( vecOrigin.x);
		WRITE_COORD( vecOrigin.y);
		WRITE_COORD( vecOrigin.z);
		WRITE_COORD( vecVelocity.x);
		WRITE_COORD( vecVelocity.y);
		WRITE_COORD( vecVelocity.z);
		WRITE_ANGLE( rotation );
		WRITE_SHORT( model );
		WRITE_BYTE ( soundtype);
		WRITE_BYTE ( 25 );// 2.5 seconds
	MESSAGE_END();
}

void SFX_Decal( const Vector &vecOrigin, int decalIndex, int entityIndex, int modelIndex )
{
	MESSAGE_BEGIN( MSG_BROADCAST, gmsg.TempEntity);
		WRITE_BYTE( TE_BSPDECAL );
		WRITE_COORD( vecOrigin.x );
		WRITE_COORD( vecOrigin.y );
		WRITE_COORD( vecOrigin.z );
		WRITE_SHORT( decalIndex );
		WRITE_SHORT( entityIndex );
		if ( entityIndex ) WRITE_SHORT( modelIndex );
	MESSAGE_END();
}

void SFX_Light ( entvars_t *pev, float iTime, float decay, int attachment )
{
	MESSAGE_BEGIN( MSG_PVS, gmsg.TempEntity, pev->origin );
		WRITE_BYTE( TE_ELIGHT );
		WRITE_SHORT( ENTINDEX( ENT(pev) ) + 0x1000 * attachment );// entity, attachment
		WRITE_COORD( pev->origin.x );		// X
		WRITE_COORD( pev->origin.y );		// Y
		WRITE_COORD( pev->origin.z );		// Z
		WRITE_COORD( pev->renderamt );	// radius * 0.1
		WRITE_BYTE( pev->rendercolor.x );	// r
		WRITE_BYTE( pev->rendercolor.y );	// g
		WRITE_BYTE( pev->rendercolor.z );	// b
		WRITE_BYTE( iTime );		// time * 10
		WRITE_COORD( decay );		// decay * 0.1
	MESSAGE_END( );
}

void SFX_Zap ( entvars_t *pev, const Vector &vecSrc, const Vector &vecDest )
{
	MESSAGE_BEGIN( MSG_BROADCAST, gmsg.TempEntity );
		WRITE_BYTE( TE_BEAMPOINTS);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecDest.x);
		WRITE_COORD(vecDest.y);
		WRITE_COORD(vecDest.z);
		WRITE_SHORT( pev->team );
		WRITE_BYTE( 0 );			//framestart
		WRITE_BYTE( (int)pev->framerate);	// framerate
		WRITE_BYTE( (int)(pev->armorvalue*10.0) ); // life
		WRITE_BYTE( pev->button );		// width
		WRITE_BYTE( pev->impulse );		// noise
		WRITE_BYTE( (int)pev->rendercolor.x );  // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.y );  // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.z );  // r, g, b
		WRITE_BYTE( pev->renderamt );		// brightness
		WRITE_BYTE( pev->speed );		// speed
	MESSAGE_END();
}

void SFX_Ring ( entvars_t *pev, entvars_t *pev2 )
{
	MESSAGE_BEGIN( MSG_BROADCAST, gmsg.TempEntity );
		WRITE_BYTE( TE_BEAMRING );
		WRITE_SHORT( ENTINDEX(ENT(pev)) );
		WRITE_SHORT( ENTINDEX(ENT(pev2)) );
		WRITE_SHORT( pev->team );
		WRITE_BYTE( 0 ); // framestart
		WRITE_BYTE( 20 ); // framerate
		WRITE_BYTE( (int)(pev->armorvalue*10) ); // life
		WRITE_BYTE( pev->button );  // width
		WRITE_BYTE( pev->impulse );   // noise
		WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
		WRITE_BYTE( pev->renderamt );	// brightness
		WRITE_BYTE( pev->speed );	// speed
	MESSAGE_END();
}