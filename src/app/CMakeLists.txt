add_definitions(-DKMPLAYER_VERSION_STRING="\\\"${KMPLAYER_VERSION_STRING}\\\"")

set(kmplayer_SRCS
    kmplayer.cpp
    kmplayer_lists.cpp
    kmplayertvsource.cpp
#kmplayerbroadcast.cpp
#kmplayervdr.cpp
)

ecm_qt_declare_logging_category(kmplayer_SRCS
    HEADER kmplayerapp_log.h
    IDENTIFIER LOG_KMPLAYER_APP
    CATEGORY_NAME kmplayer.app
    DESCRIPTION "KMPlayer App"
    EXPORT KMPLAYER
)

kf5_add_kdeinit_executable(kmplayer ${kmplayer_SRCS} main.cpp)

target_link_libraries(kdeinit_kmplayer
    kmplayercommon
    KF5::IconThemes
    Qt5::X11Extras
)

target_link_libraries(kmplayer kdeinit_kmplayer)

install(TARGETS kdeinit_kmplayer ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(TARGETS kmplayer ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES org.kde.kmplayer.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kmplayer.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES kmplayerui.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/kmplayer)
install(FILES kmplayerrc DESTINATION ${KDE_INSTALL_CONFDIR})
#install(FILES kmplayer.kcfg  DESTINATION  ${KCFG_INSTALL_DIR})

#qt5_add_dbus_interface(kmplayer_SRCS org.kde.Konqueror.Main.xml kmplayer_interface)
