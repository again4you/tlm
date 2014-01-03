/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of tlm
 *
 * Copyright (C) 2013 Intel Corporation.
 *
 * Contact: Imran Zaman <imran.zaman@intel.com>
 *          Amarnath Valluri <amarnath.valluri@linux.intel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <stdlib.h>
#include <unistd.h>
#include <glib/gstdio.h>

#include "config.h"
#include "tlm-config.h"
#include "tlm-config-general.h"
#include "tlm-log.h"

/**
 * SECTION:tlm-config
 * @short_description: tlm configuration information
 * @include: tlm/common/tlm-config.h
 *
 * #TlmConfig holds configuration information as a set of keys and values
 * (integer or strings). The key names are defined in
 * <link linkend="tlmd-General-configuration">General config keys</link>,
 * and <link linkend="tlmd-DBus-configuration">DBus config keys</link>.
 *
 * The configuration is retrieved from the tlm configuration file. See below
 * for where the file is searched for.
 *
 * <refsect1><title>Usage</title></refsect1>
 * Following code snippet demonstrates how to create and use config object:
 * |[
 *
 * TlmConfig* config = tlm_config_new ();
 * const gchar *str = tlm_config_get_string (config,
 *  TLM_CONFIG_GENERAL_SKEL_DIR, 0);
 * g_object_unref(config);
 *
 * ]|
 *
 * <refsect1><title>Where the configuration file is searched for</title>
 * </refsect1>
 *
 * If tlm has been compiled with --enable-debug, then these locations are used,
 * in decreasing order of priority:
 * - UM_CONF_FILE environment variable
 * - g_get_user_config_dir() + "tlm.conf"
 * - each of g_get_system_config_dirs() + "tlm.conf"
 *
 * Otherwise, the config file location is determined at compilation time as
 * $(sysconfdir) + "tlm.conf"
 *
 * <refsect1><title>Example configuration file</title></refsect1>
 *
 * See example configuration file here:
 * <ulink url="https://github.com/01org/tlmd/blob/master/src/common/tlm.conf.in">
 * tlm configuration file</ulink>
 *
 */

/**
 * TlmConfig:
 *
 * Opaque structure for the object.
 */

/**
 * TlmConfigClass:
 * @parent_class: parent class object
 *
 * Opaque structure for the class.
 */

struct _TlmConfigPrivate
{
    gchar *config_file_path;
    GHashTable *config_table;
};

#define TLM_CONFIG_PRIV(obj) G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
        TLM_TYPE_CONFIG, TlmConfigPrivate)

G_DEFINE_TYPE (TlmConfig, tlm_config, G_TYPE_OBJECT);

static gboolean
_load_config (
        TlmConfig *self)
{
    gchar *def_config;
    GError *err = NULL;
    gchar **groups = NULL;
    gsize n_groups = 0;
    int i,j;
    GKeyFile *settings = g_key_file_new ();

#   ifdef ENABLE_DEBUG
    const gchar * const *sysconfdirs;

    if (!self->priv->config_file_path) {
        def_config = g_strdup (g_getenv ("TLM_CONF_FILE"));
        if (!def_config)
            def_config = g_build_filename (g_get_user_config_dir(),
                                           "tlm/tlm.conf",
                                           NULL);
        if (g_access (def_config, R_OK) == 0) {
            self->priv->config_file_path = def_config;
        } else {
            g_free (def_config);
            sysconfdirs = g_get_system_config_dirs ();
            while (*sysconfdirs != NULL) {
                def_config = g_build_filename (*sysconfdirs,
                                               "tlm/tlm.conf",
                                               NULL);
                if (g_access (def_config, R_OK) == 0) {
                    self->priv->config_file_path = def_config;
                    break;
                }
                g_free (def_config);
                sysconfdirs++;
            }
        }
    }
#   else  /* ENABLE_DEBUG */
#   ifndef TLM_SYSCONF_DIR
#   error "System configuration directory not defined!"
#   endif
    def_config = g_build_filename (TLM_SYSCONF_DIR,
                                   "tlm/tlm.conf",
                                   NULL);
    if (g_access (def_config, R_OK) == 0) {
        self->priv->config_file_path = def_config;
    } else {
        g_free (def_config);
    }
#   endif  /* ENABLE_DEBUG */

    if (self->priv->config_file_path) {
        DBG ("Loading TLM config from %s", self->priv->config_file_path);
        if (!g_key_file_load_from_file (settings,
                                        self->priv->config_file_path,
                                        G_KEY_FILE_NONE, &err)) {
            WARN ("error reading config file at '%s': %s",
                 self->priv->config_file_path, err->message);
            g_error_free (err);
            g_key_file_free (settings);
            return FALSE;
        }
    }

    groups = g_key_file_get_groups (settings, &n_groups);

    for (i = 0; i < n_groups; i++) {
        GError *err = NULL;
        gsize n_keys =0;
        GHashTable *group_table = NULL;
        gchar **keys = g_key_file_get_keys (settings,
                                            groups[i],
                                            &n_keys,
                                            &err);
        if (err) {
            WARN ("fail to read group '%s': %s", groups[i], err->message);
            g_error_free (err);
            continue;
        }

        group_table = (GHashTable *) g_hash_table_lookup (
                                self->priv->config_table, groups[i]);
        if (!group_table) 
            group_table = g_hash_table_new_full (
                               g_str_hash,
                               g_str_equal,
                               g_free,
                               (GDestroyNotify)g_variant_unref);

        for (j = 0; j < n_keys; j++) {
            gchar *value = g_key_file_get_value (settings,
                                                 groups[i],
                                                 keys[j],
                                                 &err);
            if (err) {
                WARN ("fail to read key '%s/%s': %s", groups[i], keys[j],
                        err->message);
                g_error_free (err);
                continue;
            }

            DBG ("found config : '%s/%s' - '%s'", groups[i], keys[j], value);

            g_hash_table_insert (group_table,
                                 (gpointer)keys[j],
                                 (gpointer)g_variant_new_string (value));

        }

        g_hash_table_insert (self->priv->config_table,
                             g_strdup (groups[i]),
                             group_table);

        g_strfreev (keys);
    }

    g_strfreev (groups);

    g_key_file_free (settings);

    return TRUE;
}

#ifdef ENABLE_DEBUG
static void
_load_environment (
        TlmConfig *self)
{
    const gchar *e_val = 0;
    
    e_val = g_getenv ("TLM_PLUGINS_DIR");
    if (e_val)
        tlm_config_set_string (self, 
                               TLM_CONFIG_GENERAL,
                               TLM_CONFIG_GENERAL_PLUGINS_DIR,
                               e_val);


    e_val = g_getenv("TLM_ACCOUNT_PLUGIN");
    if (e_val)
        tlm_config_set_string (self, 
                               TLM_CONFIG_GENERAL, 
                               TLM_CONFIG_GENERAL_ACCOUNTS_PLUGIN, e_val);
}
#endif  /* ENABLE_DEBUG */

static void
_set_defaults (
        TlmConfig *self)
{

    /* plugins dir => TLM_PLUGINS_DIR <=> $(pkglibdir)/plugins */
    if (!tlm_config_get_string (self, 
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_PLUGINS_DIR)) {
        tlm_config_set_string (self,
                               TLM_CONFIG_GENERAL, 
                               TLM_CONFIG_GENERAL_PLUGINS_DIR,
                               TLM_PLUGINS_DIR);
    }

    /* accounts plugin => default */
    if (!tlm_config_get_string (self,
                                TLM_CONFIG_GENERAL,
                                TLM_CONFIG_GENERAL_ACCOUNTS_PLUGIN)) {
        tlm_config_set_string (self,
                               TLM_CONFIG_GENERAL,
                               TLM_CONFIG_GENERAL_ACCOUNTS_PLUGIN,
                               "default");
    }

}

/**
 * tlm_config_get_group:
 * @self: (transfer none): an instance of #TlmConfig
 * @group: the group name
 *
 * Retrives the configuration in given #group as #GHashTable
 *
 * Returns: (tranfer none): the key, value paired dictionary if found,
 * NULL otherwise.
 */
GHashTable *
tlm_config_get_group (
        TlmConfig *self,
        const gchar *group)
{
    g_return_val_if_fail (self && TLM_IS_CONFIG(self), NULL);
    g_return_val_if_fail (group && group[0], NULL);

    return (GHashTable *) g_hash_table_lookup (self->priv->config_table,
                                                  group);
}

/**
 * tlm_config_get_value:
 * @self: (transfer none): an instance of #TlmConfig
 * @group: (transfer none): the group name, NULL refers to General
 * @key: (transfer none): the key name
 *
 * Get the configuration value stored for #key in #group.
 *
 * Returns: the value corresponding to the key and group. If the
 * key does not exist, NULL is returned.
 */
GVariant *
tlm_config_get_value (
        TlmConfig *self,
        const gchar *group,
        const gchar *key)
{
    g_return_val_if_fail (self && TLM_IS_CONFIG(self), NULL);
    g_return_val_if_fail (key && key[0], NULL);

    GHashTable *group_table = tlm_config_get_group (self, group);
    if (!group_table) return NULL;

    return (GVariant *) g_hash_table_lookup (group_table, key);
}

/**
 * tlm_config_get_string:
 * @self: (transfer none): an instance of #TlmConfig
 * @group: (transfer none): the group name, NULL refers to General
 * @key: (transfer none): the key name
 *
 * Get a string configuration value.
 *
 * Returns: the value corresponding to the key as an string. If the
 * key does not exist or cannot be converted to the integer, NULL is returned.
 */
const gchar *
tlm_config_get_string (
        TlmConfig *self,
        const gchar *group,
        const gchar *key)
{
    g_return_val_if_fail (self && TLM_IS_CONFIG (self), NULL);
    g_return_val_if_fail (key && key[0], NULL);

    GVariant* value = tlm_config_get_value (self, group, key);
    if (!value) return NULL;

    return g_variant_get_string (value, NULL);
}

/**
 * tlm_config_set_string:
 * @self: (transfer none): an instance of #TlmConfig
 * @group: (transfer none): the group name
 * @key: (transfer none): the key name
 * @value: (transfer none): the value
 *
 * Sets the configuration value to the provided string.
 */
void
tlm_config_set_string (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        const gchar *value)
{
    g_return_if_fail (self && TLM_IS_CONFIG (self));
    g_return_if_fail (key && key[0]);

    if (!group) group = TLM_CONFIG_GENERAL;

    GHashTable *group_table = tlm_config_get_group (self, group);
    if (!group_table) {
        group_table = g_hash_table_new_full (g_str_hash,
                                             g_str_equal,
                                             g_free,
                                             (GDestroyNotify)g_variant_unref);
        g_hash_table_insert (self->priv->config_table,
                             (gpointer)group,
                             (gpointer)group_table);
    }

    g_hash_table_insert (group_table,
                         (gpointer) key,
                         g_variant_new_string (value));

}
/**
 * tlm_config_get_int:
 * @self: (transfer none): an instance of #TlmConfig
 * @group: the group name
 * @key: the key name
 * @retval: value to be returned in case key is not found
 *
 * Get an integer configuration value.
 *
 * Returns: the value corresponding to the key as an integer. If the key does
 * not exist or cannot be converted to the integer, retval is returned.
 */
gint
tlm_config_get_int (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        gint retval)
{
    const gchar *str_value = tlm_config_get_string (self, group, key);
    return (gint) (str_value ? atoi (str_value) : retval);
}

/**
 * tlm_config_set_int:
 * @self: (transfer none): an instance of #TlmConfig
 * @group: the group name
 * @key: the key name
 * @value: the value
 *
 * Sets the configuration value to the provided integer.
 */
void
tlm_config_set_int (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        gint value)
{
    gchar *s_value = 0;
    g_return_if_fail (self && TLM_IS_CONFIG (self));

    s_value = g_strdup_printf ("%d", value);
    if (!s_value) return;

    tlm_config_set_string (self, group, key, s_value);

    g_free (s_value);
}

/**
 * tlm_config_get_uint:
 * @self: (transfer none): an instance of #TlmConfig
 * @group: (trnsfer none): the group name
 * @key: (transfer none): the key name
 * @retval: value to be returned in case key is not found
 *
 * Get an unsigned integer configuration value.
 *
 * Returns: the value corresponding to the key as an unsigned integer. If the
 * key does not exist or cannot be converted to the integer, retval is returned.
 */
guint
tlm_config_get_uint (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        guint retval)
{
    guint value;
    const gchar *str_value = tlm_config_get_string (self, group, key);
    if (!str_value || sscanf (str_value, "%u", &value) <= 0) value = retval;

    return value;
}

/**
 * tlm_config_set_uint:
 * @self: (transfer none): an instance of #TlmConfig
 * @group: (transfer none):  the group name
 * @key: the key name
 * @value: the value
 *
 * Sets the configuration value to the provided unsigned integer.
 */
void
tlm_config_set_uint (
        TlmConfig *self,
        const gchar *group,
        const gchar *key,
        guint value)
{
    gchar *s_value = 0;
    g_return_if_fail (self && TLM_IS_CONFIG (self));

    s_value = g_strdup_printf ("%u", value);
    if (!s_value) return;

    tlm_config_set_string (self, group, key, s_value);
    g_free (s_value);
}


static void
tlm_config_dispose (
        GObject *object)
{
    TlmConfig *self = 0;
    g_return_if_fail (object && TLM_IS_CONFIG (object));

    self = TLM_CONFIG (object);

    if (self->priv->config_table) {
        g_hash_table_unref (self->priv->config_table);
        self->priv->config_table = NULL;
    }

    G_OBJECT_CLASS (tlm_config_parent_class)->dispose (object);
}

static void
tlm_config_finalize (
        GObject *object)
{
    TlmConfig *self = 0;
    g_return_if_fail (object && TLM_IS_CONFIG (object));

    self = TLM_CONFIG (object);

    if (self->priv->config_file_path) {
        g_free (self->priv->config_file_path);
        self->priv->config_file_path = NULL;
    }

    G_OBJECT_CLASS (tlm_config_parent_class)->finalize (object);
}

static void
tlm_config_class_init (
        TlmConfigClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (TlmConfigPrivate));

    object_class->dispose = tlm_config_dispose;
    object_class->finalize = tlm_config_finalize;
}

static void
tlm_config_init (
        TlmConfig *self)
{
    self->priv = TLM_CONFIG_PRIV (self);

    self->priv->config_file_path = NULL;
    self->priv->config_table = g_hash_table_new_full (
                                    g_str_hash,
                                    g_str_equal,
                                    g_free,
                                    (GDestroyNotify)g_hash_table_unref);


    if (!_load_config (self))
        WARN ("load configuration failed, using default settings");

#ifdef ENABLE_DEBUG
    _load_environment (self);
#endif
    _set_defaults (self);
}

/**
 * tlm_config_new:
 *
 * Create a #TlmConfig object.
 *
 * Returns: an instance of #TlmConfig.
 */
TlmConfig *
tlm_config_new ()
{
    return TLM_CONFIG (g_object_new (TLM_TYPE_CONFIG, NULL));
}

