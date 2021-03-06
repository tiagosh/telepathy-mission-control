/* Mission Control plugin API - Account Storage plugins.
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

/**
 * SECTION:account-storage
 * @title: McpAccountStorage
 * @short_description: Account Storage object, implemented by plugins
 * @see_also:
 * @include: mission-control-plugins/mission-control-plugins.h
 *
 * Plugins may implement #McpAccountStorage in order to provide account
 * parameter storage backends to the AccountManager object.
 *
 * To do so, the plugin must implement a #GObject subclass that implements
 * #McpAccountStorage, then return an instance of that subclass from
 * mcp_plugin_ref_nth_object().
 *
 * Many methods take "the unique name of an account" as an argument.
 * In this plugin, that means the unique "tail" of the account's
 * object path, for instance "gabble/jabber/chris_40example_2ecom".
 * The account's full object path is obtained by prepending
 * %TP_ACCOUNT_OBJECT_PATH_BASE.
 *
 * A complete implementation of this interface with all methods would
 * look something like this:
 *
 * <example><programlisting>
 * G_DEFINE_TYPE_WITH_CODE (FooPlugin, foo_plugin,
 *    G_TYPE_OBJECT,
 *    G_IMPLEMENT_INTERFACE (...);
 *    G_IMPLEMENT_INTERFACE (MCP_TYPE_ACCOUNT_STORAGE,
 *      account_storage_iface_init));
 * /<!-- -->* ... *<!-- -->/
 * static void
 * account_storage_iface_init (McpAccountStorageIface *iface)
 * {
 *   iface->priority = 0;
 *   iface->name = "foo";
 *   iface->desc = "The FOO storage backend";
 *   iface->provider = "org.freedesktop.Telepathy.MissionControl5.FooStorage";
 *
 *   iface->get_flags = foo_plugin_get_flags;
 *   iface->delete_async = foo_plugin_delete_async;
 *   iface->delete_finish = foo_plugin_delete_finish;
 *   iface->commit = foo_plugin_commit;
 *   iface->list = foo_plugin_list;
 *   iface->get_identifier = foo_plugin_get_identifier;
 *   iface->get_additional_info = foo_plugin_get_additional_info;
 *   iface->get_restrictions = foo_plugin_get_restrictions;
 *   iface->create = foo_plugin_create;
 *   iface->get_attribute = foo_plugin_get_attribute;
 *   iface->get_parameter = foo_plugin_get_parameter;
 *   iface->list_typed_parameters = foo_plugin_list_typed_parameters;
 *   iface->list_untyped_parameters = foo_plugin_list_untyped_parameters;
 *   iface->set_attribute = foo_plugin_set_attribute;
 *   iface->set_parameter = foo_plugin_set_parameter;
 * }
 * </programlisting></example>
 *
 * A single object can implement more than one interface; It is currently
 * unlikely that you would find it useful to implement anything other than
 * an account storage plugin in an account storage object, though.
 */

#include "config.h"

#include <mission-control-plugins/mission-control-plugins.h>
#include <mission-control-plugins/mcp-signals-marshal.h>
#include <mission-control-plugins/implementation.h>
#include <mission-control-plugins/debug-internal.h>
#include <glib.h>

#define MCP_DEBUG_TYPE  MCP_DEBUG_ACCOUNT_STORAGE

#ifdef ENABLE_DEBUG

#define SDEBUG(_p, _format, ...) \
  DEBUG("%s: " _format, \
        (_p != NULL) ? mcp_account_storage_name (_p) : "NULL", ##__VA_ARGS__)

#else  /* ENABLE_DEBUG */

#define SDEBUG(_p, _format, ...) do {} while (0);

#endif /* ENABLE_DEBUG */

/**
 * McpAccountStorageFlags:
 * @MCP_ACCOUNT_STORAGE_FLAG_NONE: no particular flags
 * @MCP_ACCOUNT_STORAGE_FLAG_STORES_TYPES: this backend can store parameter
 *  values' types
 *
 * Flags describing the features and capabilities of a backend.
 */

enum
{
  CREATED,
  TOGGLED,
  DELETED,
  ALTERED_ONE,
  RECONNECT,
  NO_SIGNAL
};

static guint signals[NO_SIGNAL] = { 0 };

static void
default_delete_async (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  g_task_report_new_error (storage, callback, user_data,
      default_delete_async, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
      "This storage plugin cannot delete accounts");
}

static gboolean
default_delete_finish (McpAccountStorage *storage,
    GAsyncResult *res,
    GError **error)
{
  return g_task_propagate_boolean (G_TASK (res), error);
}

static gboolean
default_commit (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account)
{
  return FALSE;
}

static gchar *
default_create (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *manager,
    const gchar *protocol,
    const gchar *identification,
    GError **error)
{
  g_set_error (error, TP_ERROR, TP_ERROR_NOT_IMPLEMENTED,
      "This storage does not implement the create() function");
  return NULL;
}

static void
default_get_identifier (McpAccountStorage *storage,
    const gchar *account,
    GValue *identifier)
{
  g_value_init (identifier, G_TYPE_STRING);
  g_value_set_string (identifier, account);
}

static GHashTable *
default_get_additional_info (McpAccountStorage *storage,
    const gchar *account)
{
  return g_hash_table_new (g_str_hash, g_str_equal);
}

static TpStorageRestrictionFlags
default_get_restrictions (McpAccountStorage *storage,
    const gchar *account)
{
  return 0;
}

static McpAccountStorageSetResult
default_set_attribute (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account,
    const gchar *attribute,
    GVariant *value,
    McpAttributeFlags flags)
{
  return MCP_ACCOUNT_STORAGE_SET_RESULT_FAILED;
}

static McpAccountStorageSetResult
default_set_parameter (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account,
    const gchar *parameter,
    GVariant *value,
    McpParameterFlags flags)
{
  return MCP_ACCOUNT_STORAGE_SET_RESULT_FAILED;
}

static gchar **
default_list_untyped_parameters (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account)
{
  return NULL;
}

static McpAccountStorageFlags
default_get_flags (McpAccountStorage *storage,
    const gchar *account)
{
  return MCP_ACCOUNT_STORAGE_FLAG_NONE;
}

static void
class_init (gpointer klass,
    gpointer data)
{
  GType type = G_TYPE_FROM_CLASS (klass);
  McpAccountStorageIface *iface = klass;

  iface->get_flags = default_get_flags;
  iface->create = default_create;
  iface->delete_async = default_delete_async;
  iface->delete_finish = default_delete_finish;
  iface->commit = default_commit;
  iface->get_identifier = default_get_identifier;
  iface->get_additional_info = default_get_additional_info;
  iface->get_restrictions = default_get_restrictions;
  iface->set_attribute = default_set_attribute;
  iface->set_parameter = default_set_parameter;
  iface->list_untyped_parameters = default_list_untyped_parameters;

  if (signals[CREATED] != 0)
    {
      DEBUG ("already registered signals");
      return;
    }

  /**
   * McpAccountStorage::created
   * @account: the unique name of the created account
   *
   * Emitted if an external entity creates an account
   * in the backend the emitting plugin handles.
   *
   * This signal does not need to be emitted before mcp_account_storage_list()
   * returns (if it is, it will be ignored). All accounts that exist
   * at the time that mcp_account_storage_list() returns must be included
   * in its result, even if they were also signalled via this signal.
   */
  signals[CREATED] = g_signal_new ("created",
      type, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__STRING, G_TYPE_NONE,
      1, G_TYPE_STRING);

  /**
   * McpAccountStorage::altered-one
   * @account: the unique name of the altered account
   * @name: either an attribute name such as DisplayName,
   *  or "param-" plus a parameter name, e.g. "param-require-encryption"
   *
   * Emitted if an external entity alters an account
   * in the backend that the emitting plugin handles.
   *
   * Before emitting this signal, the plugin must update its
   * internal cache (if any) so that mcp_account_storage_get_attribute()
   * or mcp_account_storage_get_parameter() will return the new value
   * when queried.
   *
   * Note that mcp_account_storage_get_parameter(),
   * mcp_account_storage_list_typed_parameters() and
   * mcp_account_storage_set_parameter() do not use the
   * "param-" prefix, but this signal does.
   */
  signals[ALTERED_ONE] = g_signal_new ("altered-one",
      type, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _mcp_marshal_VOID__STRING_STRING, G_TYPE_NONE,
      2, G_TYPE_STRING, G_TYPE_STRING);


  /**
   * McpAccountStorage::deleted
   * @account: the unique name of the deleted account
   *
   * Emitted if an external entity deletes an account
   * in the backend the emitting plugin handles.
   */
  signals[DELETED] = g_signal_new ("deleted",
      type, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__STRING, G_TYPE_NONE,
      1, G_TYPE_STRING);

  /**
   * McpAccountStorage::toggled
   * @account: the unique name of the toggled account
   * @enabled: #gboolean indicating whether the account is enabled
   *
   * Emitted if an external entity enables/disables an account
   * in the backend the emitting plugin handles. This is similar to
   * emitting #McpAccountStorage::altered-one for the attribute
   * "Enabled".
   *
   * Before emitting this signal, the plugin must update its
   * internal cache (if any) so that mcp_account_storage_get_attribute()
   * will return the new value for Enabled when queried.
   */
  signals[TOGGLED] = g_signal_new ("toggled",
      type, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      _mcp_marshal_VOID__STRING_BOOLEAN, G_TYPE_NONE,
      2, G_TYPE_STRING, G_TYPE_BOOLEAN);

  /**
   * McpAccountStorage::reconnect
   * @account: the unique name of the account to reconnect
   *
   * emitted if an external entity modified important parameters of the
   * account and a reconnection is required in order to apply them.
   **/
  signals[RECONNECT] = g_signal_new ("reconnect",
      type, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      g_cclosure_marshal_VOID__STRING, G_TYPE_NONE,
      1, G_TYPE_STRING);
}

GType
mcp_account_storage_get_type (void)
{
  static gsize once = 0;
  static GType type = 0;

  if (g_once_init_enter (&once))
    {
      static const GTypeInfo info =
        {
          sizeof (McpAccountStorageIface),
          NULL, /* base_init      */
          NULL, /* base_finalize  */
          class_init, /* class_init     */
          NULL, /* class_finalize */
          NULL, /* class_data     */
          0,    /* instance_size  */
          0,    /* n_preallocs    */
          NULL, /* instance_init  */
          NULL  /* value_table    */
        };

      type = g_type_register_static (G_TYPE_INTERFACE,
          "McpAccountStorage", &info, 0);
      g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);

      g_once_init_leave (&once, 1);
    }

  return type;
}

/**
 * McpAccountStorage:
 *
 * An object implementing the account storage plugin interface.
 */

/**
 * McpAccountStorageIface:
 * @parent: the standard fields for an interface
 * @priority: returned by mcp_account_storage_priority()
 * @name: returned by mcp_account_storage_name()
 * @desc: returned by mcp_account_storage_description()
 * @provider: returned by mcp_account_storage_provider()
 * @delete_async: implementation of mcp_account_storage_delete_async()
 * @delete_finish: implementation of mcp_account_storage_delete_finish()
 * @commit: implementation of mcp_account_storage_commit()
 * @list: implementation of mcp_account_storage_list()
 * @get_identifier: implementation of mcp_account_storage_get_identifier()
 * @get_additional_info: implementation of
 *  mcp_account_storage_get_additional_info()
 * @get_restrictions: implementation of mcp_account_storage_get_restrictions()
 * @create: implementation of mcp_account_storage_create()
 * @get_attribute: implementation of mcp_account_storage_get_attribute()
 * @get_parameter: implementation of mcp_account_storage_get_parameter()
 * @set_attribute: implementation of mcp_account_storage_set_attribute()
 * @set_parameter: implementation of mcp_account_storage_set_parameter()
 * @list_typed_parameters: implementation
 *  of mcp_account_storage_list_typed_parameters()
 * @list_untyped_parameters: implementation
 *  of mcp_account_storage_list_untyped_parameters()
 * @get_flags: implementation of mcp_account_storage_get_flags()
 *
 * The interface vtable for an account storage plugin.
 */

/**
 * mcp_account_storage_priority:
 * @storage: an #McpAccountStorage instance
 *
 * Gets the priority for this plugin.
 *
 * Priorities currently run from MCP_ACCOUNT_STORAGE_PLUGIN_PRIO_DEFAULT
 * (the default storage plugin priority) upwards. More-positive numbers
 * are higher priority.
 *
 * Plugins at a higher priority then MCP_ACCOUNT_STORAGE_PLUGIN_PRIO_KEYRING
 * used to have the opportunity to "steal" passwords from the gnome keyring.
 * It is no longer significant.
 *
 * Plugins at a lower priority than the default plugin will never be asked to
 * store any details, although they may still be asked to list them at startup
 * time, and may asynchronously notify MC of accounts via the signals above.
 *
 * When loading accounts at startup, plugins are consulted in order from
 * lowest to highest, so that higher priority plugins may overrule settings
 * from lower priority plugins.
 *
 * Loading all the accounts is only done at startup, before the dbus name
 * is claimed, and is therefore the only time plugins are allowed to indulge
 * in blocking calls (indeed, they are expected to carry out this operation,
 * and ONLY this operation, synchronously).
 *
 * When values are being set, the plugins are invoked from highest priority
 * to lowest, with the first plugin that claims a setting being assigned
 * ownership, and all lower priority plugins being asked to delete the
 * setting in question.
 *
 * Returns: the priority of this plugin
 **/
gint
mcp_account_storage_priority (McpAccountStorage *storage)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  g_return_val_if_fail (iface != NULL, -1);

  return iface->priority;
}

/**
 * mcp_account_storage_get_attribute:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 * @account: the unique name of the account
 * @attribute: the name of an attribute, e.g. "DisplayName"
 * @type: the expected type of @attribute, as a hint for
 *  legacy account storage plugins that do not store attributes' types
 * @flags: (allow-none) (out): used to return attribute flags
 *
 * Retrieve an attribute.
 *
 * There is no default implementation.
 * All account storage plugins must override this method.
 *
 * The returned variant does not necessarily have to match @type:
 * Mission Control will coerce it to an appropriate type if required. In
 * particular, plugins that store strongly-typed attributes may return
 * the stored type, not the expected type, if they differ.
 *
 * Returns: (transfer full): the value of the attribute, or %NULL if it
 *  is not present
 */
GVariant *
mcp_account_storage_get_attribute (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account,
    const gchar *attribute,
    const GVariantType *type,
    McpAttributeFlags *flags)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "%s.%s (type '%.*s')", account, attribute,
      (int) g_variant_type_get_string_length (type),
      g_variant_type_peek_string (type));

  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->get_attribute != NULL, FALSE);

  return iface->get_attribute (storage, am, account, attribute, type, flags);
}

/**
 * mcp_account_storage_get_parameter:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 * @account: the unique name of the account
 * @parameter: the name of a parameter, e.g. "require-encryption"
 * @type: (allow-none): the expected type of @parameter, as a hint for
 *  legacy account storage plugins that do not store parameters' types
 * @flags: (allow-none) (out): used to return parameter flags
 *
 * Retrieve a parameter.
 *
 * There is no default implementation.
 * All account storage plugins must override this method.
 *
 * The returned variant does not necessarily have to match @type:
 * Mission Control will coerce it to an appropriate type if required. In
 * particular, plugins that store strongly-typed parameters may return
 * the stored type, not the expected type, if they differ.
 *
 * If @type is %NULL, the plugin must return the parameter with its stored
 * type, or return %NULL if the type is not stored.
 *
 * Returns: (transfer full): the value of the parameter, or %NULL if it
 *  is not present
 */
GVariant *
mcp_account_storage_get_parameter (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account,
    const gchar *parameter,
    const GVariantType *type,
    McpParameterFlags *flags)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  if (type == NULL)
    SDEBUG (storage, "%s.%s (if type is stored)", account, parameter);
  else
    SDEBUG (storage, "%s.%s (type '%.*s')", account, parameter,
        (int) g_variant_type_get_string_length (type),
        g_variant_type_peek_string (type));

  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->get_parameter != NULL, FALSE);

  return iface->get_parameter (storage, am, account, parameter, type, flags);
}

/**
 * mcp_account_storage_list_typed_parameters:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 * @account: the unique name of the account
 *
 * List the names of all parameters whose corresponding types are known.
 *
 * Ideally, all parameters are <firstterm>typed parameters</firstterm>, whose
 * types are stored alongside the values. This function produces
 * those as its return value.
 *
 * However, the Mission Control API has not traditionally required
 * account-storage backends to store parameters' types, so some backends
 * will contain <firstterm>untyped parameters</firstterm>,
 * returned by mcp_account_storage_list_untyped_parameters().
 *
 * This method is mandatory to implement.
 *
 * Returns: (array zero-terminated=1) (transfer full) (element-type utf8): a #GStrv
 *  containing the typed parameters; %NULL or empty if there are no
 *  typed parameters
 */
gchar **
mcp_account_storage_list_typed_parameters (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "%s", account);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->list_typed_parameters != NULL, NULL);

  return iface->list_typed_parameters (storage, am, account);
}

/**
 * mcp_account_storage_list_untyped_parameters:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 * @account: the unique name of the account
 *
 * List the names of all parameters whose types are unknown.
 * The values are not listed, because interpreting the value
 * correctly requires a type.
 *
 * See mcp_account_storage_list_typed_parameters() for more on
 * typed vs. untyped parameters.
 *
 * The default implementation just returns %NULL, and is appropriate
 * for "legacy-free" backends that store a type with every parameter.
 *
 * Returns: (array zero-terminated=1) (transfer full) (element-type utf8): a #GStrv
 *  containing the untyped parameters; %NULL or empty if there are no
 *  untyped parameters
 */
gchar **
mcp_account_storage_list_untyped_parameters (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "%s", account);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->list_untyped_parameters != NULL, NULL);

  return iface->list_untyped_parameters (storage, am, account);
}

/**
 * mcp_account_storage_set_attribute:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 * @account: the unique name of the account
 * @attribute: the name of an attribute, e.g. "DisplayName"
 * @value: (allow-none): a value to associate with @attribute,
 *  or %NULL to delete
 * @flags: flags influencing how the attribute is to be stored
 *
 * Store an attribute.
 *
 * The plugin is expected to either quickly and synchronously
 * update its internal cache of values with @value, or to
 * decline to store the attribute.
 *
 * The plugin is not expected to write to its long term storage
 * at this point.
 *
 * There is a default implementation, which just returns %FALSE for read-only
 * storage plugins.
 *
 * Returns: %TRUE if the attribute was claimed, %FALSE otherwise
 *
 * Since: 5.15.0
 */
McpAccountStorageSetResult
mcp_account_storage_set_attribute (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account,
    const gchar *attribute,
    GVariant *value,
    McpAttributeFlags flags)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "%s.%s (type '%s')", account, attribute,
      value == NULL ? "null" : g_variant_get_type_string (value));

  g_return_val_if_fail (iface != NULL,
      MCP_ACCOUNT_STORAGE_SET_RESULT_FAILED);
  g_return_val_if_fail (iface->set_attribute != NULL,
      MCP_ACCOUNT_STORAGE_SET_RESULT_FAILED);

  return iface->set_attribute (storage, am, account, attribute, value, flags);
}

/**
 * mcp_account_storage_set_parameter:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 * @account: the unique name of the account
 * @parameter: the name of a parameter, e.g. "account" (note that there
 *  is no "param-" prefix here)
 * @value: (allow-none): a value to associate with @parameter,
 *  or %NULL to delete
 * @flags: flags influencing how the parameter is to be stored
 *
 * Store a parameter.
 *
 * The plugin is expected to either quickly and synchronously
 * update its internal cache of values with @value, or to
 * decline to store the parameter.
 *
 * The plugin is not expected to write to its long term storage
 * at this point.
 *
 * There is a default implementation, which just returns %FALSE for read-only
 * storage plugins.
 *
 * Returns: %TRUE if the parameter was claimed, %FALSE otherwise
 *
 * Since: 5.15.0
 */
McpAccountStorageSetResult
mcp_account_storage_set_parameter (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account,
    const gchar *parameter,
    GVariant *value,
    McpParameterFlags flags)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "%s.%s (type '%s')", account, parameter,
      value == NULL ? "null" : g_variant_get_type_string (value));

  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface != NULL,
      MCP_ACCOUNT_STORAGE_SET_RESULT_FAILED);
  g_return_val_if_fail (iface->set_parameter != NULL,
      MCP_ACCOUNT_STORAGE_SET_RESULT_FAILED);

  return iface->set_parameter (storage, am, account, parameter, value, flags);
}

/**
 * McpAccountStorageCreate:
 * @storage: an #McpAccountStorage instance
 * @am: an object which can be used to call back into the account manager
 * @manager: the name of the manager
 * @protocol: the name of the protocol
 * @params: A gchar * / GValue * hash table of account parameters
 * @error: a GError to fill
 *
 * An implementation of mcp_account_storage_create()
 *
 * Returns: (transfer full): the account name or %NULL
 */

/**
 * mcp_account_storage_create:
 * @storage: an #McpAccountStorage instance
 * @am: an object which can be used to call back into the account manager
 * @manager: the name of the manager
 * @protocol: the name of the protocol
 * @identification: a normalized form of the account name, or "account"
 *  if nothing is suitable (e.g. for telepathy-salut)
 * @error: a GError to fill
 *
 * Inform the plugin that a new account is being created. @manager, @protocol
 * and @identification are given to help determining the account's unique name,
 * but does not need to be stored on the account yet, mcp_account_storage_set()
 * and mcp_account_storage_commit() will be called later.
 *
 * It is recommended to use mcp_account_manager_get_unique_name() to create the
 * unique name, but it's not mandatory. One could base the unique name on an
 * internal storage identifier, prefixed with the provider's name
 * (e.g. goa__1234).
 *
 * #McpAccountStorage::created signal should not be emitted for this account,
 * not even when mcp_account_storage_commit() will be called.
 *
 * The default implementation just returns %NULL, and is appropriate for
 * read-only storage.
 *
 * Since Mission Control 5.17, all storage plugins in which new accounts
 * can be created by Mission Control must implement this method.
 * Previously, it was not mandatory.
 *
 * Returns: (transfer full): the newly allocated account name, which should
 *  be freed once the caller is done with it, or %NULL if that couldn't
 *  be done.
 */
gchar *
mcp_account_storage_create (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *manager,
    const gchar *protocol,
    const gchar *identification,
    GError **error)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "%s/%s \"%s\"", manager, protocol, identification);

  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->create != NULL, NULL);

  return iface->create (storage, am, manager, protocol, identification, error);
}

/**
 * mcp_account_storage_delete_async:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 * @account: the unique name of the account
 * @cancellable: (allow-none): optionally used to (try to) cancel the operation
 * @callback: called on success or failure
 * @user_data: data for @callback
 *
 * Delete the account @account, and commit the change,
 * emitting #McpAccountStorage::deleted afterwards.
 *
 * Unlike the 'delete' virtual method in earlier MC versions, this
 * function is expected to commit the change to long-term storage,
 * is expected to emit #McpAccountStorage::deleted, and is
 * not called for the deletion of individual attributes or parameters.
 *
 * The default implementation just returns failure (asynchronously),
 * and is appropriate for read-only storage.
 *
 * Implementations that override delete_async must also override
 * delete_finish.
 */
void
mcp_account_storage_delete_async (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "%s", account);

  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->delete_async != NULL);

  iface->delete_async (storage, am, account, cancellable, callback, user_data);
}

/**
 * mcp_account_storage_delete_finish:
 * @storage: an #McpAccountStorage instance
 * @res: the result of mcp_account_storage_delete_async()
 * @error: used to raise an error if %FALSE is returned
 *
 * Process the result of mcp_account_storage_delete_async().
 *
 * Returns: %TRUE on success, %FALSE if the account could not be deleted
 */
gboolean
mcp_account_storage_delete_finish (McpAccountStorage *storage,
    GAsyncResult *result,
    GError **error)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "");
  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->delete_finish != NULL, FALSE);

  return iface->delete_finish (storage, result, error);
}

/**
 * McpAccountStorageCommitFunc:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 * @account: the unique suffix of an account's object path
 *
 * An implementation of mcp_account_storage_commit().
 *
 * Returns: %TRUE if the commit process was started successfully
 */

/**
 * mcp_account_storage_commit:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 * @account: the unique suffix of an account's object path
 *
 * The plugin is expected to write its cache to long term storage,
 * deleting, adding or updating entries in said storage as needed.
 *
 * This call is expected to return promptly, but the plugin is
 * not required to have finished its commit operation when it returns,
 * merely to have started the operation.
 *
 * The default implementation just returns %FALSE, and is appropriate for
 * read-only storage.
 *
 * Mission Control 5.17+ no longer requires plugins to cope with
 * account == NULL.
 *
 * Returns: %TRUE if the commit process was started (but not necessarily
 * completed) successfully; %FALSE if there was a problem that was immediately
 * obvious.
 */
gboolean
mcp_account_storage_commit (McpAccountStorage *storage,
    McpAccountManager *am,
    const gchar *account)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "called for %s", account ? account : "<all accounts>");
  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->commit != NULL, FALSE);

  return iface->commit (storage, am, account);
}

/**
 * McpAccountStorageListFunc:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 *
 * An implementation of mcp_account_storage_list().
 *
 * Returns: (transfer full): a list of account names
 */

/**
 * mcp_account_storage_list:
 * @storage: an #McpAccountStorage instance
 * @am: an #McpAccountManager instance
 *
 * Load details of every account stored by this plugin into an in-memory cache
 * so that it can respond to requests promptly.
 *
 * This method is called only at initialisation time, before the dbus name
 * has been claimed, and is the only one permitted to block.
 *
 * There is no default implementation. All implementations of this interface
 * must override this method.
 *
 * Returns: (element-type utf8) (transfer full): a list of account names that
 * the plugin has settings for. The account names should be freed with
 * g_free(), and the list with g_list_free(), when the caller is done with
 * them.
 */
GList *
mcp_account_storage_list (McpAccountStorage *storage,
    McpAccountManager *am)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "");
  g_return_val_if_fail (iface != NULL, NULL);
  g_return_val_if_fail (iface->list != NULL, NULL);

  return iface->list (storage, am);
}

/**
 * McpAccountStorageGetIdentifierFunc:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account
 * @identifier: (out caller-allocates): a zero-filled #GValue whose type
 *  can be sent over D-Bus by dbus-glib, to hold the identifier.
 *
 * An implementation of mcp_account_storage_get_identifier().
 */

/**
 * mcp_account_storage_get_identifier:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account
 * @identifier: (out caller-allocates): a zero-filled #GValue whose type
 *  can be sent over D-Bus by dbus-glib, to hold the identifier.
 *
 * Get the storage-specific identifier for this account. The type is variant,
 * hence the GValue.
 *
 * The default implementation returns @account as a %G_TYPE_STRING.
 *
 * This method will only be called for the storage plugin that "owns"
 * the account.
 */
void
mcp_account_storage_get_identifier (McpAccountStorage *storage,
    const gchar *account,
    GValue *identifier)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "%s", account);
  g_return_if_fail (iface != NULL);
  g_return_if_fail (iface->get_identifier != NULL);
  g_return_if_fail (identifier != NULL);
  g_return_if_fail (!G_IS_VALUE (identifier));

  iface->get_identifier (storage, account, identifier);
}

/**
 * McpAccountStorageGetAdditionalInfoFunc
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account
 *
 * An implementation of mcp_account_storage_get_identifier().
 *
 * Returns: (transfer container) (element-type utf8 GObject.Value): additional
 *  storage-specific information
 */

/**
 * mcp_account_storage_get_additional_info:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account
 *
 * Return additional storage-specific information about this account, which is
 * made available on D-Bus but not otherwise interpreted by Mission Control.
 *
 * This method will only be called for the storage plugin that "owns"
 * the account.
 *
 * The default implementation returns an empty map.
 *
 * Returns: (transfer container) (element-type utf8 GObject.Value): additional
 *  storage-specific information, which must not be %NULL
 */
GHashTable *
mcp_account_storage_get_additional_info (McpAccountStorage *storage,
    const gchar *account)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "%s", account);
  g_return_val_if_fail (iface != NULL, FALSE);
  g_return_val_if_fail (iface->get_additional_info != NULL, FALSE);

  return iface->get_additional_info (storage, account);
}

/**
 * McpAccountStorageGetRestrictionsFunc
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account
 *
 * An implementation of mcp_account_storage_get_restrictions().
 *
 * Returns: any restrictions that apply to this account.
 */

/**
 * mcp_account_storage_get_restrictions:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account
 *
 * This method will only be called for the storage plugin that "owns"
 * the account.
 *
 * The default implementation returns 0, i.e. no restrictions.
 *
 * Returns: a bitmask of %TpStorageRestrictionFlags with the restrictions to
 *  account storage.
 */
TpStorageRestrictionFlags
mcp_account_storage_get_restrictions (McpAccountStorage *storage,
    const gchar *account)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  SDEBUG (storage, "%s", account);
  g_return_val_if_fail (iface != NULL, 0);
  g_return_val_if_fail (iface->get_restrictions != NULL, 0);

  return iface->get_restrictions (storage, account);
}

/**
 * mcp_account_storage_name:
 * @storage: an #McpAccountStorage instance
 *
 * <!-- nothing else to say -->
 *
 * Returns: the plugin's name (for logging etc)
 */
const gchar *
mcp_account_storage_name (McpAccountStorage *storage)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  g_return_val_if_fail (iface != NULL, NULL);

  return iface->name;
}

/**
 * mcp_account_storage_description:
 * @storage: an #McpAccountStorage instance
 *
 * <!-- nothing else to say -->
 *
 * Returns: the plugin's description (for logging etc)
 */
const gchar *
mcp_account_storage_description (McpAccountStorage *storage)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  g_return_val_if_fail (iface != NULL, NULL);

  return iface->desc;
}

/**
 * mcp_account_storage_provider:
 * @storage: an #McpAccountStorage instance
 *
 * <!-- nothing else to say -->
 *
 * Returns: a D-Bus namespaced name for this plugin, or "" if none
 *  was provided in #McpAccountStorageIface.provider.
 */
const gchar *
mcp_account_storage_provider (McpAccountStorage *storage)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  g_return_val_if_fail (iface != NULL, NULL);

  return iface->provider != NULL ? iface->provider : "";
}

/**
 * mcp_account_storage_emit_created:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the created account
 *
 * Emits the #McpAccountStorage::created signal.
 */
void
mcp_account_storage_emit_created (McpAccountStorage *storage,
    const gchar *account)
{
  SDEBUG (storage, "%s", account);
  g_signal_emit (storage, signals[CREATED], 0, account);
}

/**
 * mcp_account_storage_emit_altered_one:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the altered account
 * @key: the key of the altered property: either an attribute name
 *  like "DisplayName", or "param-" plus a parameter name like "account"
 *
 * Emits the #McpAccountStorage::altered-one signal
 */
void
mcp_account_storage_emit_altered_one (McpAccountStorage *storage,
    const gchar *account,
    const gchar *key)
{
  SDEBUG (storage, "%s", account);
  g_signal_emit (storage, signals[ALTERED_ONE], 0, account, key);
}

/**
 * mcp_account_storage_emit_deleted:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the deleted account
 *
 * Emits the #McpAccountStorage::deleted signal
 */
void
mcp_account_storage_emit_deleted (McpAccountStorage *storage,
    const gchar *account)
{
  SDEBUG (storage, "%s", account);
  g_signal_emit (storage, signals[DELETED], 0, account);
}

/**
 * mcp_account_storage_emit_toggled:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account
 * @enabled: %TRUE if the account is now enabled
 *
 * Emits ::toggled signal
 */
void
mcp_account_storage_emit_toggled (McpAccountStorage *storage,
    const gchar *account,
    gboolean enabled)
{
  SDEBUG (storage, "%s: Enabled=%s", account, enabled ? "True" : "False");
  g_signal_emit (storage, signals[TOGGLED], 0, account, enabled);
}

/**
 * mcp_account_storage_emit_reconnect:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account to reconnect
 *
 * Emits ::reconnect signal
 */
void
mcp_account_storage_emit_reconnect (McpAccountStorage *storage,
    const gchar *account)
{
  SDEBUG (storage, "%s", account);
  g_signal_emit (storage, signals[RECONNECT], 0, account);
}

/**
 * mcp_account_storage_get_flags:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account to inspect
 *
 * Get the backend's features and capabilities. The default implementation
 * returns %MCP_ACCOUNT_STORAGE_FLAG_NONE. Additionally providing
 * %MCP_ACCOUNT_STORAGE_FLAG_STORES_TYPES is strongly recommended.
 *
 * Returns: a bitmask of API features that apply to @account
 */
McpAccountStorageFlags
mcp_account_storage_get_flags (McpAccountStorage *storage,
    const gchar *account)
{
  McpAccountStorageIface *iface = MCP_ACCOUNT_STORAGE_GET_IFACE (storage);

  g_return_val_if_fail (iface != NULL, MCP_ACCOUNT_STORAGE_FLAG_NONE);
  g_return_val_if_fail (iface->get_flags != NULL,
      MCP_ACCOUNT_STORAGE_FLAG_NONE);

  return iface->get_flags (storage, account);
}

/**
 * mcp_account_storage_has_all_flags:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account to inspect
 * @require_all: bitwise "or" of zero or more flags
 *
 * Return whether this account has all of the specified flags,
 * according to mcp_account_storage_get_flags().
 *
 * If @require_all is 0, the result will always be %TRUE
 * (the account has all of the flags in the empty set).
 *
 * Returns: %TRUE if @account has every flag in @require_all
 */
gboolean
mcp_account_storage_has_all_flags (McpAccountStorage *storage,
    const gchar *account,
    McpAccountStorageFlags require_all)
{
  return ((mcp_account_storage_get_flags (storage, account) & require_all) ==
    require_all);
}


/**
 * mcp_account_storage_has_any_flag:
 * @storage: an #McpAccountStorage instance
 * @account: the unique name of the account to inspect
 * @require_one: bitwise "or" of one or more flags
 *
 * Return whether this account has at least one of the required flags,
 * according to mcp_account_storage_get_flags().
 *
 * If @require_one is 0, the result will always be %FALSE
 * (it is not true that the account has at least one of the flags
 * in the empty set).
 *
 * Returns: %TRUE if @account has every flag in @require_all
 */
gboolean
mcp_account_storage_has_any_flag (McpAccountStorage *storage,
    const gchar *account,
    McpAccountStorageFlags require_one)
{
  return ((mcp_account_storage_get_flags (storage, account) & require_one)
      != 0);
}
