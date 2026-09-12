set(ADA_TEST_WORK_DIR ${CMAKE_CURRENT_BINARY_DIR}/work)

# add_ada_test(<name> SOURCES <file>...)
#
# Builds the sources into a program with the ada driver and compares its
# standard output with <name>.expected.
function(add_ada_test name)
    cmake_parse_arguments(ADA_TEST "" "" "SOURCES" ${ARGN})

    set(sources "")
    foreach(source ${ADA_TEST_SOURCES})
        if(sources STREQUAL "")
            set(sources "${CMAKE_CURRENT_SOURCE_DIR}/ada/${source}")
        else()
            set(sources "${sources}|${CMAKE_CURRENT_SOURCE_DIR}/ada/${source}")
        endif()
    endforeach()

    add_test(NAME ada.${name}
        COMMAND ${CMAKE_COMMAND}
            -DNAME=${name}
            -DADA=$<TARGET_FILE:ada>
            -DSOURCES=${sources}
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/ada/${name}.expected
            -DWORKDIR=${ADA_TEST_WORK_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/RunAdaTest.cmake
    )
endfunction()

# add_ada_error_test(<name> SOURCES <file>...)
#
# Compiles sources that are meant to be rejected and compares the diagnostics
# with <name>.expected.
function(add_ada_error_test name)
    cmake_parse_arguments(ADA_ERROR "" "" "SOURCES" ${ARGN})

    set(sources "")
    foreach(source ${ADA_ERROR_SOURCES})
        if(sources STREQUAL "")
            set(sources "${CMAKE_CURRENT_SOURCE_DIR}/ada/${source}")
        else()
            set(sources "${sources}|${CMAKE_CURRENT_SOURCE_DIR}/ada/${source}")
        endif()
    endforeach()

    add_test(NAME error.${name}
        COMMAND ${CMAKE_COMMAND}
            -DNAME=${name}
            -DADAC=$<TARGET_FILE:adac>
            -DSOURCES=${sources}
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/ada/${name}.expected
            -DWORKDIR=${ADA_TEST_WORK_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckAdaErrors.cmake
    )
endfunction()

# add_ada_ir_test(<name> SOURCES <file>...)
#
# Compares the generated QBE IL against golden/<name>.ssa.
function(add_ada_ir_test name)
    cmake_parse_arguments(ADA_IR "" "" "SOURCES" ${ARGN})

    set(sources "")
    foreach(source ${ADA_IR_SOURCES})
        if(sources STREQUAL "")
            set(sources "${CMAKE_CURRENT_SOURCE_DIR}/ada/${source}")
        else()
            set(sources "${sources}|${CMAKE_CURRENT_SOURCE_DIR}/ada/${source}")
        endif()
    endforeach()

    add_test(NAME ir.${name}
        COMMAND ${CMAKE_COMMAND}
            -DNAME=${name}
            -DADAC=$<TARGET_FILE:adac>
            -DSOURCES=${sources}
            -DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/golden/${name}.ssa
            -DWORKDIR=${ADA_TEST_WORK_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckAdaIr.cmake
    )
endfunction()

add_ada_test(hello SOURCES hello.adb)
add_ada_test(arithmetic SOURCES arithmetic.adb)
add_ada_test(control SOURCES control.adb)
add_ada_test(subprograms SOURCES subprograms.adb)
add_ada_test(composite SOURCES composite.adb)
add_ada_test(packages SOURCES packages.adb)
add_ada_test(checks SOURCES checks.adb)
add_ada_test(strings SOURCES strings.adb)
add_ada_test(ranges SOURCES ranges.adb)
add_ada_test(floats SOURCES floats.adb)
add_ada_test(bases SOURCES bases.adb)
add_ada_test(textfiles SOURCES textfiles.adb)
add_ada_test(generics SOURCES generics.adb)
add_ada_test(seqio SOURCES seqio.adb)
add_ada_test(directio SOURCES directio.adb)
add_ada_test(streamio SOURCES streamio.adb)
add_ada_test(typeio SOURCES typeio.adb)
add_ada_test(enumio SOURCES enumio.adb)
add_ada_test(storage SOURCES storage.adb)
add_ada_test(cases SOURCES cases.adb)
add_ada_test(access SOURCES access.adb)
add_ada_test(private SOURCES private.adb)
add_ada_test(variants SOURCES variants.adb)

add_ada_error_test(floaterrors SOURCES floaterrors.adb)
add_ada_error_test(baseerrors SOURCES baseerrors.adb)
add_ada_error_test(genericerrors SOURCES genericerrors.adb)
add_ada_error_test(ioerrors SOURCES ioerrors.adb)
add_ada_error_test(caseerrors SOURCES caseerrors.adb)
add_ada_error_test(privateerrors SOURCES privateerrors.adb)
add_ada_error_test(varianterrors SOURCES varianterrors.adb)

add_ada_ir_test(hello SOURCES hello.adb)

add_ada_test(overflow32 SOURCES overflow32.adb)

add_ada_test(overflow64 SOURCES overflow64.adb)

add_ada_test(integerwidth SOURCES integerwidth.adb)

add_ada_test(integerdivision SOURCES integerdivision.adb)

add_ada_test(integerconversions SOURCES integerconversions.adb)

add_ada_test(integerloops SOURCES integerloops.adb)

add_ada_test(integerstatic SOURCES integerstatic.adb)

add_ada_test(longintegerio SOURCES longintegerio.adb)

add_ada_test(overloads SOURCES overloads.adb)

add_ada_error_test(overloaderrors SOURCES overloaderrors.adb)

add_test(NAME harness.exit_status
    COMMAND ${CMAKE_COMMAND}
        -DADA=$<TARGET_FILE:ada>
        -DWORKDIR=${ADA_TEST_WORK_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckExitStatus.cmake
)

add_ada_test(arraycompare SOURCES arraycompare.adb)
add_ada_test(recordcompare SOURCES recordcompare.adb)
add_ada_error_test(comparisonerrors SOURCES comparisonerrors.adb)

add_ada_test(defaultcalls SOURCES defaultcalls.adb)
add_ada_test(parameterless SOURCES parameterless.adb)
add_ada_error_test(parameterlesserrors SOURCES parameterlesserrors.adb)

add_ada_error_test(defaulterrors SOURCES defaulterrors.adb)
