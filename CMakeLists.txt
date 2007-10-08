project(kmplayer)

find_package(KDE4 REQUIRED)

INCLUDE(UsePkgConfig)

PKGCONFIG(cairo CAIROIncDir CAIROLinkDir CAIROLinkFlags CAIROCflags)

if (CAIROCflags)
    set(HAVE_CAIRO 1)
endif (CAIROCflags)

PKGCONFIG(dbus-glib-1 GLibDBusIncDir GLibDBusLinkDir GLibDBusLinkFlags GLibDBusCflags)

if (GLibDBusCflags)
    set(HAVE_GDBUS 1)
    MESSAGE("have GDBUS")
    PKGCONFIG(gtk+-x11-2.0 GTKIncDir GTKLinkDir GTKLinkFlags GTKCflags)
    if (GTKCflags)
        MESSAGE("have GTK")
        set(HAVE_NPP 1)
    endif (GTKCflags)
endif (GLibDBusCflags)

set(CMAKE_VERBOSE_MAKEFILE ON)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS} -DHAVE_CONFIG_H=1)

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES}  )

configure_file (config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

add_subdirectory(src)
add_subdirectory(icons)
#add_subdirectory(mimetypes)
