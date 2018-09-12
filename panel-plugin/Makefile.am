AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	-DSBINDIR="\"$(sbindir)\""	\
	-DPKGDATA_DIR=\""$(datadir)/gooroom-applauncher"\" \
	-DGMENU_I_KNOW_THIS_IS_UNSTABLE \
	-DSN_API_NOT_YET_FROZEN	\
	$(PLATFORM_CPPFLAGS)



plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = \
	libapplauncher-plugin.la

BUILT_SOURCES = \
	applauncher-resources.c \
	applauncher-resources.h

libapplauncher_plugin_la_SOURCES = \
	panel-glib.c    \
	xfce-spawn.c    \
	applauncher-window.c    \
	applauncher-appitem.c   \
	applauncher-indicator.c \
	applauncher-plugin.c	\
	$(BUILT_SOURCES)

libapplauncher_plugin_la_CFLAGS = \
	$(GLIB_CFLAGS) \
	$(GTK_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(LIBGNOME_MENU_CFLAGS)	\
	$(PLATFORM_CFLAGS)	\
	$(LIBSTARTUP_NOTIFICATION_CFLAGS)

libapplauncher_plugin_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBXFCE4PANEL_LIBS)	\
	$(LIBGNOME_MENU_LIBS)	\
	$(LIBSTARTUP_NOTIFICATION_LIBS)

libapplauncher_plugin_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)



resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/gresource.xml)
applauncher-resources.c: gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name applauncher $<
applauncher-resources.h: gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name applauncher $<


#
# Desktop file
#
desktopdir = \
	$(datadir)/xfce4/panel/plugins

desktop_in_files = \
	applauncher-plugin.desktop.in.in

desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(desktop_in_files)

DISTCLEANFILES = $(desktop_DATA)
