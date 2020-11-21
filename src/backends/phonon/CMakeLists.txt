set(kphononplayer_SRCS
    phononplayer.cpp
)

qt5_add_dbus_adaptor(kphononplayer_SRCS
    ${CMAKE_SOURCE_DIR}/src/lib/org.kde.kmplayer.agent.xml
    phononplayer.h Agent
)
qt5_add_dbus_adaptor(kphononplayer_SRCS
    ${CMAKE_SOURCE_DIR}/src/lib/org.kde.kmplayer.streamagent.xml
    phononplayer.h Stream
)

add_executable(kphononplayer ${kphononplayer_SRCS})

target_link_libraries(kphononplayer
    Phonon::phonon4qt5
    ${XCB_LIBRARIES}
    Qt5::Core
    Qt5::DBus
    Qt5::Widgets
    Qt5::X11Extras
)

install(TARGETS kphononplayer ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
