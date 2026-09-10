cmake_minimum_required(VERSION 3.20)

project(ada_compiler)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# Where an installed compiler keeps the parts that are not programs.  The names
# are the ones GNAT uses: the environment is source to be read, and the run time
# is a library to be linked.
set(ADA_INSTALL_LIBRARY ${CMAKE_INSTALL_LIBDIR}/ada)
set(ADA_INSTALL_INCLUDE ${ADA_INSTALL_LIBRARY}/adainclude)
set(ADA_INSTALL_ARCHIVE ${ADA_INSTALL_LIBRARY}/adalib)

set(QBE_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/qbe)

file(GLOB_RECURSE QBE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/qbe/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/qbe/*.h
)

find_program(QBE_MAKE_EXECUTABLE NAMES make gmake REQUIRED)

add_custom_command(
    OUTPUT "${QBE_BUILD_DIR}/qbe.stamp"
    COMMAND
        "${CMAKE_COMMAND}" -E make_directory "${QBE_BUILD_DIR}"
    COMMAND
        "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/qbe"
        "${QBE_BUILD_DIR}"
    COMMAND
        "${QBE_MAKE_EXECUTABLE}" -C "${QBE_BUILD_DIR}" qbe
    COMMAND
        "${CMAKE_COMMAND}" -E touch "${QBE_BUILD_DIR}/qbe.stamp"
    DEPENDS
        ${QBE_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/qbe/Makefile"
    BYPRODUCTS
        "${QBE_BUILD_DIR}/qbe"
    COMMENT "Building qbe in the CMake build tree"
    VERBATIM
)

add_custom_target(qbe_build ALL
    DEPENDS "${QBE_BUILD_DIR}/qbe.stamp"
)

add_executable(qbe IMPORTED GLOBAL)
set_target_properties(qbe PROPERTIES
    IMPORTED_LOCATION "${QBE_BUILD_DIR}/qbe"
)
add_dependencies(qbe qbe_build)

add_subdirectory(runtime)
add_subdirectory(adac)
add_subdirectory(ada)

enable_testing()
add_subdirectory(tests)

# qbe is an imported target, being built by its own Makefile, so the program it
# leaves behind is installed as a file rather than as a target.
install(PROGRAMS "${QBE_BUILD_DIR}/qbe"
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/runtime/ada/
    DESTINATION ${ADA_INSTALL_INCLUDE}
    FILES_MATCHING
        PATTERN "*.ads"
        PATTERN "*.adb"
)
