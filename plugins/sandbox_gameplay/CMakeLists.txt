set(SANDBOX_GAMEPLAY_PLUGIN_SOURCES
    src/plugin_sandbox_gameplay.cpp
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin.json
    ${CMAKE_CURRENT_BINARY_DIR}/plugin.json
    COPYONLY
)

add_library(sandbox_plugin_gameplay_static STATIC ${SANDBOX_GAMEPLAY_PLUGIN_SOURCES})

target_include_directories(sandbox_plugin_gameplay_static
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ENGINE_ROOT}/engine
)

target_compile_features(sandbox_plugin_gameplay_static PUBLIC cxx_std_20)

target_compile_options(sandbox_plugin_gameplay_static
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>
)

target_precompile_headers(sandbox_plugin_gameplay_static
    PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${ENGINE_ROOT}/engine/public/engine/pch.hpp>"
)

target_link_libraries(sandbox_plugin_gameplay_static
    PUBLIC
        engine_core
        plugin_input_runtime_static
        plugin_animation_runtime_static
        plugin_state_machine_runtime_static
        plugin_animation_graph_static
        plugin_terrain_runtime_static
)
