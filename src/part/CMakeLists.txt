configure_file(kmplayer_part.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/kmplayer_part.desktop @ONLY)

add_definitions(-DTRANSLATION_DOMAIN=\"kmplayer\")

add_definitions(-DKMPLAYER_VERSION_STRING="\\\"${KMPLAYER_VERSION_STRING}\\\"")

set(kmplayerpart_SRCS kmplayer_part.cpp)

ecm_qt_declare_logging_category(kmplayerpart_SRCS
    HEADER kmplayerpart_log.h
    IDENTIFIER LOG_KMPLAYER_PART
    CATEGORY_NAME kmplayer.kpart
    DESCRIPTION "KMPlayer KPart"
    EXPORT KMPLAYER
)

qt5_add_resources(kmplayerpart_SRCS
    kmplayer_part.qrc
)

add_library(kmplayerpart MODULE ${kmplayerpart_SRCS})
kcoreaddons_desktop_to_json(kmplayerpart ${CMAKE_CURRENT_BINARY_DIR}/kmplayer_part.desktop)

target_link_libraries(kmplayerpart
    kmplayercommon
    KF5::IconThemes
)

install(TARGETS kmplayerpart DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/parts)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kmplayer_part.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR})
install(FILES pluginsinfo DESTINATION ${KDE_INSTALL_DATADIR}/kmplayer)
