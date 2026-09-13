add_test(NAME ada.importedfloatabi
    COMMAND ${CMAKE_COMMAND}
        -DADA=$<TARGET_FILE:ada>
        -DCC=${CMAKE_C_COMPILER}
        -DRUNTIME=$<TARGET_FILE:adart>
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/ada/importedfloatabi.adb
        -DHELPERS=${CMAKE_CURRENT_SOURCE_DIR}/ImportedFloatAbi.c
        -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/ada/importedfloatabi.expected
        -DWORKDIR=${ADA_TEST_WORK_DIR}/importedfloatabi
        -P ${CMAKE_CURRENT_SOURCE_DIR}/RunImportedFloatAbi.cmake
)
