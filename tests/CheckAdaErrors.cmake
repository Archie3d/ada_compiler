# Checks that adac rejects a program, comparing the diagnostics it writes with
# a recorded reference.

if(NOT DEFINED NAME OR NOT DEFINED SOURCES OR NOT DEFINED EXPECTED)
    message(FATAL_ERROR "CheckAdaErrors.cmake requires NAME, SOURCES and EXPECTED")
endif()

string(REPLACE "|" ";" source_list "${SOURCES}")
set(work "${WORKDIR}/${NAME}")
file(MAKE_DIRECTORY "${work}")

execute_process(
    COMMAND "${ADAC}" -o "${work}/${NAME}.ssa" ${source_list}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
)
if(status EQUAL 0)
    message(FATAL_ERROR "adac accepted ${NAME}, which is meant to be rejected")
endif()
if(NOT "${status}" MATCHES "^[1-9][0-9]*$" OR errors STREQUAL "")
    message(FATAL_ERROR "adac failed without a normal diagnostic: ${status}\n${output}${errors}")
endif()

# A diagnostic names the file the way it was given on the command line.
foreach(source ${source_list})
    get_filename_component(base "${source}" NAME)
    string(REPLACE "${source}" "${base}" errors "${errors}")
endforeach()

file(READ "${EXPECTED}" expected)
if(NOT errors STREQUAL expected)
    message(FATAL_ERROR "diagnostics differ for ${NAME}\n--- expected ---\n${expected}\n--- actual ---\n${errors}")
endif()
