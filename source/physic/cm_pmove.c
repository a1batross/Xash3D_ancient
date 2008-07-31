//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        cm_pmove.c - player movement code
//=======================================================================

#include "cm_local.h"

int		characterID; 
uint		m_jumpTimer;
bool		m_isStopped;
bool		m_isAirBorne;
float		m_maxStepHigh;
float		m_yawAngle;
float		m_maxTranslation;
vec3_t		m_size;
vec3_t		m_stepContact;
matrix4x4		m_matrix;

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server
typedef struct
{
	vec3_t		origin;
	vec3_t		velocity;
	vec3_t		previous_origin;
	vec3_t		previous_velocity;
	int		previous_waterlevel;

	vec3_t		movedir;	// already aligned by world
	vec3_t		forward, right, up;
	float		frametime;
	int		msec;

	bool		walking;
	bool		onladder;
	trace_t		groundtrace;
	float		impact_speed;

	csurface_t	*groundsurface;
	cplane_t		groundplane;
	int		groundcontents;


} pml_t;

pmove_t		*pm;
pml_t		pml;

/*
================
CM_UpdateViewAngles

This can be used as another entry point when only the viewangles
are being updated isntead of a full move
================
*/
void CM_UpdateViewAngles( entity_state_t *ps, const usercmd_t *cmd )
{
	short		temp;
	int		i;

	if( ps->pm_type == PM_INTERMISSION )
	{
		return;		// no view changes at all
	}
	if( ps->pm_type == PM_DEAD )
	{
		return;		// no view changes at all
	}
	if( pm->ps.pm_flags & PMF_TIME_TELEPORT)
	{
		ps->viewangles[YAW] = SHORT2ANGLE( pm->cmd.angles[YAW] - pm->ps.delta_angles[YAW] );
		ps->viewangles[PITCH] = 0;
		ps->viewangles[ROLL] = 0;
	}

	// circularly clamp the angles with deltas
	for( i = 0; i < 3; i++ )
	{
		temp = pm->cmd.angles[i] + pm->ps.delta_angles[i];
		ps->viewangles[i] = SHORT2ANGLE(temp);
	}

	// don't let the player look up or down more than 90 degrees
	if( ps->viewangles[PITCH] > 89 && ps->viewangles[PITCH] < 180 )
		ps->viewangles[PITCH] = 89;
	else if( ps->viewangles[PITCH] < 271 && ps->viewangles[PITCH] >= 180 )
		ps->viewangles[PITCH] = 271;
}


void CM_CmdUpdateForce( void )
{
	float	fmove, smove;
	int	i;

	fmove = pm->cmd.forwardmove;
	smove = pm->cmd.sidemove;

	// project moves down to flat plane
	pml.forward[2] = pml.right[2] = 0;
	VectorNormalize( pml.forward );
	VectorNormalize( pml.right );

	for( i = 0; i < 3; i++ )
		pml.movedir[i] = pml.forward[i] * fmove + pml.right[i] * smove;

	ConvertDirectionToPhysic( pml.movedir );

	if( pm->cmd.upmove > 0.0f )
	{
		m_jumpTimer = 4;
	}
}

void CM_ServerMove( pmove_t *pmove )
{
	float	mass, Ixx, Iyy, Izz, dist, floor;
	float	deltaHeight, steerAngle, accelY, timestepInv;
	vec3_t	force, omega, torque, heading, velocity, step;
	matrix4x4	matrix, collisionPadding, transpose;

	pm = pmove;

	CM_UpdateViewAngles( &pm->ps, &pm->cmd );
	AngleVectors( pm->ps.viewangles, pml.forward, pml.right, pml.up );
	CM_CmdUpdateForce(); // get movement direction

	// Get the current world timestep
	pml.frametime = NewtonGetTimeStep( gWorld );
	timestepInv = 1.0f / pml.frametime;

	// get the character mass
	NewtonBodyGetMassMatrix( pm->body, &mass, &Ixx, &Iyy, &Izz );

	// apply the gravity force, cheat a little with the character gravity
	VectorSet( force, 0.0f, mass * -9.8f, 0.0f );

	// Get the velocity vector
	NewtonBodyGetVelocity( pm->body, &velocity[0] );

	// determine if the character have to be snap to the ground
	NewtonBodyGetMatrix( pm->body, &matrix[0][0] );

	// if the floor is with in reach then the character must be snap to the ground
	// the max allow distance for snapping i 0.25 of a meter
	if( m_isAirBorne && !m_jumpTimer )
	{ 
		floor = CM_FindFloor( matrix[3], m_size[2] + 0.25f );
		deltaHeight = ( matrix[3][1] - m_size[2] ) - floor;

		if((deltaHeight < (0.25f - 0.001f)) && (deltaHeight > 0.01f))
		{
			// snap to floor only if the floor is lower than the character feet		
			accelY = - (deltaHeight * timestepInv + velocity[1]) * timestepInv;
			force[1] = mass * accelY;
		}
	}
	else if( m_jumpTimer == 4 )
	{
		vec3_t	veloc = { 0.0f, 0.3f, 0.0f };
		NewtonAddBodyImpulse( pm->body, &veloc[0], &matrix[3][0] );
	}

	m_jumpTimer = m_jumpTimer ? m_jumpTimer - 1 : 0;

	{
		float	speed_factor;
		vec3_t	tmp1, tmp2, result;

		speed_factor = sqrt( DotProduct( pml.movedir, pml.movedir ) + 1.0e-6f );  
		VectorScale( pml.movedir, 1.0f / speed_factor, heading ); 

		VectorScale( heading, mass * 20.0f, tmp1 );
		VectorScale( heading, 2.0f * DotProduct( velocity, heading ), tmp2 );

		VectorSubtract( tmp1, tmp2, result );
		VectorAdd( force, result, force );
		NewtonBodySetForce( pm->body, &force[0] );

		VectorScale( force, pml.frametime / mass, tmp1 );
		VectorAdd( tmp1, velocity, step );
		VectorScale( step, pml.frametime, step );
	}

	VectorSet(step, DotProduct(step,cm.matrix[0]),DotProduct(step,cm.matrix[1]),DotProduct(step,cm.matrix[2])); 	
	MatrixLoadIdentity( collisionPadding );

	step[1] = 0.0f;

	dist = DotProduct( step, step );

	if( dist > m_maxTranslation * m_maxTranslation )
	{
		// when the velocity is high enough that can miss collision we will enlarge the collision 
		// long the vector velocity
		dist = sqrt( dist );
		VectorScale( step, 1.0f / dist, step );

		// make a rotation matrix that will align the velocity vector with the front vector
		collisionPadding[0][0] =  step[0];
		collisionPadding[0][2] = -step[2];
		collisionPadding[2][0] =  step[2];
		collisionPadding[2][2] =  step[0];

		// get the transpose of the matrix                    
		MatrixTranspose( transpose, collisionPadding );
		VectorScale( transpose[0], dist / m_maxTranslation, transpose[0] ); // scale factor

		// calculate and oblique scale matrix by using a similar transformation matrix of the for, R'* S * R
		MatrixConcat( collisionPadding, collisionPadding, transpose );
	}

	// set the collision modifierMatrix;
//NewtonConvexHullModifierSetMatrix( NewtonBodyGetCollision(pm->body), &collisionPadding[0][0]);
          
	steerAngle = asin( bound( -1.0f, pml.forward[2], 1.0f ));
	
	// calculate the torque vector
	NewtonBodyGetOmega( pm->body, &omega[0]);

	VectorSet( torque, 0.0f, 0.5f * Iyy * (steerAngle * timestepInv - omega[1] ) * timestepInv, 0.0f );
	NewtonBodySetTorque( pm->body, &torque[0] );


	// assume the character is on the air. this variable will be set to false if the contact detect 
	// the character is landed 
	m_isAirBorne = true;
	VectorSet( m_stepContact, 0.0f, -m_size[2], 0.0f );   

	pm->ps.viewheight = 22;
	NewtonUpVectorSetPin( cm.upVector, &vec3_up[0] );
}

void CM_ClientMove( pmove_t *pmove )
{

}

physbody_t *Phys_CreatePlayer( sv_edict_t *ed, cmodel_t *mod, matrix4x3 transform )
{
	NewtonCollision	*col, *hull;
	NewtonBody	*body;

	matrix4x4		trans;
	vec3_t		radius, mins, maxs, upDirection;

	if( !cm_physics_model->integer )
		return NULL;

	// setup matrixes
	MatrixLoadIdentity( trans );

	if( mod )
	{
		// player m_size
		VectorCopy( mod->mins, mins );
		VectorCopy( mod->maxs, maxs );
		VectorSubtract( maxs, mins, m_size );
		VectorScale( m_size, 0.5f, radius );
	}
	ConvertDimensionToPhysic( m_size );
	ConvertDimensionToPhysic( radius );

	VectorSet( m_stepContact, 0.0f, -m_size[2], 0.0f );   
	m_maxTranslation = m_size[0] * 0.25f;
	m_maxStepHigh = -m_size[2] * 0.5f;

	VectorCopy( transform[3], trans[3] );	// copy position only

	trans[3][1] = CM_FindFloor( trans[3], 32768 ) + radius[2]; // merge floor position

	// place a sphere at the center
	col = NewtonCreateSphere( gWorld, radius[0], radius[2], radius[1], NULL ); 

	// wrap the character collision under a transform, modifier for tunneling trught walls avoidance
	hull = NewtonCreateConvexHullModifier( gWorld, col );
	NewtonReleaseCollision( gWorld, col );		
	body = NewtonCreateBody( gWorld, hull );	// create the rigid body
	NewtonBodySetAutoFreeze( body, 0 );		// disable auto freeze management for the player
	NewtonWorldUnfreezeBody( gWorld, body );	// keep the player always active 

	// reset angular and linear damping
	NewtonBodySetLinearDamping( body, 0.0f );
	NewtonBodySetAngularDamping( body, vec3_origin );
	NewtonBodySetMaterialGroupID( body, characterID );// set material Id for this object

	// setup generic callback to engine.dll
	NewtonBodySetUserData( body, ed );
	NewtonBodySetTransformCallback( body, Callback_ApplyTransform );
	NewtonBodySetForceAndTorqueCallback( body, Callback_PmoveApplyForce );
	NewtonBodySetMassMatrix( body, 20.0f, m_size[0], m_size[1], m_size[2] ); // 20 kg
	NewtonBodySetMatrix(body, &trans[0][0] );// origin

	// release the collision geometry when not need it
	NewtonReleaseCollision( gWorld, hull );

  	// add and up vector constraint to help in keeping the body upright
	VectorSet( upDirection, 0.0f, 1.0f, 0.0f );
	cm.upVector = NewtonConstraintCreateUpVector( gWorld, &upDirection[0], body ); 
	NewtonBodySetContinuousCollisionMode( body, 1 );

	return (physbody_t *)body;
}

/*
===============================================================================
			PMOVE ENTRY POINT
===============================================================================
*/
void CM_PlayerMove( pmove_t *pmove, bool clientmove )
{
	if( !cm_physics_model->integer ) return;

	if( clientmove ) CM_ClientMove( pmove );
	else CM_ServerMove( pmove );
}