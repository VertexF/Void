cmake_minimum_required(VERSION 3.26.0)

project(Void
        VERSION 0.0.0
        LANGUAGES CXX C
        DESCRIPTION "Void Practice Game Engine")

set(AIR_SOURCE src/main.cpp
               src/Shaders/mainShader.frag
               src/Shaders/mainShader.vert
               src/Shaders/compile.cmd)

add_executable(Void ${AIR_SOURCE})

if (WIN32)
    target_compile_definitions(Void PRIVATE
                               _CRT_SECURE_NO_WARNINGS
                               WIN32_LEAN_AND_MEAN
                               NOMINMAX)
endif()

if(MSVC)
    set(SDL3_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/vender/vulkan/Include")
    set(VULKAN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/SDL3/include")
    set(CGLM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender/cglm/include")
    set(STB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vender")

    set(VULKAN_LIB "${CMAKE_CURRENT_SOURCE_DIR}/vender/vulkan/Lib/vulkan-1.lib")
    set(SDL_LIB "${CMAKE_CURRENT_SOURCE_DIR}/vender/SDL3/lib/x64/SDL3.lib")

    #Set the location of the engine source.
    set(ENGINE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src")
else()
endif()

target_include_directories(Void SYSTEM PUBLIC  ${VULKAN_INCLUDE_DIR} 
                                               ${CMAKE_CURRENT_SOURCE_DIR}
                                               ${ENGINE_INCLUDE}
                                               ${SDL3_INCLUDES}
                                               ${CGLM_INCLUDE_DIR}
                                               ${STB_INCLUDE_DIR})

add_compile_definitions(SDL_MAIN_HANDLED)

if (WIN32)
    target_link_libraries(Void PRIVATE
                          ${SDL_LIB})
else()
    target_link_libraries(Void PRIVATE
                          dl
                          pthread)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${AIR_SOURCE})

target_link_libraries(Void PRIVATE ${VULKAN_LIB})

target_compile_options(Void PRIVATE
                       $<$<CXX_COMPILER_ID:MSVC>:/W4>
                       $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic>
)

if(MSVC)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Void)
endif()

#if (WIN32)
#    set(DLLS_TO_COPY "${CMAKE_CURRENT_SOURCE_DIR}/binaries/SDL2-2.28.5/lib/x64/SDL2.dll")
#
#    foreach(DLL ${DLLS_TO_COPY})
#        add_custom_command(TARGET Air POST_BUILD
#            COMMAND ${CMAKE_COMMAND} -E copy ${DLL} "$(OutDir)"
#            VERBATIM
#        )
#    endforeach()
#endif()


