//=======================================================================
//			Copyright XashXT Group 2007 ©
//			cm_callback.c - generic callbacks
//=======================================================================

#include "cm_local.h"
#include "matrix_lib.h"

int Callback_ContactBegin( const NewtonMaterial* material, const NewtonBody* body0, const NewtonBody* body1 )
{
	// save the collision bodies
	cms.touch_info.m_body0 = (NewtonBody*) body0;
	cms.touch_info.m_body1 = (NewtonBody*) body1;
	cms.touch_info.normal_speed = 0.0f;// clear the contact normal speed 
	cms.touch_info.tangent_speed = 0.0f;// clear the contact sliding speed 

	// return one the tell Newton the application wants to process this contact
	return 1;
}

// this callback is called for every contact between the two bodies
int Callback_ContactProcess( const NewtonMaterial* material, const NewtonContact* contact )
{
	float	speed0, speed1;
	vec3_t	normal;

	// get the maximum normal speed of this impact. this can be used for particles of playing collision sound
	speed0 = NewtonMaterialGetContactNormalSpeed( material, contact );
	if( speed0 > cms.touch_info.normal_speed )
	{
		// save the position of the contact (for 3d sound of particles effects)
		cms.touch_info.normal_speed = speed0;
		NewtonMaterialGetContactPositionAndNormal( material, &cms.touch_info.position[0], &normal[0] );
	}

	// get the maximum of the two sliding contact speed
	speed0 = NewtonMaterialGetContactTangentSpeed( material, contact, 0 );
	speed1 = NewtonMaterialGetContactTangentSpeed( material, contact, 1 );

	if( speed1 > speed0 ) speed0 = speed1;

	// Get the maximum tangent speed of this contact. this can be used for particles(sparks) of playing scratch sounds 
	if( speed0 > cms.touch_info.tangent_speed )
	{
		// save the position of the contact (for 3d sound of particles effects)
		cms.touch_info.tangent_speed = speed0;
		NewtonMaterialGetContactPositionAndNormal( material, &cms.touch_info.position[0], &normal[0] );
	}

	// return one to tell Newton we want to accept this contact
	return 1;
}

// this function is call after all contacts for this pairs is processed
void Callback_ContactEnd( const NewtonMaterial* material )
{
	edict_t *edict = (edict_t *)NewtonBodyGetUserData( cms.touch_info.m_body0 );
	int id = NewtonBodyGetMaterialGroupID( cms.touch_info.m_body0 );

	// if the max contact speed is larger than some minimum value. play a sound
	if( cms.touch_info.normal_speed > 15.0f && cms.mat[id].num_impactsounds )
	{
		float	pitch = cms.touch_info.normal_speed - 15.0f;
		int	snd = Com_RandomLong( 0, cms.mat[id].num_impactsounds - 1 );
		
		// TODO: play impact sound here
		pi.PlaySound( edict, 0.95f, pitch, cms.mat[id].impact_sounds[snd] );
		// FIXME: get sound from material desc
	}

	// if the max contact speed is larger than some minimum value. play a sound
	if( cms.touch_info.normal_speed > 5.0f  && cms.mat[id].num_pushsounds )
	{
		float	pitch = cms.touch_info.normal_speed - 5.0f;
		int	snd = Com_RandomLong( 0, cms.mat[id].num_pushsounds - 1 );

		// TODO: play scratch sound here
		pi.PlaySound( edict, 0.95f, pitch, cms.mat[id].push_sounds[snd] );
	}
}

void Callback_ApplyForce( const NewtonBody* body ) 
{ 
	float	mass; 
	vec3_t	m_size, force, torque; 

	NewtonBodyGetMassMatrix (body, &mass, &m_size[0], &m_size[1], &m_size[2]); 

	VectorSet( torque, 0.0f, 0.0f, 0.0f );
	VectorSet( force, 0.0f, -9.8f * mass, 0.0f );

	NewtonBodyAddForce (body, force); 
	NewtonBodyAddTorque (body, torque); 
}

void Callback_ApplyForce_NoGravity( const NewtonBody* body )
{
	float	mass; 
	vec3_t	m_size, force, torque; 

	NewtonBodyGetMassMatrix( body, &mass, &m_size[0], &m_size[1], &m_size[2] ); 

	VectorSet( torque, 0.0f, 0.0f, 0.0f );
	VectorSet( force, 0.0f, 0.0f, 0.0f );

	NewtonBodyAddForce( body, force ); 
	NewtonBodyAddTorque( body, torque );
}

void Callback_PmoveApplyForce( const NewtonBody* body )
{
	// grab state and jump to CM_ServerMove
	pi.ClientMove((edict_t *)NewtonBodyGetUserData( body ));
}

void Callback_ApplyTransform( const NewtonBody* body, const float* src )
{
	edict_t		*edict = (edict_t *)NewtonBodyGetUserData( body );
	matrix4x4		tmp;
	matrix3x3		dst;
	vec3_t		origin;

	Matrix4x4_FromArrayFloatGL( tmp, src ); 
	Matrix4x4_OriginFromMatrix( tmp, origin );
	Matrix4x4_ToMatrix3x3( dst, tmp );
	ConvertPositionToGame( origin );

	pi.Transform( edict, origin, dst );
}

void Callback_Static( const NewtonBody* body, const float* src )
{
	// this does nothing
}