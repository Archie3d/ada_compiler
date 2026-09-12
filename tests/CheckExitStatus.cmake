# Matching stdout must not hide an unhandled exception and nonzero exit status.
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DNAME=exitfailure
        -DADA=${ADA}
        -DSOURCES=${CMAKE_CURRENT_LIST_DIR}/ada/exitfailure.adb
        -DEXPECTED=${CMAKE_CURRENT_LIST_DIR}/ada/exitfailure.expected
        -DWORKDIR=${WORKDIR}
        -P ${CMAKE_CURRENT_LIST_DIR}/RunAdaTest.cmake
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
)
if(status STREQUAL "0" OR NOT errors MATCHES "exitfailure exited with 1")
    message(FATAL_ERROR "Runtime failure was not detected as expected:\n${output}${errors}")
endif()
