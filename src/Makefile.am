INCLUDES = -I$(kde_includes)/kio $(all_includes)

KDE_ICON=AUTO

METASOURCES = AUTO

lib_LTLIBRARIES=kdeinit_kmplayer.la

kde_module_LTLIBRARIES = kparts_kmplayer.la

kparts_kmplayer_la_SOURCES = kmplayer_part.cpp kmplayer_koffice_part.cpp kmplayerview.cpp kmplayerconfig.cpp pref.cpp kmplayerprocess.cpp kmplayer_callback.skel kmplayer_backend.stub

kparts_kmplayer_la_LDFLAGS= $(all_libraries) -module $(KDE_PLUGIN) $(LIB_ARTS) -lm
kparts_kmplayer_la_LIBADD= $(LIB_KFILE) $(LIB_KPARTS) $(LIB_KIO) $(LIB_KOFFICE) -lkmediaplayer

kdeinit_kmplayer_la_SOURCES= main.cpp kmplayerapp.cpp

kdeinit_kmplayer_la_LIBADD= kparts_kmplayer.la
kdeinit_kmplayer_la_LDFLAGS= $(all_libraries) $(KDE_RPATH) -module -avoid-version

EXTRA_PROGRAMS = kxineplayer
if include_kxineplayer
kxineplayer_app = kxineplayer
endif

bin_PROGRAMS= kmplayer $(kxineplayer_app)

kmplayer_LDADD= kdeinit_kmplayer.la
kmplayer_LDFLAGS= $(all_libraries) $(KDE_RPATH)
kmplayer_SOURCES= dummy.cpp

kxineplayer_LDADD= $(LIB_XINE)
kxineplayer_CFLAGS= $(CFLAGS_XINE)
kxineplayer_LDFLAGS= $(all_libraries) $(KDE_RPATH) -$(LIB_QT) -lDCOP
kxineplayer_SOURCES= xineplayer.cpp kmplayer_backend.skel kmplayer_callback.stub

CLEANFILES = dummy.cpp

dummy.cpp:
	echo > dummy.cpp

applnk_DATA = kmplayer.desktop
applnkdir = $(kde_appsdir)/Multimedia

kde_services_DATA = kmplayer_component.desktop $(SERVICES_KOFFICE)

rc_DATA = kmplayerui.rc kmplayerpartui.rc
rcdir = $(kde_datadir)/kmplayer

conf_DATA = kmplayerrc
confdir = $(kde_confdir)

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kmplayer.pot
