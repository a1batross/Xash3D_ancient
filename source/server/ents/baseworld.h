//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#ifndef BASEWORLD_H
#define BASEWORLD_H

class CWorld : public CBaseEntity
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData *pkvd );
	void PostActivate( void );
};

extern BOOL g_startSuit;
extern CWorld *g_pWorld;

#endif //BASEWORLD_H