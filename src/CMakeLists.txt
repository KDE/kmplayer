set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUGFULL "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUGFULL}")
set(CMAKE_CXX_FLAGS_PROFILE "-DDEBUG ${CMAKE_CXX_FLAGS_PROFILE}")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)

configure_file(config-kmplayer.h.in ${CMAKE_CURRENT_BINARY_DIR}/config-kmplayer.h)

add_subdirectory(lib)
add_subdirectory(part)
add_subdirectory(app)
add_subdirectory(backends)

ecm_qt_install_logging_categories(
    EXPORT KMPLAYER
    FILE kmplayer.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)
