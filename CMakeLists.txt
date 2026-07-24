cmake_minimum_required(VERSION 3.26.0)

project(Void
        VERSION 0.0.0
        LANGUAGES CXX C
        DESCRIPTION "Void Practice Game Engine")

#C++ is only "required" to remove warnings for libraries
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mssse3")
endif()

set(SDL3_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/vender/SDL/include")
if(WIN32)
    set(VULKAN_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include")
else(UNIX)
    set(VULKAN_INCLUDE_DIR "$ENV{VULKAN_SDK}/include")
endif()
set(CGLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/cglm/include")
set(TLSF_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/tlsf")
set(STACK_WALKER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/StackWalker/Main/StackWalker")
set(RAPID_HASH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/rapidhash")
set(VENDIR_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/vender")
set(IMGUI_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/vender/imgui")
set(CGLTF_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/vender/cgltf")
set(SPRIV_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/vender/SPIRV-Reflect")
set(MESHOP_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/vender/meshoptimizer/src/")
set(KTX_OTHER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/KTX-Software/other_include")
set(KTX_LIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/KTX-Software/lib/include")
set(KTX_LIB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/KTX-Software/external/dfdutils/KHR")
#Set the location of the engine source.
set(ENGINE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(JOLT_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src/Physics/")

message(STATUS ${VULKAN_INCLUDE_DIR})

if(MSVC)
    set(VULKAN_LIB "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib")
    set(SDL_LIB "${CMAKE_CURRENT_SOURCE_DIR}/libs/Win/SDL3/x64/SDL3.lib")
else()
    set(VULKAN_LIB "/home/user/Documents/git/Void/libs/Lin/Vulkan/libvulkan.so")
    set(SDL_LIB "/home/user/Documents/git/Void/libs/Lin/SDL3/x64/libSDL3.so")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES "src/Shaders/*.vert" "src/Shaders/*.frag" "src/Shaders/*.comp" "src/Shaders/*.mesh" "src/Shaders/*.task")
file(GLOB_RECURSE GLSL_HEADER_FILES "src/Shaders/*.h")

#set(SLANG_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/Shaders/coreShader1.slang)

###Void

set(VOID_SOURCE src/main.cpp
                src/Application/GameCamera.hpp
                src/Application/GameCamera.cpp
                src/Application/Input.hpp
                src/Application/Input.cpp
                src/Application/Window.hpp
                src/Application/Window.cpp
                src/Application/Keys.hpp
                src/Application/Keys.cpp
                src/Application/Audio.hpp
                src/Application/Audio.cpp
                src/Application/UserInterface.hpp
                src/Application/UserInterface.cpp

                src/Physics/Physics.hpp
                src/Physics/Physics.cpp
                src/Physics/ContactListener.hpp
                src/Physics/ContactListener.cpp
                src/Physics/CollisionGeometry.h

                src/Graphics/VoidImgui.hpp
                src/Graphics/VoidImgui.cpp
                src/Graphics/CommandBuffer.hpp
                src/Graphics/CommandBuffer.cpp
                src/Graphics/GPUDevice.cpp
                src/Graphics/GPUDevice.hpp
                src/Graphics/GPUProfiler.hpp
                src/Graphics/GPUProfiler.cpp
                src/Graphics/GPUResources.hpp
                src/Graphics/GPUResources.cpp
                src/Graphics/LoadGLTF.hpp
                src/Graphics/LoadGLTF.cpp
                src/Graphics/Skybox.hpp
                src/Graphics/Skybox.cpp
                src/Graphics/ShaderData.hpp
                src/Graphics/2DRenderer.hpp
                src/Graphics/2DRenderer.cpp
                src/Graphics/ParticleRenderer.hpp
                src/Graphics/ParticleRenderer.cpp

                src/Game/Scene.hpp
                src/Game/Scene.cpp
                src/Game/Player.hpp
                src/Game/Player.cpp
                src/Game/Utils.hpp
                src/Game/Game.hpp
                src/Game/Game.cpp
                src/Game/MainMenu.hpp
                src/Game/MainMenu.cpp
)

add_executable(Void ${VOID_SOURCE} ${GLSL_SOURCE_FILES} ${GLSL_HEADER_FILES})

if (WIN32)
    target_compile_definitions(Void PRIVATE
                               CGLM_FORCE_DEPTH_ZERO_TO_ONE
                               _CRT_SECURE_NO_WARNINGS
                               WIN32_LEAN_AND_MEAN
                               NOMINMAX
                               #VOID_IMGUI                        
                               VK_USE_PLATFORM_WIN32_KHR)
else(UNIX)
    target_compile_definitions(Void PRIVATE 
                               CGLM_FORCE_DEPTH_ZERO_TO_ONE
                               #VOID_IMGUI                     
                               VK_USE_PLATFORM_WAYLAND_KHR)
    target_compile_options(Void PRIVATE -Wno-unused-function)
    target_compile_options(Void PRIVATE -Wno-conversion-null)
endif()

target_include_directories(Void SYSTEM PUBLIC  ${VULKAN_INCLUDE_DIR} 
                                               ${CMAKE_CURRENT_SOURCE_DIR}
                                               ${ENGINE_INCLUDE}
                                               ${SDL3_INCLUDES}
                                               ${RAPID_HASH_DIR}
                                               ${TLSF_INCLUDE_DIR}
                                               ${CGLM_INCLUDE_DIR}
                                               ${VENDER_INCLUDE}
                                               ${IMGUI_INCLUDE}
                                               ${CGLTF_INCLUDE}
                                               ${SPRIV_INCLUDE}
                                               ${MESHOP_INCLUDE}
                                               ${JOLT_INCLUDE}
                                               ${KTX_LIB_INCLUDE_DIR})

add_compile_definitions(SDL_MAIN_HANDLED)

if (WIN32)
    target_link_libraries(Void PRIVATE
                          ${SDL_LIB}
                          ${VULKAN_LIB}
                          ${VOLD_LIB})
else()
    target_link_libraries(Void PRIVATE
                          dl
                          pthread
                          ${SDL_LIB}
                          ${VULKAN_LIB})
endif()

###Foundation

set(FOUNDATION_SOURCE src/Foundation/Array.hpp
                      src/Foundation/Assert.hpp
                      src/Foundation/Bit.cpp
                      src/Foundation/Bit.hpp
                      src/Foundation/BlobSerialisation.cpp
                      src/Foundation/BlobSerialisation.hpp
                      src/Foundation/Blob.hpp
                      src/Foundation/Camera.cpp
                      src/Foundation/Camera.hpp
                      src/Foundation/Colour.cpp
                      src/Foundation/Colour.hpp
                      src/Foundation/File.cpp
                      src/Foundation/File.hpp
                      src/Foundation/HashMap.hpp
                      src/Foundation/HashMap.cpp
                      src/Foundation/Log.hpp
                      src/Foundation/Memory.cpp
                      src/Foundation/Memory.hpp
                      src/Foundation/Numerics.cpp
                      src/Foundation/Numerics.hpp
                      src/Foundation/Platform.hpp
                      src/Foundation/Process.cpp
                      src/Foundation/Process.hpp
                      src/Foundation/RelativeDataStructures.hpp
                      src/Foundation/ResourcePool.cpp
                      src/Foundation/ResourcePool.hpp
                      src/Foundation/String.cpp
                      src/Foundation/String.hpp
                      src/Foundation/Time.cpp
                      src/Foundation/Time.hpp
                      src/Foundation/WindowsDeclarations.hpp
)
add_library(Foundation STATIC ${FOUNDATION_SOURCE})

target_compile_definitions(Foundation PRIVATE
                                      CGLM_FORCE_DEPTH_ZERO_TO_ONE
                                      _CRT_SECURE_NO_WARNINGS
                                      TRACY_ENABLE
                                      TRACY_ON_DEMAND
                                      TRACY_NO_SYSTEM_TRACING)

if(WIN32)
    target_compile_definitions(Foundation PRIVATE
                                          WIN32_LEAN_AND_MEAN
                                          NOMINMAX)
endif()


target_include_directories(Foundation SYSTEM PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}
                           ${CGLM_INCLUDE_DIR}
                           ${TLSF_INCLUDE_DIR}
                           ${STACK_WALKER_DIR}
                           ${RAPID_HASH_DIR}
                           ${ENGINE_INCLUDE}
                           ${VENDOR_INCLUDE})

###External

add_library(External STATIC
    vender/tlsf/tlsf.c
    vender/tlsf/tlsf.h

    vender/imgui/imconfig.h
    vender/imgui/imgui_draw.cpp
    vender/imgui/backends/imgui_impl_sdl3.cpp
    vender/imgui/backends/imgui_impl_sdl3.h
    vender/imgui/imgui_internal.h
    vender/imgui/imgui_tables.cpp
    vender/imgui/imgui_widgets.cpp
    vender/imgui/imgui.cpp
    vender/imgui/imgui.h
    vender/imgui/imstb_rectpack.h
    vender/imgui/imstb_textedit.h
    vender/imgui/imstb_truetype.h

    #vender/tracy/tracy/Tracy.hpp
    #vender/tracy/tracy/TracyVulkan.hpp
    #vender/tracy/TracyClient.cpp

    #vender/enkiTS/LockLessMultiReadPipe.h
    #vender/enkiTS/TaskScheduler.cpp
    #vender/enkiTS/TaskScheduler.h

    vender/meshoptimizer/src/allocator.cpp
    vender/meshoptimizer/src/clusterizer.cpp
    vender/meshoptimizer/src/indexanalyzer.cpp
    vender/meshoptimizer/src/indexcodec.cpp
    vender/meshoptimizer/src/indexgenerator.cpp
    vender/meshoptimizer/src/meshoptimizer.h
    vender/meshoptimizer/src/overdrawoptimizer.cpp
    vender/meshoptimizer/src/partition.cpp
    vender/meshoptimizer/src/quantization.cpp
    vender/meshoptimizer/src/rasterizer.cpp
    vender/meshoptimizer/src/simplifier.cpp
    vender/meshoptimizer/src/spatialorder.cpp
    vender/meshoptimizer/src/stripifier.cpp
    vender/meshoptimizer/src/vcacheoptimizer.cpp
    vender/meshoptimizer/src/vertexcodec.cpp
    vender/meshoptimizer/src/vertexfilter.cpp
    vender/meshoptimizer/src/vfetchoptimizer.cpp

    vender/miniaudio.h
    vender/miniaudio.c

    vender/KTX-Software/lib/src/texture.c
    vender/KTX-Software/lib/src/hashlist.c
    vender/KTX-Software/lib/src/checkheader.c
    vender/KTX-Software/lib/src/swap.c
    vender/KTX-Software/lib/src/memstream.c
    vender/KTX-Software/lib/src/filestream.c
    vender/KTX-Software/lib/src/vkloader.c
)

set_target_properties(External PROPERTIES SYSTEM ON)

if (WIN32)
    target_sources(External PRIVATE
                   vender/StackWalker/Main/StackWalker/StackWalker.cpp
                   vender/StackWalker/Main/StackWalker/StackWalker.h)
endif()

target_compile_definitions(External PRIVATE
                          TRACY_ENABLE
                          TRACY_ON_DEMAND
                          TRACY_NO_SYSTEM_TRACING)

if (WIN32)
    target_include_directories(External PRIVATE
                               ${SDL3_INCLUDES}
                               ${IMGUI_INCLUDE}
                               ${KTX_LIB_INCLUDE_DIR}
                               ${KTX_OTHER_INCLUDE_DIR})
else()
    target_link_libraries(External PRIVATE
                          dl)
    target_include_directories(External PRIVATE
                               ${SDL3_INCLUDES}
                               ${IMGUI_INCLUDE}
                               ${KTX_LIB_INCLUDE_DIR}
                               ${KTX_OTHER_INCLUDE_DIR})
endif()

###Physics

# Select X86 processor features to use (if everything is off it will be SSE2 compatible)
option(USE_SSE4_1 "Enable SSE4.1" ON)
option(USE_SSE4_2 "Enable SSE4.2" ON)
option(USE_AVX "Enable AVX" ON)
option(USE_AVX2 "Enable AVX2" ON)
option(USE_AVX512 "Enable AVX512" OFF)
option(USE_LZCNT "Enable LZCNT" ON)
option(USE_TZCNT "Enable TZCNT" ON)
option(USE_F16C "Enable F16C" ON)
option(USE_FMADD "Enable FMADD" ON)

set(JOLT_PHYSICS_ROOT src/Physics/Jolt)

set(JOLT_PHYSICS_SRC_FILES
    ${JOLT_PHYSICS_ROOT}/AABBTree/AABBTreeBuilder.cpp
    ${JOLT_PHYSICS_ROOT}/AABBTree/AABBTreeBuilder.h
    ${JOLT_PHYSICS_ROOT}/AABBTree/AABBTreeToBuffer.h
    ${JOLT_PHYSICS_ROOT}/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat.h
    ${JOLT_PHYSICS_ROOT}/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags.h
    ${JOLT_PHYSICS_ROOT}/ConfigurationString.h
    ${JOLT_PHYSICS_ROOT}/Compute/ComputeBuffer.h
    ${JOLT_PHYSICS_ROOT}/Compute/ComputeQueue.h
    ${JOLT_PHYSICS_ROOT}/Compute/ComputeSystem.cpp
    ${JOLT_PHYSICS_ROOT}/Compute/ComputeSystem.h
    ${JOLT_PHYSICS_ROOT}/Compute/ComputeShader.h
    ${JOLT_PHYSICS_ROOT}/Core/ARMNeon.h
    ${JOLT_PHYSICS_ROOT}/Core/RISCVVector.h
    ${JOLT_PHYSICS_ROOT}/Core/Array.h
    ${JOLT_PHYSICS_ROOT}/Core/Atomics.h
    ${JOLT_PHYSICS_ROOT}/Core/BinaryHeap.h
    ${JOLT_PHYSICS_ROOT}/Core/ByteBuffer.h
    ${JOLT_PHYSICS_ROOT}/Core/Color.cpp
    ${JOLT_PHYSICS_ROOT}/Core/Color.h
    ${JOLT_PHYSICS_ROOT}/Core/Core.h
    ${JOLT_PHYSICS_ROOT}/Core/Factory.cpp
    ${JOLT_PHYSICS_ROOT}/Core/Factory.h
    ${JOLT_PHYSICS_ROOT}/Core/FixedSizeFreeList.h
    ${JOLT_PHYSICS_ROOT}/Core/FixedSizeFreeList.inl
    ${JOLT_PHYSICS_ROOT}/Core/FPControlWord.h
    ${JOLT_PHYSICS_ROOT}/Core/FPException.h
    ${JOLT_PHYSICS_ROOT}/Core/FPFlushDenormals.h
    ${JOLT_PHYSICS_ROOT}/Core/HashCombine.h
    ${JOLT_PHYSICS_ROOT}/Core/HashTable.h
    ${JOLT_PHYSICS_ROOT}/Core/IncludeWindows.h
    ${JOLT_PHYSICS_ROOT}/Core/InsertionSort.h
    ${JOLT_PHYSICS_ROOT}/Core/IssueReporting.cpp
    ${JOLT_PHYSICS_ROOT}/Core/IssueReporting.h
    ${JOLT_PHYSICS_ROOT}/Core/JobSystem.h
    ${JOLT_PHYSICS_ROOT}/Core/JobSystem.inl
    ${JOLT_PHYSICS_ROOT}/Core/JobSystemSingleThreaded.cpp
    ${JOLT_PHYSICS_ROOT}/Core/JobSystemSingleThreaded.h
    ${JOLT_PHYSICS_ROOT}/Core/JobSystemThreadPool.cpp
    ${JOLT_PHYSICS_ROOT}/Core/JobSystemThreadPool.h
    ${JOLT_PHYSICS_ROOT}/Core/JobSystemWithBarrier.cpp
    ${JOLT_PHYSICS_ROOT}/Core/JobSystemWithBarrier.h
    ${JOLT_PHYSICS_ROOT}/Core/LinearCurve.cpp
    ${JOLT_PHYSICS_ROOT}/Core/LinearCurve.h
    ${JOLT_PHYSICS_ROOT}/Core/LockFreeHashMap.h
    ${JOLT_PHYSICS_ROOT}/Core/LockFreeHashMap.inl
    ${JOLT_PHYSICS_ROOT}/Core/LSANSuppressions.h
    ${JOLT_PHYSICS_ROOT}/Core/Memory.cpp
    ${JOLT_PHYSICS_ROOT}/Core/Memory.h
    ${JOLT_PHYSICS_ROOT}/Core/Mutex.h
    ${JOLT_PHYSICS_ROOT}/Core/MutexArray.h
    ${JOLT_PHYSICS_ROOT}/Core/NonCopyable.h
    ${JOLT_PHYSICS_ROOT}/Core/ObjectToIDMap.h
    ${JOLT_PHYSICS_ROOT}/Core/Prefetch.h
    ${JOLT_PHYSICS_ROOT}/Core/Profiler.cpp
    ${JOLT_PHYSICS_ROOT}/Core/Profiler.h
    ${JOLT_PHYSICS_ROOT}/Core/Profiler.inl
    ${JOLT_PHYSICS_ROOT}/Core/QuickSort.h
    ${JOLT_PHYSICS_ROOT}/Core/Reference.h
    ${JOLT_PHYSICS_ROOT}/Core/Result.h
    ${JOLT_PHYSICS_ROOT}/Core/RTTI.cpp
    ${JOLT_PHYSICS_ROOT}/Core/RTTI.h
    ${JOLT_PHYSICS_ROOT}/Core/ScopeExit.h
    ${JOLT_PHYSICS_ROOT}/Core/Semaphore.cpp
    ${JOLT_PHYSICS_ROOT}/Core/Semaphore.h
    ${JOLT_PHYSICS_ROOT}/Core/StaticArray.h
    ${JOLT_PHYSICS_ROOT}/Core/STLAlignedAllocator.h
    ${JOLT_PHYSICS_ROOT}/Core/STLAllocator.h
    ${JOLT_PHYSICS_ROOT}/Core/STLLocalAllocator.h
    ${JOLT_PHYSICS_ROOT}/Core/STLTempAllocator.h
    ${JOLT_PHYSICS_ROOT}/Core/StreamIn.h
    ${JOLT_PHYSICS_ROOT}/Core/StreamOut.h
    ${JOLT_PHYSICS_ROOT}/Core/StreamUtils.h
    ${JOLT_PHYSICS_ROOT}/Core/StreamWrapper.h
    ${JOLT_PHYSICS_ROOT}/Core/StridedPtr.h
    ${JOLT_PHYSICS_ROOT}/Core/StringTools.cpp
    ${JOLT_PHYSICS_ROOT}/Core/StringTools.h
    ${JOLT_PHYSICS_ROOT}/Core/TempAllocator.h
    ${JOLT_PHYSICS_ROOT}/Core/TickCounter.cpp
    ${JOLT_PHYSICS_ROOT}/Core/TickCounter.h
    ${JOLT_PHYSICS_ROOT}/Core/UnorderedMap.h
    ${JOLT_PHYSICS_ROOT}/Core/UnorderedMapFwd.h
    ${JOLT_PHYSICS_ROOT}/Core/UnorderedSet.h
    ${JOLT_PHYSICS_ROOT}/Core/UnorderedSetFwd.h
    ${JOLT_PHYSICS_ROOT}/Geometry/AABox.h
    ${JOLT_PHYSICS_ROOT}/Geometry/AABox4.h
    ${JOLT_PHYSICS_ROOT}/Geometry/ClipPoly.h
    ${JOLT_PHYSICS_ROOT}/Geometry/ClosestPoint.h
    ${JOLT_PHYSICS_ROOT}/Geometry/ConvexHullBuilder.cpp
    ${JOLT_PHYSICS_ROOT}/Geometry/ConvexHullBuilder.h
    ${JOLT_PHYSICS_ROOT}/Geometry/ConvexHullBuilder2D.cpp
    ${JOLT_PHYSICS_ROOT}/Geometry/ConvexHullBuilder2D.h
    ${JOLT_PHYSICS_ROOT}/Geometry/ConvexSupport.h
    ${JOLT_PHYSICS_ROOT}/Geometry/Ellipse.h
    ${JOLT_PHYSICS_ROOT}/Geometry/EPAConvexHullBuilder.h
    ${JOLT_PHYSICS_ROOT}/Geometry/EPAPenetrationDepth.h
    ${JOLT_PHYSICS_ROOT}/Geometry/GJKClosestPoint.h
    ${JOLT_PHYSICS_ROOT}/Geometry/IndexedTriangle.h
    ${JOLT_PHYSICS_ROOT}/Geometry/Indexify.cpp
    ${JOLT_PHYSICS_ROOT}/Geometry/Indexify.h
    ${JOLT_PHYSICS_ROOT}/Geometry/MortonCode.h
    ${JOLT_PHYSICS_ROOT}/Geometry/OrientedBox.cpp
    ${JOLT_PHYSICS_ROOT}/Geometry/OrientedBox.h
    ${JOLT_PHYSICS_ROOT}/Geometry/Plane.h
    ${JOLT_PHYSICS_ROOT}/Geometry/RayAABox.h
    ${JOLT_PHYSICS_ROOT}/Geometry/RayCapsule.h
    ${JOLT_PHYSICS_ROOT}/Geometry/RayCylinder.h
    ${JOLT_PHYSICS_ROOT}/Geometry/RaySphere.h
    ${JOLT_PHYSICS_ROOT}/Geometry/RayTriangle.h
    ${JOLT_PHYSICS_ROOT}/Geometry/Sphere.h
    ${JOLT_PHYSICS_ROOT}/Geometry/Triangle.h
    ${JOLT_PHYSICS_ROOT}/Jolt.h
    ${JOLT_PHYSICS_ROOT}/Math/BVec16.h
    ${JOLT_PHYSICS_ROOT}/Math/BVec16.inl
    ${JOLT_PHYSICS_ROOT}/Math/DMat44.h
    ${JOLT_PHYSICS_ROOT}/Math/DMat44.inl
    ${JOLT_PHYSICS_ROOT}/Math/Double3.h
    ${JOLT_PHYSICS_ROOT}/Math/DVec3.h
    ${JOLT_PHYSICS_ROOT}/Math/DVec3.inl
    ${JOLT_PHYSICS_ROOT}/Math/DynMatrix.h
    ${JOLT_PHYSICS_ROOT}/Math/EigenValueSymmetric.h
    ${JOLT_PHYSICS_ROOT}/Math/FindRoot.h
    ${JOLT_PHYSICS_ROOT}/Math/Float2.h
    ${JOLT_PHYSICS_ROOT}/Math/Float3.h
    ${JOLT_PHYSICS_ROOT}/Math/Float4.h
    ${JOLT_PHYSICS_ROOT}/Math/GaussianElimination.h
    ${JOLT_PHYSICS_ROOT}/Math/HalfFloat.h
    ${JOLT_PHYSICS_ROOT}/Math/Mat44.h
    ${JOLT_PHYSICS_ROOT}/Math/Mat44.inl
    ${JOLT_PHYSICS_ROOT}/Math/Math.h
    ${JOLT_PHYSICS_ROOT}/Math/MathTypes.h
    ${JOLT_PHYSICS_ROOT}/Math/Matrix.h
    ${JOLT_PHYSICS_ROOT}/Math/Quat.h
    ${JOLT_PHYSICS_ROOT}/Math/Quat.inl
    ${JOLT_PHYSICS_ROOT}/Math/Real.h
    ${JOLT_PHYSICS_ROOT}/Math/Swizzle.h
    ${JOLT_PHYSICS_ROOT}/Math/Trigonometry.h
    ${JOLT_PHYSICS_ROOT}/Math/UVec4.h
    ${JOLT_PHYSICS_ROOT}/Math/UVec4.inl
    ${JOLT_PHYSICS_ROOT}/Math/Vec3.cpp
    ${JOLT_PHYSICS_ROOT}/Math/Vec3.h
    ${JOLT_PHYSICS_ROOT}/Math/Vec3.inl
    ${JOLT_PHYSICS_ROOT}/Math/Vec4.h
    ${JOLT_PHYSICS_ROOT}/Math/Vec4.inl
    ${JOLT_PHYSICS_ROOT}/Math/Vector.h
    ${JOLT_PHYSICS_ROOT}/ObjectStream/ObjectStream.h
    ${JOLT_PHYSICS_ROOT}/ObjectStream/SerializableAttribute.h
    ${JOLT_PHYSICS_ROOT}/ObjectStream/SerializableAttributeEnum.h
    ${JOLT_PHYSICS_ROOT}/ObjectStream/SerializableAttributeTyped.h
    ${JOLT_PHYSICS_ROOT}/ObjectStream/SerializableObject.cpp
    ${JOLT_PHYSICS_ROOT}/ObjectStream/SerializableObject.h
    ${JOLT_PHYSICS_ROOT}/ObjectStream/TypeDeclarations.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/AllowedDOFs.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/Body.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Body/Body.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/Body.inl
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyAccess.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyActivationListener.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyCreationSettings.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyCreationSettings.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyFilter.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyID.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyInterface.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyInterface.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyLock.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyLockInterface.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyLockMulti.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyManager.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyManager.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyPair.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/BodyType.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/MassProperties.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Body/MassProperties.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/MotionProperties.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Body/MotionProperties.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/MotionProperties.inl
    ${JOLT_PHYSICS_ROOT}/Physics/Body/MotionQuality.h
    ${JOLT_PHYSICS_ROOT}/Physics/Body/MotionType.h
    ${JOLT_PHYSICS_ROOT}/Physics/Character/Character.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Character/Character.h
    ${JOLT_PHYSICS_ROOT}/Physics/Character/CharacterBase.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Character/CharacterBase.h
    ${JOLT_PHYSICS_ROOT}/Physics/Character/CharacterID.h
    ${JOLT_PHYSICS_ROOT}/Physics/Character/CharacterVirtual.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Character/CharacterVirtual.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/AABoxCast.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/ActiveEdgeMode.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/ActiveEdges.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BackFaceMode.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/BroadPhase.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/BroadPhase.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/BroadPhaseBruteForce.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/BroadPhaseBruteForce.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/BroadPhaseLayer.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/BroadPhaseQuadTree.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/BroadPhaseQuadTree.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/BroadPhaseQuery.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterMask.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/QuadTree.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/BroadPhase/QuadTree.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CastConvexVsTriangles.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CastConvexVsTriangles.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CastResult.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CastSphereVsTriangles.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CastSphereVsTriangles.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollectFacesMode.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollideConvexVsTriangles.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollideConvexVsTriangles.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollidePointResult.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollideShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollideShapeVsShapePerLeaf.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollideSphereVsTriangles.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollideSphereVsTriangles.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollisionCollector.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollisionCollectorImpl.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollisionDispatch.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollisionDispatch.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollisionGroup.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/CollisionGroup.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/ContactListener.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/EstimateCollisionResponse.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/EstimateCollisionResponse.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/GroupFilter.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/GroupFilter.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/GroupFilterTable.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/GroupFilterTable.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/InternalEdgeRemovingCollector.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/ManifoldBetweenTwoFaces.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/ManifoldBetweenTwoFaces.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/NarrowPhaseQuery.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/NarrowPhaseQuery.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/NarrowPhaseStats.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/NarrowPhaseStats.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/ObjectLayer.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/ObjectLayerPairFilterMask.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/ObjectLayerPairFilterTable.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/PhysicsMaterial.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/PhysicsMaterial.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/PhysicsMaterialSimple.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/PhysicsMaterialSimple.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/RayCast.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/BoxShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/BoxShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/CapsuleShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/CapsuleShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/CompoundShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/CompoundShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/CompoundShapeVisitors.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/ConvexHullShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/ConvexHullShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/ConvexShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/ConvexShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/CylinderShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/CylinderShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/DecoratedShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/DecoratedShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/EmptyShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/EmptyShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/GetTrianglesContext.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/HeightFieldShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/HeightFieldShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/MeshShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/MeshShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/MutableCompoundShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/MutableCompoundShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/OffsetCenterOfMassShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/OffsetCenterOfMassShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/PlaneShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/PlaneShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/PolyhedronSubmergedVolumeCalculator.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/RotatedTranslatedShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/RotatedTranslatedShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/ScaledShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/ScaledShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/ScaleHelpers.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/Shape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/Shape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/SphereShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/SphereShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/StaticCompoundShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/StaticCompoundShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/SubShapeID.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/SubShapeIDPair.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/TaperedCapsuleShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/TaperedCapsuleShape.gliffy
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/TaperedCapsuleShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/TaperedCylinderShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/TaperedCylinderShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/TriangleShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/Shape/TriangleShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/ShapeCast.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/ShapeFilter.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/SimShapeFilter.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/SimShapeFilterWrapper.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/SortReverseAndStore.h
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/TransformedShape.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Collision/TransformedShape.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/CalculateSolverSteps.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConeConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConeConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/Constraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/Constraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintManager.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintManager.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/AngleConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/AxisConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/ContactConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/DualAxisConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/GearConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/HingeRotationConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/IndependentAxisConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/PointConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/RackAndPinionConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/RotationEulerConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/RotationQuatConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/SpringPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ConstraintPart/SwingTwistConstraintPart.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ContactConstraintManager.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/ContactConstraintManager.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/DistanceConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/DistanceConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/FixedConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/FixedConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/GearConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/GearConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/HingeConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/HingeConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/MotorSettings.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/MotorSettings.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/PathConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/PathConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/PathConstraintPath.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/PathConstraintPath.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/PathConstraintPathHermite.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/PathConstraintPathHermite.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/PointConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/PointConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/PulleyConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/PulleyConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/RackAndPinionConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/RackAndPinionConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/SixDOFConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/SixDOFConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/SliderConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/SliderConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/SpringSettings.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/SpringSettings.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/SwingTwistConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/SwingTwistConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/TwoBodyConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Constraints/TwoBodyConstraint.h
    ${JOLT_PHYSICS_ROOT}/Physics/DeterminismLog.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/DeterminismLog.h
    ${JOLT_PHYSICS_ROOT}/Physics/EActivation.h
    ${JOLT_PHYSICS_ROOT}/Physics/EPhysicsUpdateError.h
    ${JOLT_PHYSICS_ROOT}/Physics/IslandBuilder.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/IslandBuilder.h
    ${JOLT_PHYSICS_ROOT}/Physics/LargeIslandSplitter.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/LargeIslandSplitter.h
    ${JOLT_PHYSICS_ROOT}/Physics/PhysicsLock.h
    ${JOLT_PHYSICS_ROOT}/Physics/PhysicsScene.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/PhysicsScene.h
    ${JOLT_PHYSICS_ROOT}/Physics/PhysicsSettings.h
    ${JOLT_PHYSICS_ROOT}/Physics/PhysicsStepListener.h
    ${JOLT_PHYSICS_ROOT}/Physics/PhysicsSystem.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/PhysicsSystem.h
    ${JOLT_PHYSICS_ROOT}/Physics/PhysicsUpdateContext.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/PhysicsUpdateContext.h
    ${JOLT_PHYSICS_ROOT}/Physics/Ragdoll/Ragdoll.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/Ragdoll/Ragdoll.h
    ${JOLT_PHYSICS_ROOT}/Physics/StateRecorder.h
    ${JOLT_PHYSICS_ROOT}/Physics/StateRecorderImpl.cpp
    ${JOLT_PHYSICS_ROOT}/Physics/StateRecorderImpl.h
    ${JOLT_PHYSICS_ROOT}/RegisterTypes.cpp
    ${JOLT_PHYSICS_ROOT}/RegisterTypes.h
    ${JOLT_PHYSICS_ROOT}/Skeleton/SkeletalAnimation.cpp
    ${JOLT_PHYSICS_ROOT}/Skeleton/SkeletalAnimation.h
    ${JOLT_PHYSICS_ROOT}/Skeleton/Skeleton.cpp
    ${JOLT_PHYSICS_ROOT}/Skeleton/Skeleton.h
    ${JOLT_PHYSICS_ROOT}/Skeleton/SkeletonMapper.cpp
    ${JOLT_PHYSICS_ROOT}/Skeleton/SkeletonMapper.h
    ${JOLT_PHYSICS_ROOT}/Skeleton/SkeletonPose.cpp
    ${JOLT_PHYSICS_ROOT}/Skeleton/SkeletonPose.h
    ${JOLT_PHYSICS_ROOT}/TriangleSplitter/TriangleSplitter.cpp
    ${JOLT_PHYSICS_ROOT}/TriangleSplitter/TriangleSplitter.h
    ${JOLT_PHYSICS_ROOT}/TriangleSplitter/TriangleSplitterBinning.cpp
    ${JOLT_PHYSICS_ROOT}/TriangleSplitter/TriangleSplitterBinning.h
    ${JOLT_PHYSICS_ROOT}/TriangleSplitter/TriangleSplitterMean.cpp
    ${JOLT_PHYSICS_ROOT}/TriangleSplitter/TriangleSplitterMean.h
)

# Ignore shader files for compilation, we'll compile them manually
set_source_files_properties(${JOLT_PHYSICS_SHADERS} PROPERTIES HEADER_FILE_ONLY ON)

if (WIN32)
    # Add natvis file
    set(JOLT_PHYSICS_SRC_FILES ${JOLT_PHYSICS_SRC_FILES} ${JOLT_PHYSICS_ROOT}/Jolt.natvis)
endif()

# Vulkan support
#find_package(Vulkan)
# Vulkan source files
set(JOLT_PHYSICS_SRC_FILES
    ${JOLT_PHYSICS_SRC_FILES}
    ${JOLT_PHYSICS_ROOT}/Compute/VK/BufferVK.h
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeBufferVK.cpp
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeBufferVK.h
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeQueueVK.cpp
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeQueueVK.h
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeShaderVK.cpp
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeShaderVK.h
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeSystemVK.cpp
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeSystemVK.h
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeSystemVKImpl.cpp
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeSystemVKImpl.h
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeSystemVKWithAllocator.cpp
    ${JOLT_PHYSICS_ROOT}/Compute/VK/ComputeSystemVKWithAllocator.h
    ${JOLT_PHYSICS_ROOT}/Compute/VK/IncludeVK.h
)

set (PHYSICS_REPO_ROOT "src/Physics")
# Group intermediate files
#source_group(Intermediate FILES ${JOLT_PHYSICS_SPV_SHADERS})
# Group source files

#source_group(TREE ${JOLT_PHYSICS_SHADER_HEADERS})

add_library(Jolt STATIC ${JOLT_PHYSICS_SRC_FILES} ${JOLT_PHYSICS_SHADERS} ${JOLT_PHYSICS_SHADER_HEADERS} ${JOLT_PHYSICS_SPV_SHADERS})

add_library(Jolt::Jolt ALIAS Jolt)

# Use repository as include directory when building, install directory when installing
#target_include_directories(Jolt PUBLIC
#	$<BUILD_INTERFACE:${PHYSICS_REPO_ROOT}>
#	$<INSTALL_INTERFACE:include/>)

target_compile_definitions(Jolt PUBLIC JPH_USE_VK)

target_include_directories(Jolt PUBLIC ${VULKAN_INCLUDE_DIR} ${JOLT_INCLUDE} ${CMAKE_CURRENT_SOURCE_DIR} ${ENGINE_INCLUDE} ${JOLT_RENDERER_INCLUDE} ${JOLT_BASE_RENDERER_INCLUDE})
target_link_libraries(Jolt LINK_PUBLIC ${VULKAN_LIB})

target_compile_definitions(Jolt PUBLIC JPH_USE_CPU_COMPUTE)
#target_compile_definitions(Jolt PUBLIC "JPH_DEBUG_RENDERER")
set(JOLT_PROFILE_DEFINE JPH_PROFILE_ENABLED)

# Emit the instruction set definitions to ensure that child projects use the same settings even if they override the used instruction sets (a mismatch causes link errors)
function(EMIT_X86_INSTRUCTION_SET_DEFINITIONS)
    if (USE_AVX512)
        target_compile_definitions(Jolt PUBLIC JPH_USE_AVX512)
    endif()
    if (USE_AVX2)
        target_compile_definitions(Jolt PUBLIC JPH_USE_AVX2)
    endif()
    if (USE_AVX)
        target_compile_definitions(Jolt PUBLIC JPH_USE_AVX)
    endif()
    if (USE_SSE4_1)
        target_compile_definitions(Jolt PUBLIC JPH_USE_SSE4_1)
    endif()
    if (USE_SSE4_2)
        target_compile_definitions(Jolt PUBLIC JPH_USE_SSE4_2)
    endif()
    if (USE_LZCNT)
        target_compile_definitions(Jolt PUBLIC JPH_USE_LZCNT)
    endif()
    if (USE_TZCNT)
        target_compile_definitions(Jolt PUBLIC JPH_USE_TZCNT)
    endif()
    if (USE_F16C)
        target_compile_definitions(Jolt PUBLIC JPH_USE_F16C)
    endif()
    if (USE_FMADD AND NOT CROSS_PLATFORM_DETERMINISTIC)
        target_compile_definitions(Jolt PUBLIC JPH_USE_FMADD)
    endif()
endfunction()

# Add the compiler commandline flags to select the right instruction sets
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86" OR "${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        if (USE_AVX512)
            target_compile_options(Jolt PUBLIC /arch:AVX512)
        elseif (USE_AVX2)
            target_compile_options(Jolt PUBLIC /arch:AVX2)
        elseif (USE_AVX)
            target_compile_options(Jolt PUBLIC /arch:AVX)
        endif()
        EMIT_X86_INSTRUCTION_SET_DEFINITIONS()
    endif()
else()
    if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AMD64" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86" OR "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i386")
        # x86 and x86_64
        # On 32-bit builds we need to default to using SSE instructions, the x87 FPU instructions have higher intermediate precision
        # which will cause problems in the collision detection code (the effect is similar to leaving FMA on, search for
        # JPH_PRECISE_MATH_ON for the locations where this is a problem).

        if (USE_AVX512)
            target_compile_options(Jolt PUBLIC -mavx512f -mavx512vl -mavx512dq -mavx2 -mbmi -mpopcnt -mlzcnt -mf16c)
        elseif (USE_AVX2)
            target_compile_options(Jolt PUBLIC -mavx2 -mbmi -mpopcnt -mlzcnt -mf16c)
        elseif (USE_AVX)
            target_compile_options(Jolt PUBLIC -mavx -mpopcnt)
        elseif (USE_SSE4_2)
            target_compile_options(Jolt PUBLIC -msse4.2 -mpopcnt)
        elseif (USE_SSE4_1)
            target_compile_options(Jolt PUBLIC -msse4.1)
        else()
            target_compile_options(Jolt PUBLIC -msse2)
        endif()
        if (USE_LZCNT)
            target_compile_options(Jolt PUBLIC -mlzcnt)
        endif()
        if (USE_TZCNT)
            target_compile_options(Jolt PUBLIC -mbmi)
        endif()
        if (USE_F16C)
            target_compile_options(Jolt PUBLIC -mf16c)
        endif()
        if (USE_FMADD AND NOT CROSS_PLATFORM_DETERMINISTIC)
            target_compile_options(Jolt PUBLIC -mfma)
        endif()

        if (NOT MSVC)
            target_compile_options(Jolt PUBLIC -mfpmath=sse)
        endif()

        EMIT_X86_INSTRUCTION_SET_DEFINITIONS()
    endif()
endif()

target_link_libraries(Void PRIVATE Foundation External Jolt)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VOID_SOURCE} ${GLSL_SOURCE_FILES} ${GLSL_HEADER_FILES} ${FOUNDATION_SOURCE} ${JOLT_PHYSICS_SRC_FILES})

target_compile_options(Void PRIVATE
                       $<$<CXX_COMPILER_ID:MSVC>:/W4 /MP>
                       $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -fsanitize=address -fno-omit-frame-pointer>
)

target_link_options(Void PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>: -fsanitize=address>)

if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Void)
endif()

set(THINGS_TO_COPY "${ENGINE_INCLUDE}/Assets")

message(STATUS "${ENGINE_INCLUDE}/Assets")

if (WIN32)
    list(APPEND THINGS_TO_COPY "${CMAKE_CURRENT_SOURCE_DIR}/libs/Win/SDL3/x64/SDL3.dll")
else(UNIX)
    list(APPEND THINGS_TO_COPY "${CMAKE_CURRENT_SOURCE_DIR}/libs/Lin/SDL3/x64/libSDL3.so.0.2.28"
                               "${CMAKE_CURRENT_SOURCE_DIR}/libs/Lin/SDL3/x64/libSDL3.so.0"
                               "${CMAKE_CURRENT_SOURCE_DIR}/libs/Lin/SDL3/x64/libSDL3.so"
                               "${CMAKE_CURRENT_SOURCE_DIR}/libs/Lin/Vulkan/libvulkan.so.1.4.350"
                               "${CMAKE_CURRENT_SOURCE_DIR}/libs/Lin/Vulkan/libvulkan.so.1"
                               "${CMAKE_CURRENT_SOURCE_DIR}/libs/Lin/Vulkan/libvulkan.so"
                               "${CMAKE_CURRENT_SOURCE_DIR}/libs/Lin/run_game.sh")
endif()

file(COPY ${THINGS_TO_COPY} DESTINATION ${CMAKE_BINARY_DIR})

#NOTE At release this needs to be updated to look for glslang at a particular location.
if(UNIX)
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/bin/glslangValidator")
elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

set(SPIRV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Assets/Shaders/")
foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(STEM ${GLSL} NAME)
    set(SPIRV "${SPIRV_OUTPUT_DIR}${STEM}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUTPUT_DIR}"
        COMMAND ${GLSL_VALIDATOR} --target-env vulkan1.4 -l ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL} ${GLSL_HEADER_FILES}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

#if(UNIX)
#    set(SLANG_COMPILER "slangc")
#elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
#    set(SLANG_COMPILER "$ENV{VULKAN_SDK}/Bin/slangc.exe")
#else()
#    set(SLANG_COMPILER "$ENV{VULKAN_SDK}/Bin32/slangc.exe")
#endif()
#
#set(SPIRV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Assets/Shaders/")
#foreach(SLANG ${SLANG_SOURCE_FILES})
#	get_filename_component(STEM ${SLANG} NAME)
#    set(SPIRV "${SPIRV_OUTPUT_DIR}${STEM}.spv")
#	message(STATUS ${SLANG})
#    add_custom_command(
#        OUTPUT ${SPIRV}
#        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUTPUT_DIR}"
#        COMMAND ${SLANG_COMPILER} -target spirv ${SLANG} -o ${SPIRV} -fvk-use-scalar-layout
#        DEPENDS ${SLANG} ${SLANG_HEADER_FILES}
#    )
#    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
#endforeach()


add_custom_target(CompileShaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(Void CompileShaders)
