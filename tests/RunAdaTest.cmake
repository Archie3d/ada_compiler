# Runs one Ada test program end to end through the ada driver, then compares
# the program output against the recorded expectation.

if(NOT DEFINED NAME OR NOT DEFINED SOURCES OR NOT DEFINED EXPECTED)
    message(FATAL_ERROR "RunAdaTest.cmake requires NAME, SOURCES and EXPECTED")
endif()

string(REPLACE "|" ";" source_list "${SOURCES}")
set(work "${WORKDIR}/${NAME}")
file(MAKE_DIRECTORY "${work}")

set(program "${work}/${NAME}")

execute_process(
    COMMAND "${ADA}" -o "${program}" ${source_list}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "ada failed:\n${output}${errors}")
endif()

# Programs that write files do so beside their own executable, so a test never
# leaves anything behind in the source tree.
execute_process(
    COMMAND "${program}"
    WORKING_DIRECTORY "${work}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE actual
    ERROR_VARIABLE errors
)

file(READ "${EXPECTED}" expected)
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "output mismatch for ${NAME}\n--- expected ---\n${expected}\n--- actual ---\n${actual}")
endif()
