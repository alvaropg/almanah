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
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#ifdef ENABLE_ENCRYPTION
#define LIBCRYPTUI_API_SUBJECT_TO_CHANGE
#include <cryptui-key-combo.h>
#include <cryptui-keyset.h>
#include <cryptui.h>
#include <atk/atk.h>
#endif /* ENABLE_ENCRYPTION */

#include "preferences-dialog.h"
#include "interface.h"
#include "main.h"
#include "main-window.h"

static void almanah_preferences_dialog_dispose (GObject *object);
#ifdef ENABLE_ENCRYPTION
static void pd_key_combo_changed_cb (GtkComboBox *combo_box, AlmanahPreferencesDialog *preferences_dialog);
static void pd_new_key_button_clicked_cb (GtkButton *button, AlmanahPreferencesDialog *preferences_dialog);
#endif /* ENABLE_ENCRYPTION */
#ifdef ENABLE_SPELL_CHECKING
static void pd_response_cb (GtkDialog *dialog, gint response_id, AlmanahPreferencesDialog *preferences_dialog);
static void spell_checking_enabled_notify_cb (GConfClient *client, guint connection_id, GConfEntry *entry, AlmanahPreferencesDialog *self);
static void pd_spell_checking_enabled_check_button_toggled_cb (GtkToggleButton *toggle_button, gpointer user_data);
#endif /* ENABLE_SPELL_CHECKING */

struct _AlmanahPreferencesDialogPrivate {
#ifdef ENABLE_ENCRYPTION
	CryptUIKeyset *keyset;
	CryptUIKeyStore *key_store;
	GtkComboBox *key_combo;
#endif /* ENABLE_ENCRYPTION */
#ifdef ENABLE_SPELL_CHECKING
	guint spell_checking_enabled_id;
	GtkCheckButton *spell_checking_enabled_check_button;
#endif /* ENABLE_SPELL_CHECKING */
};

G_DEFINE_TYPE (AlmanahPreferencesDialog, almanah_preferences_dialog, GTK_TYPE_DIALOG)
#define ALMANAH_PREFERENCES_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ALMANAH_TYPE_PREFERENCES_DIALOG, AlmanahPreferencesDialogPrivate))

static void
almanah_preferences_dialog_class_init (AlmanahPreferencesDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (AlmanahPreferencesDialogPrivate));
	gobject_class->dispose = almanah_preferences_dialog_dispose;
}

static void
almanah_preferences_dialog_init (AlmanahPreferencesDialog *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, ALMANAH_TYPE_PREFERENCES_DIALOG, AlmanahPreferencesDialogPrivate);

#ifdef ENABLE_SPELL_CHECKING
	g_signal_connect (self, "response", G_CALLBACK (pd_response_cb), self);
#endif /* ENABLE_SPELL_CHECKING */
	gtk_window_set_modal (GTK_WINDOW (self), FALSE);
	gtk_window_set_title (GTK_WINDOW (self), _("Almanah Preferences"));
	gtk_widget_set_size_request (GTK_WIDGET (self), 400, -1);
	gtk_window_set_resizable (GTK_WINDOW (self), TRUE); /* needs to be resizeable so long keys can be made visible in the list */
}

static void
almanah_preferences_dialog_dispose (GObject *object)
{
	AlmanahPreferencesDialogPrivate *priv = ALMANAH_PREFERENCES_DIALOG (object)->priv;

#ifdef ENABLE_ENCRYPTION
	if (priv->keyset != NULL) {
		g_object_unref (priv->keyset);
		priv->keyset = NULL;
	}

	if (priv->key_store != NULL) {
		g_object_unref (priv->key_store);
		priv->key_store = NULL;
	}
#endif /* ENABLE_ENCRYPTION */
#ifdef ENABLE_SPELL_CHECKING
	gconf_client_notify_remove (almanah->gconf_client, priv->spell_checking_enabled_id);
#endif /* ENABLE_SPELL_CHECKING */

	/* Chain up to the parent class */
	G_OBJECT_CLASS (almanah_preferences_dialog_parent_class)->dispose (object);
}

/* Filter the key list so it's not pages and pages long */
static gboolean
key_store_filter_cb (CryptUIKeyset *keyset, const gchar *key, gpointer user_data)
{
	guint flags = cryptui_keyset_key_flags (keyset, key);
	return flags & CRYPTUI_FLAG_CAN_SIGN; /* if the key can sign, we have the private key part and can decrypt the database */
}

AlmanahPreferencesDialog *
almanah_preferences_dialog_new (void)
{
	GtkBuilder *builder;
	GtkTable *table;
#ifdef ENABLE_ENCRYPTION
	GtkWidget *label, *button;
	AtkObject *a11y_label, *a11y_key_combo;
	gchar *key;
#endif /* ENABLE_ENCRYPTION */
	AlmanahPreferencesDialog *preferences_dialog;
	AlmanahPreferencesDialogPrivate *priv;
	GError *error = NULL;
	const gchar *interface_filename = almanah_get_interface_filename ();
	const gchar *object_names[] = {
		"almanah_preferences_dialog",
		NULL
	};

	builder = gtk_builder_new ();

	if (gtk_builder_add_objects_from_file (builder, interface_filename, (gchar**) object_names, &error) == FALSE) {
		/* Show an error */
		GtkWidget *dialog = gtk_message_dialog_new (NULL,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				_("UI file \"%s\" could not be loaded"), interface_filename);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
		g_object_unref (builder);

		return NULL;
	}

	gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
	preferences_dialog = ALMANAH_PREFERENCES_DIALOG (gtk_builder_get_object (builder, "almanah_preferences_dialog"));
	gtk_builder_connect_signals (builder, preferences_dialog);

	if (preferences_dialog == NULL) {
		g_object_unref (builder);
		return NULL;
	}

	priv = ALMANAH_PREFERENCES_DIALOG (preferences_dialog)->priv;
	table = GTK_TABLE (gtk_builder_get_object (builder, "almanah_pd_table"));

#ifdef ENABLE_ENCRYPTION
	/* Grab our child widgets */
	label = gtk_label_new (_("Encryption key: "));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (table, label, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 0, 0);

	priv->keyset = cryptui_keyset_new ("openpgp", FALSE);
	priv->key_store = cryptui_key_store_new (priv->keyset, FALSE, _("None (don't encrypt)"));
	cryptui_key_store_set_filter (priv->key_store, key_store_filter_cb, NULL);
	priv->key_combo = cryptui_key_combo_new (priv->key_store);

	gtk_table_attach_defaults (table, GTK_WIDGET (priv->key_combo), 2, 3, 1, 2);

	/* Set up the accessibility relationships */
	a11y_label = gtk_widget_get_accessible (GTK_WIDGET (label));
	a11y_key_combo = gtk_widget_get_accessible (GTK_WIDGET (priv->key_combo));
	atk_object_add_relationship (a11y_label, ATK_RELATION_LABEL_FOR, a11y_key_combo);
	atk_object_add_relationship (a11y_key_combo, ATK_RELATION_LABELLED_BY, a11y_label);

	/* Set the selected key combo value */
	key = gconf_client_get_string (almanah->gconf_client, ENCRYPTION_KEY_GCONF_PATH, NULL);
	if (key != NULL && *key == '\0') {
		g_free (key);
		key = NULL;
	}

	cryptui_key_combo_set_key (priv->key_combo, key);
	g_free (key);

	g_signal_connect (priv->key_combo, "changed", G_CALLBACK (pd_key_combo_changed_cb), preferences_dialog);

	button = gtk_button_new_with_mnemonic (_("New _Key"));
	gtk_table_attach (table, button, 3, 4, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect (button, "clicked", G_CALLBACK (pd_new_key_button_clicked_cb), preferences_dialog);
#endif /* ENABLE_ENCRYPTION */

#ifdef ENABLE_SPELL_CHECKING
	/* Set up the "Enable spell checking" check button */
	priv->spell_checking_enabled_check_button = GTK_CHECK_BUTTON (gtk_check_button_new_with_mnemonic (_("Enable _spell checking")));
	gtk_table_attach_defaults (table, GTK_WIDGET (priv->spell_checking_enabled_check_button), 1, 4, 2, 3);

	spell_checking_enabled_notify_cb (NULL, 0, NULL, preferences_dialog);
	g_signal_connect (priv->spell_checking_enabled_check_button, "toggled", G_CALLBACK (pd_spell_checking_enabled_check_button_toggled_cb), preferences_dialog);
	priv->spell_checking_enabled_id = gconf_client_notify_add (almanah->gconf_client, "/apps/almanah/spell_checking_enabled",
								   (GConfClientNotifyFunc) spell_checking_enabled_notify_cb, preferences_dialog,
								   NULL, NULL);
#endif /* ENABLE_SPELL_CHECKING */

	g_object_unref (builder);

	return preferences_dialog;
}

#ifdef ENABLE_ENCRYPTION
static void
pd_key_combo_changed_cb (GtkComboBox *combo_box, AlmanahPreferencesDialog *preferences_dialog)
{
	const gchar *key;
	GError *error = NULL;

	/* Save the new encryption key to GConf */
	key = cryptui_key_combo_get_key (preferences_dialog->priv->key_combo);
	if (key == NULL)
		key = "";

	if (gconf_client_set_string (almanah->gconf_client, ENCRYPTION_KEY_GCONF_PATH, key, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (almanah->preferences_dialog),
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error saving the encryption key"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
	}
}

static void
pd_new_key_button_clicked_cb (GtkButton *button, AlmanahPreferencesDialog *preferences_dialog)
{
	/* NOTE: pilfered from cryptui_need_to_get_keys */
	const gchar *argv[2] = { "seahorse", NULL };
	GError *error = NULL;

	if (g_spawn_async (NULL, (gchar**) argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error) == FALSE) {
		GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (almanah->preferences_dialog),
							    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							    _("Error opening Seahorse"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_error_free (error);
	}
}
#endif /* ENABLE_ENCRYPTION */

#ifdef ENABLE_SPELL_CHECKING
static void
pd_response_cb (GtkDialog *dialog, gint response_id, AlmanahPreferencesDialog *preferences_dialog)
{
	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
spell_checking_enabled_notify_cb (GConfClient *client, guint connection_id, GConfEntry *entry, AlmanahPreferencesDialog *self)
{
	gboolean enabled;

	enabled = gconf_client_get_bool (almanah->gconf_client, "/apps/almanah/spell_checking_enabled", NULL);

	if (almanah->debug)
		g_debug ("spell_checking_enabled_notify_cb called with %u.", enabled);

	g_signal_handlers_block_by_func (self->priv->spell_checking_enabled_check_button, pd_spell_checking_enabled_check_button_toggled_cb, self);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->priv->spell_checking_enabled_check_button), enabled);
	g_signal_handlers_unblock_by_func (self->priv->spell_checking_enabled_check_button, pd_spell_checking_enabled_check_button_toggled_cb, self);

	if (enabled == TRUE) {
		GError *error = NULL;

		almanah_main_window_enable_spell_checking (ALMANAH_MAIN_WINDOW (almanah->main_window), &error);

		if (error != NULL) {
			GtkWidget *dialog = gtk_message_dialog_new (NULL,
								    GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
								    _("Spelling checker could not be initialized"));
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			g_error_free (error);
		}
	} else {
		almanah_main_window_disable_spell_checking (ALMANAH_MAIN_WINDOW (almanah->main_window));
	}
}

static void
pd_spell_checking_enabled_check_button_toggled_cb (GtkToggleButton *toggle_button, gpointer user_data)
{
	gconf_client_set_bool (almanah->gconf_client, "/apps/almanah/spell_checking_enabled",
			       gtk_toggle_button_get_active (toggle_button), NULL);
}

#endif /* ENABLE_SPELL_CHECKING */
