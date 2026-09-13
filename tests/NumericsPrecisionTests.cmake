set(NUMERICS_SEPARATE_LIBM OFF)
if(UNIX AND NOT APPLE)
    set(NUMERICS_SEPARATE_LIBM ON)
endif()
foreach(bits IN ITEMS 32 64)
    add_test(NAME ada.numerics${bits}
        COMMAND ${CMAKE_COMMAND}
            -DADA=$<TARGET_FILE:ada>
            -DCC=${CMAKE_C_COMPILER}
            -DRUNTIME=$<TARGET_FILE:adart>
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/ada/numerics${bits}.adb
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/ada/numerics${bits}.expected
            -DWORKDIR=${ADA_TEST_WORK_DIR}/numerics${bits}
            -DBITS=${bits}
            -DSEPARATE_LIBM=${NUMERICS_SEPARATE_LIBM}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/RunNumericsPrecision.cmake
    )
endforeach()

add_executable(numerics_precision_test NumericsPrecision.c ${CMAKE_SOURCE_DIR}/runtime/adanumerics.c)
target_include_directories(numerics_precision_test PRIVATE ${CMAKE_SOURCE_DIR}/runtime)
target_compile_options(numerics_precision_test PRIVATE -UNDEBUG)
if(NUMERICS_SEPARATE_LIBM)
    target_link_libraries(numerics_precision_test PRIVATE m)
endif()
add_test(NAME runtime.numerics_precision COMMAND numerics_precision_test)
