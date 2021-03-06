/* Mission Control plugin API - interface to an McdAccountManager for plugins
 *
 * Copyright © 2010 Nokia Corporation
 * Copyright © 2010 Collabora Ltd.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <mission-control-plugins/mission-control-plugins.h>
#include <mission-control-plugins/implementation.h>
#include <mission-control-plugins/debug-internal.h>

#define MCP_DEBUG_TYPE MCP_DEBUG_ACCOUNT

/**
 * SECTION:account
 * @title: McpAccountManager
 * @short_description: Object representing the account manager, implemented
 *    by Mission Control
 * @see_also: #McpAccountStorage
 * @include: mission-control-plugins/mission-control-plugins.h
 *
 * This object represents the Telepathy AccountManager.
 *
 * Most virtual methods on the McpAccountStorageIface interface receive an
 * object provided by Mission Control that implements this interface.
 * It can be used to manipulate Mission Control's in-memory cache of accounts.
 *
 * Only Mission Control should implement this interface.
 */

GType
mcp_account_manager_get_type (void)
{
  static gsize once = 0;
  static GType type = 0;

  if (g_once_init_enter (&once))
    {
      static const GTypeInfo info = {
          sizeof (McpAccountManagerIface),
          NULL, /* base_init */
          NULL, /* base_finalize */
          NULL, /* class_init */
          NULL, /* class_finalize */
          NULL, /* class_data */
          0, /* instance_size */
          0, /* n_preallocs */
          NULL, /* instance_init */
          NULL /* value_table */
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
          "McpAccountManager", &info, 0);
      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
      g_once_init_leave (&once, 1);
    }

  return type;
}

/**
 * mcp_account_manager_get_unique_name:
 * @mcpa: an #McpAccountManager instance
 * @manager: the name of the manager
 * @protocol: the name of the protocol
 * @identification: the result of calling IdentifyAccount for this account
 *
 * Generate and return the canonical unique name of this [new] account.
 * Should not be called for accounts which have already had a name
 * assigned: Intended for use when a plugin encounters an account which
 * MC has not previously seen before (ie one created by a 3rd party
 * in the back-end that the plugin in question provides an interface to).
 *
 * Changed in 5.17: instead of a map from string to GValue, the last
 * argument is the result of calling IdentifyAccount on the parameters,
 * which normalizes the account's name in a protocol-dependent way.
 * Use mcp_account_manager_identify_account_async() to do that.
 *
 * Returns: the newly allocated account name, which should be freed
 * once the caller is done with it.
 */
gchar *
mcp_account_manager_get_unique_name (McpAccountManager *mcpa,
    const gchar *manager,
    const gchar *protocol,
    const gchar *identification)
{
  McpAccountManagerIface *iface = MCP_ACCOUNT_MANAGER_GET_IFACE (mcpa);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->unique_name != NULL, NULL);

  return iface->unique_name (mcpa, manager, protocol, identification);
}

void
mcp_account_manager_identify_account_async (McpAccountManager *mcpa,
    const gchar *manager,
    const gchar *protocol,
    GVariant *parameters,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  McpAccountManagerIface *iface = MCP_ACCOUNT_MANAGER_GET_IFACE (mcpa);

  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->identify_account_async != NULL);
  g_return_if_fail (iface->identify_account_finish != NULL);

  g_return_if_fail (manager != NULL);
  g_return_if_fail (protocol != NULL);
  g_return_if_fail (parameters != NULL);
  g_return_if_fail (g_variant_is_of_type (parameters, G_VARIANT_TYPE_VARDICT));

  iface->identify_account_async (mcpa, manager, protocol, parameters,
      cancellable, callback, user_data);
}

/**
 * Returns: (transfer full): a newly allocated string, free with g_free()
 */
gchar *
mcp_account_manager_identify_account_finish (McpAccountManager *mcpa,
    GAsyncResult *res,
    GError **error)
{
  McpAccountManagerIface *iface = MCP_ACCOUNT_MANAGER_GET_IFACE (mcpa);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->identify_account_async != NULL, NULL);
  g_return_val_if_fail (iface->identify_account_finish != NULL, NULL);

  return iface->identify_account_finish (mcpa, res, error);
}

/**
 * mcp_account_manager_escape_variant_for_keyfile:
 * @mcpa: a #McpAccountManager
 * @variant: a #GVariant with a supported #GVariantType
 *
 * Escape @variant so it could be passed to g_key_file_set_value().
 * For instance, escaping the boolean value TRUE returns "true",
 * and escaping the string value containing one space returns "\s".
 *
 * It is a programming error to use an unsupported type.
 * The supported types are currently %G_VARIANT_TYPE_STRING,
 * %G_VARIANT_TYPE_BOOLEAN, %G_VARIANT_TYPE_INT32, %G_VARIANT_TYPE_UINT32,
 * %G_VARIANT_TYPE_INT64, %G_VARIANT_TYPE_UINT64, %G_VARIANT_TYPE_BYTE,
 * %G_VARIANT_TYPE_STRING_ARRAY, %G_VARIANT_TYPE_OBJECT_PATH and
 * %G_VARIANT_TYPE_OBJECT_PATH_ARRAY.
 *
 * Returns: (transfer full): the escaped form of @variant
 */
gchar *
mcp_account_manager_escape_variant_for_keyfile (const McpAccountManager *mcpa,
    GVariant *variant)
{
  McpAccountManagerIface *iface = MCP_ACCOUNT_MANAGER_GET_IFACE (mcpa);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->escape_variant_for_keyfile != NULL, NULL);

  return iface->escape_variant_for_keyfile (mcpa, variant);
}

/**
 * mcp_account_manager_unescape_variant_from_keyfile:
 * @mcpa: a #McpAccountManager
 * @escaped: a string that could have come from g_key_file_get_value()
 * @type: the type of the variant to which to unescape
 *
 * Unescape @escaped as if it had appeared in a #GKeyFile, with syntax
 * appropriate for @type.
 *
 * It is a programming error to use an unsupported type.
 *
 * Returns: (transfer full): the unescaped form of @escaped
 *  (*not* a floating reference)
 */
GVariant *
mcp_account_manager_unescape_variant_from_keyfile (
    const McpAccountManager *mcpa,
    const gchar *escaped,
    const GVariantType *type,
    GError **error)
{
  McpAccountManagerIface *iface = MCP_ACCOUNT_MANAGER_GET_IFACE (mcpa);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->unescape_variant_from_keyfile != NULL, NULL);

  return iface->unescape_variant_from_keyfile (mcpa, escaped, type, error);
}
