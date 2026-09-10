set(TARGET adart)

# The C run time every compiled program is linked against.  It is a library
# rather than a pair of sources handed to the C compiler each time, so that an
# installed compiler carries it the way it carries anything else.
add_library(${TARGET} STATIC)

target_sources(${TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/adart.c
        ${CMAKE_CURRENT_SOURCE_DIR}/adaio.c
)

target_include_directories(${TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

install(TARGETS ${TARGET}
    ARCHIVE DESTINATION ${ADA_INSTALL_ARCHIVE}
)
