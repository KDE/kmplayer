set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUGFULL "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUGFULL}")
set(CMAKE_CXX_FLAGS_PROFILE "-DDEBUG ${CMAKE_CXX_FLAGS_PROFILE}")

########### next target ###############

add_definitions(-DKMPLAYER_VERSION_STRING="\\\"${KMPLAYER_VERSION_STRING}\\\"")

include_directories(${CMAKE_CURRENT_BINARY_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR} ${CAIRO_INCLUDE_DIRS} ${EXPAT_INCLUDE_DIRS})

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

qt5_add_dbus_adaptor(libkmplayercommon_SRCS org.kde.kmplayer.master.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/kmplayerprocess.h KMPlayer::MasterProcessInfo)
qt5_add_dbus_adaptor(libkmplayercommon_SRCS
    org.kde.kmplayer.streammaster.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/kmplayerprocess.h KMPlayer::MasterProcess)
qt5_add_dbus_adaptor(libkmplayercommon_SRCS org.kde.kmplayer.part.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/kmplayerpartbase.h KMPlayer::PartBase)

if (KMPLAYER_WITH_NPP)
    qt5_add_dbus_adaptor(libkmplayercommon_SRCS org.kde.kmplayer.callback.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/kmplayerprocess.h KMPlayer::NpPlayer)
    qt5_add_dbus_adaptor(libkmplayercommon_SRCS org.kde.kmplayer.stream.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/kmplayerprocess.h KMPlayer::NpStream)
endif (KMPLAYER_WITH_NPP)

add_library(kmplayercommon SHARED ${libkmplayercommon_SRCS})

generate_export_header(kmplayercommon)

target_link_libraries(kmplayercommon
    PUBLIC
        KF5::MediaPlayer
        KF5::KDELibs4Support
    PRIVATE
        ${CAIRO_LIBRARIES}
        ${XCB_LIBRARIES}
        ${EXPAT_LIBRARIES}
        Qt5::Svg
        Qt5::X11Extras
)

install(TARGETS kmplayercommon ${INSTALL_TARGETS_DEFAULT_ARGS} )

########### next target ###############

configure_file(kmplayer_part.desktop ${CMAKE_CURRENT_BINARY_DIR}/kmplayer_part.desktop @ONLY)

set(kmplayerpart_SRCS kmplayer_part.cpp)

ecm_qt_declare_logging_category(kmplayerpart_SRCS
    HEADER kmplayerpart_log.h
    IDENTIFIER LOG_KMPLAYER_PART
    CATEGORY_NAME kmplayer.kpart
    DESCRIPTION "KMPlayer KPart"
    EXPORT KMPLAYER
)

add_library(kmplayerpart MODULE ${kmplayerpart_SRCS})
kcoreaddons_desktop_to_json(kmplayerpart ${CMAKE_CURRENT_BINARY_DIR}/kmplayer_part.desktop)

target_link_libraries(kmplayerpart
    kmplayercommon
)

install(TARGETS kmplayerpart DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/parts)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kmplayer_part.desktop  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR})

########### next target ###############

set(kmplayer_SRCS
  kmplayer.cpp
  kmplayer_lists.cpp
  kmplayertvsource.cpp
)

#kmplayerbroadcast.cpp
#kmplayervdr.cpp

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
)

target_link_libraries(kmplayer kdeinit_kmplayer)

install(TARGETS kdeinit_kmplayer  ${INSTALL_TARGETS_DEFAULT_ARGS} )

install(TARGETS kmplayer ${INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES org.kde.kmplayer.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES org.kde.kmplayer.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

#qt5_add_dbus_interface(kmplayer_SRCS org.kde.Konqueror.Main.xml kmplayer_interface)

########### next target ###############

if (KMPLAYER_WITH_NPP)
    MESSAGE("build npp")
    set(knpplayer_SRCS npplayer.cpp)

    add_executable(knpplayer ${knpplayer_SRCS})

    include_directories(${GLibDBus_INCLUDE_DIRS} ${GTK_INCLUDE_DIRS} ${GThread_INCLUDE_DIRS} ${GModule_INCLUDE_DIRS})

    target_link_libraries(knpplayer ${GLibDBus_LIBRARIES} ${GTK_LIBRARIES} ${GThread_LIBRARIES} ${GModule_LIBRARIES}
        ${X11_X11_LIB} )

    install(TARGETS knpplayer ${INSTALL_TARGETS_DEFAULT_ARGS})

endif (KMPLAYER_WITH_NPP)
#kxineplayer
#kgstplayer
#kxvplayer

########### next target ###############

set(kphononplayer_SRCS
  phononplayer.cpp
)

qt5_add_dbus_adaptor(kphononplayer_SRCS org.kde.kmplayer.slave.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/phononplayer.h Slave)
qt5_add_dbus_adaptor(kphononplayer_SRCS org.kde.kmplayer.streamslave.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/phononplayer.h Stream)

add_executable(kphononplayer ${kphononplayer_SRCS})

target_link_libraries(kphononplayer Phonon::phonon4qt5 ${XCB_LIBRARIES}
    Qt5::Core Qt5::DBus Qt5::Widgets Qt5::X11Extras
)

install(TARGETS kphononplayer ${INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############

install(FILES pluginsinfo bookmarks.xml DESTINATION ${DATA_INSTALL_DIR}/kmplayer)
install(FILES kmplayerui.rc kmplayerpartui.rc DESTINATION ${KXMLGUI_INSTALL_DIR}/kmplayer)
install(FILES kmplayerrc DESTINATION ${CONFIG_INSTALL_DIR})
#install(FILES kmplayer.kcfg  DESTINATION  ${KCFG_INSTALL_DIR})


ecm_qt_install_logging_categories(
    EXPORT KMPLAYER
    FILE kmplayer.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)
