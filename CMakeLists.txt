cmake_minimum_required(VERSION 3.26.0)

project(Void
        VERSION 0.0.0
        LANGUAGES CXX C
        DESCRIPTION "Void Practice Game Engine")

set(SDL3_INCLUDES "$ENV{VULKAN_SDK}/Include")
set(VULKAN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/SDL/include")
set(CGLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/cglm/include")
set(VENDER_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/vender")

if(MSVC)
    set(VULKAN_LIB "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib")
    set(SDL_LIB "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL3/x64/SDL3.lib")

    #Set the location of the engine source.
    set(ENGINE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src")
else()
endif()

file(GLOB_RECURSE GLSL_SOURCE_FILES "src/Shaders/*.vert" "src/Shaders/*.frag" "src/Shaders/*.comp" "src/Shaders/*.mesh" "src/Shaders/*.task")
file(GLOB_RECURSE GLSL_HEADER_FILES "src/Shaders/*.h")

set(VOID_SOURCE src/main.cpp)

add_executable(Void ${VOID_SOURCE} ${GLSL_SOURCE_FILES} ${GLSL_HEADER_FILES})

if (WIN32)
    target_compile_definitions(Void PRIVATE
                               _CRT_SECURE_NO_WARNINGS
                               WIN32_LEAN_AND_MEAN
                               NOMINMAX)
endif()

target_include_directories(Void SYSTEM PUBLIC  ${VULKAN_INCLUDE_DIR} 
                                               ${CMAKE_CURRENT_SOURCE_DIR}
                                               ${ENGINE_INCLUDE}
                                               ${SDL3_INCLUDES}
                                               ${CGLM_INCLUDE_DIR}
                                               ${VENDER_INCLUDE})

add_compile_definitions(SDL_MAIN_HANDLED)


set(FOUNDATION_SOURCE #src/Foundation/Array.hpp
                      #src/Foundation/Array.cpp
                      src/Foundation/Assert.hpp
                      #src/Foundation/Bit.cpp
                      #src/Foundation/Bit.hpp
                      #src/Foundation/BlobSerialisation.cpp
                      #src/Foundation/BlobSerialisation.hpp
                      #src/Foundation/Blob.hpp
                      #src/Foundation/Camera.cpp
                      #src/Foundation/Camera.hpp
                      src/Foundation/Colour.cpp
                      src/Foundation/Colour.hpp
                      #src/Foundation/DataStructures.cpp
                      #src/Foundation/DataStructures.hpp
                      #src/Foundation/File.cpp
                      #src/Foundation/File.hpp
                      #src/Foundation/HashMap.hpp
                      #src/Foundation/HashMap.cpp
                      src/Foundation/Log.cpp
                      src/Foundation/Log.hpp
                      #src/Foundation/MemoryUtils.hpp
                      #src/Foundation/Memory.cpp
                      #src/Foundation/Memory.hpp
                      #src/Foundation/Numerics.cpp
                      #src/Foundation/Numerics.hpp
                      src/Foundation/Platform.hpp
                      #src/Foundation/Process.cpp
                      #src/Foundation/Process.hpp
                      #src/Foundation/RelativeDataStructures.hpp
                      #src/Foundation/RelativeDataStructures.cpp
                      #src/Foundation/ResourceManager.cpp
                      #src/Foundation/ResourceManager.hpp

                      #src/Foundation/String.cpp
                      #src/Foundation/String.hpp
                      #src/Foundation/Time.cpp
                      #src/Foundation/Time.hpp
                      #src/Foundation/WindowsDeclarations.hpp
)
add_library(Foundation STATIC ${FOUNDATION_SOURCE})

target_compile_definitions(Foundation PRIVATE
    _CRT_SECURE_NO_WARNINGS

    TRACY_ENABLE
    TRACY_ON_DEMAND
    TRACY_NO_SYSTEM_TRACING
)

target_include_directories(Foundation SYSTEM PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}
                           ${ENGINE_INCLUDE}
                           ${VENDOR_INCLUDE}
)

if (WIN32)
    target_link_libraries(Void PRIVATE
                          ${SDL_LIB})
else()
    target_link_libraries(Void PRIVATE
                          dl
                          pthread)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${VOID_SOURCE} ${GLSL_SOURCE_FILES} ${GLSL_HEADER_FILES} ${FOUNDATION_SOURCE})

target_link_libraries(Void PRIVATE ${VULKAN_LIB})

target_compile_options(Void PRIVATE
                       $<$<CXX_COMPILER_ID:MSVC>:/W4>
                       $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic>
)

if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Void)
endif()

if (WIN32)
    set(THINGS_TO_COPY "${CMAKE_CURRENT_SOURCE_DIR}/libs/SDL3/x64/SDL3.dll"
	                   "${ENGINE_INCLUDE}/Assets")

	file(COPY ${THINGS_TO_COPY} DESTINATION ${CMAKE_BINARY_DIR})
endif()

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