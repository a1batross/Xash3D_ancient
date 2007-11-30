# Microsoft Developer Studio Project File - Name="bullet" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=bullet - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "bullet.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "bullet.mak" CFG="bullet - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "bullet - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "bullet - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "bullet - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "debug"
# PROP BASE Intermediate_Dir "debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\physic\bullet\!debug"
# PROP Intermediate_Dir "..\temp\physic\bullet\!debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE MTL /nologo /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /mktyplib203 /o "NUL" /win32 /D "_DEBUG" /D "_LIB" /D "_WINDOWS"
LINK32=link.exe
# ADD BASE LINK32 user32.lib gdi32.lib advapi32.lib /nologo /machine:I386
# ADD LINK32 shell32.lib user32.lib gdi32.lib advapi32.lib /nologo /version:4.0 /machine:I386 /debug /pdbtype:sept /subsystem:windows
# ADD BASE CPP /nologo /G5 /W3 /D "WIN32" /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GR /ZI /Od /I "./" /D "_LIB" /D "_MT" /D "_MBCS" /D "_DEBUG" /D "_WINDOWS" /D "WIN32" /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409 /fo"bullet\libbulletmath.res"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo /o"..\temp\physic\bullet\!debug\physic.bsc"
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"physic.lib"

!ELSEIF  "$(CFG)" == "bullet - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "release"
# PROP BASE Intermediate_Dir "release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\physic\bullet\!release"
# PROP Intermediate_Dir "..\temp\physic\bullet\!release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE MTL /nologo /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /mktyplib203 /o "NUL" /win32 /D "NDEBUG" /D "_LIB" /D "_WINDOWS"
LINK32=link.exe
# ADD BASE LINK32 user32.lib gdi32.lib advapi32.lib /nologo /machine:I386
# ADD LINK32 shell32.lib user32.lib gdi32.lib advapi32.lib /nologo /version:4.0 /machine:I386 /OPT:NOREF /subsystem:windows
# ADD BASE CPP /nologo /G5 /W3 /D "WIN32" /FD /c
# ADD CPP /nologo /MD /W3 /Ot /Og /Oi /Oy /Ob2 /Gy /I "./" /D "_LIB" /D "_MT" /D "_MBCS" /D "NDEBUG" /D "_WINDOWS" /D "WIN32" /FD /GF /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo /o"..\temp\physic\bullet\!release\physic.bsc"
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"physic.lib"

!ENDIF 

# Begin Target

# Name "bullet - Win32 Debug"
# Name "bullet - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=LinearMath\btAlignedAllocator.cpp
# End Source File
# Begin Source File

SOURCE=LinearMath\btGeometryUtil.cpp
# End Source File
# Begin Source File

SOURCE=LinearMath\btQuickprof.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=LinearMath\btAabbUtil2.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btAlignedAllocator.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btAlignedObjectArray.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btDefaultMotionState.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btGeometryUtil.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btIDebugDraw.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btList.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btMatrix3x3.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btMinMax.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btMotionState.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btPoint3.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btPoolAllocator.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btQuadWord.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btQuaternion.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btQuickprof.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btRandom.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btScalar.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btSimdMinMax.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btStackAlloc.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btTransform.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btTransformUtil.h
# End Source File
# Begin Source File

SOURCE=LinearMath\btVector3.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btAxisSweep3.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btAxisSweep3.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btBoxShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btBoxShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btBroadphaseInterface.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btBroadphaseProxy.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btBroadphaseProxy.h
# End Source File
# Begin Source File

SOURCE=.\btBulletCollisionCommon.h
# End Source File
# Begin Source File

SOURCE=.\btBulletDynamicsCommon.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btBvhTriangleMeshShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btBvhTriangleMeshShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCapsuleShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCapsuleShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionConfiguration.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionCreateFunc.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionDispatcher.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionDispatcher.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCollisionMargin.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionObject.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionObject.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCollisionShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCollisionShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCompoundCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCompoundCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCompoundShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCompoundShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConcaveShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConcaveShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConeShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConeShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btConeTwistConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btConeTwistConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btConstraintSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btContactConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btContactConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btContactSolverInfo.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btContinuousConvexCollision.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btContinuousConvexCollision.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btContinuousDynamicsWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btContinuousDynamicsWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btConvexCast.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btConvexCast.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btConvexConcaveCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btConvexConcaveCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btConvexConvexAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btConvexConvexAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexHullShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexHullShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexInternalShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexInternalShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btConvexPenetrationDepthSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexTriangleMeshShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexTriangleMeshShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCylinderShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCylinderShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btDefaultCollisionConfiguration.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btDefaultCollisionConfiguration.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btDiscreteCollisionDetectorInterface.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btDiscreteDynamicsWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btDiscreteDynamicsWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btDispatcher.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btDispatcher.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btDynamicsWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btEmptyCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btEmptyCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btEmptyShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btEmptyShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btGeneric6DofConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btGeneric6DofConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkConvexCast.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkConvexCast.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkEpa.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkEpa.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkEpaPenetrationDepthSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkEpaPenetrationDepthSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkPairDetector.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkPairDetector.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btHeightfieldTerrainShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btHeightfieldTerrainShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btHingeConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btHingeConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btJacobianEntry.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btManifoldPoint.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btManifoldResult.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btManifoldResult.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btMinkowskiPenetrationDepthSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btMinkowskiPenetrationDepthSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btMinkowskiSumShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btMinkowskiSumShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btMultiSapBroadphase.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btMultiSapBroadphase.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btMultiSphereShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btMultiSphereShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btOptimizedBvh.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btOptimizedBvh.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btOverlappingPairCache.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btOverlappingPairCache.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btPersistentManifold.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btPersistentManifold.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btPoint2PointConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btPoint2PointConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btPointCollector.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btPolyhedralConvexShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btPolyhedralConvexShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btRaycastCallback.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btRaycastCallback.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Vehicle\btRaycastVehicle.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Vehicle\btRaycastVehicle.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btRigidBody.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btRigidBody.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btSequentialImpulseConstraintSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btSequentialImpulseConstraintSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btSimpleBroadphase.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btSimpleBroadphase.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btSimpleDynamicsWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btSimpleDynamicsWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btSimplexSolverInterface.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSimulationIslandManager.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSimulationIslandManager.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btSolve2LinearConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btSolve2LinearConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btSolverBody.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btSolverConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereBoxCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereBoxCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btSphereShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btSphereShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereSphereCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereSphereCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereTriangleCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereTriangleCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btStaticPlaneShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btStaticPlaneShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btStridingMeshInterface.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btStridingMeshInterface.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btSubSimplexConvexCast.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btSubSimplexConvexCast.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTetrahedronShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTetrahedronShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleBuffer.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleBuffer.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleCallback.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleCallback.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleIndexVertexArray.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleIndexVertexArray.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleMesh.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleMesh.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleMeshShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleMeshShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btTypedConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btTypedConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btUniformScalingShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btUniformScalingShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btUnionFind.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btUnionFind.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Vehicle\btVehicleRaycaster.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btVoronoiSimplexSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btVoronoiSimplexSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Vehicle\btWheelInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Vehicle\btWheelInfo.h
# End Source File
# Begin Source File

SOURCE=".\btBulletApi.cpp"
# End Source File
# Begin Source File

SOURCE=".\btBulletApi.h"
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\SphereTriangleDetector.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\SphereTriangleDetector.h
# End Source File
# End Target
# End Project
