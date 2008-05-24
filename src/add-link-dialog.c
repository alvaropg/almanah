/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Diary
 * Copyright (C) Philip Withnall 2008 <philip@tecnocode.co.uk>
 * 
 * Diary is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * Diary is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Diary.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "main.h"
#include "link.h"
#include "add-link-dialog.h"

void
diary_hide_ald (void)
{
	/* Resize the table so we lose all the custom widgets */
	gtk_widget_hide_all (diary->add_link_dialog);
	gtk_table_resize (diary->ald_table, 1, 2);
}

void
ald_destroy_cb (GtkWindow *window, gpointer user_data)
{
	diary_hide_ald ();
}

void
ald_type_combo_box_changed_cb (GtkComboBox *self, gpointer user_data)
{
	GtkTreeIter iter;
	gchar *type;
	const DiaryLinkType *link_type;

	if (gtk_combo_box_get_active_iter (self, &iter) == FALSE)
		return;

	gtk_tree_model_get (gtk_combo_box_get_model (self), &iter, 1, &type, -1);
	link_type = diary_link_get_type (type);

	g_assert (link_type != NULL);
	link_type->build_dialog_func (type, diary->ald_table);

	g_free (type);
}
