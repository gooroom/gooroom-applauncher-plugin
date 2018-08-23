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

#ifndef __APPLAUNCHER_PLUGIN_H__
#define __APPLAUNCHER_PLUGIN_H__

#include <glib.h>
#include <libxfce4panel/libxfce4panel.h>

G_BEGIN_DECLS
typedef struct _ApplauncherPluginClass ApplauncherPluginClass;
typedef struct _ApplauncherPlugin      ApplauncherPlugin;

#define TYPE_APPLAUNCHER_PLUGIN            (applauncher_plugin_get_type ())
#define APPLAUNCHER_PLUGIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_APPLAUNCHER_PLUGIN, ApplauncherPlugin))
#define APPLAUNCHER_PLUGIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_APPLAUNCHER_PLUGIN, ApplauncherPluginClass))
#define IS_APPLAUNCHER_PLUGIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_APPLAUNCHER_PLUGIN))
#define IS_APPLAUNCHER_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_APPLAUNCHER_PLUGIN))
#define APPLAUNCHER_PLUGIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_APPLAUNCHER_PLUGIN, ApplauncherPluginClass))

GType applauncher_plugin_get_type      (void) G_GNUC_CONST;

void  applauncher_plugin_register_type (XfcePanelTypeModule *type_module);

G_END_DECLS

#endif /* !__APPLAUNCHER_PLUGIN_H__ */
