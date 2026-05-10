cmake_minimum_required(VERSION 3.25)

project(sandbox_app LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(sandbox_read_local_engine_root out_var)
    set(_local_config "${CMAKE_CURRENT_SOURCE_DIR}/project.local.json")
    if(NOT EXISTS "${_local_config}")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    file(READ "${_local_config}" _local_json)
    string(JSON _engine_root ERROR_VARIABLE _engine_root_error GET "${_local_json}" engineRoot)
    if(_engine_root_error)
        set(_engine_root "")
    endif()
    set(${out_var} "${_engine_root}" PARENT_SCOPE)
endfunction()

function(sandbox_resolve_engine_root out_var)
    if(DEFINED ENGINE_ROOT AND NOT "${ENGINE_ROOT}" STREQUAL "")
        set(_engine_root "${ENGINE_ROOT}")
    elseif(DEFINED ENV{MY_ENGINE_ROOT} AND NOT "$ENV{MY_ENGINE_ROOT}" STREQUAL "")
        set(_engine_root "$ENV{MY_ENGINE_ROOT}")
    else()
        sandbox_read_local_engine_root(_engine_root)
    endif()

    if(_engine_root STREQUAL "")
        set(_engine_root "../my_engine")
    endif()

    if(NOT IS_ABSOLUTE "${_engine_root}")
        get_filename_component(_engine_root "${CMAKE_CURRENT_SOURCE_DIR}/${_engine_root}" ABSOLUTE)
    else()
        get_filename_component(_engine_root "${_engine_root}" ABSOLUTE)
    endif()

    if(NOT EXISTS "${_engine_root}/CMakeLists.txt")
        message(FATAL_ERROR
            "Engine root does not look valid: ${_engine_root}\n"
            "Configure with -DENGINE_ROOT=<path-to-engine>, set MY_ENGINE_ROOT, or create project.local.json."
        )
    endif()

    set(${out_var} "${_engine_root}" PARENT_SCOPE)
endfunction()

sandbox_resolve_engine_root(SANDBOX_ENGINE_ROOT)
set(ENGINE_ROOT "${SANDBOX_ENGINE_ROOT}" CACHE PATH "Engine repository root" FORCE)
set(ENGINE_DERIVED_IMPORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/temp/derived/imports" CACHE PATH "Project-local derived import root" FORCE)

if(NOT TARGET engine_core)
    add_subdirectory("${SANDBOX_ENGINE_ROOT}" "${CMAKE_BINARY_DIR}/engine")
endif()

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" SANDBOX_PROJECT_ROOT_CMAKE_PATH)
file(TO_CMAKE_PATH "${SANDBOX_ENGINE_ROOT}" SANDBOX_ENGINE_ROOT_CMAKE_PATH)
include("${SANDBOX_ENGINE_ROOT}/cmake/engine_plugins.cmake")

file(GLOB SANDBOX_PLUGIN_DIRS LIST_DIRECTORIES TRUE CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*")
foreach(sandbox_plugin_dir IN LISTS SANDBOX_PLUGIN_DIRS)
    if(IS_DIRECTORY "${sandbox_plugin_dir}" AND EXISTS "${sandbox_plugin_dir}/CMakeLists.txt" AND EXISTS "${sandbox_plugin_dir}/plugin.json")
        add_subdirectory("${sandbox_plugin_dir}")
    endif()
endforeach()

engine_collect_runtime_plugin_targets("${CMAKE_CURRENT_SOURCE_DIR}/project.json" SANDBOX_RUNTIME_PLUGIN_TARGETS)

add_executable(sandbox_app
    src/main.cpp
    src/sandbox_app.cpp
)

target_include_directories(sandbox_app
    PRIVATE
        ${SANDBOX_ENGINE_ROOT}/engine
)

target_compile_definitions(sandbox_app
    PRIVATE
        SANDBOX_PROJECT_ROOT="${SANDBOX_PROJECT_ROOT_CMAKE_PATH}"
        SANDBOX_ENGINE_ROOT="${SANDBOX_ENGINE_ROOT_CMAKE_PATH}"
)

target_link_libraries(sandbox_app
    PRIVATE
        engine_core
        ${SANDBOX_RUNTIME_PLUGIN_TARGETS}
)

set_target_properties(sandbox_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/sandbox_app
)

if(EMSCRIPTEN)
    set_target_properties(sandbox_app PROPERTIES
        SUFFIX ".js"
    )
    target_link_options(sandbox_app
        PRIVATE
            "-sALLOW_MEMORY_GROWTH=1"
            "-sMAX_WEBGL_VERSION=2"
            "-sMIN_WEBGL_VERSION=2"
            "-sUSE_WEBGL2=1"
            "-sEXIT_RUNTIME=0"
            "-sASSERTIONS=1"
            "-sFORCE_FILESYSTEM=1"
            "-sSTACK_SIZE=1048576"
    )
else()
    set_target_properties(sandbox_app PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
