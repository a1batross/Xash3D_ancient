# Microsoft Developer Studio Project File - Name="physic" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=physic - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "physic.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "physic.mak" CFG="physic - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "physic - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "physic - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "physic - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\temp\physic\!release"
# PROP Intermediate_Dir "..\temp\physic\!release"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PHYSIC_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "../public" /I "./" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 /nologo /dll /pdb:none /machine:I386 /nodefaultlib:"libc.lib" /libpath:"../public/libs/"
# Begin Custom Build
TargetDir=\XASH3D\src_main\!source\temp\physic\!release
InputPath=\XASH3D\src_main\!source\temp\physic\!release\physic.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\physic.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\physic.dll "D:\Xash3D\bin\physic.dll"

# End Custom Build

!ELSEIF  "$(CFG)" == "physic - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\temp\physic\!debug"
# PROP Intermediate_Dir "..\temp\physic\!debug"
# PROP Ignore_Export_Lib 1
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "PHYSIC_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "../public" /I "./" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /dll /debug /machine:I386 /nodefaultlib:"libc.lib" /pdbtype:sept /libpath:"../public/libs/"
# SUBTRACT LINK32 /nodefaultlib
# Begin Custom Build
TargetDir=\XASH3D\src_main\!source\temp\physic\!debug
InputPath=\XASH3D\src_main\!source\temp\physic\!debug\physic.dll
SOURCE="$(InputPath)"

"D:\Xash3D\bin\physic.dll" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	copy $(TargetDir)\physic.dll "D:\Xash3D\bin\physic.dll"

# End Custom Build

!ENDIF 

# Begin Target

# Name "physic - Win32 Release"
# Name "physic - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\LinearMath\btAlignedAllocator.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btAxisSweep3.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btBoxShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btBroadphaseProxy.cpp
# End Source File
# Begin Source File

SOURCE=.\btBulletBspLoader.cpp
# End Source File
# Begin Source File

SOURCE=.\btBulletStudioLoader.cpp
# End Source File
# Begin Source File

SOURCE=.\btBulletWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btBvhTriangleMeshShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCapsuleShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionDispatcher.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionObject.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCollisionShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCompoundCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCompoundShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConcaveShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConeShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btConeTwistConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btContactConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btContinuousConvexCollision.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btContinuousDynamicsWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btConvexCast.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btConvexConcaveCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btConvexConvexAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexHullShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexInternalShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexTriangleMeshShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCylinderShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btDefaultCollisionConfiguration.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btDiscreteDynamicsWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btDispatcher.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btEmptyCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btEmptyShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btGeneric6DofConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btGeometryUtil.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkConvexCast.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkEpa.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkEpaPenetrationDepthSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkPairDetector.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btHeightfieldTerrainShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btHingeConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btManifoldResult.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btMinkowskiPenetrationDepthSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btMinkowskiSumShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btMultiSapBroadphase.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btMultiSphereShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btOptimizedBvh.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btOverlappingPairCache.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btPersistentManifold.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btPoint2PointConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btPolyhedralConvexShape.cpp
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btQuickprof.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btRaycastCallback.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Vehicle\btRaycastVehicle.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btRigidBody.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btSequentialImpulseConstraintSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btSimpleBroadphase.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btSimpleDynamicsWorld.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSimulationIslandManager.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btSolve2LinearConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereBoxCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btSphereShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereSphereCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereTriangleCollisionAlgorithm.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btStaticPlaneShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btStridingMeshInterface.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btSubSimplexConvexCast.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTetrahedronShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleBuffer.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleCallback.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleIndexVertexArray.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleMesh.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleMeshShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btTypedConstraint.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btUniformScalingShape.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btUnionFind.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btVoronoiSimplexSolver.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Vehicle\btWheelInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\physic.c
# End Source File
# Begin Source File

SOURCE=.\pworld.cpp
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\SphereTriangleDetector.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\LinearMath\btAabbUtil2.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btAlignedAllocator.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btAlignedObjectArray.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btAxisSweep3.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btBoxShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btBroadphaseInterface.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btBroadphaseProxy.h
# End Source File
# Begin Source File

SOURCE=.\btBulletBspLoader.h
# End Source File
# Begin Source File

SOURCE=.\btBulletCollisionCommon.h
# End Source File
# Begin Source File

SOURCE=.\btBulletDynamicsCommon.h
# End Source File
# Begin Source File

SOURCE=.\btBulletStudioLoader.h
# End Source File
# Begin Source File

SOURCE=.\btBulletWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btBvhTriangleMeshShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCapsuleShape.h
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

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionDispatcher.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCollisionMargin.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionObject.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCollisionShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCollisionWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btCompoundCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCompoundShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConcaveShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConeShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btConeTwistConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btConstraintSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btContactConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btContactSolverInfo.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btContinuousConvexCollision.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btContinuousDynamicsWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btConvexCast.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btConvexConcaveCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btConvexConvexAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexHullShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexInternalShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btConvexPenetrationDepthSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btConvexTriangleMeshShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btCylinderShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btDefaultCollisionConfiguration.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btDefaultMotionState.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btDiscreteCollisionDetectorInterface.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btDiscreteDynamicsWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btDispatcher.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btDynamicsWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btEmptyCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btEmptyShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btGeneric6DofConstraint.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btGeometryUtil.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkConvexCast.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkEpa.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkEpaPenetrationDepthSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btGjkPairDetector.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btHeightfieldTerrainShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btHingeConstraint.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btIDebugDraw.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btJacobianEntry.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btList.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btManifoldPoint.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btManifoldResult.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btMatrix3x3.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btMinkowskiPenetrationDepthSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btMinkowskiSumShape.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btMinMax.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btMotionState.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btMultiSapBroadphase.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btMultiSphereShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btOptimizedBvh.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btOverlappingPairCache.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btPersistentManifold.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btPoint2PointConstraint.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btPoint3.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btPointCollector.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btPolyhedralConvexShape.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btPoolAllocator.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btQuadWord.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btQuaternion.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btQuickprof.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btRandom.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btRaycastCallback.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Vehicle\btRaycastVehicle.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btRigidBody.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btScalar.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btSequentialImpulseConstraintSolver.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btSimdMinMax.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\BroadphaseCollision\btSimpleBroadphase.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Dynamics\btSimpleDynamicsWorld.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btSimplexSolverInterface.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSimulationIslandManager.h
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

SOURCE=.\BulletCollision\CollisionDispatch\btSphereBoxCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btSphereShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereSphereCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btSphereTriangleCollisionAlgorithm.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btStackAlloc.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btStaticPlaneShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btStridingMeshInterface.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btSubSimplexConvexCast.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTetrahedronShape.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btTransform.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btTransformUtil.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleBuffer.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleCallback.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleIndexVertexArray.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleMesh.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleMeshShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btTriangleShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\ConstraintSolver\btTypedConstraint.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionShapes\btUniformScalingShape.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\btUnionFind.h
# End Source File
# Begin Source File

SOURCE=.\LinearMath\btVector3.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Vehicle\btVehicleRaycaster.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\NarrowPhaseCollision\btVoronoiSimplexSolver.h
# End Source File
# Begin Source File

SOURCE=.\BulletDynamics\Vehicle\btWheelInfo.h
# End Source File
# Begin Source File

SOURCE=.\physic.h
# End Source File
# Begin Source File

SOURCE=.\BulletCollision\CollisionDispatch\SphereTriangleDetector.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
