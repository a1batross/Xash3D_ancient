//=======================================================================
//			Copyright XashXT Group 2007 ©
//			physic.h - physics library header
//=======================================================================
#ifndef BASE_PHYSICS_H
#define BASE_PHYSICS_H

#include <stdio.h>
#include <windows.h>
#include <basetypes.h>
#include <basemath.h>
#include <ref_system.h>
#include <ref_stdlib.h>

extern physic_imp_t pi;
extern stdlib_api_t std;
extern byte *physpool;
extern NewtonWorld	*gWorld;
long _ftol2( double dblSource );
void Phys_LoadBSP( uint *buffer );
void Phys_FreeBSP( void );
void DebugShowCollision ( void );
void Phys_Frame( float time );
void Phys_CreateBOX( sv_edict_t *ed, vec3_t mins, vec3_t maxs, vec3_t org, vec3_t ang, NewtonCollision **newcol, NewtonBody **newbody );
void Phys_RemoveBOX( NewtonBody *body );

#define Msg std.printf
#define MsgDev std.dprintf
#define MsgWarn std.wprintf
#define Host_Error std.error
	
typedef struct NewtonUserMeshCollisionCollideDescTag
{
	float		m_boxP0[4];		// lower bounding box of intersection query in local space
	float		m_boxP1[4];		// upper bounding box of intersection query in local space
	void*		m_userData;		// user data passed to the collison geometry at creation time
	int		m_faceCount;		// the applycation should set here how many polygons inteset the query
	float*		m_vertex;			// the applycation should the pointer to the vertex array. 
	int		m_vertexStrideInBytes;	// the applycation should set here the size of each vertex
 	int*		m_userAttribute;		// the applycation should set here the pointer to the user data, one for each face
	int*		m_faceIndexCount;		// the applycation should set here the pointer to the vertex count of each face.
	int*		m_faceVertexIndex;		// the applycation should set here the pointer index array for each vertex on a face.
	NewtonBody*	m_objBody;		// pointer to the colliding body
	NewtonBody*	m_polySoupBody;		// pointer to the rigid body owner of this collision tree 

} NewtonUserMeshCollisionCollideDesc;

typedef struct NewtonUserMeshCollisionRayHitDescTag
{
	float		m_p0[4];			// ray origin in collision local space
	float		m_p1[4];			// ray destination in collision local space   
	float		m_normalOut[4];		// copy here the normal at the rat intesection
	int		m_userIdOut;		// copy here a user defined id for further feedback  
	void*		m_userData;		// user data passed to the collison geometry at creation time

} NewtonUserMeshCollisionRayHitDesc;

typedef struct NewtonHingeSliderUpdateDescTag
{
	float		m_accel;
	float		m_minFriction;
	float		m_maxFriction;
	float		m_timestep;

} NewtonHingeSliderUpdateDesc;

// Newton callback
typedef void* (*NewtonAllocMemory) (int sizeInBytes);
typedef void (*NewtonFreeMemory) (void *ptr, int sizeInBytes);
typedef void (*NewtonSerialize) (void* serializeHandle, const void* buffer, size_t size);
typedef void (*NewtonDeserialize) (void* serializeHandle, void* buffer, size_t size);
typedef void (*NewtonUserMeshCollisionCollideCallback) (NewtonUserMeshCollisionCollideDesc* collideDescData);
typedef float (*NewtonUserMeshCollisionRayHitCallback) (NewtonUserMeshCollisionRayHitDesc* lineDescData);
typedef void (*NewtonUserMeshCollisionDestroyCallback) (void* descData);
typedef void (*NewtonTreeCollisionCallback) (const NewtonBody* bodyWithTreeCollision, const NewtonBody* body, const float* vertex, int vertexstrideInBytes, int indexCount, const int* indexArray); 
typedef void (*NewtonBodyDestructor) (const NewtonBody* body);
typedef void (*NewtonApplyForceAndTorque) (const NewtonBody* body);
typedef void (*NewtonBodyActivationState) (const NewtonBody* body, unsigned state);
typedef void (*NewtonSetTransform) (const NewtonBody* body, const float* matrix);
typedef void (*NewtonSetRagDollTransform) (const NewtonRagDollBone* bone);
typedef void (*NewtonGetBuoyancyPlane) (void *context, const float* globalSpaceMatrix, float* globalSpacePlane);
typedef void (*NewtonVehicleTireUpdate) (const NewtonJoint* vehicle);
typedef uint (*NewtonWorldRayPrefilterCallback)(const NewtonBody* body, const NewtonCollision* collision, void* userData);
typedef float (*NewtonWorldRayFilterCallback)(const NewtonBody* body, const float* hitNormal, int collisionID, void* userData, float intersetParam);
typedef void (*NewtonBodyLeaveWorld) (const NewtonBody* body);
typedef int  (*NewtonContactBegin) (const NewtonMaterial* material, const NewtonBody* body0, const NewtonBody* body1);
typedef int  (*NewtonContactProcess) (const NewtonMaterial* material, const NewtonContact* contact);
typedef void (*NewtonContactEnd) (const NewtonMaterial* material);
typedef void (*NewtonBodyIterator) (const NewtonBody* body);
typedef void (*NewtonCollisionIterator) (const NewtonBody* body, int vertexCount, const float* FaceArray, int faceId);
typedef void (*NewtonBallCallBack) (const NewtonJoint* ball);
typedef unsigned (*NewtonHingeCallBack) (const NewtonJoint* hinge, NewtonHingeSliderUpdateDesc* desc);
typedef unsigned (*NewtonSliderCallBack) (const NewtonJoint* slider, NewtonHingeSliderUpdateDesc* desc);
typedef unsigned (*NewtonUniversalCallBack) (const NewtonJoint* universal, NewtonHingeSliderUpdateDesc* desc);
typedef unsigned (*NewtonCorkscrewCallBack) (const NewtonJoint* corkscrew, NewtonHingeSliderUpdateDesc* desc);
typedef void (*NewtonUserBilateralCallBack) (const NewtonJoint* userJoint);
typedef void (*NewtonConstraintDestructor) (const NewtonJoint* me);

// Xash callback utility
void* Palloc (int size );
void Pfree (void *ptr, int size );

// world control functions
NewtonWorld* NewtonCreate (NewtonAllocMemory malloc, NewtonFreeMemory mfree);
void NewtonDestroy (const NewtonWorld* newtonWorld);
void NewtonDestroyAllBodies (const NewtonWorld* newtonWorld);
void NewtonUpdate (const NewtonWorld* newtonWorld, float timestep);
void NewtonSetPlatformArchitecture (const NewtonWorld* newtonWorld, int mode);
void NewtonSetSolverModel (const NewtonWorld* newtonWorld, int model);
void NewtonSetFrictionModel (const NewtonWorld* newtonWorld, int model);
float NewtonGetTimeStep (const NewtonWorld* newtonWorld);
void NewtonSetMinimumFrameRate (const NewtonWorld* newtonWorld, float frameRate);
void NewtonSetBodyLeaveWorldEvent (const NewtonWorld* newtonWorld, NewtonBodyLeaveWorld callback); 
void NewtonSetWorldSize (const NewtonWorld* newtonWorld, const float* minPoint, const float* maxPoint); 
void NewtonWorldFreezeBody (const NewtonWorld* newtonWorld, const NewtonBody* body);
void NewtonWorldUnfreezeBody (const NewtonWorld* newtonWorld, const NewtonBody* body);
void NewtonWorldForEachBodyDo (const NewtonWorld* newtonWorld, NewtonBodyIterator callback);
void NewtonWorldSetUserData (const NewtonWorld* newtonWorld, void* userData);
void* NewtonWorldGetUserData (const NewtonWorld* newtonWorld);
int NewtonWorldGetVersion (const NewtonWorld* newtonWorld);
void NewtonWorldRayCast (const NewtonWorld* newtonWorld, const float* p0, const float* p1, NewtonWorldRayFilterCallback filter, void* userData);

// Physics Material Section
int NewtonMaterialGetDefaultGroupID(const NewtonWorld* newtonWorld);
int NewtonMaterialCreateGroupID(const NewtonWorld* newtonWorld);
void NewtonMaterialDestroyAllGroupID(const NewtonWorld* newtonWorld);
void NewtonMaterialSetDefaultSoftness (const NewtonWorld* newtonWorld, int id0, int id1, float value);
void NewtonMaterialSetDefaultElasticity (const NewtonWorld* newtonWorld, int id0, int id1, float elasticCoef);
void NewtonMaterialSetDefaultCollidable (const NewtonWorld* newtonWorld, int id0, int id1, int state);
void NewtonMaterialSetContinuousCollisionMode (const NewtonWorld* newtonWorld, int id0, int id1, int state);
void NewtonMaterialSetDefaultFriction (const NewtonWorld* newtonWorld, int id0, int id1, float staticFriction, float kineticFriction);
void NewtonMaterialSetCollisionCallback (const NewtonWorld* newtonWorld, int id0, int id1, void* userData, NewtonContactBegin begin, NewtonContactProcess process, NewtonContactEnd end);
void* NewtonMaterialGetUserData (const NewtonWorld* newtonWorld, int id0, int id1);

// Physics Contact control functions
void NewtonMaterialDisableContact (const NewtonMaterial* material);
float NewtonMaterialGetCurrentTimestep (const NewtonMaterial* material);
void *NewtonMaterialGetMaterialPairUserData (const NewtonMaterial* material);
uint NewtonMaterialGetContactFaceAttribute (const NewtonMaterial* material);
uint NewtonMaterialGetBodyCollisionID (const NewtonMaterial* material, const NewtonBody* body);
float NewtonMaterialGetContactNormalSpeed (const NewtonMaterial* material, const NewtonContact* contactlHandle);
void NewtonMaterialGetContactForce (const NewtonMaterial* material, float* force);
void NewtonMaterialGetContactPositionAndNormal (const NewtonMaterial* material, float* posit, float* normal);
void NewtonMaterialGetContactTangentDirections (const NewtonMaterial* material, float* dir0, float* dir);
float NewtonMaterialGetContactTangentSpeed (const NewtonMaterial* material, const NewtonContact* contactlHandle, int index);
void NewtonMaterialSetContactSoftness (const NewtonMaterial* material, float softness);
void NewtonMaterialSetContactElasticity (const NewtonMaterial* material, float restitution);
void NewtonMaterialSetContactFrictionState (const NewtonMaterial* material, int state, int index);
void NewtonMaterialSetContactStaticFrictionCoef (const NewtonMaterial* material, float coef, int index);
void NewtonMaterialSetContactKineticFrictionCoef (const NewtonMaterial* material, float coef, int index);
void NewtonMaterialSetContactNormalAcceleration (const NewtonMaterial* material, float accel);
void NewtonMaterialSetContactNormalDirection (const NewtonMaterial* material, const float* directionVector);
void NewtonMaterialSetContactTangentAcceleration (const NewtonMaterial* material, float accel, int index);
void NewtonMaterialContactRotateTangentDirections (const NewtonMaterial* material, const float* directionVector);

// convex collision primitives creation functions
NewtonCollision* NewtonCreateNull (const NewtonWorld* newtonWorld);
NewtonCollision* NewtonCreateSphere (const NewtonWorld* newtonWorld, float radiusX, float radiusY, float radiusZ, const float *offsetMatrix);
NewtonCollision* NewtonCreateBox (const NewtonWorld* newtonWorld, float dx, float dy, float dz, const float *offsetMatrix);
NewtonCollision* NewtonCreateCone (const NewtonWorld* newtonWorld, float radius, float height, const float *offsetMatrix);
NewtonCollision* NewtonCreateCapsule (const NewtonWorld* newtonWorld, float radius, float height, const float *offsetMatrix);
NewtonCollision* NewtonCreateCylinder (const NewtonWorld* newtonWorld, float radius, float height, const float *offsetMatrix);
NewtonCollision* NewtonCreateChamferCylinder (const NewtonWorld* newtonWorld, float radius, float height, const float *offsetMatrix);
NewtonCollision* NewtonCreateConvexHull (const NewtonWorld* newtonWorld, int count, const float* vertexCloud, int strideInBytes, const float *offsetMatrix);
NewtonCollision* NewtonCreateConvexHullModifier (const NewtonWorld* newtonWorld, const NewtonCollision* convexHullCollision);

void NewtonConvexHullModifierGetMatrix (const NewtonCollision* convexHullCollision, float* matrix);
void NewtonConvexHullModifierSetMatrix (const NewtonCollision* convexHullCollision, const float* matrix);
void NewtonConvexCollisionSetUserID (const NewtonCollision* convexCollision, uint id);
uint  NewtonConvexCollisionGetUserID (const NewtonCollision* convexCollision);
float NewtonConvexCollisionCalculateVolume (const NewtonCollision* convexCollision);
void NewtonConvexCollisionCalculateInertialMatrix (const NewtonCollision* convexCollision, float* inertia, float* origin);	
void NewtonCollisionMakeUnique (const NewtonWorld* newtonWorld, const NewtonCollision* collision);
void NewtonReleaseCollision (const NewtonWorld* newtonWorld, const NewtonCollision* collision);

// complex collision primitives creation functions
// note: can only be used with static bodies (bodies with infinite mass)
NewtonCollision* NewtonCreateCompoundCollision (const NewtonWorld* newtonWorld, int count, NewtonCollision* const collisionPrimitiveArray[]);
NewtonCollision* NewtonCreateUserMeshCollision (const NewtonWorld* newtonWorld, const float *minBox, const float *maxBox, void *userData, NewtonUserMeshCollisionCollideCallback collideCallback, NewtonUserMeshCollisionRayHitCallback rayHitCallback, NewtonUserMeshCollisionDestroyCallback destroyCallback);

// CollisionTree Utility functions
NewtonCollision* NewtonCreateTreeCollision (const NewtonWorld* newtonWorld, NewtonTreeCollisionCallback userCallback);
void NewtonTreeCollisionBeginBuild (const NewtonCollision* treeCollision);
void NewtonTreeCollisionAddFace (const NewtonCollision* treeCollision, int vertexCount, const float* vertexPtr, int strideInBytes, int faceAttribute);
void NewtonTreeCollisionEndBuild (const NewtonCollision* treeCollision, int optimize);
void NewtonTreeCollisionSerialize (const NewtonCollision* treeCollision, NewtonSerialize serializeFunction, void* serializeHandle);
NewtonCollision* NewtonCreateTreeCollisionFromSerialization (const NewtonWorld* newtonWorld, NewtonTreeCollisionCallback userCallback, NewtonDeserialize deserializeFunction, void* serializeHandle);
int NewtonTreeCollisionGetFaceAtribute (const NewtonCollision* treeCollision, const int* faceIndexArray); 
void NewtonTreeCollisionSetFaceAtribute (const NewtonCollision* treeCollision, const int* faceIndexArray, int attribute); 

	
// General purpose collision library functions
int NewtonCollisionPointDistance (const NewtonWorld* newtonWorld, const float *point, const NewtonCollision* collsion, const float* matrix, float* contact, float* normal);
int NewtonCollisionClosestPoint (const NewtonWorld* newtonWorld, const NewtonCollision* collsionA, const float* matrixA, const NewtonCollision* collsionB, const float* matrixB, float* contactA, float* contactB, float* normalAB);
int NewtonCollisionCollide (const NewtonWorld* newtonWorld, int maxSize, const NewtonCollision* collsionA, const float* matrixA, const NewtonCollision* collsionB, const float* matrixB, float* contacts, float* normals, float* penetration);
int NewtonCollisionCollideContinue (const NewtonWorld* newtonWorld, int maxSize, const float timestap, const NewtonCollision* collsionA, const float* matrixA, const float* velocA, const float* omegaA, const NewtonCollision* collsionB, const float* matrixB, const float* velocB, const float* omegaB, float* timeOfImpact, float* contacts, float* normals, float* penetration);
float NewtonCollisionRayCast (const NewtonCollision* collision, const float* p0, const float* p1, float* normals, int* attribute);
void NewtonCollisionCalculateAABB (const NewtonCollision* collision, const float *matrix, float* p0, float* p1);

// transforms utility functions
void NewtonGetEulerAngle (const float* matrix, float* eulersAngles);
void NewtonSetEulerAngle (const float* eulersAngles, float* matrix);

// body manipulation functions
NewtonBody* NewtonCreateBody (const NewtonWorld* newtonWorld, const NewtonCollision* collision);
void  NewtonDestroyBody(const NewtonWorld* newtonWorld, const NewtonBody* body);

void  NewtonBodyAddForce (const NewtonBody* body, const float* force);
void  NewtonBodyAddTorque (const NewtonBody* body, const float* torque);

void  NewtonBodySetMatrix (const NewtonBody* body, const float* matrix);
void  NewtonBodySetMatrixRecursive (const NewtonBody* body, const float* matrix);
void  NewtonBodySetMassMatrix (const NewtonBody* body, float mass, float Ixx, float Iyy, float Izz);
void  NewtonBodySetMaterialGroupID (const NewtonBody* body, int id);
void  NewtonBodySetContinuousCollisionMode (const NewtonBody* body, uint state);
void  NewtonBodySetJointRecursiveCollision (const NewtonBody* body, uint state);
void  NewtonBodySetOmega (const NewtonBody* body, const float* omega);
void  NewtonBodySetVelocity (const NewtonBody* body, const float* velocity);
void  NewtonBodySetForce (const NewtonBody* body, const float* force);
void  NewtonBodySetTorque (const NewtonBody* body, const float* torque);

void  NewtonBodySetCentreOfMass  (const NewtonBody* body, const float* com);
void  NewtonBodySetLinearDamping (const NewtonBody* body, float linearDamp);
void  NewtonBodySetAngularDamping (const NewtonBody* body, const float* angularDamp);
void  NewtonBodySetUserData (const NewtonBody* body, void* userData);
void  NewtonBodyCoriolisForcesMode (const NewtonBody* body, int mode);
void  NewtonBodySetCollision (const NewtonBody* body, const NewtonCollision* collision);
void  NewtonBodySetAutoFreeze (const NewtonBody* body, int state);
void  NewtonBodySetFreezeTreshold (const NewtonBody* body, float freezeSpeed2, float freezeOmega2, int framesCount);
	
void  NewtonBodySetTransformCallback (const NewtonBody* body, NewtonSetTransform callback);
void  NewtonBodySetDestructorCallback (const NewtonBody* body, NewtonBodyDestructor callback);
void  NewtonBodySetAutoactiveCallback (const NewtonBody* body, NewtonBodyActivationState callback);
void  NewtonBodySetForceAndTorqueCallback (const NewtonBody* body, NewtonApplyForceAndTorque callback);
NewtonApplyForceAndTorque NewtonBodyGetForceAndTorqueCallback (const NewtonBody* body);
	
void* NewtonBodyGetUserData (const NewtonBody* body);
NewtonWorld* NewtonBodyGetWorld (const NewtonBody* body);
NewtonCollision* NewtonBodyGetCollision (const NewtonBody* body);
int   NewtonBodyGetMaterialGroupID (const NewtonBody* body);
int   NewtonBodyGetContinuousCollisionMode (const NewtonBody* body);
int   NewtonBodyGetJointRecursiveCollision (const NewtonBody* body);

void  NewtonBodyGetMatrix(const NewtonBody* body, float* matrix);
void  NewtonBodyGetMassMatrix (const NewtonBody* body, float* mass, float* Ixx, float* Iyy, float* Izz);
void  NewtonBodyGetInvMass(const NewtonBody* body, float* invMass, float* invIxx, float* invIyy, float* invIzz);
void  NewtonBodyGetOmega(const NewtonBody* body, float* vector);
void  NewtonBodyGetVelocity(const NewtonBody* body, float* vector);
void  NewtonBodyGetForce(const NewtonBody* body, float* vector);
void  NewtonBodyGetTorque(const NewtonBody* body, float* vector);
void  NewtonBodyGetCentreOfMass (const NewtonBody* body, float* com);

int   NewtonBodyGetSleepingState(const NewtonBody* body);
int   NewtonBodyGetAutoFreeze(const NewtonBody* body);
float NewtonBodyGetLinearDamping (const NewtonBody* body);
void  NewtonBodyGetAngularDamping (const NewtonBody* body, float* vector);
void  NewtonBodyGetAABB (const NewtonBody* body, float* p0, float* p1);	
void  NewtonBodyGetFreezeTreshold (const NewtonBody* body, float* freezeSpeed2, float* freezeOmega2);

void  NewtonBodyAddBuoyancyForce (const NewtonBody* body, float fluidDensity, float fluidLinearViscosity, float fluidAngularViscosity, const float* gravityVector, NewtonGetBuoyancyPlane buoyancyPlane, void *context);
void NewtonBodyForEachPolygonDo (const NewtonBody* body, NewtonCollisionIterator callback);
void NewtonAddBodyImpulse (const NewtonBody* body, const float* pointDeltaVeloc, const float* pointPosit);

// Common joint functions
void* NewtonJointGetUserData (const NewtonJoint* joint);
void NewtonJointSetUserData (const NewtonJoint* joint, void* userData);
int NewtonJointGetCollisionState (const NewtonJoint* joint);
void NewtonJointSetCollisionState (const NewtonJoint* joint, int state);
float NewtonJointGetStiffness (const NewtonJoint* joint);
void NewtonJointSetStiffness (const NewtonJoint* joint, float state);
void NewtonDestroyJoint(const NewtonWorld* newtonWorld, const NewtonJoint* joint);
void NewtonJointSetDestructor (const NewtonJoint* joint, NewtonConstraintDestructor destructor);

// Ball and Socket joint functions
NewtonJoint* NewtonConstraintCreateBall (const NewtonWorld* newtonWorld, const float* pivotPoint, const NewtonBody* childBody, const NewtonBody* parentBody);
void NewtonBallSetUserCallback (const NewtonJoint* ball, NewtonBallCallBack callback);
void NewtonBallGetJointAngle (const NewtonJoint* ball, float* angle);
void NewtonBallGetJointOmega (const NewtonJoint* ball, float* omega);
void NewtonBallGetJointForce (const NewtonJoint* ball, float* force);
void NewtonBallSetConeLimits (const NewtonJoint* ball, const float* pin, float maxConeAngle, float maxTwistAngle);

// Hinge joint functions
NewtonJoint* NewtonConstraintCreateHinge (const NewtonWorld* newtonWorld, const float* pivotPoint, const float* pinDir, const NewtonBody* childBody, const NewtonBody* parentBody);
void NewtonHingeSetUserCallback (const NewtonJoint* hinge, NewtonHingeCallBack callback);
float NewtonHingeGetJointAngle (const NewtonJoint* hinge);
float NewtonHingeGetJointOmega (const NewtonJoint* hinge);
void NewtonHingeGetJointForce (const NewtonJoint* hinge, float* force);
float NewtonHingeCalculateStopAlpha (const NewtonJoint* hinge, const NewtonHingeSliderUpdateDesc* desc, float angle);

// Slider joint functions
NewtonJoint* NewtonConstraintCreateSlider (const NewtonWorld* newtonWorld, const float* pivotPoint, const float* pinDir, const NewtonBody* childBody, const NewtonBody* parentBody);
void NewtonSliderSetUserCallback (const NewtonJoint* slider, NewtonSliderCallBack callback);
float NewtonSliderGetJointPosit (const NewtonJoint* slider);
float NewtonSliderGetJointVeloc (const NewtonJoint* slider);
void NewtonSliderGetJointForce (const NewtonJoint* slider, float* force);
float NewtonSliderCalculateStopAccel (const NewtonJoint* slider, const NewtonHingeSliderUpdateDesc* desc, float position);


// Corkscrew joint functions
NewtonJoint* NewtonConstraintCreateCorkscrew (const NewtonWorld* newtonWorld, const float* pivotPoint, const float* pinDir, const NewtonBody* childBody, const NewtonBody* parentBody);
void NewtonCorkscrewSetUserCallback (const NewtonJoint* corkscrew, NewtonCorkscrewCallBack callback);
float NewtonCorkscrewGetJointPosit (const NewtonJoint* corkscrew);
float NewtonCorkscrewGetJointAngle (const NewtonJoint* corkscrew);
float NewtonCorkscrewGetJointVeloc (const NewtonJoint* corkscrew);
float NewtonCorkscrewGetJointOmega (const NewtonJoint* corkscrew);
void NewtonCorkscrewGetJointForce (const NewtonJoint* corkscrew, float* force);
float NewtonCorkscrewCalculateStopAlpha (const NewtonJoint* corkscrew, const NewtonHingeSliderUpdateDesc* desc, float angle);
float NewtonCorkscrewCalculateStopAccel (const NewtonJoint* corkscrew, const NewtonHingeSliderUpdateDesc* desc, float position);

// Universal joint functions
NewtonJoint* NewtonConstraintCreateUniversal (const NewtonWorld* newtonWorld, const float* pivotPoint, const float* pinDir0, const float* pinDir1, const NewtonBody* childBody, const NewtonBody* parentBody);
void NewtonUniversalSetUserCallback (const NewtonJoint* universal, NewtonUniversalCallBack callback);
float NewtonUniversalGetJointAngle0 (const NewtonJoint* universal);
float NewtonUniversalGetJointAngle1 (const NewtonJoint* universal);
float NewtonUniversalGetJointOmega0 (const NewtonJoint* universal);
float NewtonUniversalGetJointOmega1 (const NewtonJoint* universal);
void NewtonUniversalGetJointForce (const NewtonJoint* universal, float* force);
float NewtonUniversalCalculateStopAlpha0 (const NewtonJoint* universal, const NewtonHingeSliderUpdateDesc* desc, float angle);
float NewtonUniversalCalculateStopAlpha1 (const NewtonJoint* universal, const NewtonHingeSliderUpdateDesc* desc, float angle);


// Up vector joint functions
NewtonJoint* NewtonConstraintCreateUpVector (const NewtonWorld* newtonWorld, const float* pinDir, const NewtonBody* body); 
void NewtonUpVectorGetPin (const NewtonJoint* upVector, float *pin);
void NewtonUpVectorSetPin (const NewtonJoint* upVector, const float *pin);


// User defined bilateral Joint
NewtonJoint* NewtonConstraintCreateUserJoint (const NewtonWorld* newtonWorld, int maxDOF, NewtonUserBilateralCallBack callback, const NewtonBody* childBody, const NewtonBody* parentBody); 
void NewtonUserJointAddLinearRow (const NewtonJoint* joint, const float *pivot0, const float *pivot1, const float *dir);
void NewtonUserJointAddAngularRow (const NewtonJoint* joint, float relativeAngle, const float *dir);
void NewtonUserJointAddGeneralRow (const NewtonJoint* joint, const float *jacobian0, const float *jacobian1);
void NewtonUserJointSetRowMinimumFriction (const NewtonJoint* joint, float friction);
void NewtonUserJointSetRowMaximumFriction (const NewtonJoint* joint, float friction);
void NewtonUserJointSetRowAcceleration (const NewtonJoint* joint, float acceleration);
void NewtonUserJointSetRowSpringDamperAcceleration (const NewtonJoint* joint, float springK, float springD);
void NewtonUserJointSetRowStiffness (const NewtonJoint* joint, float stiffness);
float NewtonUserJointGetRowForce (const NewtonJoint* joint, int row);

// Rag doll joint container functions
NewtonRagDoll* NewtonCreateRagDoll (const NewtonWorld* newtonWorld);
void NewtonDestroyRagDoll (const NewtonWorld* newtonWorld, const NewtonRagDoll* ragDoll);
void NewtonRagDollBegin (const NewtonRagDoll* ragDoll);
void NewtonRagDollEnd (const NewtonRagDoll* ragDoll);

//void NewtonRagDollSetFriction (const NewtonRagDoll* ragDoll, float friction);
NewtonRagDollBone* NewtonRagDollFindBone (const NewtonRagDoll* ragDoll, int id);
//NewtonRagDollBone* NewtonRagDollGetRootBone (const NewtonRagDoll* ragDoll);

void NewtonRagDollSetForceAndTorqueCallback (const NewtonRagDoll* ragDoll, NewtonApplyForceAndTorque callback);
void NewtonRagDollSetTransformCallback (const NewtonRagDoll* ragDoll, NewtonSetRagDollTransform callback);
NewtonRagDollBone* NewtonRagDollAddBone (const NewtonRagDoll* ragDoll, const NewtonRagDollBone* parent, void *userData, float mass, const float* matrix, const NewtonCollision* boneCollision, const float* size);
void* NewtonRagDollBoneGetUserData (const NewtonRagDollBone* bone);
NewtonBody* NewtonRagDollBoneGetBody (const NewtonRagDollBone* bone);
void NewtonRagDollBoneSetID (const NewtonRagDollBone* bone, int id);
void NewtonRagDollBoneSetLimits (const NewtonRagDollBone* bone, const float* coneDir, float minConeAngle, float maxConeAngle, float maxTwistAngle, const float* bilateralConeDir, float negativeBilateralConeAngle, float positiveBilateralConeAngle);
	
//NewtonRagDollBone* NewtonRagDollBoneGetChild (const NewtonRagDollBone* bone);
//NewtonRagDollBone* NewtonRagDollBoneGetSibling (const NewtonRagDollBone* bone);
//NewtonRagDollBone* NewtonRagDollBoneGetParent (const NewtonRagDollBone* bone);
//void NewtonRagDollBoneSetLocalMatrix (const NewtonRagDollBone* bone, float* matrix);
//void NewtonRagDollBoneSetGlobalMatrix (const NewtonRagDollBone* bone, float* matrix);
void NewtonRagDollBoneGetLocalMatrix (const NewtonRagDollBone* bone, float* matrix);
void NewtonRagDollBoneGetGlobalMatrix (const NewtonRagDollBone* bone, float* matrix);

// Vehicle joint functions
NewtonJoint* NewtonConstraintCreateVehicle (const NewtonWorld* newtonWorld, const float* upDir, const NewtonBody* body); 
void NewtonVehicleReset (const NewtonJoint* vehicle); 
void NewtonVehicleSetTireCallback (const NewtonJoint* vehicle, NewtonVehicleTireUpdate update);
void* NewtonVehicleAddTire (const NewtonJoint* vehicle, const float* localMatrix, const float* pin, float mass, float width, float radius, float suspesionShock, float suspesionSpring, float suspesionLength, void* userData, int collisionID);
void NewtonVehicleRemoveTire (const NewtonJoint* vehicle, void* tireId);
void* NewtonVehicleGetFirstTireID (const NewtonJoint* vehicle);
void* NewtonVehicleGetNextTireID (const NewtonJoint* vehicle, void* tireId);
int NewtonVehicleTireIsAirBorne (const NewtonJoint* vehicle, void* tireId);
int NewtonVehicleTireLostSideGrip (const NewtonJoint* vehicle, void* tireId);
int NewtonVehicleTireLostTraction (const NewtonJoint* vehicle, void* tireId);
void* NewtonVehicleGetTireUserData (const NewtonJoint* vehicle, void* tireId);
float NewtonVehicleGetTireOmega (const NewtonJoint* vehicle, void* tireId);
float NewtonVehicleGetTireNormalLoad (const NewtonJoint* vehicle, void* tireId);
float NewtonVehicleGetTireSteerAngle (const NewtonJoint* vehicle, void* tireId);
float NewtonVehicleGetTireLateralSpeed (const NewtonJoint* vehicle, void* tireId);
float NewtonVehicleGetTireLongitudinalSpeed (const NewtonJoint* vehicle, void* tireId);
void NewtonVehicleGetTireMatrix (const NewtonJoint* vehicle, void* tireId, float* matrix);
void NewtonVehicleSetTireTorque (const NewtonJoint* vehicle, void* tireId, float torque);
void NewtonVehicleSetTireSteerAngle (const NewtonJoint* vehicle, void* tireId, float angle);
void NewtonVehicleSetTireMaxSideSleepSpeed (const NewtonJoint* vehicle, void* tireId, float speed);
void NewtonVehicleSetTireSideSleepCoeficient (const NewtonJoint* vehicle, void* tireId, float coeficient);
void NewtonVehicleSetTireMaxLongitudinalSlideSpeed (const NewtonJoint* vehicle, void* tireId, float speed);
void NewtonVehicleSetTireLongitudinalSlideCoeficient (const NewtonJoint* vehicle, void* tireId, float coeficient);
float NewtonVehicleTireCalculateMaxBrakeAcceleration (const NewtonJoint* vehicle, void* tireId);
void NewtonVehicleTireSetBrakeAcceleration (const NewtonJoint* vehicle, void* tireId, float accelaration, float torqueLimit);

#endif//BASE_PHYSICS_H