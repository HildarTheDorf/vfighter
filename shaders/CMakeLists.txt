project(vfighter_shaders)

function(add_shaders)
    set(ALL_SHADER_OUTPUTS "")
    foreach(shader ${ARGN})
        set(SHADER_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/${shader})
        set(SHADER_OUTPUT ${shader}.spv)

        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND glslc ARGS -O --target-env=vulkan1.1 -o ${SHADER_OUTPUT} ${SHADER_INPUT}
            MAIN_DEPENDENCY ${SHADER_INPUT}
            VERBATIM
        )
        set(ALL_SHADER_OUTPUTS ${ALL_SHADER_OUTPUTS} ${SHADER_OUTPUT})
    endforeach()

    add_custom_target(vfighter_shaders DEPENDS ${ALL_SHADER_OUTPUTS})
endfunction()

add_shaders(main.frag main.vert)