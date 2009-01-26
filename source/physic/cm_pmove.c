//=======================================================================
//			Copyright XashXT Group 2007 ©
//		        cm_pmove.c - player movement code
//=======================================================================

#include "cm_local.h"
#include "matrix_lib.h"

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
	entity_state_t	*state;
	usercmd_t		*cmd;
	physbody_t	*body;

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

pml_t pml;

/*
================
CM_UpdateViewAngles

This can be used as another entry point when only the viewangles
are being updated isntead of a full move
================
*/
void CM_UpdateViewAngles( entity_state_t *state, const usercmd_t *cmd )
{
	short	temp;
	int	i;

	if( state->flags & FL_FROZEN )
	{
		return;		// no view changes at all
	}
	if( state->health <= 0 )
	{
		return;		// no view changes at all
	}
	if( state->ed_flags & ESF_NO_PREDICTION )
	{
		state->viewangles[YAW] = cmd->angles[YAW] - state->delta_angles[YAW];
		state->viewangles[PITCH] = 0;
		state->viewangles[ROLL] = 0;
	}

	// circularly clamp the angles with deltas
	for( i = 0; i < 3; i++ )
	{
		temp = cmd->angles[i] + state->delta_angles[i];
		state->viewangles[i] = temp;
	}

	// don't let the player look up or down more than 90 degrees
	if( state->viewangles[PITCH] > 89 && state->viewangles[PITCH] < 180 )
		state->viewangles[PITCH] = 89;
	else if( state->viewangles[PITCH] < 271 && state->viewangles[PITCH] >= 180 )
		state->viewangles[PITCH] = 271;
}


void CM_CmdUpdateForce( void )
{
	float	fmove, smove;
	int	i;

	fmove = pml.cmd->forwardmove;
	smove = pml.cmd->sidemove;

	// project moves down to flat plane
	pml.forward[2] = pml.right[2] = 0;
	VectorNormalize( pml.forward );
	VectorNormalize( pml.right );

	for( i = 0; i < 3; i++ )
		pml.movedir[i] = pml.forward[i] * fmove + pml.right[i] * smove;

	ConvertDirectionToPhysic( pml.movedir );

	if( pml.cmd->upmove > 0.0f )
	{
		m_jumpTimer = 4;
	}
}

void CM_ServerMove( entity_state_t *state, usercmd_t *cmd, physbody_t *body )
{
	float	mass, Ixx, Iyy, Izz, dist, floor;
	float	deltaHeight, steerAngle, accelY, timestepInv;
	vec3_t	force, omega, torque, heading, velocity, step;
	matrix4x4	matrix, collisionPadding, transpose;

	pml.state = state;
	pml.body = body;
	pml.cmd = cmd;

	CM_UpdateViewAngles( state, cmd );
	AngleVectors( pml.state->viewangles, pml.forward, pml.right, pml.up );
	CM_CmdUpdateForce(); // get movement direction

	// Get the current world timestep
	pml.frametime = NewtonGetTimeStep( gWorld );
	timestepInv = 1.0f / pml.frametime;

	// get the character mass
	NewtonBodyGetMassMatrix( pml.body, &mass, &Ixx, &Iyy, &Izz );

	// apply the gravity force, cheat a little with the character gravity
	VectorSet( force, 0.0f, mass * -9.8f, 0.0f );

	// Get the velocity vector
	NewtonBodyGetVelocity( pml.body, &velocity[0] );

	// determine if the character have to be snap to the ground
	NewtonBodyGetMatrix( pml.body, &matrix[0][0] );

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
		NewtonAddBodyImpulse( pml.body, &veloc[0], &matrix[3][0] );
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
		NewtonBodySetForce( pml.body, &force[0] );

		VectorScale( force, pml.frametime / mass, tmp1 );
		VectorAdd( tmp1, velocity, step );
		VectorScale( step, pml.frametime, step );
	}

	VectorSet(step, DotProduct(step,cm.matrix[0]),DotProduct(step,cm.matrix[1]),DotProduct(step,cm.matrix[2])); 	
	Matrix4x4_LoadIdentity( collisionPadding );

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
		Matrix4x4_Transpose( transpose, collisionPadding );
		VectorScale( transpose[0], dist / m_maxTranslation, transpose[0] ); // scale factor

		// calculate and oblique scale matrix by using a similar transformation matrix of the for, R'* S * R
		Matrix4x4_Concat( collisionPadding, collisionPadding, transpose );
	}

	// set the collision modifierMatrix;
//NewtonConvexHullModifierSetMatrix( NewtonBodyGetCollision(pm->body), &collisionPadding[0][0]);
          
	steerAngle = asin( bound( -1.0f, pml.forward[2], 1.0f ));
	
	// calculate the torque vector
	NewtonBodyGetOmega( pml.body, &omega[0]);

	VectorSet( torque, 0.0f, 0.5f * Iyy * (steerAngle * timestepInv - omega[1] ) * timestepInv, 0.0f );
	NewtonBodySetTorque( pml.body, &torque[0] );


	// assume the character is on the air. this variable will be set to false if the contact detect 
	// the character is landed 
	m_isAirBorne = true;
	VectorSet( m_stepContact, 0.0f, -m_size[2], 0.0f );   

	NewtonUpVectorSetPin( cms.upVector, &vec3_up[0] );
}

void CM_ClientMove( entity_state_t *state, usercmd_t *cmd, physbody_t *body )
{

}

physbody_t *Phys_CreatePlayer( edict_t *ed, cmodel_t *mod, const vec3_t origin, const matrix3x3 matrix )
{
	NewtonCollision	*col, *hull;
	NewtonBody	*body;

	matrix4x4		trans;
	vec3_t		radius, mins, maxs, upDirection;

	if( !cm_physics_model->integer )
		return NULL;

	// setup matrixes
	Matrix4x4_LoadIdentity( trans );

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

	VectorCopy( origin, trans[3] );	// copy position only
	ConvertPositionToPhysic( trans[3] );

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
	cms.upVector = NewtonConstraintCreateUpVector( gWorld, &upDirection[0], body ); 
	NewtonBodySetContinuousCollisionMode( body, 1 );

	return (physbody_t *)body;
}

/*
===============================================================================
			PMOVE ENTRY POINT
===============================================================================
*/
void CM_PlayerMove( entity_state_t *state, usercmd_t *cmd, physbody_t *body, bool clientmove )
{
	if( !cm_physics_model->integer ) return;

	if( clientmove ) CM_ClientMove( state, cmd, body );
	else CM_ServerMove( state, cmd, body );
}