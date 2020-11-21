add_definitions(-DTRANSLATION_DOMAIN=\"kmplayer\")

include_directories(
    ${CAIRO_INCLUDE_DIRS}
    ${EXPAT_INCLUDE_DIRS}
)

set(libkmplayercommon_SRCS
    kmplayerview.cpp
    playmodel.cpp
    playlistview.cpp
    kmplayercontrolpanel.cpp
    kmplayerconfig.cpp
    pref.cpp
    kmplayerprocess.cpp
    kmplayerpartbase.cpp
    kmplayerplaylist.cpp
    kmplayer_asx.cpp
    kmplayer_smil.cpp
    kmplayer_rp.cpp
    kmplayer_rss.cpp
    kmplayer_atom.cpp
    kmplayer_opml.cpp
    kmplayer_xspf.cpp
    expression.cpp
    mediaobject.cpp
    triestring.cpp
    surface.cpp
    viewarea.cpp
)

ecm_qt_declare_logging_category(libkmplayercommon_SRCS
    HEADER kmplayercommon_log.h
    IDENTIFIER LOG_KMPLAYER_COMMON
    CATEGORY_NAME kmplayer.common
    DESCRIPTION "KMPlayer Common lib"
    EXPORT KMPLAYER
)

qt5_add_dbus_adaptor(libkmplayercommon_SRCS
    org.kde.kmplayer.master.xml
    kmplayerprocess.h KMPlayer::MasterProcessInfo
)
qt5_add_dbus_adaptor(libkmplayercommon_SRCS
    org.kde.kmplayer.streammaster.xml
    kmplayerprocess.h KMPlayer::MasterProcess
)
qt5_add_dbus_adaptor(libkmplayercommon_SRCS
    org.kde.kmplayer.part.xml
    kmplayerpartbase.h KMPlayer::PartBase
)

if (KMPLAYER_WITH_NPP)
    qt5_add_dbus_adaptor(libkmplayercommon_SRCS
        org.kde.kmplayer.callback.xml
        kmplayerprocess.h KMPlayer::NpPlayer
    )
    qt5_add_dbus_adaptor(libkmplayercommon_SRCS
        org.kde.kmplayer.stream.xml
        kmplayerprocess.h KMPlayer::NpStream
    )
endif (KMPLAYER_WITH_NPP)

set_source_files_properties(org.freedesktop.PowerManagement.Inhibit.xml
    PROPERTIES NO_NAMESPACE TRUE
)
qt5_add_dbus_interfaces(libkmplayercommon_SRCS org.freedesktop.PowerManagement.Inhibit.xml)

add_library(kmplayercommon SHARED ${libkmplayercommon_SRCS})

generate_export_header(kmplayercommon)

target_link_libraries(kmplayercommon
    PUBLIC
        KF5::MediaPlayer
    PRIVATE
        KF5::IconThemes
        KF5::Bookmarks
        Qt5::Svg
        Qt5::X11Extras
        ${CAIRO_LIBRARIES}
        ${XCB_LIBRARIES}
        ${EXPAT_LIBRARIES}
)

install(TARGETS kmplayercommon ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES bookmarks.xml DESTINATION ${KDE_INSTALL_DATADIR}/kmplayer)
