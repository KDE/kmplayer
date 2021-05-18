add_executable(kphononplayer)

set(kphononplayer_dbus_SRCS)
qt5_add_dbus_adaptor(kphononplayer_dbus_SRCS
    ${CMAKE_SOURCE_DIR}/src/lib/org.kde.kmplayer.agent.xml
    phononplayer.h Agent
)
qt5_add_dbus_adaptor(kphononplayer_dbus_SRCS
    ${CMAKE_SOURCE_DIR}/src/lib/org.kde.kmplayer.streamagent.xml
    phononplayer.h Stream
)

target_sources(kphononplayer PRIVATE
    phononplayer.cpp
    ${kphononplayer_dbus_SRCS}
)

target_link_libraries(kphononplayer
    Phonon::phonon4qt5
    ${XCB_LIBRARIES}
    Qt5::Core
    Qt5::DBus
    Qt5::Widgets
    Qt5::X11Extras
)

install(TARGETS kphononplayer ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
