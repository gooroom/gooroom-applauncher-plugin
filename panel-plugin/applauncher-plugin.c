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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "applauncher-window.h"
#include "applauncher-plugin.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <glib-object.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include <xfconf/xfconf.h>


#define PANEL_TRAY_ICON_SIZE        (32)



struct _ApplauncherPluginClass
{
  XfcePanelPluginClass __parent__;
};

/* plugin structure */
struct _ApplauncherPlugin
{
	XfcePanelPlugin      __parent__;

	GtkWidget         *button;

	ApplauncherWindow *popup_window;
};


/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN (ApplauncherPlugin, applauncher_plugin)


static gboolean
on_popup_window_closed (gpointer data)
{
	ApplauncherPlugin *plugin = APPLAUNCHER_PLUGIN (data);

	if (plugin->popup_window != NULL) {
		gtk_widget_destroy (GTK_WIDGET (plugin->popup_window));
		plugin->popup_window = NULL;
    }

	xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (plugin), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button), FALSE);

	return TRUE;
}

static gboolean
on_popup_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event->type == GDK_KEY_PRESS && event->keyval == GDK_Escape) {
		on_popup_window_closed (data);
		return TRUE;
	}

	return FALSE;
}

static void
on_popup_window_realized (GtkWidget *widget, gpointer data)
{
	gint x, y;
	ApplauncherPlugin *plugin = APPLAUNCHER_PLUGIN (data);

	xfce_panel_plugin_position_widget (XFCE_PANEL_PLUGIN (plugin), widget, plugin->button, &x, &y);
	gtk_window_move (GTK_WINDOW (widget), x, y);
}

static ApplauncherWindow *
popup_window_new (ApplauncherPlugin *plugin, GdkEventButton *event)
{
	ApplauncherWindow *window;

	window = applauncher_window_new ();
	gtk_window_set_screen (GTK_WINDOW (window), gtk_widget_get_screen (GTK_WIDGET (plugin)));

	g_signal_connect (G_OBJECT (window), "realize", G_CALLBACK (on_popup_window_realized), plugin);
	g_signal_connect_swapped (G_OBJECT (window), "delete-event", G_CALLBACK (on_popup_window_closed), plugin);
	g_signal_connect (G_OBJECT (window), "key-press-event", G_CALLBACK (on_popup_key_press_event), plugin);
	g_signal_connect_swapped (G_OBJECT (window), "focus-out-event", G_CALLBACK (on_popup_window_closed), plugin);

	gtk_widget_set_size_request (GTK_WIDGET (window), 435, 480);
	gtk_widget_show_all (GTK_WIDGET (window));

	xfce_panel_plugin_block_autohide (XFCE_PANEL_PLUGIN (plugin), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (plugin->button), TRUE);

	return window;
}

static gboolean
on_plugin_button_pressed (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	ApplauncherPlugin *plugin = APPLAUNCHER_PLUGIN (data);

	if (event->button == 1 || event->button == 2) {
		if (event->type == GDK_BUTTON_PRESS) {
			if (plugin->popup_window != NULL) {
				on_popup_window_closed (plugin);
			} else {
				plugin->popup_window = popup_window_new (plugin, event);
			}

			return TRUE;
		}
	}

	/* bypass GTK_TOGGLE_BUTTON's handler and go directly to the plugin's one */
	return (*GTK_WIDGET_CLASS (applauncher_plugin_parent_class)->button_press_event) (GTK_WIDGET (plugin), event);
}

static void
applauncher_plugin_free_data (XfcePanelPlugin *panel_plugin)
{
    ApplauncherPlugin *plugin = APPLAUNCHER_PLUGIN (panel_plugin);

    if (plugin->popup_window != NULL)
        on_popup_window_closed (plugin);
}

static gboolean
applauncher_plugin_size_changed (XfcePanelPlugin *panel_plugin, gint size)
{
	ApplauncherPlugin *plugin = APPLAUNCHER_PLUGIN (panel_plugin);

	if (xfce_panel_plugin_get_mode (panel_plugin) == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL) {
		gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), -1, size);
	} else {
		gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), size, -1);
	}

	return TRUE;
}

static void
applauncher_plugin_mode_changed (XfcePanelPlugin *plugin, XfcePanelPluginMode mode)
{
	applauncher_plugin_size_changed (plugin, xfce_panel_plugin_get_size (plugin));
}

static void
applauncher_plugin_init (ApplauncherPlugin *plugin)
{
//	plugin->button         = NULL;
//	plugin->popup_window   = NULL;

	xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	plugin->button = xfce_panel_create_toggle_button ();
	xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->button);
	gtk_container_add (GTK_CONTAINER (plugin), plugin->button);

	GdkPixbuf *pix;
	pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                    "applauncher-plugin-panel",
                                    PANEL_TRAY_ICON_SIZE,
                                    GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

	if (pix) {
		GtkWidget *img_tray = gtk_image_new_from_pixbuf (pix);
		gtk_image_set_pixel_size (GTK_IMAGE (img_tray), PANEL_TRAY_ICON_SIZE);
		gtk_container_add (GTK_CONTAINER (plugin->button), img_tray);
		g_object_unref (G_OBJECT (pix));
	}

	g_signal_connect (G_OBJECT (plugin->button), "button-press-event", G_CALLBACK (on_plugin_button_pressed), plugin);

	gtk_widget_show_all (plugin->button);
}

static void
applauncher_plugin_class_init (ApplauncherPluginClass *klass)
{
	XfcePanelPluginClass *plugin_class;

	plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
//	plugin_class->free_data = applauncher_plugin_free_data;
	plugin_class->size_changed = applauncher_plugin_size_changed;
	plugin_class->mode_changed = applauncher_plugin_mode_changed;
}
