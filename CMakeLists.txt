project(kmplayer)

find_package(KDE4 REQUIRED)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS} -DHAVE_CONFIG_H=1)

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES}  )

configure_file (config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

add_subdirectory(src)
add_subdirectory(icons)
#add_subdirectory(mimetypes)
