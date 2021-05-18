configure_file(kmplayer_part.desktop.in ${CMAKE_CURRENT_BINARY_DIR}/kmplayer_part.desktop @ONLY)

add_library(kmplayerpart MODULE)

target_compile_definitions(kmplayerpart PRIVATE
    -DTRANSLATION_DOMAIN=\"kmplayer\"
    -DKMPLAYER_VERSION_STRING="\\\"${KMPLAYER_VERSION_STRING}\\\""
)

target_sources(kmplayerpart PRIVATE
    kmplayer_part.cpp
    kmplayer_part.qrc
)

ecm_qt_declare_logging_category(kmplayerpart
    HEADER kmplayerpart_log.h
    IDENTIFIER LOG_KMPLAYER_PART
    CATEGORY_NAME kmplayer.kpart
    DESCRIPTION "KMPlayer KPart"
    EXPORT KMPLAYER
)

kcoreaddons_desktop_to_json(kmplayerpart ${CMAKE_CURRENT_BINARY_DIR}/kmplayer_part.desktop)

target_link_libraries(kmplayerpart
    kmplayercommon
    KF5::IconThemes
)

install(TARGETS kmplayerpart DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/parts)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kmplayer_part.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR})
install(FILES pluginsinfo DESTINATION ${KDE_INSTALL_DATADIR}/kmplayer)
