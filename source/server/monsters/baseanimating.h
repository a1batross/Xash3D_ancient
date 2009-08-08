//=======================================================================
//			Copyright (C) XashXT Group 2006
//=======================================================================

#ifndef BASEANIMATING_H
#define BASEANIMATING_H

class CBaseAnimating : public CBaseLogic
{
public:
	virtual int		Save( CSave &save );
	virtual int		Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	// Basic Monster Animation functions
	float StudioFrameAdvance( float flInterval = 0.0 );
	int  GetSequenceFlags( void );
	int  LookupActivity ( int activity );
	int  LookupActivityHeaviest ( int activity );
	int  LookupSequence ( const char *label );
	float SequenceDuration( int iSequence );
	void ResetSequenceInfo ( );
	void DispatchAnimEvents ( float flFutureInterval = 0.1 );
	virtual CBaseAnimating*	GetBaseAnimating() { return this; }
	virtual void HandleAnimEvent( MonsterEvent_t *pEvent ) { return; };
	float SetBoneController ( int iController, float flValue );
	void InitBoneControllers ( void );
	float SetBlending ( int iBlender, float flValue );
	void GetBonePosition ( int iBone, Vector &origin, Vector &angles );
	void GetAutomovement( Vector &origin, Vector &angles, float flInterval = 0.1 );
	int  FindTransition( int iEndingSequence, int iGoalSequence, int *piDir );
	BOOL GetAttachment ( int iAttachment, Vector &origin, Vector &angles );
	void SetBodygroup( int iGroup, int iValue );
	int GetBodygroup( int iGroup );
	int GetBoneCount( void );
	void SetBones( float (*data)[3], int datasize );

	int ExtractBbox( int sequence, float *mins, float *maxs );
	void SetSequenceBox( void );

	// animation needs
	float	m_flFrameRate;		// computed FPS for current sequence
	float	m_flGroundSpeed;	// computed linear movement rate for current sequence
	float	m_flLastEventCheck;	// last time the event list was checked
	BOOL	m_fSequenceFinished;// flag set when StudioAdvanceFrame moves across a frame boundry
	BOOL	m_fSequenceLoops;	// true if the sequence loops
};

#endif //BASEANIMATING_H