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
set(VULKAN_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include")
set(CGLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/cglm/include")
set(TLSF_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/tlsf")
set(STACK_WALKER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/StackWalker/Main/StackWalker")
set(RAPID_HASH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/rapidhash")
set(VENDER_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/vender")
#Set the location of the engine source.
set(ENGINE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(MSVC)
    set(VULKAN_LIB "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib")
    set(SDL_LIB "${CMAKE_CURRENT_SOURCE_DIR}/libs/Win/SDL3/x64/SDL3.lib")
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES "src/Shaders/*.vert" "src/Shaders/*.frag" "src/Shaders/*.comp" "src/Shaders/*.mesh" "src/Shaders/*.task")
file(GLOB_RECURSE GLSL_HEADER_FILES "src/Shaders/*.h")

set(VOID_SOURCE src/main.cpp
                src/Application/GameCamera.hpp
                src/Application/GameCamera.cpp
                src/Application/Input.hpp
                src/Application/Input.cpp
                src/Application/Window.hpp
                src/Application/Window.cpp
                src/Application/Keys.hpp
                src/Application/Keys.cpp
                src/Graphics/VulkanRenderer.hpp
                src/Graphics/VulkanRenderer.cpp)

add_executable(Void ${VOID_SOURCE} ${GLSL_SOURCE_FILES} ${GLSL_HEADER_FILES})

if (WIN32)
    target_compile_definitions(Void PRIVATE
                               _CRT_SECURE_NO_WARNINGS
                               WIN32_LEAN_AND_MEAN
                               NOMINMAX
                               #VOID_IMGUI                        
                               VK_USE_PLATFORM_WIN32_KHR)
else(UNIX)
    target_compile_definitions(Void PRIVATE 
                               #VOID_IMGUI                     
                               VK_USE_PLATFORM_WAYLAND_KHR)
endif()

target_include_directories(Void SYSTEM PUBLIC  ${VULKAN_INCLUDE_DIR} 
                                               ${CMAKE_CURRENT_SOURCE_DIR}
                                               ${ENGINE_INCLUDE}
                                               ${SDL3_INCLUDES}
                                               ${RAPID_HASH_DIR}
                                               ${TLSF_INCLUDE_DIR}
                                               ${CGLM_INCLUDE_DIR}
                                               ${VENDER_INCLUDE})

add_compile_definitions(SDL_MAIN_HANDLED)


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
                      src/Foundation/ResourceManager.cpp
                      src/Foundation/ResourceManager.hpp
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

if (WIN32)
    target_link_libraries(Void PRIVATE
                          ${SDL_LIB})
else()
    target_link_libraries(Void PRIVATE
                          dl
                          pthread
                          SDL3
                          vulkan)
endif()

add_library(External STATIC
    vender/tlsf/tlsf.c
    vender/tlsf/tlsf.h

    #vender/json.hpp

    #vender/imgui/imconfig.h
    #vender/imgui/imgui_draw.cpp
    #vender/imgui/imgui_impl_sdl2.cpp
    #vender/imgui/imgui_impl_sdl2.h
    #vender/imgui/imgui_impl_sdlrenderer2.h
    #vender/imgui/imgui_impl_sdlrenderer2.cpp
    #vender/imgui/imgui_internal.h
    #vender/imgui/imgui_memory_editor.h
    #vender/imgui/imgui_tables.cpp
    #vender/imgui/imgui_widgets.cpp
    #vender/imgui/imgui.cpp
    #vender/imgui/imgui.h
    #vender/imgui/imstb_rectpack.h
    #vender/imgui/imstb_textedit.h
    #vender/imgui/imstb_truetype.h
    #vender/stb_image.h
    #vender/imgui/TextEditor.cpp
    #vender/imgui/TextEditor.h

    #vender/tracy/tracy/Tracy.hpp
    #vender/tracy/tracy/TracyVulkan.hpp
    #vender/tracy/TracyClient.cpp

    #vender/enkiTS/LockLessMultiReadPipe.h
    #vender/enkiTS/TaskScheduler.cpp
    #vender/enkiTS/TaskScheduler.h

    #vender/meshoptimizer/allocator.cpp
    #vender/meshoptimizer/clusterizer.cpp
    #vender/meshoptimizer/indexanalyzer.cpp
    #vender/meshoptimizer/indexcodec.cpp
    #vender/meshoptimizer/indexgenerator.cpp
    #vender/meshoptimizer/meshoptimizer.h
    #vender/meshoptimizer/overdrawoptimizer.cpp
    #vender/meshoptimizer/partition.cpp
    #vender/meshoptimizer/quantization.cpp
    #vender/meshoptimizer/rasterizer.cpp
    #vender/meshoptimizer/simplifier.cpp
    #vender/meshoptimizer/spatialorder.cpp
    #vender/meshoptimizer/stripifier.cpp
    #vender/meshoptimizer/vcacheoptimizer.cpp
    #vender/meshoptimizer/vertexcodec.cpp
    #vender/meshoptimizer/vertexfilter.cpp
    #vender/meshoptimizer/vfetchoptimizer.cpp
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
    target_include_directories(External SYSTEM PRIVATE
                               ${SDL3_INCLUDES})
else()
    target_link_libraries(External PRIVATE
                          dl)
    target_include_directories(External SYSTEM PRIVATE
                               ${SDL3_INCLUDES})
endif()

target_link_libraries(Void PRIVATE Foundation External)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VOID_SOURCE} ${GLSL_SOURCE_FILES} ${GLSL_HEADER_FILES} ${FOUNDATION_SOURCE})

target_link_libraries(Void PRIVATE ${VULKAN_LIB})

target_compile_options(Void PRIVATE
                       $<$<CXX_COMPILER_ID:MSVC>:/W4>
                       $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic>
)

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
                               "${CMAKE_CURRENT_SOURCE_DIR}/libs/Lin/SDL3/x64/run_game.sh"
                               "${CMAKE_CURRENT_SOURCE_DIR}/libs/Lin/SDL3/x64/install.sh")
endif()

file(COPY ${THINGS_TO_COPY} DESTINATION ${CMAKE_BINARY_DIR})

#NOTE At release this needs to be updated to look for glslc at a particular location.
if(UNIX)
    set(GLSL_VALIDATOR "glslc")
elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslc.exe")
else()
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslc.exe")
endif()

set(SPIRV_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Assets/Shaders/")
foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(STEM ${GLSL} NAME)
    set(SPIRV "${SPIRV_OUTPUT_DIR}${STEM}.spv")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUTPUT_DIR}"
        COMMAND ${GLSL_VALIDATOR} --target-env=vulkan1.3 -O ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL} ${GLSL_HEADER_FILES}
    )
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(CompileShaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(Void CompileShaders)
