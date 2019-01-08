/*
 *  Copyright (C) 2015-2017 Gooroom <gooroom@gooroom.kr>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include <math.h>

#include <gmenu-tree.h>

#include "xfce-spawn.h"
#include "panel-glib.h"
#include "applauncher-window.h"
#include "applauncher-indicator.h"
#include "applauncher-appitem.h"




static GSList *get_all_applications_from_dir (GMenuTreeDirectory   *directory,
                                              GSList               *list,
                                              GSList               *blacklist);

struct _ApplauncherWindowPrivate
{
	GtkWidget  *grid;
	GtkWidget  *ent_search;
	GtkWidget  *box_bottom;

	ApplauncherIndicator *pages;

	GList *grid_children;

	GSList *apps;
	GSList *filtered_apps;

	int grid_x;
	int grid_y;
	int icon_size;

	int item_width;
	int item_height;

	gchar *filter_text;
	gchar **blacklist;

	guint idle_entry_changed_id;
};


G_DEFINE_TYPE_WITH_PRIVATE (ApplauncherWindow, applauncher_window, GTK_TYPE_WINDOW)


static gchar *
desktop_working_directory_get (const gchar *id)
{
	gchar *wd = NULL;

	GDesktopAppInfo *dt_info = g_desktop_app_info_new (id);
	if (dt_info) {
		wd = g_desktop_app_info_get_string (dt_info, G_KEY_FILE_DESKTOP_KEY_PATH);
	}

	return wd;
}

static gboolean
desktop_has_name (const gchar *id, const gchar *name)
{
	const char *desktop;
	gboolean ret = FALSE;

	GDesktopAppInfo *dt_info = g_desktop_app_info_new (id);
	if (dt_info) {
		desktop = g_desktop_app_info_get_filename (dt_info);
	} else {
		desktop = NULL;
	}

	if (desktop) {
		GKeyFile *keyfile = g_key_file_new ();

		if (g_key_file_load_from_file (keyfile, desktop,
					G_KEY_FILE_KEEP_COMMENTS |
					G_KEY_FILE_KEEP_TRANSLATIONS,
					NULL)) {
			gsize num_keys, i;
			gchar **keys = g_key_file_get_keys (keyfile, "Desktop Entry", &num_keys, NULL);

			for (i = 0; i < num_keys; i++) {
				if (!g_str_has_prefix (keys[i], "Name"))
					continue;

				gchar *value = g_key_file_get_value (keyfile, "Desktop Entry", keys[i], NULL);
				if (value) {
					if (panel_g_utf8_strstrcase (value, name) != NULL) {
						ret = TRUE;
					}
				}
				g_free (value);
			}
			g_strfreev (keys);
		}
		g_key_file_free (keyfile);
	}

	g_object_unref (dt_info);

	return ret;
}

static gchar *
find_desktop_by_id (GList *apps, const gchar *find_str)
{
	GList *l = NULL;
	gchar *ret = NULL;

	if (!find_str || g_str_equal (find_str, ""))
		return NULL;

	for (l = apps; l; l = l->next) {
		GAppInfo *appinfo = G_APP_INFO (l->data);
		if (appinfo) {
			const gchar *id = g_app_info_get_id (appinfo);

			if (g_str_equal (id, find_str)) {
				ret = g_strdup (id);
				break;
			}

			if (desktop_has_name (id, find_str)) {
				ret = g_strdup (id);
				break;
			}
		}
	}

	return ret;
}

static gboolean
has_blacklist (GMenuTreeEntry *entry, GSList *blacklist)
{
	g_return_val_if_fail (entry && blacklist, FALSE);

	GAppInfo *appinfo;
	gboolean ret = FALSE;

	appinfo = G_APP_INFO (gmenu_tree_entry_get_app_info (entry));
	if (appinfo) {
		const char *id = g_app_info_get_id (appinfo);
		if (id) {
			GSList *l = NULL;
			for (l = blacklist; l; l = l->next) {
				gchar *_id = (gchar *)l->data;
				if (g_str_equal (_id, ""))
					continue;

				if (g_str_equal (_id, id)) {
					ret = TRUE;
					break;
				}
			}
		}
	}

	return ret;
}

static GSList *
get_application_blacklist (void)
{
	guint i;
	GSettings *settings;
	gchar **blacklist = NULL;
	GList *all_apps = NULL;
	GSList *blacklist_apps = NULL, *l = NULL;

	settings = g_settings_new ("apps.gooroom-applauncher-plugin");
	blacklist = g_settings_get_strv (settings, "blacklist");
	g_object_unref (settings);

	all_apps = g_app_info_get_all ();

	for (i = 0; blacklist[i]; i++) {
		if (!g_str_equal (blacklist[i], "")) {
			// find desktop file
			gchar *desktop = find_desktop_by_id (all_apps, blacklist[i]);
			if (desktop) {
				if (!g_slist_find_custom (blacklist_apps, desktop, (GCompareFunc) g_utf8_collate)) {
					blacklist_apps = g_slist_append (blacklist_apps, desktop);
				}
			}
		}
	}

	g_strfreev (blacklist);

	return blacklist_apps;
}

/* Copied from gnome-panel-3.26.0/gnome-panel/menu.c:
 * get_applications_menu () */
static gchar *
get_applications_menu (void)
{
	const gchar *xdg_menu_prefx = g_getenv ("XDG_MENU_PREFIX");

	if (xdg_menu_prefx == NULL )
		return g_strdup ("gnome-applications.menu");

	if (strlen (xdg_menu_prefx) == 0)
		return g_strdup ("gnome-applications.menu");

	return g_strdup_printf ("%sapplications.menu", xdg_menu_prefx);
}


/* Copied from gnome-panel-3.26.0/gnome-panel/panel-run-dialog.c:
 * get_all_applications_from_alias () */
static GSList *
get_all_applications_from_alias (GMenuTreeAlias   *alias,
                                 GSList           *list,
                                 GSList           *blacklist)
{
	switch (gmenu_tree_alias_get_aliased_item_type (alias))
	{
		case GMENU_TREE_ITEM_ENTRY: {
			GMenuTreeEntry *entry = gmenu_tree_alias_get_aliased_entry (alias);
			if (!has_blacklist (entry, blacklist))
				/* pass on the reference */
				list = g_slist_append (list, entry);
			break;
		}

		case GMENU_TREE_ITEM_DIRECTORY: {
			GMenuTreeDirectory *directory = gmenu_tree_alias_get_aliased_directory (alias);
			list = get_all_applications_from_dir (directory, list, blacklist);
			gmenu_tree_item_unref (directory);
			break;
		}

		default:
			break;
	}

	return list;
}

/* Copied from gnome-panel-3.26.0/gnome-panel/panel-run-dialog.c:
 * get_all_applications_from_dir () */
static GSList *
get_all_applications_from_dir (GMenuTreeDirectory  *directory,
                               GSList              *list,
                               GSList              *blacklist)
{
	GMenuTreeIter *iter;
	GMenuTreeItemType next_type;

	iter = gmenu_tree_directory_iter (directory);

	while ((next_type = gmenu_tree_iter_next (iter)) != GMENU_TREE_ITEM_INVALID) {
		switch (next_type) {
			case GMENU_TREE_ITEM_ENTRY: {
				GMenuTreeEntry *entry = gmenu_tree_iter_get_entry (iter);
				if (!has_blacklist (entry, blacklist))
					list = g_slist_append (list, entry);
				break;
			}

			case GMENU_TREE_ITEM_DIRECTORY: {
				GMenuTreeDirectory *dir = gmenu_tree_iter_get_directory (iter);
				list = get_all_applications_from_dir (dir, list, blacklist);
				gmenu_tree_item_unref (dir);
				break;
			}

			case GMENU_TREE_ITEM_ALIAS: {
				GMenuTreeAlias *alias = gmenu_tree_iter_get_alias (iter);
				list = get_all_applications_from_alias (alias, list, blacklist);
				gmenu_tree_item_unref (alias);
				break;
			}

			default:
			break;
		}
	}

	gmenu_tree_iter_unref (iter);

	return list;
}

static gboolean
has_application (GSList *list, GMenuTreeEntry *entry)
{
	const gchar *id;
	if (entry) {
		GAppInfo *app_info = G_APP_INFO (gmenu_tree_entry_get_app_info (entry));
		if (app_info) {
			id = g_app_info_get_id (app_info);
		} else {
			id = NULL;
		}
	} else {
		id = NULL;
	}

	if (!id) return FALSE;

	GSList *l = NULL;
	for (l = list; l; l = l->next) {
		GMenuTreeEntry *entry = (GMenuTreeEntry *)l->data;
		if (entry) {
			GAppInfo *app_info = G_APP_INFO (gmenu_tree_entry_get_app_info (entry));
			if (app_info) {
				const gchar *_id = g_app_info_get_id (app_info);
				if (g_str_equal (_id, id))
					return TRUE;
			}
		}
	}

	return FALSE;
}

/* Copied from gnome-panel-3.26.0/gnome-panel/panel-run-dialog.c:
 * get_all_applications () */
static GSList *
get_all_applications (GSList *blacklist)
{
	GMenuTree          *tree;
	GMenuTreeDirectory *root;
	GSList             *list = NULL;
	gchar *applications_menu = NULL;

	applications_menu = get_applications_menu ();

	tree = gmenu_tree_new (applications_menu, GMENU_TREE_FLAGS_SORT_DISPLAY_NAME);
	g_free (applications_menu);

	if (!gmenu_tree_load_sync (tree, NULL)) {
		g_object_unref (tree);
		return NULL;
	}

	root = gmenu_tree_get_root_directory (tree);

	list = get_all_applications_from_dir (root, NULL, blacklist);

	gmenu_tree_item_unref (root);
	g_object_unref (tree);

	return list;
}

static int
get_total_pages (ApplauncherWindow *window, GSList *list)
{
	ApplauncherWindowPrivate *priv = window->priv;

	guint size = 0;
	int num_pages = 0;

	size = g_slist_length (list);
	num_pages = (int)(size / (priv->grid_y * priv->grid_x));

	if ((size %  (priv->grid_y * priv->grid_x)) > 0) {
		num_pages += 1;
	}

	return num_pages;
}

static void
update_pages (ApplauncherWindow *window)
{
	ApplauncherWindowPrivate *priv = window->priv;

	g_return_if_fail (priv->filtered_apps != NULL);

	guint size = 0;
	gint filtered_pages = 0;

	size = g_slist_length (priv->filtered_apps);
	filtered_pages = (int)(size / (priv->grid_y * priv->grid_x));

	if ((size %  (priv->grid_y * priv->grid_x)) > 0) {
		filtered_pages += 1;
	}

	// Update pages
	if (filtered_pages > 1) {
		gtk_widget_set_visible (GTK_WIDGET (priv->pages), TRUE);
		GList *children = applauncher_indicator_get_children (priv->pages);
		guint total_pages = g_list_length (children);

		int p;
		for (p = 1; p <= total_pages; p++) {
			GtkWidget *child = g_list_nth_data (children, p - 1);
			if (child) {
				gboolean visible = (p > filtered_pages) ? FALSE : TRUE;
				gtk_widget_set_visible (child, visible);
			}
		}
	} else {
		gtk_widget_set_visible (GTK_WIDGET (priv->pages), FALSE);
	}
}

static void
update_grid (ApplauncherWindow *window)
{
	ApplauncherWindowPrivate *priv = window->priv;

	gint r, c;
	if (priv->filtered_apps == NULL) {
		for (r = 0; r < priv->grid_x; r++) {
			for (c = 0; c < priv->grid_y; c++) {
				gint pos = c + (r * priv->grid_y);
				ApplauncherAppItem *item = g_list_nth_data (priv->grid_children, pos);
				if (item) {
					gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
					applauncher_appitem_change_app (item, NULL, "", "");
				}
			}
		}
		return;
	}

	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();

	gint active = applauncher_indicator_get_active (priv->pages);
	gint item_iter = active * priv->grid_y * priv->grid_x;

	for (r = 0; r < priv->grid_x; r++) {
		for (c = 0; c < priv->grid_y; c++) {
			gint pos = c + (r * priv->grid_y); // position in table right now
			ApplauncherAppItem *item = g_list_nth_data (priv->grid_children, pos);
			if (item_iter < g_slist_length (priv->filtered_apps)) {
				GMenuTreeEntry *entry = g_slist_nth_data (priv->filtered_apps, item_iter);

				if (!entry) {
					item_iter++;
					gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
					continue;
				}

				GAppInfo *app_info = G_APP_INFO (gmenu_tree_entry_get_app_info (entry));

				if (!app_info) {
					item_iter++;
					gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
					continue;
				}

				GIcon *icon = g_app_info_get_icon (app_info);
				const gchar *name = g_app_info_get_name (app_info);
				const gchar *desc = g_app_info_get_description (app_info);

				gtk_widget_set_sensitive (GTK_WIDGET (item), TRUE);
				if (desc == NULL || g_strcmp0 (desc, "") == 0) {
					applauncher_appitem_change_app (item, icon, name, name);
				} else {
					gchar *tooltip = g_strdup_printf ("%s:\n%s", name, desc);
					applauncher_appitem_change_app (item, icon, name, tooltip);
					g_free (tooltip);
				}
			} else { // fill with a blank one
				gtk_widget_set_sensitive (GTK_WIDGET (item), FALSE);
				applauncher_appitem_change_app (item, NULL, "", "");
			}

			item_iter++;
		}
	}

	// Update number of pages
	update_pages (window);
}

static void
pages_activate_cb (ApplauncherIndicator *indicator, gpointer data)
{
	ApplauncherWindow *window = APPLAUNCHER_WINDOW (data);

	update_grid (window);
}

static void
search (ApplauncherWindow *window)
{
	ApplauncherWindowPrivate *priv = window->priv;

	GSList *l = NULL, *apps = NULL;
	for (l = priv->apps; l; l = l->next) {
		GMenuTreeEntry *entry = (GMenuTreeEntry *)l->data;

		if (!entry) continue;

		GAppInfo *app_info = G_APP_INFO (gmenu_tree_entry_get_app_info (entry));

		if (!app_info) continue;

		const gchar *exec = g_app_info_get_executable (app_info);
		if (exec && panel_g_utf8_strstrcase (exec, priv->filter_text) != NULL) {
			if (!has_application (apps, entry))
				apps = g_slist_append (apps, entry);

			continue;
		}

		const gchar *id = g_app_info_get_id (app_info);
		if (desktop_has_name (id, priv->filter_text)) {
			if (!has_application (apps, entry))
				apps = g_slist_append (apps, entry);

			continue;
		}
	}

	g_slist_free (priv->filtered_apps);
	priv->filtered_apps = apps;

	int total_pages = get_total_pages (window, priv->filtered_apps);
	if (total_pages > 1) {
		applauncher_indicator_set_active (priv->pages, 0);
	} else {
		update_grid (window);
	}
}

static void
show_error_dialog (GtkWindow  *parent,
                   gboolean    auto_destroy,
                   const char *title,
                   const char *primary_text,
                   const char *secondary_text)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (parent, 0,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "%s", primary_text);

	gtk_window_set_title (GTK_WINDOW (dialog), title);

	if (secondary_text != NULL)
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", secondary_text);

	gtk_widget_show_all (dialog);

	if (auto_destroy) {
		g_signal_connect_swapped (G_OBJECT (dialog), "response",
				G_CALLBACK (gtk_widget_destroy),
				G_OBJECT (dialog));
	}
}


static gboolean
command_is_executable (const char   *command,
                       int          *argcp,
                       char       ***argvp)
{
	gboolean   result;
	char     **argv;
	char      *path;
	int        argc;

	result = g_shell_parse_argv (command, &argc, &argv, NULL);

	if (!result)
		return FALSE;

	path = g_find_program_in_path (argv[0]);

	if (!path) {
		g_strfreev (argv);
		return FALSE;
	}

	/* If we pass an absolute path to g_find_program it just returns
	 * that absolute path without checking if it is executable. Also
	 * make sure its a regular file so we don't try to launch
	 * directories or device nodes.
	 */
	if (!g_file_test (path, G_FILE_TEST_IS_EXECUTABLE) ||
        !g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
		g_free (path);
		g_strfreev (argv);
		return FALSE;
	}

	g_free (path);

	if (argcp)
		*argcp = argc;
	if (argvp)
		*argvp = argv;

	return TRUE;
}

static gboolean
launch_command (ApplauncherWindow *window,
                const char        *command,
                const char        *locale_command,
                const char        *working_directory)
{
	GdkScreen  *screen;
	gboolean    result;
	GError     *error = NULL;
	char      **argv;
	int         argc;
	GPid        pid;

	ApplauncherWindowPrivate *priv = window->priv;

	const gchar *p;
	GString *string = g_string_sized_new (100);

	for (p = locale_command; *p != '\0'; ++p) {
		if (G_UNLIKELY (p[0] == '%' && p[1] != '\0')) {
			switch (*++p) {
				case '%':
					g_string_append_c (string, '%');
					break;
					/* skip all the other %? values for now we don't have dnd anyways */
			}
		} else {
			g_string_append_c (string, *p);
		}
	}

	if (!command_is_executable (string->str, &argc, &argv))
		return FALSE;

	screen = gtk_window_get_screen (GTK_WINDOW (window));

	result = xfce_spawn_on_screen (screen, working_directory,
                                   argv, NULL, G_SPAWN_SEARCH_PATH,
                                   TRUE, gtk_get_current_event_time (),
                                   NULL, &error);

	if (!result || error) {
		gchar *primary = g_markup_printf_escaped (_("Could not run command '%s'"), command);

		show_error_dialog (GTK_WINDOW (window), TRUE, _("Application Launching Error"), primary, error->message);

		g_free (primary);

		g_error_free (error);
	}

	g_strfreev (argv);

	return result;
}

static void
on_appitem_button_clicked_cb (GtkButton *button, gpointer data)
{
	ApplauncherWindow *window = APPLAUNCHER_WINDOW (data);
	ApplauncherWindowPrivate *priv = window->priv;

	gint index = g_list_index (priv->grid_children, button);

	if (index < 0)
		return;

	gint active = applauncher_indicator_get_active (priv->pages);
	gint pos = index + (active * priv->grid_y * priv->grid_x);

	GMenuTreeEntry *entry = g_slist_nth_data (priv->filtered_apps, pos);
	if (!entry)
		return;

	GAppInfo *app_info = G_APP_INFO (gmenu_tree_entry_get_app_info (entry));
	const char *cmdline = g_app_info_get_commandline (app_info);
	gchar *command = g_strdup (cmdline);
	command = g_strchug (command);

	if (!command || !command[0]) {
		g_free (command);
		return;
	}

	GError *error = NULL;
	gchar *disk = g_locale_from_utf8 (command, -1, NULL, NULL, &error);

	if (!disk || error) {
		gchar *primary = g_markup_printf_escaped (_("Could not run command '%s'"), command);

		show_error_dialog (GTK_WINDOW (window), TRUE, _("Application Launching Error"), primary, error->message);

		g_free (primary);

		g_error_free (error);

		return;
	}

    /* if it's an absolute path or not a URI, it's possibly an executable,
     * so try it before displaying it */
	gchar *scheme = g_uri_parse_scheme (disk);
    if (g_path_is_absolute (disk) || !scheme) {
		const gchar *id = g_app_info_get_id (app_info);
		gchar *wd = desktop_working_directory_get (id);
		launch_command (window, command, disk, wd);
		g_free (wd);
	}

	g_free (command);
	g_free (disk);
}


static void
search_entry_changed_idle_destroyed (gpointer data)
{
	APPLAUNCHER_WINDOW (data)->priv->idle_entry_changed_id = 0;
}

static gboolean
search_entry_changed_idle (gpointer data)
{
	ApplauncherWindow *window = APPLAUNCHER_WINDOW (data);
	ApplauncherWindowPrivate *priv = window->priv;

	const gchar *text = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->ent_search)));

	g_free (priv->filter_text);
	priv->filter_text = (text == NULL) ? g_strdup ("") : g_strdup (text);

	search (window);

	return FALSE;
}

static void
on_search_entry_changed_cb (ApplauncherWindow *window)
{
	ApplauncherWindowPrivate *priv = window->priv;

	if (priv->idle_entry_changed_id != 0) {
		g_source_remove (priv->idle_entry_changed_id);
		priv->idle_entry_changed_id == 0;
	}

	priv->idle_entry_changed_id =
		gdk_threads_add_idle_full (G_PRIORITY_DEFAULT,
                                   search_entry_changed_idle,
                                   window,
                                   search_entry_changed_idle_destroyed);
}

static void
on_search_entry_activate_cb (GtkEditable *entry,
                             gpointer     data)
{
	ApplauncherWindow *window = APPLAUNCHER_WINDOW (data);
	ApplauncherWindowPrivate *priv = window->priv;

	guint size = g_slist_length (priv->filtered_apps);
	if (size == 0) return;

	GtkWidget *focus = gtk_container_get_focus_child (GTK_CONTAINER (priv->grid));

	if (focus) {
		gtk_button_clicked (GTK_BUTTON (focus));
		return;
	}

	ApplauncherAppItem *item = g_list_nth_data (priv->grid_children, 0);

	gtk_widget_grab_focus (GTK_WIDGET (item));
	if (size == 1) {
		gtk_button_clicked (GTK_BUTTON (item));
	}
}

static void
on_search_entry_icon_release_cb (GtkEntry             *entry,
                                 GtkEntryIconPosition  icon_pos,
                                 GdkEvent             *event,
                                 gpointer              user_data)
{
	if (icon_pos == GTK_ENTRY_ICON_SECONDARY) {
		gtk_entry_set_text (GTK_ENTRY (entry), "");
	}
}

static void
populate_grid (ApplauncherWindow *window)
{
	ApplauncherWindowPrivate *priv = window->priv;

	int r, c;
	for (r = 0; r < priv->grid_x; r++) {
		for (c = 0; c < priv->grid_y; c++) {
			ApplauncherAppItem *item = applauncher_appitem_new (priv->icon_size);
			gtk_widget_set_size_request (GTK_WIDGET (item), priv->item_width, priv->item_height);
			gtk_grid_attach (GTK_GRID (priv->grid), GTK_WIDGET (item), c, r, 1, 1);
			gtk_widget_show (GTK_WIDGET (item));

			priv->grid_children = g_list_append (priv->grid_children, item);

			g_signal_connect (G_OBJECT (item), "clicked", G_CALLBACK (on_appitem_button_clicked_cb), window);
		}
	}
}

static void
applauncher_window_page_left (ApplauncherWindow *window)
{
	ApplauncherWindowPrivate *priv = window->priv;

	gint active = applauncher_indicator_get_active (priv->pages);

	if (active >= 1) {
		applauncher_indicator_set_active (priv->pages, active - 1);
	}
}

static void
applauncher_window_page_right (ApplauncherWindow *window)
{
	ApplauncherWindowPrivate *priv = window->priv;

	gint total_pages = get_total_pages (window, priv->filtered_apps);
	gint active = applauncher_indicator_get_active (priv->pages);

	if ((active + 1) < total_pages) {
		applauncher_indicator_set_active (priv->pages, active + 1);
	}
}

static gint
applauncher_window_scroll (GtkWidget      *widget,
                           GdkEventScroll *event)
{
	ApplauncherWindow *window = APPLAUNCHER_WINDOW (widget);

	if (event->direction == GDK_SCROLL_UP) {
		applauncher_window_page_left (window);
	} else if (event->direction == GDK_SCROLL_DOWN) {
		applauncher_window_page_right (window);
	} else {
		return FALSE;
	}

	return TRUE;
}

static gboolean
applauncher_window_key_press_event (GtkWidget   *widget,
                                    GdkEventKey *event)
{
	ApplauncherWindow *window = APPLAUNCHER_WINDOW (widget);
	ApplauncherWindowPrivate *priv = window->priv;

	switch (event->keyval)
	{
		case GDK_KEY_Escape:
			gtk_widget_destroy (widget);
		break;

		case GDK_KEY_Left:
		{
			GtkWidget *focus = gtk_container_get_focus_child (GTK_CONTAINER (priv->grid));
			if (focus) {
				gint index = g_list_index (priv->grid_children, focus);
				if ((index %  priv->grid_y) == 0) {
					applauncher_window_page_left (window);
				}
			}
			break;
		}

		case GDK_KEY_Right:
		{
			GtkWidget *focus = gtk_container_get_focus_child (GTK_CONTAINER (priv->grid));
			if (focus) {
				gint index = g_list_index (priv->grid_children, focus);
				if ((index %  priv->grid_y) == (priv->grid_y - 1)) {
					applauncher_window_page_right (window);
				}
			}
			break;
		}

		default:
		break;
	}

	return GTK_WIDGET_CLASS (applauncher_window_parent_class)->key_press_event (widget, event);
}

static gboolean
applauncher_window_draw (GtkWidget *widget, cairo_t *cr)
{
	cairo_t *ctx;
	cairo_pattern_t *pattern;
	GtkAllocation alloc;

	gtk_widget_get_allocation (widget, &alloc);

	ctx = gdk_cairo_create (gtk_widget_get_window (widget));

#if 0
	pattern = cairo_pattern_create_linear (alloc.x, alloc.y, alloc.x, alloc.y + alloc.height);

	cairo_pattern_add_color_stop_rgba (pattern, 0.0, 0.0, 0.0, 0.0, 0.1);
	cairo_pattern_add_color_stop_rgba (pattern, 0.50, 0.0, 0.0, 0.0, 0.85);
	cairo_pattern_add_color_stop_rgba (pattern, 0.99, 0.0, 0.0, 0.0, 0.50);
	cairo_set_source (ctx, pattern);
	cairo_pattern_destroy (pattern);

	cairo_paint (ctx);
#endif

	cairo_set_source_rgba (ctx, 0.0, 0.0, 0.0, 0.8);
	cairo_paint (ctx);

	cairo_destroy (ctx);

	return GTK_WIDGET_CLASS (applauncher_window_parent_class)->draw (widget, cr);
}

static void
applauncher_window_init (ApplauncherWindow *window)
{
	ApplauncherWindowPrivate *priv;

	priv = window->priv = applauncher_window_get_instance_private (window);

	gtk_widget_init_template (GTK_WIDGET (window));

	priv->apps = NULL;
	priv->filtered_apps = NULL;
	priv->grid_children = NULL;

	priv->filter_text = NULL;
	priv->idle_entry_changed_id = 0;

	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
	gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
	gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
	gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_NORMAL);

	GdkScreen *screen = gtk_window_get_screen (GTK_WINDOW (window));
	gtk_widget_set_visual (GTK_WIDGET (window), gdk_screen_get_rgba_visual (screen));
	GdkDisplay *display = gdk_screen_get_display (screen);
	GdkMonitor *primary = gdk_display_get_primary_monitor (display);

	GdkRectangle area;
	gdk_monitor_get_geometry (primary, &area);

	// Set icon size
	double suggested_size = pow (area.width * area.height, (double)(1.0/3.0)) / 1.6;

	if (suggested_size < 27) {
		priv->icon_size = 24;
	} else if (suggested_size >= 27 && suggested_size < 40) {
		priv->icon_size = 32;
	} else if (suggested_size >= 40 && suggested_size < 56) {
		priv->icon_size = 48;
	} else if (suggested_size >= 56) {
		priv->icon_size = 64;
	}

	GSList *blacklist = get_application_blacklist ();
	priv->apps = get_all_applications (blacklist);
	g_slist_free_full (blacklist, (GDestroyNotify)g_free);

	GSList *l = NULL;
	for (l = priv->apps; l; l = l->next) {
		if (!has_application (priv->filtered_apps, l->data))
			priv->filtered_apps = g_slist_append (priv->filtered_apps, l->data);
	}

	if ((area.width / area.height) < 1.4) { // Monitor 5:4, 4:3
		priv->grid_x = 4;
		priv->grid_y = 4;
	} else { // Monitor 16:9
		priv->grid_x = 3;
		priv->grid_y = 6;
	}

	priv->item_width = area.width / (priv->grid_y *2);
	priv->item_height = area.height / (priv->grid_x *2);

	int r, c;
	for (r = 0; r < priv->grid_x; r++)
		gtk_grid_insert_row (GTK_GRID (priv->grid), r);
	for (c = 0; c < priv->grid_y; c++)
		gtk_grid_insert_column (GTK_GRID (priv->grid), c);

	populate_grid (window);

	priv->pages = applauncher_indicator_new ();
	gtk_box_set_spacing (GTK_BOX (priv->pages), 36);
	gtk_box_pack_start (GTK_BOX (priv->box_bottom), GTK_WIDGET (priv->pages), FALSE, FALSE, 0);

	int total_pages = get_total_pages (window, priv->apps);
	if (total_pages > 1) {
		gtk_widget_show (GTK_WIDGET (priv->pages));

		g_signal_connect (G_OBJECT (priv->pages), "child-activate", G_CALLBACK (pages_activate_cb), window);

		int p;
		for (p = 1; p <= total_pages; p++) {
			char *string = g_strdup_printf ("%d", p);
			applauncher_indicator_append (priv->pages, string);
			g_free (string);
		}

		applauncher_indicator_set_active (priv->pages, 0);
	} else {
		gtk_widget_hide (GTK_WIDGET (priv->pages));
		update_grid (window);
	}

	g_signal_connect_swapped (G_OBJECT (priv->ent_search), "changed",
               G_CALLBACK (on_search_entry_changed_cb), window);

	g_signal_connect (G_OBJECT (priv->ent_search), "icon-release",
                      G_CALLBACK (on_search_entry_icon_release_cb), window);

	g_signal_connect (G_OBJECT (priv->ent_search), "activate",
                      G_CALLBACK (on_search_entry_activate_cb), window);

	gtk_widget_add_events (GTK_WIDGET (window), GDK_SCROLL_MASK);
}

static void
applauncher_window_finalize (GObject *object)
{
	ApplauncherWindow *window = APPLAUNCHER_WINDOW (object);
	ApplauncherWindowPrivate *priv = window->priv;

	g_slist_free_full (priv->apps, (GDestroyNotify)gmenu_tree_item_unref);
	g_slist_free (priv->filtered_apps);

	if (priv->idle_entry_changed_id != 0) {
		g_source_remove (priv->idle_entry_changed_id);
		priv->idle_entry_changed_id = 0;
	}

	(*G_OBJECT_CLASS (applauncher_window_parent_class)->finalize) (object);
}

static void
applauncher_window_class_init (ApplauncherWindowClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = applauncher_window_finalize;

	widget_class->scroll_event = applauncher_window_scroll;
	widget_class->draw = applauncher_window_draw;
	widget_class->key_press_event = applauncher_window_key_press_event;

	gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass),
                                     "/kr/gooroom/applauncher/window.ui");

	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), ApplauncherWindow, ent_search);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), ApplauncherWindow, grid);
	gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), ApplauncherWindow, box_bottom);
}

ApplauncherWindow *
applauncher_window_new (void)
{
  return g_object_new (APPLAUNCHER_TYPE_WINDOW, NULL);
}
