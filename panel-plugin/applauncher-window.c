/*
 * Copyright (C) 2011 Nick Schermer <nick@xfce.org>
 * Copyright (C) 2015-2017 Gooroom <gooroom@gooroom.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include "appfinder-private.h"
#include "applauncher-window.h"
#include "appfinder-model.h"


static void       applauncher_window_finalize                      (GObject                   *object);
static void       applauncher_window_view                          (ApplauncherWindow         *window);
static gboolean   applauncher_window_completion_match_func         (GtkEntryCompletion        *completion,
                                                                    const gchar               *key,
                                                                    GtkTreeIter               *iter,
                                                                    gpointer                   data);
static void       applauncher_window_entry_changed                 (ApplauncherWindow         *window);
static void       applauncher_window_entry_activate                (GtkEditable               *entry,
                                                                    ApplauncherWindow         *window);
static gboolean   applauncher_window_item_visible                  (GtkTreeModel              *model,
                                                                    GtkTreeIter               *iter,
                                                                    gpointer                   data);
static void       applauncher_window_row_activated                 (ApplauncherWindow         *window);
static void       applauncher_window_execute                       (ApplauncherWindow         *window);
static gint       applauncher_window_sort_items                    (GtkTreeModel              *model,
                                                                    GtkTreeIter               *a,
                                                                    GtkTreeIter               *b,
                                                                    gpointer                   data);

struct _ApplauncherWindowClass
{
  GtkWindowClass __parent__;
};

struct _ApplauncherWindow
{
  GtkWindow __parent__;

  XfceAppfinderModel         *model;

  GtkTreeModel               *sort_model;
  GtkTreeModel               *filter_model;

  GtkEntryCompletion         *completion;

  GtkWidget                  *entry;
  GtkWidget                  *view;
  GtkWidget                  *viewscroll;

  GarconMenuDirectory        *filter_category;
  gchar                      *filter_text;

  guint                       idle_entry_changed_id;
};



G_DEFINE_TYPE (ApplauncherWindow, applauncher_window, GTK_TYPE_WINDOW)



static void
applauncher_window_class_init (ApplauncherWindowClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *gtkwidget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = applauncher_window_finalize;
}



static void
applauncher_window_init (ApplauncherWindow *window)
{
  GtkWidget          *vbox;
  GtkWidget          *entry;
  GtkWidget          *scroll;
  GtkWidget          *image;
  GtkWidget          *hbox;
  GtkEntryCompletion *completion;

  window->model = xfce_appfinder_model_get ();
  window->filter_category = g_object_new (GARCON_TYPE_MENU_DIRECTORY,
                                          "name", _("All Applications"),
                                          "icon-name", "applications-other", NULL);

  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_UTILITY);
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW (window), TRUE);
  gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
  gtk_window_stick (GTK_WINDOW (window));

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
  gtk_widget_show (vbox);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, TRUE, 10);
  gtk_widget_show (hbox);

  image = gtk_image_new_from_icon_name (XFCE_APPFINDER_STOCK_FIND, GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_size_request (image, 16, 16);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 10);
  gtk_widget_show (image);

  window->entry = entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 10);
  gtk_widget_show (entry);

  g_signal_connect_swapped (G_OBJECT (entry), "changed",
      G_CALLBACK (applauncher_window_entry_changed), window);
  g_signal_connect (G_OBJECT (entry), "activate",
      G_CALLBACK (applauncher_window_entry_activate), window);

  window->completion = completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (window->model));
  gtk_entry_completion_set_match_func (completion, applauncher_window_completion_match_func, window, NULL);
  gtk_entry_completion_set_text_column (completion, XFCE_APPFINDER_MODEL_COLUMN_COMMAND);
  gtk_entry_completion_set_popup_completion (completion, TRUE);
  gtk_entry_completion_set_popup_single_match (completion, TRUE);
  gtk_entry_completion_set_inline_completion (completion, TRUE);

  window->viewscroll = scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 10);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_NONE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scroll);

  /* set the icon or tree view */
  applauncher_window_view (window);

  /* update completion (remove completed text of restart completion) */
  completion = gtk_entry_get_completion (GTK_ENTRY (window->entry));
  if (completion != NULL)
    gtk_editable_delete_selection (GTK_EDITABLE (window->entry));

  gtk_entry_set_completion (GTK_ENTRY (window->entry), NULL);

  /* update state */
  applauncher_window_entry_changed (window);
}

static void
applauncher_window_finalize (GObject *object)
{
  ApplauncherWindow *window = APPLAUNCHER_WINDOW (object);

  if (window->idle_entry_changed_id != 0)
    g_source_remove (window->idle_entry_changed_id);

  g_object_unref (G_OBJECT (window->model));
  g_object_unref (G_OBJECT (window->filter_model));
  g_object_unref (G_OBJECT (window->sort_model));
  g_object_unref (G_OBJECT (window->completion));

  if (window->filter_category != NULL)
    g_object_unref (G_OBJECT (window->filter_category));

  g_free (window->filter_text);

  (*G_OBJECT_CLASS (applauncher_window_parent_class)->finalize) (object);
}

static void
applauncher_window_set_item_width (ApplauncherWindow *window)
{
  gint                   width = 0;
  XfceAppfinderIconSize  icon_size;

  appfinder_return_if_fail (GTK_IS_ICON_VIEW (window->view));

  g_object_get (G_OBJECT (window->model), "icon-size", &icon_size, NULL);

  /* some hard-coded values for the cell size that seem to work fine */
  switch (icon_size)
    {
    case XFCE_APPFINDER_ICON_SIZE_SMALLEST:
      width = 16 * 3.75;
      break;

    case XFCE_APPFINDER_ICON_SIZE_SMALLER:
      width = 24 * 3;
      break;

    case XFCE_APPFINDER_ICON_SIZE_SMALL:
      width = 36 * 2.5;
      break;

    case XFCE_APPFINDER_ICON_SIZE_NORMAL:
      width = 48 * 2;
      break;

    case XFCE_APPFINDER_ICON_SIZE_LARGE:
      width = 64 * 1.5;
      break;

    case XFCE_APPFINDER_ICON_SIZE_LARGER:
      width = 96 * 1.75;
      break;

    case XFCE_APPFINDER_ICON_SIZE_LARGEST:
      width = 128 * 1.25;
      break;
    }

  gtk_icon_view_set_item_orientation (GTK_ICON_VIEW (window->view), GTK_ORIENTATION_VERTICAL);
  gtk_icon_view_set_item_width (GTK_ICON_VIEW (window->view), width);
}

static void
applauncher_window_view (ApplauncherWindow *window)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeSelection  *selection;
  GtkWidget         *view;
  gboolean           icon_view;

  window->filter_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (window->model), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (window->filter_model), applauncher_window_item_visible, window, NULL);

  window->sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (window->filter_model));
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (window->sort_model), applauncher_window_sort_items, window->entry, NULL);

  /* Disable sort model for icon view, since it does not work as expected */
  /* Example: the user searches for some app and then deletes the text entry. */
  /* Repeat this operation a couple of times, you will notice that sorting is incorrect. */
  window->view = view = gtk_icon_view_new_with_model (window->filter_model/*sort_model*/);
  gtk_icon_view_set_selection_mode (GTK_ICON_VIEW (view), GTK_SELECTION_BROWSE);
  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_ICON);
  gtk_icon_view_set_text_column (GTK_ICON_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_TITLE);
//  gtk_icon_view_set_tooltip_column (GTK_ICON_VIEW (view), XFCE_APPFINDER_MODEL_COLUMN_TOOLTIP);
  applauncher_window_set_item_width (window);

  g_signal_connect_swapped (G_OBJECT (view), "item-activated",
      G_CALLBACK (applauncher_window_row_activated), window);

  gtk_container_add (GTK_CONTAINER (window->viewscroll), view);
  gtk_widget_show (view);
}

static gboolean
applauncher_window_view_get_selected (ApplauncherWindow  *window,
                                      GtkTreeModel        **model,
                                      GtkTreeIter          *iter)
{
  GtkTreeSelection *selection;
  gboolean          have_iter;
  GList            *items;

  appfinder_return_val_if_fail (IS_APPLAUNCHER_WINDOW (window), FALSE);
  appfinder_return_val_if_fail (model != NULL, FALSE);
  appfinder_return_val_if_fail (iter != NULL, FALSE);

  items = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (window->view));
  appfinder_assert (g_list_length (items) <= 1);
  if (items != NULL)
    {
      *model = gtk_icon_view_get_model (GTK_ICON_VIEW (window->view));
      have_iter = gtk_tree_model_get_iter (*model, iter, items->data);

      gtk_tree_path_free (items->data);
      g_list_free (items);
    }
  else
    {
      have_iter = FALSE;
    }

  return have_iter;
}

static gboolean
applauncher_window_completion_match_func (GtkEntryCompletion *completion,
                                             const gchar        *key,
                                             GtkTreeIter        *iter,
                                             gpointer            data)
{
  const gchar *text;

  ApplauncherWindow *window = APPLAUNCHER_WINDOW (data);

  appfinder_return_val_if_fail (GTK_IS_ENTRY_COMPLETION (completion), FALSE);
  appfinder_return_val_if_fail (IS_APPLAUNCHER_WINDOW (data), FALSE);
  appfinder_return_val_if_fail (GTK_TREE_MODEL (window->model)
      == gtk_entry_completion_get_model (completion), FALSE);

  /* don't use the casefolded key generated by gtk */
  text = gtk_entry_get_text (GTK_ENTRY (window->entry));

  return xfce_appfinder_model_get_visible_command (window->model, iter, text);
}

static gboolean
applauncher_window_entry_changed_idle (gpointer data)
{
  ApplauncherWindow *window = APPLAUNCHER_WINDOW (data);
  const gchar         *text;
  gchar               *normalized;

  text = gtk_entry_get_text (GTK_ENTRY (window->entry));

  g_free (window->filter_text);

  if (IS_STRING (text))
    {
      normalized = g_utf8_normalize (text, -1, G_NORMALIZE_ALL);
      window->filter_text = g_utf8_casefold (normalized, -1);
      g_free (normalized);
    }
  else
    {
      window->filter_text = NULL;
    }

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (window->filter_model));

  return FALSE;
}

static void
applauncher_window_entry_changed_idle_destroyed (gpointer data)
{
  APPLAUNCHER_WINDOW (data)->idle_entry_changed_id = 0;
}

static void
applauncher_window_entry_changed (ApplauncherWindow *window)
{
  if (window->idle_entry_changed_id != 0)
    g_source_remove (window->idle_entry_changed_id);

  window->idle_entry_changed_id =
      gdk_threads_add_idle_full (G_PRIORITY_DEFAULT, applauncher_window_entry_changed_idle,
                       window, applauncher_window_entry_changed_idle_destroyed);
}

static void
applauncher_window_entry_activate (GtkEditable         *entry,
                                   ApplauncherWindow *window)
{
  GtkTreePath *path;
  gboolean     cursor_set = FALSE;

  if (gtk_icon_view_get_visible_range (GTK_ICON_VIEW (window->view), &path, NULL))
    {
      gtk_icon_view_select_path (GTK_ICON_VIEW (window->view), path);
      gtk_icon_view_set_cursor (GTK_ICON_VIEW (window->view), path, NULL, FALSE);
      gtk_tree_path_free (path);

      cursor_set = TRUE;
    }

  if (cursor_set)
    gtk_widget_grab_focus (window->view);
  else
    applauncher_window_execute (window);
}

static gboolean
applauncher_window_item_visible (GtkTreeModel *model,
                                    GtkTreeIter  *iter,
                                    gpointer      data)
{
  ApplauncherWindow *window = APPLAUNCHER_WINDOW (data);
  /* don't use the casefolded key generated by gtk */
//  const gchar *filter_string= gtk_entry_get_text (GTK_ENTRY (window->entry));

  return xfce_appfinder_model_get_visible (XFCE_APPFINDER_MODEL (model), iter,
//                                           window->filter_category,
                                           NULL,
                                           window->filter_text);
}

static void
applauncher_window_row_activated (ApplauncherWindow *window)
{
  applauncher_window_execute (window);
}

static void
applauncher_window_execute (ApplauncherWindow *window)
{
  GtkTreeModel *model, *child_model;
  GtkTreeIter   iter, child_iter;
  GError       *error = NULL;
  gboolean      result = FALSE;
  GdkScreen    *screen;
  gboolean      regular_command = FALSE;

  screen = gtk_window_get_screen (GTK_WINDOW (window));

  if (applauncher_window_view_get_selected (window, &model, &iter))
    {
      child_model = model;

      if (GTK_IS_TREE_MODEL_SORT (model))
        {
          gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model), &child_iter, &iter);
          iter = child_iter;
          child_model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model));
        }

      gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (child_model), &child_iter, &iter);
      result = xfce_appfinder_model_execute (window->model, &child_iter, screen, &regular_command, &error);

      if (!result && regular_command)
        {
          gchar *primary = g_markup_printf_escaped (_("Failed to execute program"));

          xfce_message_dialog (NULL,
                              _("Launch Error"), GTK_STOCK_DIALOG_ERROR,
                              primary, (error != NULL) ? error->message : "",
                              GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
                              NULL);

          g_free (primary);
        }
    }

  if (error != NULL)
      g_error_free (error);
}

static gint
applauncher_window_sort_items (GtkTreeModel *model,
                                  GtkTreeIter  *a,
                                  GtkTreeIter  *b,
                                  gpointer      data)
{
  gchar        *normalized, *casefold, *title_a, *title_b, *found;
  GtkWidget    *entry = GTK_WIDGET (data);
  gint          result = -1;

  appfinder_return_val_if_fail (GTK_IS_ENTRY (entry), 0);

  normalized = g_utf8_normalize (gtk_entry_get_text (GTK_ENTRY (entry)), -1, G_NORMALIZE_ALL);
  casefold = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  gtk_tree_model_get (model, a, XFCE_APPFINDER_MODEL_COLUMN_TITLE, &title_a, -1);
  normalized = g_utf8_normalize (title_a, -1, G_NORMALIZE_ALL);
  title_a = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  gtk_tree_model_get (model, b, XFCE_APPFINDER_MODEL_COLUMN_TITLE, &title_b, -1);
  normalized = g_utf8_normalize (title_b, -1, G_NORMALIZE_ALL);
  title_b = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  if (strcmp (casefold, "") == 0)
    result = g_strcmp0 (title_a, title_b);
  else
    {
      found = g_strrstr (title_a, casefold);
      if (found)
        result -= (G_MAXINT - (found - title_a));

      found = g_strrstr (title_b, casefold);
      if (found)
        result += (G_MAXINT - (found - title_b));
    }

  g_free (casefold);
  g_free (title_a);
  g_free (title_b);
  return result;
}

ApplauncherWindow *
applauncher_window_new (void)
{
  return g_object_new (APPLAUNCHER_TYPE_WINDOW,
                       "type", GTK_WINDOW_TOPLEVEL,
                       NULL);
}
