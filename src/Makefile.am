AM_CPPFLAGS= -I$(kde_includes)/kio $(CFLAGS_GST) $(all_includes)

METASOURCES= AUTO

lib_LTLIBRARIES= libkmplayercommon.la

libkmplayercommon_la_SOURCES = kmplayerview.cpp playlistview.cpp kmplayercontrolpanel.cpp kmplayerconfig.cpp pref.cpp kmplayerprocess.cpp kmplayer_callback.skel kmplayer_backend.stub kmplayerpartbase.cpp kmplayerplaylist.cpp kmplayer_asx.cpp kmplayer_smil.cpp kmplayer_rp.cpp kmplayer_rss.cpp kmplayer_atom.cpp kmplayer_xspf.cpp kmplayerpartbase.skel
libkmplayercommon_la_LDFLAGS = -avoid-version $(all_libraries)
libkmplayercommon_la_LIBADD = -lkmediaplayer $(LIB_KPARTS) $(LIB_KUTILS) $(LIB_EXPAT) -lm

if include_koffice_support
kofficeplugin_lib= libkmplayerkofficepart.la
SERVICES_KOFFICE = kmplayer_koffice.desktop
endif

kde_module_LTLIBRARIES= libkmplayerpart.la $(kofficeplugin_lib)

libkmplayerpart_la_SOURCES= kmplayer_part.cpp
libkmplayerpart_la_LDFLAGS= -avoid-version $(all_libraries) $(KDE_RPATH)
libkmplayerpart_la_LIBADD= libkmplayercommon.la

libkmplayerkofficepart_la_SOURCES=kmplayer_koffice_part.cpp
libkmplayerkofficepart_la_LDFLAGS= -avoid-version $(all_libraries) $(KDE_RPATH)
libkmplayerkofficepart_la_LIBADD= libkmplayercommon.la $(LIB_KOFFICE)

kdeinit_LTLIBRARIES=kmplayer.la
kmplayer_la_SOURCES= main.cpp kmplayerapp.cpp kmplayertvsource.cpp kmplayerbroadcast.cpp kmplayervdr.cpp
kmplayer_la_LIBADD= libkmplayercommon.la
kmplayer_la_LDFLAGS= -module $(KDE_PLUGIN)

EXTRA_PROGRAMS = kxineplayer kxvplayer kgstplayer
if include_kxineplayer
kxineplayer_app = kxineplayer
endif
if include_kgstplayer
kgstplayer_app = kgstplayer
endif

bin_PROGRAMS= $(kxineplayer_app) kxvplayer $(kgstplayer_app)

noinst_LTLIBRARIES = libkmplayerbackend.la
libkmplayerbackend_la_SOURCES = kmplayer_backend.skel kmplayer_callback.stub

kxineplayer_LDADD= libkmplayerbackend.la $(LIB_XINE) -lDCOP
kxineplayer_CFLAGS= $(CFLAGS_XINE)
kxineplayer_LDFLAGS= $(all_libraries) $(KDE_RPATH)
kxineplayer_SOURCES= xineplayer.cpp

kxvplayer_LDADD= libkmplayerbackend.la -lDCOP -lXv
kxvplayer_LDFLAGS= $(all_libraries) $(KDE_RPATH)
kxvplayer_SOURCES= xvplayer.cpp

kgstplayer_LDADD= libkmplayerbackend.la $(LIB_GST) $(LIB_GST_PLUGINS) -lgstinterfaces-0.10 -lDCOP
kgstplayer_LDFLAGS= $(all_libraries) $(KDE_RPATH)
kgstplayer_SOURCES= gstplayer.cpp

xdg_apps_DATA = kmplayer.desktop

kde_services_DATA = kmplayer_part.desktop $(SERVICES_KOFFICE)

rc_DATA = kmplayerui.rc kmplayerpartui.rc
rcdir = $(kde_datadir)/kmplayer

conf_DATA = kmplayerrc
confdir = $(kde_confdir)

appsdatadir=$(kde_datadir)/kmplayer
appsdata_DATA= bookmarks.xml pluginsinfo noise.gif

dummy.cpp:
	echo > dummy.cpp

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kmplayer.pot
