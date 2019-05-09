set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUGFULL "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUGFULL}")
set(CMAKE_CXX_FLAGS_PROFILE "-DDEBUG ${CMAKE_CXX_FLAGS_PROFILE}")

########### next target ###############

SET(optionalCFlags)
SET(optionalLinkFlags)

IF (KMPLAYER_WITH_CAIRO)
    SET(optionalCFlags ${optionalCFlags} ${CAIROCFlags})
    SET(optionalLinkFlags ${optionalLinkFlags} ${CAIROLinkFlags})
    MESSAGE("have cairo cflags:${optionalCFlags} ldflags:${optionalLinkFlags}")
endif (KMPLAYER_WITH_CAIRO)

add_definitions(-DKMPLAYER_VERSION_STRING="\\\"${KMPLAYER_VERSION_STRING}\\\"")

ADD_DEFINITIONS(${CAIROCFlags})

include_directories(${CMAKE_CURRENT_BINARY_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR} ${CAIROIncDir} ${EXPAT_INCLUDE_DIRS})

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

MESSAGE("extra ${CAIROCflags} ${GLibDBusCflags}  libs ${CAIROLinkFlags} ${GLibDBusLinkFlags}")

SET_TARGET_PROPERTIES(kmplayercommon PROPERTIES COMPILE_FLAGS
    "${CAIROCflags}")

target_link_libraries(kmplayercommon
   LINK_PRIVATE
  ${CAIROLinkFlags}
  KF5::KDELibs4Support
  KF5::Parts
  ${XCB_LIBRARIES}
  ${EXPAT_LIBRARIES}
  KF5::MediaPlayer
  Qt5::Svg
  Qt5::X11Extras
)

install(TARGETS kmplayercommon ${INSTALL_TARGETS_DEFAULT_ARGS} )

########### next target ###############

set(kmplayerpart_SRCS kmplayer_part.cpp)

add_library(kmplayerpart MODULE ${kmplayerpart_SRCS})

target_link_libraries(kmplayerpart
  kmplayercommon
  KF5::MediaPlayer
  KF5::KDELibs4Support
  KF5::Parts
)

install(TARGETS kmplayerpart DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES kmplayer_part.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

########### next target ###############

set(kmplayer_SRCS
  kmplayer.cpp
  kmplayer_lists.cpp
  kmplayertvsource.cpp
)

#kmplayerbroadcast.cpp
#kmplayervdr.cpp

kf5_add_kdeinit_executable(kmplayer ${kmplayer_SRCS} main.cpp)

target_link_libraries(kdeinit_kmplayer
  kmplayercommon
  KF5::MediaPlayer
  KF5::KDELibs4Support
  KF5::Parts
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

    SET_TARGET_PROPERTIES(knpplayer PROPERTIES COMPILE_FLAGS
        "${GLibDBusCflags} ${GTKCflags} ${GThreadCflags} ${GModuleCflags}"
    )

    target_link_libraries(knpplayer ${GLibDBusLinkFlags} ${GTKLinkFlags} ${GThreadLinkFlags} ${GModuleLinkFlags}
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
