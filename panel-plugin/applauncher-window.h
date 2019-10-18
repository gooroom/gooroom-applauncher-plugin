/*
 * Copyright (C) 2015-2019 Gooroom <gooroom@gooroom.kr>
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

#ifndef __APPLAUNCHER_WINDOW_H__
#define __APPLAUNCHER_WINDOW_H__

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define APPLAUNCHER_TYPE_WINDOW            (applauncher_window_get_type ())
#define APPLAUNCHER_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), APPLAUNCHER_TYPE_WINDOW, ApplauncherWindow))
#define APPLAUNCHER_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), APPLAUNCHER_TYPE_WINDOW, ApplauncherWindowClass))
#define IS_APPLAUNCHER_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), APPLAUNCHER_TYPE_WINDOW))
#define IS_APPLAUNCHER_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), APPLAUNCHER_TYPE_WINDOW))
#define APPLAUNCHER_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), APPLAUNCHER_TYPE_WINDOW, ApplauncherWindowClass))

typedef struct _ApplauncherWindowPrivate       ApplauncherWindowPrivate;
typedef struct _ApplauncherWindowClass         ApplauncherWindowClass;
typedef struct _ApplauncherWindow              ApplauncherWindow;

struct _ApplauncherWindowClass
{
	GtkWindowClass __parent__;
};

struct _ApplauncherWindow
{
	GtkWindow __parent__;

	ApplauncherWindowPrivate *priv;
};


GType      applauncher_window_get_type     (void) G_GNUC_CONST;

ApplauncherWindow *applauncher_window_new  (void);

G_END_DECLS

#endif /* !__APPLAUNCHER_WINDOW_H__ */

