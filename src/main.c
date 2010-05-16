/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Almanah
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
 * 
 * Almanah is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Almanah is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Almanah.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf.h>

#include "main.h"
#include "storage-manager.h"
#include "event-manager.h"
#include "main-window.h"

Almanah *almanah;

static G_GNUC_NORETURN void
storage_manager_disconnected_cb (AlmanahStorageManager *self, const gchar *gpgme_error_message, const gchar *warning_message, gpointer user_data)
{
	if (gpgme_error_message != NULL || warning_message != NULL) {
		GtkWidget *dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error encrypting database"));

		if (gpgme_error_message != NULL && warning_message != NULL)
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s %s", warning_message, gpgme_error_message);
		else if (gpgme_error_message != NULL)
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", gpgme_error_message);
		else
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", warning_message);

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	g_object_unref (almanah->storage_manager);
	g_object_unref (almanah->gconf_client);
	g_object_unref (almanah->page_setup);
	g_object_unref (almanah->print_settings);

	g_free (almanah);

	if (gtk_main_level () > 0)
		gtk_main_quit ();

	exit (0);
}

void
almanah_quit (void)
{
	g_signal_connect (almanah->storage_manager, "disconnected", G_CALLBACK (storage_manager_disconnected_cb), NULL);
	almanah_storage_manager_disconnect (almanah->storage_manager, NULL);

	if (almanah->add_definition_dialog != NULL)
		gtk_widget_destroy (almanah->add_definition_dialog);
	if (almanah->search_dialog != NULL)
		gtk_widget_destroy (almanah->search_dialog);
	if (almanah->date_entry_dialog != NULL)
		gtk_widget_destroy (almanah->date_entry_dialog);
#ifdef ENABLE_ENCRYPTION
	if (almanah->preferences_dialog != NULL)
		gtk_widget_destroy (almanah->preferences_dialog);
#endif /* ENABLE_ENCRYPTION */
	if (almanah->definition_manager_window != NULL)
		gtk_widget_destroy (almanah->definition_manager_window);
	gtk_widget_destroy (almanah->main_window);

	g_object_unref (almanah->event_manager);

	/* Quitting is actually done in storage_manager_disconnected_cb, which is called once
	 * the storage manager has encrypted the DB and disconnected from it. */
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	gboolean debug = FALSE, import_mode = FALSE;
	gchar *db_filename;

	const GOptionEntry options[] = {
		{ "debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debug mode"), NULL },
		{ "import-mode", 0, 0, G_OPTION_ARG_NONE, &import_mode, N_("Enable import mode"), NULL },
		{ NULL }
	};

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	g_thread_init (NULL);
	gtk_set_locale ();
	gtk_init (&argc, &argv);
	g_set_application_name (_("Almanah Diary"));
	gtk_window_set_default_icon_name ("almanah");

	/* Options */
	context = g_option_context_new (_("- Manage your diary"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("Command-line options could not be parsed"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		exit (1);
	}

	g_option_context_free (context);

	/* Setup */
	almanah = g_new0 (Almanah, 1);
	almanah->debug = debug;
	almanah->import_mode = import_mode;

	/* Open GConf */
	almanah->gconf_client = gconf_client_get_default ();
	gconf_client_add_dir (almanah->gconf_client, "/apps/almanah", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	/* Ensure the DB directory exists */
	if (g_file_test (g_get_user_data_dir (), G_FILE_TEST_IS_DIR) == FALSE)
		g_mkdir_with_parents (g_get_user_data_dir (), 0700);

	/* Open the DB */
	db_filename = g_build_filename (g_get_user_data_dir (), "diary.db", NULL);
	almanah->storage_manager = almanah_storage_manager_new (db_filename);
	g_free (db_filename);

	if (almanah_storage_manager_connect (almanah->storage_manager, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error opening database"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		almanah_quit ();
	}

	/* Create the event manager */
	almanah->event_manager = almanah_event_manager_new ();

	/* Set up printing objects */
	almanah->print_settings = gtk_print_settings_new ();
	almanah->page_setup = gtk_page_setup_new ();

	/* Create and show the interface */
	almanah->main_window = GTK_WIDGET (almanah_main_window_new ());
	gtk_widget_show_all (almanah->main_window);

	gtk_main ();
	return 0;
}
