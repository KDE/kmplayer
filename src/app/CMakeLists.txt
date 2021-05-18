kf5_add_kdeinit_executable(kmplayer)

target_sources(kdeinit_kmplayer PRIVATE
    kmplayer.cpp
    kmplayer_lists.cpp
    kmplayertvsource.cpp
#kmplayerbroadcast.cpp
#kmplayervdr.cpp
    main.cpp

    kmplayer.qrc
)

ecm_qt_declare_logging_category(kdeinit_kmplayer
    HEADER kmplayerapp_log.h
    IDENTIFIER LOG_KMPLAYER_APP
    CATEGORY_NAME kmplayer.app
    DESCRIPTION "KMPlayer App"
    EXPORT KMPLAYER
)

target_compile_definitions(kdeinit_kmplayer PRIVATE
    -DKMPLAYER_VERSION_STRING="\\\"${KMPLAYER_VERSION_STRING}\\\""
)

target_link_libraries(kdeinit_kmplayer
    kmplayercommon
    KF5::IconThemes
    Qt5::X11Extras
)

install(TARGETS kdeinit_kmplayer ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(TARGETS kmplayer ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES org.kde.kmplayer.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.kde.kmplayer.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES kmplayerrc DESTINATION ${KDE_INSTALL_CONFDIR})
#install(FILES kmplayer.kcfg  DESTINATION  ${KCFG_INSTALL_DIR})

#qt5_add_dbus_interface(kmplayer_SRCS org.kde.Konqueror.Main.xml kmplayer_interface)
