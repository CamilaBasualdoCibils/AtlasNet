function(add_run_target)
    # ---- Arguments ----
    set(options)
    set(oneValueArgs
        NAME            # name of the run target
        TARGET          # executable target to run
        WORKING_DIR     # optional
    )
    set(multiValueArgs
        ARGS            # command line arguments
        ENV             # environment variables (KEY=VALUE)
    )

    cmake_parse_arguments(RUN
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    if (NOT RUN_NAME)
        message(FATAL_ERROR "add_run_target: NAME is required")
    endif()

    if (NOT RUN_TARGET)
        message(FATAL_ERROR "add_run_target: TARGET is required")
    endif()

    if (NOT TARGET ${RUN_TARGET})
        message(FATAL_ERROR "add_run_target: TARGET '${RUN_TARGET}' does not exist")
    endif()

    if (NOT RUN_WORKING_DIR)
        set(RUN_WORKING_DIR $<TARGET_FILE_DIR:${RUN_TARGET}>)
    endif()

    add_custom_target(${RUN_NAME}
        COMMAND
            ${CMAKE_COMMAND} -E env
            ${RUN_ENV}
            $<TARGET_FILE:${RUN_TARGET}>
            ${RUN_ARGS}
        DEPENDS ${RUN_TARGET}
        WORKING_DIRECTORY ${RUN_WORKING_DIR}
        USES_TERMINAL
    )
endfunction()