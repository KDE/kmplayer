cmake_minimum_required(VERSION 2.8.9)
project(kmplayer)

cmake_policy(VERSION 2.6)

SET(KMPLAYER_MAJOR_VERSION "0")
SET(KMPLAYER_MINOR_VERSION "11")
SET(KMPLAYER_PATCH_VERSION "3d")

SET(KMPLAYER_VERSION_STRING
    "${KMPLAYER_MAJOR_VERSION}.${KMPLAYER_MINOR_VERSION}.${KMPLAYER_PATCH_VERSION}")

find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include (MacroLibrary)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
include_directories( ${KDE4_INCLUDES})

if(NOT WIN32)
  OPTION(KMPLAYER_BUILT_WITH_CAIRO "Enable Cairo support" ON)
  OPTION(KMPLAYER_BUILT_WITH_NPP "Build NPP player" ON)
  OPTION(KMPLAYER_BUILT_WITH_EXPAT "Use expat XML parser" OFF)
  INCLUDE(UsePkgConfig)

  if (KMPLAYER_BUILT_WITH_CAIRO)

    PKGCONFIG(cairo CAIROIncDir CAIROLinkDir CAIROLinkFlags CAIROCflags)

    if (CAIROCflags)
        set(KMPLAYER_WITH_CAIRO 1)
    endif (CAIROCflags)
  endif (KMPLAYER_BUILT_WITH_CAIRO)

  if (KMPLAYER_BUILT_WITH_NPP)
    PKGCONFIG(dbus-glib-1 GLibDBusIncDir GLibDBusLinkDir GLibDBusLinkFlags GLibDBusCflags)
    PKGCONFIG(gmodule-2.0 GModuleIncDir GModuleLinkDir GModuleLinkFlags GModuleCflags)

    if (GLibDBusCflags)
      set(KMPLAYER_WITH_GDBUS 1)
      PKGCONFIG(gtk+-x11-2.0 GTKIncDir GTKLinkDir GTKLinkFlags GTKCflags)
      PKGCONFIG(gthread-2.0 GThreadIncDir GThreadLinkDir GThreadLinkFlags GThreadCflags)
      if (GTKCflags)
          set(KMPLAYER_WITH_NPP 1)
      endif (GTKCflags)
    endif (GLibDBusCflags)
  endif (KMPLAYER_BUILT_WITH_NPP)

  if (KMPLAYER_BUILT_WITH_EXPAT)
      INCLUDE(FindEXPAT)
      if (EXPAT_FOUND)
          set(KMPLAYER_WITH_EXPAT 1)
      else (EXPAT_FOUND)
          MESSAGE(Expat found)
      endif (EXPAT_FOUND)
  endif (KMPLAYER_BUILT_WITH_EXPAT)

endif(NOT WIN32)


check_include_files(stdint.h HAVE_STDINT_H)

configure_file (config-kmplayer.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kmplayer.h )

add_subdirectory(src)
add_subdirectory(icons)
#add_subdirectory(doc)
add_subdirectory(data)

#include(MacroOptionalAddSubdirectory)
#macro_optional_add_subdirectory(po)
