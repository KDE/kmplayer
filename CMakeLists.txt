cmake_minimum_required(VERSION 3.5)

project(kmplayer)

SET(KMPLAYER_MAJOR_VERSION "0")
SET(KMPLAYER_MINOR_VERSION "12")
SET(KMPLAYER_PATCH_VERSION "0b")

SET(KMPLAYER_VERSION_STRING
    "${KMPLAYER_MAJOR_VERSION}.${KMPLAYER_MINOR_VERSION}.${KMPLAYER_PATCH_VERSION}")

set(QT_MIN_VERSION "5.12.0")
set(KF5_MIN_VERSION "5.69.0")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMInstallIcons)
#include(ECMSetupVersion)
include(FeatureSummary)
Include(CheckIncludeFiles)
include(ECMSetupVersion)
include(FindXCB)

ecm_setup_version(${KMPLAYER_VERSION_STRING} VARIABLE_PREFIX KMPLAYERPRIVATE
    SOVERSION ${KMPLAYER_MAJOR_VERSION}
)
find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Widgets Svg X11Extras)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS Config CoreAddons Init I18n KDELibs4Support KIO MediaPlayer Parts WidgetsAddons)
find_package(KF5DocTools ${KF5_MIN_VERSION})
find_package(X11 REQUIRED)
find_package(Phonon4Qt5 REQUIRED NO_MODULE)

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

if(NOT WIN32)
  OPTION(KMPLAYER_BUILT_WITH_CAIRO "Enable Cairo support" ON)
  OPTION(KMPLAYER_BUILT_WITH_NPP "Build NPP player" ON)
  OPTION(KMPLAYER_BUILT_WITH_EXPAT "Use expat XML parser" OFF)
  include(FindPkgConfig)

  if (KMPLAYER_BUILT_WITH_CAIRO)

    pkg_check_modules(CAIRO cairo)

    if(CAIRO_FOUND)
        set(KMPLAYER_WITH_CAIRO 1)
    endif()
  endif (KMPLAYER_BUILT_WITH_CAIRO)

  if (KMPLAYER_BUILT_WITH_NPP)
    pkg_check_modules(GLibDBus dbus-glib-1)
    pkg_check_modules(GModule gmodule-2.0)

    if(GLibDBus_FOUND)
      set(KMPLAYER_WITH_GDBUS 1)
      pkg_check_modules(GTK gtk+-x11-2.0)
      pkg_check_modules(GThread gthread-2.0)
      if (GTK_FOUND)
          set(KMPLAYER_WITH_NPP 1)
      endif()
    endif()
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
if (KF5DocTools_FOUND)
  add_subdirectory(doc)
  kdoctools_install(po)
endif()
add_subdirectory(data)

ki18n_install(po)
