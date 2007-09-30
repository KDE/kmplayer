set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUGFULL "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUGFULL}")
set(CMAKE_CXX_FLAGS_PROFILE "-DDEBUG ${CMAKE_CXX_FLAGS_PROFILE}")

########### next target ###############

include_directories(${CMAKE_CURRENT_BINARY_DIR}/..)

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

kde4_add_library(kmplayercommon SHARED ${libkmplayercommon_SRCS})

target_link_libraries(kmplayercommon
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KPARTS_LIBS}
  ${KDE4_KDEUI_LIBS}
  ${QT_QT3SUPPORT_LIBRARY}
  ${KDE4_KDE3SUPPORT_LIBRARY}
)

install(TARGETS kmplayercommon DESTINATION ${LIB_INSTALL_DIR} )

########### next target ###############

set(kmplayerpart_SRCS kmplayer_part.cpp)

kde4_add_plugin(kmplayerpart WITH_PREFIX ${kmplayerpart_SRCS})

target_link_libraries(kmplayerpart
  kmplayercommon
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KPARTS_LIBS}
  ${KDE4_KDEUI_LIBS}
  ${QT_QT3SUPPORT_LIBRARY}
  ${KDE4_KDE3SUPPORT_LIBRARY}
)

install(TARGETS kmplayerpart DESTINATION ${PLUGIN_INSTALL_DIR} )

########### next target ###############

set(kmplayer_SRCS
  main.cpp
  kmplayerapp.cpp
  kmplayertvsource.cpp
  kmplayerbroadcast.cpp
  kmplayervdr.cpp
)

kde4_add_executable(kmplayer ${kmplayer_SRCS})

target_link_libraries(kmplayer
  kmplayercommon
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KPARTS_LIBS}
  ${KDE4_KDEUI_LIBS}
  ${QT_QT3SUPPORT_LIBRARY}
  ${KDE4_KDE3SUPPORT_LIBRARY}
)

install(TARGETS kmplayer DESTINATION ${BIN_INSTALL_DIR})

########### next target ###############

#kxineplayer
#kgstplayer
#kxvplayer
#knppplayer
