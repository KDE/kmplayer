INCLUDES= -I$(kde_includes)/kio $(all_includes)

KDE_ICON=AUTO

METASOURCES= AUTO

lib_LTLIBRARIES= libkmplayercommon.la

libkmplayercommon_la_SOURCES = kmplayerview.cpp kmplayerconfig.cpp pref.cpp kmplayerprocess.cpp kmplayer_callback.skel kmplayer_backend.stub kmplayer_part.cpp kmplayer_koffice_part.cpp
libkmplayercommon_la_LDFLAGS = -avoid-version $(all_libraries)
libkmplayercommon_la_LIBADD = -lkmediaplayer $(LIB_KOFFICE) $(LIB_KPARTS) $(LIB_ARTS) -lm

kde_module_LTLIBRARIES= libkmplayerpart.la

libkmplayerpart_la_SOURCES= dummy.cpp
libkmplayerpart_la_LDFLAGS= -avoid-version $(all_libraries) $(KDE_RPATH)
libkmplayerpart_la_LIBADD= libkmplayercommon.la

kdeinit_LTLIBRARIES=kmplayer.la
kmplayer_la_SOURCES= main.cpp kmplayerapp.cpp
kmplayer_la_LIBADD= libkmplayercommon.la
kmplayer_la_LDFLAGS= -module $(KDE_PLUGIN)

EXTRA_PROGRAMS = kxineplayer
if include_kxineplayer
kxineplayer_app = kxineplayer
endif

bin_PROGRAMS= $(kxineplayer_app)

kxineplayer_LDADD= $(LIB_XINE)
kxineplayer_CFLAGS= $(CFLAGS_XINE)
kxineplayer_LDFLAGS= $(all_libraries) $(KDE_RPATH) -lDCOP
kxineplayer_SOURCES= xineplayer.cpp kmplayer_backend.skel kmplayer_callback.stub

applnk_DATA = kmplayer.desktop
applnkdir = $(kde_appsdir)/Multimedia

if include_koffice_support
SERVICES_KOFFICE = kmplayer_koffice.desktop
endif

kde_services_DATA = kmplayer_component.desktop $(SERVICES_KOFFICE)

rc_DATA = kmplayerui.rc kmplayerpartui.rc
rcdir = $(kde_datadir)/kmplayer

conf_DATA = kmplayerrc
confdir = $(kde_confdir)

dummy.cpp:
	echo > dummy.cpp

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kmplayer.pot
