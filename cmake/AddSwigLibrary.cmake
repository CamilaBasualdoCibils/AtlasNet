function(add_swig_library_for_target TARGET_NAME)
    get_target_property(TARGET_SRCS ${TARGET_NAME} SOURCES)
    get_target_property(TARGET_INCLUDES ${TARGET_NAME} INCLUDE_DIRECTORIES)

    set(SWIG_I ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.i)

    # Collect headers
    set(HEADERS "")
    foreach(src IN LISTS TARGET_SRCS)
        if(src MATCHES "\\.(h|hpp)$")
            get_filename_component(abs "${src}" ABSOLUTE)
            list(APPEND HEADERS "${abs}")
        endif()
    endforeach()

    # Force CMake to reconfigure if headers change
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${HEADERS})

    # Generate .i at CONFIGURE TIME (simple & correct)
    file(WRITE "${SWIG_I}" "%module ${TARGET_NAME}\n\n%{\n")

    foreach(h IN LISTS HEADERS)
        file(APPEND "${SWIG_I}" "#include \"${h}\"\n")
    endforeach()

    file(APPEND "${SWIG_I}" "%}\n\n")

    foreach(h IN LISTS HEADERS)
        file(APPEND "${SWIG_I}" "%include \"${h}\"\n")
    endforeach()

    # SWIG target
    swig_add_library(${TARGET_NAME}_swig
        TYPE MODULE
        LANGUAGE javascript
        SOURCES ${SWIG_I}
    )
    # Force SWIG wrapper to be compiled as C++
get_target_property(SWIG_SRCS ${TARGET_NAME}_swig SOURCES)
set_source_files_properties(${SWIG_SRCS} PROPERTIES LANGUAGE CXX)

    swig_link_libraries(${TARGET_NAME}_swig ${TARGET_NAME})

    set_property(TARGET ${TARGET_NAME}_swig PROPERTY
        SWIG_COMPILE_OPTIONS
            -c++
            -javascript
            -napi
            -DSWIG
    )

    target_compile_features(${TARGET_NAME}_swig PRIVATE cxx_std_20)

    target_compile_definitions(${TARGET_NAME}_swig PRIVATE SWIG)

    target_include_directories(${TARGET_NAME}_swig PRIVATE ${TARGET_INCLUDES})

    set_target_properties(${TARGET_NAME}_swig PROPERTIES
        PREFIX ""
        SUFFIX ".node"
        OUTPUT_NAME "${TARGET_NAME}"
        LIBRARY_OUTPUT_DIRECTORY ${NATIVE_JS_OUT}
        RUNTIME_OUTPUT_DIRECTORY ${NATIVE_JS_OUT}
    )
endfunction()
