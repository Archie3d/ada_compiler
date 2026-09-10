set(TARGET ada)

find_program(ADA_HOST_CC NAMES cc gcc clang REQUIRED)

add_executable(${TARGET})

target_sources(${TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Toolchain.cpp
)

target_include_directories(${TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# The driver needs to find its companions.  The build tree locations are
# compiled in, an install is looked for beside the driver itself or at the
# prefix named here, and each one can be overridden through an environment
# variable.  The name of the library directory travels too, since
# GNUInstallDirs may have settled on lib64.
target_compile_definitions(${TARGET}
    PRIVATE
        ADAC_DEFAULT_PATH="$<TARGET_FILE:adac>"
        QBE_DEFAULT_PATH="${QBE_BUILD_DIR}/qbe"
        CC_DEFAULT_PATH="${ADA_HOST_CC}"
        ADA_RUNTIME_DEFAULT_PATH="$<TARGET_FILE:adart>"
        ADA_LIBRARY_DEFAULT_PATH="${CMAKE_SOURCE_DIR}/runtime/ada"
        ADA_INSTALL_LIBDIR="${CMAKE_INSTALL_LIBDIR}"
        ADA_LIBRARY_INSTALL_PATH="${CMAKE_INSTALL_FULL_LIBDIR}/ada/adainclude"
        ADA_RUNTIME_INSTALL_PATH="${CMAKE_INSTALL_FULL_LIBDIR}/ada/adalib/libadart.a"
)

add_dependencies(${TARGET} adac qbe_build adart)

install(TARGETS ${TARGET}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
