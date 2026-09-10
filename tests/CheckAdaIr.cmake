# Compares the QBE intermediate language produced by adac with a recorded
# reference, so that changes to code generation are noticed.

string(REPLACE "|" ";" source_list "${SOURCES}")
set(work "${WORKDIR}/${NAME}")
file(MAKE_DIRECTORY "${work}")
set(ir "${work}/${NAME}.golden.ssa")

execute_process(
    COMMAND "${ADAC}" -o "${ir}" ${source_list}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "adac failed:\n${output}${errors}")
endif()

file(READ "${ir}" actual)
file(READ "${GOLDEN}" expected)
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "generated IL differs from ${GOLDEN}\n--- expected ---\n${expected}\n--- actual ---\n${actual}")
endif()
