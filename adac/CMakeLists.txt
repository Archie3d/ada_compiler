set(TARGET adac)

add_executable(${TARGET})

target_sources(${TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Ast.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Diagnostics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Token.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Lexer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Parser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Type.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Scope.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Sema.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/QbeEmitter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/UnitLoader.cpp
)

target_include_directories(${TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Where the predefined environment lives when nothing on the command line and
# nothing in ADA_INCLUDE_PATH says otherwise.  The build tree location is
# compiled in for convenience; an install is instead found at run time beside
# adac's own executable, since a relocated install would leave a compiled-in
# absolute path pointing nowhere.  The name of the library directory travels
# too, since GNUInstallDirs may have settled on lib64.
target_compile_definitions(${TARGET}
    PRIVATE
        ADA_LIBRARY_DEFAULT_PATH="${CMAKE_SOURCE_DIR}/runtime/ada"
        ADA_INSTALL_LIBDIR="${CMAKE_INSTALL_LIBDIR}"
)

install(TARGETS ${TARGET}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
