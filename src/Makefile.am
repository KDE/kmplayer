INCLUDES = -I$(kde_includes)/kio -I$(kde_includes)/arts $(all_includes)

SUBDIRS=

lib_LTLIBRARIES=kdeinit_kmplayer.la

kde_module_LTLIBRARIES = kparts_kmplayer.la

kparts_kmplayer_la_SOURCES= kmplayer_part.cpp kmplayerview.cpp configdialog.ui kmplayerconfig.cpp

kparts_kmplayer_la_LDFLAGS= $(all_libraries) -module $(KDE_PLUGIN) -lartskde -lm
kparts_kmplayer_la_LIBADD= $(LIB_KFILE) $(LIB_KPARTS) $(LIB_KIO) -lkmediaplayer
kparts_kmplayer_la_METASOURCES= kmplayer_part.moc kmplayerview.moc configdialog.moc kmplayerconfig.moc

kdeinit_kmplayer_la_SOURCES= main.cpp kmplayerapp.cpp

kdeinit_kmplayer_la_LIBADD= kparts_kmplayer.la
kdeinit_kmplayer_la_LDFLAGS= $(all_libraries) $(KDE_RPATH) -module -avoid-version
kdeinit_kmplayer_la_METASOURCES= kmplayer.moc kmplayersource.moc

bin_PROGRAMS= kmplayer

kmplayer_LDADD= kdeinit_kmplayer.la
kmplayer_LDFLAGS= $(all_libraries) $(KDE_RPATH)
kmplayer_SOURCES= dummy.cpp


CLEANFILES = dummy.cpp

dummy.cpp:
	echo > dummy.cpp

applnk_DATA = kmplayer.desktop
applnkdir = $(kde_appsdir)/Multimedia

kde_services_DATA = kmplayer_component.desktop

data_DATA = kmplayerui.rc kmplayerpartui.rc
datadir = $(kde_datadir)/kmplayer

conf_DATA = kmplayerrc
confdir = $(kde_confdir)

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kmplayer.pot
