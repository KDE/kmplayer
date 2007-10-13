project(kmplayer)

find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include (MacroLibrary)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories( ${KDE4_INCLUDES})

if(NOT WIN32)
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

endif(NOT WIN32)

configure_file (config-kmplayer.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kmplayer.h )

add_subdirectory(src)
add_subdirectory(icons)
#add_subdirectory(mimetypes)
