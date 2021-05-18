add_executable(knpplayer)

target_include_directories(knpplayer PRIVATE
    ${GLibDBus_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
    ${GThread_INCLUDE_DIRS}
    ${GModule_INCLUDE_DIRS}
)

target_sources(knpplayer PRIVATE
    npplayer.cpp
)

target_link_libraries(knpplayer
    ${GLibDBus_LIBRARIES}
    ${GTK_LIBRARIES}
    ${GThread_LIBRARIES}
    ${GModule_LIBRARIES}
    ${X11_X11_LIB}
)

install(TARGETS knpplayer ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
