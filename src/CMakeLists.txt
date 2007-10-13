set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUGFULL "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUGFULL}")
set(CMAKE_CXX_FLAGS_PROFILE "-DDEBUG ${CMAKE_CXX_FLAGS_PROFILE}")

########### next target ###############

SET(optionalCFlags)
SET(optionalLinkFlags)

IF (HAVE_CAIRO)
    SET(optionalCFlags ${optionalCFlags} ${CAIROCFlags})
    SET(optionalLinkFlags ${optionalLinkFlags} ${CAIROLinkFlags})
    MESSAGE("have cairo cflags:${optionalCFlags} ldflags:${optionalLinkFlags}")
endif (HAVE_CAIRO)

add_definitions(-DQT3_SUPPORT -DKDE3_SUPPORT)

ADD_DEFINITIONS(${CAIROCFlags})

include_directories(${CMAKE_CURRENT_BINARY_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR} ${CAIROIncDir})


set(libkmplayercommon_SRCS
  kmplayerview.cpp
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
  kmplayer_xspf.cpp
  triestring.cpp
  viewarea.cpp
)

if (HAVE_NPP)
    qt4_add_dbus_adaptor(libkmplayercommon_SRCS org.kde.kmplayer.callback.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/kmplayerprocess.h KMPlayer::NpPlayer)
    qt4_add_dbus_adaptor(libkmplayercommon_SRCS org.kde.kmplayer.stream.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/kmplayerprocess.h KMPlayer::NpStream)
endif (HAVE_NPP)

kde4_add_library(kmplayercommon SHARED ${libkmplayercommon_SRCS})

MESSAGE("extra ${CAIROCflags} ${GLibDBusCflags}  libs ${CAIROLinkFlags} ${GLibDBusLinkFlags}")

SET_TARGET_PROPERTIES(kmplayercommon PROPERTIES COMPILE_FLAGS
    "${CAIROCflags} ${GLibDBusCflags}")

SET_TARGET_PROPERTIES(kmplayercommon PROPERTIES LINK_FLAGS
    "${CAIROLinkFlags} ${GLibDBusLinkFlags}"
)
target_link_libraries(kmplayercommon
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KPARTS_LIBS}
  ${KDE4_KDEUI_LIBS}
  ${QT_QT3SUPPORT_LIBRARY}
  ${KDE4_KDE3SUPPORT_LIBRARY}
  kmediaplayer
)

install(TARGETS kmplayercommon DESTINATION ${LIB_INSTALL_DIR} )

########### next target ###############

set(kmplayerpart_SRCS kmplayer_part.cpp)

kde4_add_plugin(kmplayerpart WITH_PREFIX ${kmplayerpart_SRCS})

target_link_libraries(kmplayerpart
  kmplayercommon
  ${KDE4_KPARTS_LIBS}
  ${KDE4_KDEUI_LIBS}
  ${QT_QT3SUPPORT_LIBRARY}
  ${KDE4_KDE3SUPPORT_LIBRARY}
)

install(TARGETS kmplayerpart DESTINATION ${PLUGIN_INSTALL_DIR} )

install( FILES kmplayer_part.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

########### next target ###############

set(kmplayer_SRCS
  kmplayerapp.cpp
  kmplayertvsource.cpp
)

#kmplayerbroadcast.cpp
#kmplayervdr.cpp

kde4_add_kdeinit_executable(kmplayer ${kmplayer_SRCS} main.cpp)

target_link_libraries(kdeinit_kmplayer
  kmplayercommon
  ${KDE4_KPARTS_LIBS}
  ${KDE4_KDEUI_LIBS}
  ${QT_QT3SUPPORT_LIBRARY}
  ${KDE4_KDE3SUPPORT_LIBRARY}
)

target_link_libraries(kmplayer kdeinit_kmplayer)

install(TARGETS kdeinit_kmplayer  DESTINATION ${LIB_INSTALL_DIR} )

install(TARGETS kmplayer DESTINATION ${BIN_INSTALL_DIR})

install( FILES kmplayer.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )

#qt4_add_dbus_interface(kmplayer_SRCS org.kde.Konqueror.Main.xml kmplayer_interface)

########### next target ###############

if (HAVE_NPP)
    MESSAGE("build npp")
    set(knpplayer_SRCS npplayer.c)

    add_executable(knpplayer ${knpplayer_SRCS})

    SET_TARGET_PROPERTIES(knpplayer PROPERTIES COMPILE_FLAGS
        "${GLibDBusCflags} ${GTKCflags}"
    )
    SET_TARGET_PROPERTIES(knpplayer PROPERTIES LINK_FLAGS
        "${GLibDBusLinkFlags} ${GTKLinkFlags}"
    )

    install(TARGETS knpplayer DESTINATION ${BIN_INSTALL_DIR})

endif (HAVE_NPP)
#kxineplayer
#kgstplayer
#kxvplayer
#knppplayer

########### install files ###############

install(FILES kmplayerui.rc kmplayerpartui.rc noise.gif pluginsinfo DESTINATION  ${DATA_INSTALL_DIR}/kmplayer)
#install(FILES kmplayer.kcfg  DESTINATION  ${KCFG_INSTALL_DIR})
