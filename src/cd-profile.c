/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>
#include <lcms2.h>

#include "cd-common.h"
#include "cd-profile.h"

static void     cd_profile_finalize	(GObject     *object);

#define CD_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CD_TYPE_PROFILE, CdProfilePrivate))

/**
 * CdProfilePrivate:
 *
 * Private #CdProfile data
 **/
struct _CdProfilePrivate
{
	gchar				*filename;
	gchar				*id;
	gchar				*object_path;
	gchar				*qualifier;
	gchar				*checksum;
	gchar				*title;
	GDBusConnection			*connection;
	guint				 registration_id;
	guint				 watcher_id;
};

enum {
	SIGNAL_INVALIDATE,
	SIGNAL_LAST
};

enum {
	PROP_0,
	PROP_OBJECT_PATH,
	PROP_ID,
	PROP_QUALIFIER,
	PROP_TITLE,
	PROP_FILENAME,
	PROP_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
G_DEFINE_TYPE (CdProfile, cd_profile, G_TYPE_OBJECT)

/**
 * cd_profile_get_object_path:
 **/
const gchar *
cd_profile_get_object_path (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->object_path;
}

/**
 * cd_profile_get_id:
 **/
const gchar *
cd_profile_get_id (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->id;
}

/**
 * cd_profile_set_id:
 **/
void
cd_profile_set_id (CdProfile *profile, const gchar *id)
{
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_free (profile->priv->id);

	profile->priv->object_path = g_build_filename (COLORD_DBUS_PATH,
						       "profiles",
						       id,
						       NULL);
	profile->priv->id = g_strdup (id);

	/* make sure object path is sane */
	cd_main_ensure_dbus_path (profile->priv->object_path);
}

/**
 * cd_profile_get_filename:
 **/
const gchar *
cd_profile_get_filename (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->filename;
}

/**
 * cd_profile_dbus_emit_property_changed:
 **/
static void
cd_profile_dbus_emit_property_changed (CdProfile *profile,
				       const gchar *property_name,
				       GVariant *property_value)
{
	gboolean ret;
	GError *error_local = NULL;
	GVariantBuilder builder;
	GVariantBuilder invalidated_builder;

	/* build the dict */
	g_variant_builder_init (&invalidated_builder, G_VARIANT_TYPE ("as"));
	g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
	g_variant_builder_add (&builder,
			       "{sv}",
			       property_name,
			       property_value);
	g_dbus_connection_emit_signal (profile->priv->connection,
				       NULL,
				       profile->priv->object_path,
				       "org.freedesktop.DBus.Properties",
				       "PropertiesChanged",
				       g_variant_new ("(sa{sv}as)",
				       COLORD_DBUS_INTERFACE_PROFILE,
				       &builder,
				       &invalidated_builder),
				       &error_local);
	g_assert_no_error (error_local);

	/* emit signal */
	ret = g_dbus_connection_emit_signal (profile->priv->connection,
					     NULL,
					     cd_profile_get_object_path (profile),
					     COLORD_DBUS_INTERFACE_PROFILE,
					     "Changed",
					     NULL,
					     &error_local);
	if (!ret) {
		g_warning ("failed to send signal %s", error_local->message);
		g_error_free (error_local);
	}
}

/**
 * cd_profile_dbus_method_call:
 **/
static void
cd_profile_dbus_method_call (GDBusConnection *connection_, const gchar *sender,
			    const gchar *object_path, const gchar *interface_name,
			    const gchar *method_name, GVariant *parameters,
			    GDBusMethodInvocation *invocation, gpointer user_data)
{
	gboolean ret;
	gchar *property_name = NULL;
	gchar *property_value = NULL;
	GError *error = NULL;
	CdProfile *profile = CD_PROFILE (user_data);

	/* return '' */
	if (g_strcmp0 (method_name, "SetProperty") == 0) {

		/* require auth */
		ret = cd_main_sender_authenticated (invocation,
						    sender,
						    "org.freedesktop.color-manager.modify-profile");
		if (!ret)
			goto out;

		/* set, and parse */
		g_variant_get (parameters, "(ss)",
			       &property_name,
			       &property_value);
		g_debug ("Attempting to set %s to %s",
			 property_name, property_value);
		if (g_strcmp0 (property_name, "Filename") == 0) {
			ret = cd_profile_set_filename (profile, property_value, &error);
			if (!ret) {
				g_dbus_method_invocation_return_gerror (invocation,
									error);
				g_error_free (error);
				goto out;
			}
			cd_profile_dbus_emit_property_changed (profile,
							       property_name,
							       g_variant_new_string (property_value));
			cd_profile_dbus_emit_property_changed (profile,
							       "Title",
							       g_variant_new_string (profile->priv->title));
			g_dbus_method_invocation_return_value (invocation, NULL);
			goto out;
		}
		if (g_strcmp0 (property_name, "Qualifier") == 0) {
			cd_profile_set_qualifier (profile, property_value);
			cd_profile_dbus_emit_property_changed (profile,
							       property_name,
							       g_variant_new_string (property_value));
			g_dbus_method_invocation_return_value (invocation, NULL);
			goto out;
		}
		g_dbus_method_invocation_return_error (invocation,
						       CD_MAIN_ERROR,
						       CD_MAIN_ERROR_FAILED,
						       "property %s not understood",
						       property_name);
		goto out;
	}

	/* we suck */
	g_critical ("failed to process method %s", method_name);
out:
	g_free (property_name);
	g_free (property_value);
}

/**
 * cd_profile_dbus_get_property:
 **/
static GVariant *
cd_profile_dbus_get_property (GDBusConnection *connection_, const gchar *sender,
			     const gchar *object_path, const gchar *interface_name,
			     const gchar *property_name, GError **error,
			     gpointer user_data)
{
	gchar **profiles = NULL;
	GVariant *retval = NULL;
	CdProfile *profile = CD_PROFILE (user_data);

	if (g_strcmp0 (property_name, "Title") == 0) {
		if (profile->priv->title != NULL)
			retval = g_variant_new_string (profile->priv->title);
		else
			retval = g_variant_new_string ("");
		goto out;
	}
	if (g_strcmp0 (property_name, "ProfileId") == 0) {
		if (profile->priv->id != NULL)
			retval = g_variant_new_string (profile->priv->id);
		else
			retval = g_variant_new_string ("");
		goto out;
	}
	if (g_strcmp0 (property_name, "Qualifier") == 0) {
		if (profile->priv->qualifier != NULL)
			retval = g_variant_new_string (profile->priv->qualifier);
		else
			retval = g_variant_new_string ("");
		goto out;
	}
	if (g_strcmp0 (property_name, "Filename") == 0) {
		if (profile->priv->filename != NULL)
			retval = g_variant_new_string (profile->priv->filename);
		else
			retval = g_variant_new_string ("");
		goto out;
	}

	g_critical ("failed to set property %s", property_name);
out:
	g_strfreev (profiles);
	return retval;
}

/**
 * cd_profile_register_object:
 **/
gboolean
cd_profile_register_object (CdProfile *profile,
			    GDBusConnection *connection,
			    GDBusInterfaceInfo *info,
			    GError **error)
{
	GError *error_local = NULL;
	gboolean ret = FALSE;

	static const GDBusInterfaceVTable interface_vtable = {
		cd_profile_dbus_method_call,
		cd_profile_dbus_get_property,
		NULL
	};

	profile->priv->connection = connection;
	profile->priv->registration_id = g_dbus_connection_register_object (
		connection,
		profile->priv->object_path,
		info,
		&interface_vtable,
		profile,  /* user_data */
		NULL,  /* user_data_free_func */
		&error_local); /* GError** */
	if (profile->priv->registration_id == 0) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "failed to register object: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cd_profile_set_filename:
 **/
gboolean
cd_profile_set_filename (CdProfile *profile, const gchar *filename, GError **error)
{
	gboolean ret = FALSE;
	gchar *data = NULL;
	gchar text[1024];
	gsize len;
	GError *error_local = NULL;
	cmsHPROFILE lcms_profile = NULL;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);

	/* copy the profile path */
	if (profile->priv->filename != NULL) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "profile '%s' filename already set",
			     profile->priv->object_path);
		goto out;
	}

	/* parse the ICC file */
	ret = g_file_get_contents (filename, &data, &len, &error_local);
	if (!ret) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "failed to open profile: %s",
			     error_local->message);
		g_error_free (error_local);
		goto out;
	}
	lcms_profile = cmsOpenProfileFromMem (data, len);
	if (lcms_profile == NULL) {
		g_set_error (error,
			     CD_MAIN_ERROR,
			     CD_MAIN_ERROR_FAILED,
			     "failed to parse %s",
			     filename);
		goto out;
	}

	/* get the description as the title */
	cmsGetProfileInfoASCII (lcms_profile,
				cmsInfoDescription,
				"en", "US",
				text, 1024);
	profile->priv->title = g_strdup (text);

	/* generate and set checksum */
	profile->priv->checksum =
		g_compute_checksum_for_data (G_CHECKSUM_MD5,
					     (const guchar *) data,
					    len);

	/* success */
	g_free (profile->priv->filename);
	profile->priv->filename = g_strdup (filename);
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	g_free (data);
	return ret;
}

/**
 * cd_profile_get_qualifier:
 **/
const gchar *
cd_profile_get_qualifier (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->qualifier;
}

/**
 * cd_profile_set_qualifier:
 **/
void
cd_profile_set_qualifier (CdProfile *profile, const gchar *qualifier)
{
	g_return_if_fail (CD_IS_PROFILE (profile));
	g_free (profile->priv->qualifier);
	profile->priv->qualifier = g_strdup (qualifier);
}

/**
 * cd_profile_get_title:
 **/
const gchar *
cd_profile_get_title (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->title;
}

/**
 * cd_profile_get_checksum:
 **/
const gchar *
cd_profile_get_checksum (CdProfile *profile)
{
	g_return_val_if_fail (CD_IS_PROFILE (profile), NULL);
	return profile->priv->checksum;
}

/**
 * cd_profile_name_vanished_cb:
 **/
static void
cd_profile_name_vanished_cb (GDBusConnection *connection,
			     const gchar *name,
			     gpointer user_data)
{
	CdProfile *profile = CD_PROFILE (user_data);
	g_debug ("emit 'invalidate' as %s vanished", name);
	g_signal_emit (profile, signals[SIGNAL_INVALIDATE], 0);
}

/**
 * cd_profile_watch_sender:
 **/
void
cd_profile_watch_sender (CdProfile *profile, const gchar *sender)
{
	g_return_if_fail (CD_IS_PROFILE (profile));
	profile->priv->watcher_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
						      sender,
						      G_BUS_NAME_WATCHER_FLAGS_NONE,
						      NULL,
						      cd_profile_name_vanished_cb,
						      profile,
						      NULL);
}

/**
 * cd_profile_get_property:
 **/
static void
cd_profile_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CdProfile *profile = CD_PROFILE (object);
	CdProfilePrivate *priv = profile->priv;

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_value_set_string (value, priv->object_path);
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_profile_set_property:
 **/
static void
cd_profile_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdProfile *profile = CD_PROFILE (object);
	CdProfilePrivate *priv = profile->priv;

	switch (prop_id) {
	case PROP_OBJECT_PATH:
		g_free (priv->object_path);
		priv->object_path = g_strdup (g_value_get_string (value));
		break;
	case PROP_ID:
		g_free (priv->id);
		priv->id = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * cd_profile_class_init:
 **/
static void
cd_profile_class_init (CdProfileClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = cd_profile_finalize;
	object_class->get_property = cd_profile_get_property;
	object_class->set_property = cd_profile_set_property;

	/**
	 * CdProfile:object-path:
	 */
	pspec = g_param_spec_string ("object-path", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OBJECT_PATH, pspec);

	/**
	 * CdProfile:id:
	 */
	pspec = g_param_spec_string ("id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	/**
	 * CdProfile::invalidate:
	 **/
	signals[SIGNAL_INVALIDATE] =
		g_signal_new ("invalidate",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (CdProfileClass, invalidate),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (CdProfilePrivate));
}

/**
 * cd_profile_init:
 **/
static void
cd_profile_init (CdProfile *profile)
{
	profile->priv = CD_PROFILE_GET_PRIVATE (profile);
}

/**
 * cd_profile_finalize:
 **/
static void
cd_profile_finalize (GObject *object)
{
	CdProfile *profile = CD_PROFILE (object);
	CdProfilePrivate *priv = profile->priv;

	if (priv->watcher_id > 0)
		g_bus_unwatch_name (priv->watcher_id);
	if (priv->registration_id > 0) {
		g_dbus_connection_unregister_object (priv->connection,
						     priv->registration_id);
	}
	g_free (priv->filename);
	g_free (priv->qualifier);
	g_free (priv->title);
	g_free (priv->id);
	g_free (priv->checksum);
	g_free (priv->object_path);

	G_OBJECT_CLASS (cd_profile_parent_class)->finalize (object);
}

/**
 * cd_profile_new:
 **/
CdProfile *
cd_profile_new (void)
{
	CdProfile *profile;
	profile = g_object_new (CD_TYPE_PROFILE, NULL);
	return CD_PROFILE (profile);
}
