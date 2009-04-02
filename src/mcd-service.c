/* vi: set et sw=4 ts=8 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 8 -*- */
/*
 * This file is part of mission-control
 *
 * Copyright (C) 2007 Nokia Corporation. 
 *
 * Contact: Naba Kumar  <naba.kumar@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
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
 *
 */

/**
 * SECTION:mcd-service
 * @title: McdService
 * @short_description: Service interface implementation
 * @see_also: 
 * @stability: Unstable
 * @include: mcd-service.h
 * 
 * It is the frontline interface object that exposes mission-control to outside
 * world through a dbus interface. It basically subclasses McdMaster and
 * wraps up everything inside it and translate them into mission-control
 * dbus interface.
 */

#include <dbus/dbus.h>
#include <string.h>
#include <dlfcn.h>
#include <sched.h>
#include <stdlib.h>

#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <gconf/gconf-client.h>

#include "mcd-signals-marshal.h"
#include "mcd-dispatcher.h"
#include "mcd-dispatcher-context.h"
#include "mcd-account-compat.h"
#include "mcd-connection.h"
#include "mcd-service.h"

#include <libmcclient/mc-errors.h>

/* DBus service specifics */
#define MISSION_CONTROL_DBUS_SERVICE "org.freedesktop.Telepathy.MissionControl"
#define MISSION_CONTROL_DBUS_OBJECT  "/org/freedesktop/Telepathy/MissionControl"
#define MISSION_CONTROL_DBUS_IFACE   "org.freedesktop.Telepathy.MissionControl"

#define LAST_MC_PRESENCE (TP_CONNECTION_PRESENCE_TYPE_BUSY + 1)

typedef enum {
    MC_STATUS_DISCONNECTED,
    MC_STATUS_CONNECTING,
    MC_STATUS_CONNECTED,
} McStatus;

static GObjectClass *parent_class = NULL;

#define MCD_OBJECT_PRIV(mission) (G_TYPE_INSTANCE_GET_PRIVATE ((mission), \
				   MCD_TYPE_SERVICE, \
				   McdServicePrivate))

G_DEFINE_TYPE (McdService, mcd_service, MCD_TYPE_MASTER);

/* Private */

typedef struct _McdServicePrivate
{
    McdPresenceFrame *presence_frame;
    McdDispatcher *dispatcher;

    McStatus last_status;

    gboolean is_disposed;
} McdServicePrivate;

static void
mcd_service_obtain_bus_name (McdService * obj)
{
    DBusError error;
    DBusGConnection *connection;
    
    g_object_get (obj, "dbus-connection", &connection, NULL);
    
    dbus_error_init (&error);
    
    DEBUG ("Requesting MC dbus service");
    
    dbus_bus_request_name (dbus_g_connection_get_connection (connection),
			   MISSION_CONTROL_DBUS_SERVICE, 0, &error);
    if (dbus_error_is_set (&error))
    {
	g_error ("Service name '%s' is already in use - request failed",
		 MISSION_CONTROL_DBUS_SERVICE);
	dbus_error_free (&error);
    }
}

static void
_on_presence_requested (McdPresenceFrame * presence_frame,
			TpConnectionPresenceType presence,
			gchar * presence_message, McdService * obj)
{
    /* Begin shutdown if it is offline request */
    if (presence == TP_CONNECTION_PRESENCE_TYPE_OFFLINE ||
	presence == TP_CONNECTION_PRESENCE_TYPE_UNSET)
	mcd_controller_shutdown (MCD_CONTROLLER (obj),
				 "Offline presence requested");
    else
	/* If there is a presence request, make sure shutdown is canceled */
	mcd_controller_cancel_shutdown (MCD_CONTROLLER (obj));
    
    /* Emit the AccountStatusChanged signal */
#ifndef NO_NEW_PRESENCE_SIGNALS
    g_signal_emit_by_name (G_OBJECT (obj),
			   "presence-requested", presence, presence_message);
#endif
}

static void
_on_presence_actual (McdPresenceFrame * presence_frame,
		     TpConnectionPresenceType presence,
		     gchar * presence_message, McdService * obj)
{
    /* Emit the AccountStatusChanged signal */
#ifndef NO_NEW_PRESENCE_SIGNALS
    g_signal_emit_by_name (G_OBJECT (obj), "presence-changed", presence, presence_message);
#endif
}

static void
mcd_service_disconnect (McdMission *mission)
{
    MCD_MISSION_CLASS (mcd_service_parent_class)->disconnect (mission);
    mcd_controller_shutdown (MCD_CONTROLLER (mission), "Disconnected");
}

static void
_on_status_actual (McdPresenceFrame *presence_frame,
		   TpConnectionStatus tpstatus,
		   McdService *service)
{
    McdServicePrivate *priv = MCD_OBJECT_PRIV (service);
    TpConnectionPresenceType req_presence;
    McStatus status;

    req_presence = mcd_presence_frame_get_requested_presence (presence_frame);
    switch (tpstatus)
    {
    case TP_CONNECTION_STATUS_CONNECTED:
	status = MC_STATUS_CONNECTED;
	break;
    case TP_CONNECTION_STATUS_CONNECTING:
	status = MC_STATUS_CONNECTING;
	break;
    case TP_CONNECTION_STATUS_DISCONNECTED:
	status = MC_STATUS_DISCONNECTED;
	break;
    default:
	status = MC_STATUS_DISCONNECTED;
	g_warning ("Unexpected status %d", tpstatus);
    }

    if (status != priv->last_status)
    {
	priv->last_status = status;
    }
}

static void
mcd_dispose (GObject * obj)
{
    McdServicePrivate *priv;
    McdService *self = MCD_OBJECT (obj);

    priv = MCD_OBJECT_PRIV (self);

    if (priv->is_disposed)
    {
	return;
    }

    priv->is_disposed = TRUE;

    if (priv->presence_frame)
    {
	g_signal_handlers_disconnect_by_func (priv->presence_frame,
					      _on_presence_requested, self);
	g_signal_handlers_disconnect_by_func (priv->presence_frame,
					      _on_presence_actual, self);
	g_signal_handlers_disconnect_by_func (priv->presence_frame,
					      _on_status_actual, self);
	g_object_unref (priv->presence_frame);
    }

    if (self->main_loop)
    {
	g_main_loop_quit (self->main_loop);
	g_main_loop_unref (self->main_loop);
	self->main_loop = NULL;
    }

    if (G_OBJECT_CLASS (parent_class)->dispose)
    {
	G_OBJECT_CLASS (parent_class)->dispose (obj);
    }
}

static void
mcd_service_constructed (GObject *obj)
{
    McdServicePrivate *priv = MCD_OBJECT_PRIV (obj);

    DEBUG ("called");
    g_object_get (obj,
                  "presence-frame", &priv->presence_frame,
		  NULL);

    /* Setup presence signals */
    g_signal_connect (priv->presence_frame, "presence-requested",
		      G_CALLBACK (_on_presence_requested), obj);
    g_signal_connect (priv->presence_frame, "presence-actual",
		      G_CALLBACK (_on_presence_actual), obj);

    /* Setup dispatcher signals */

    mcd_service_obtain_bus_name (MCD_OBJECT (obj));
    mcd_debug_print_tree (obj);

    if (G_OBJECT_CLASS (parent_class)->constructed)
	G_OBJECT_CLASS (parent_class)->constructed (obj);
}

static void
mcd_service_init (McdService * obj)
{
    McdServicePrivate *priv = MCD_OBJECT_PRIV (obj);

    obj->main_loop = g_main_loop_new (NULL, FALSE);

    priv->last_status = -1;
    DEBUG ("called");
}

static void
mcd_service_class_init (McdServiceClass * self)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (self);
    McdMissionClass *mission_class = MCD_MISSION_CLASS (self);

    parent_class = g_type_class_peek_parent (self);
    gobject_class->constructed = mcd_service_constructed;
    gobject_class->dispose = mcd_dispose;
    mission_class->disconnect = mcd_service_disconnect;

    g_type_class_add_private (gobject_class, sizeof (McdServicePrivate));

#ifndef NO_NEW_PRESENCE_SIGNALS
    /* PresenceRequested signal */
    g_signal_new ("presence-requested",
		  G_OBJECT_CLASS_TYPE (self),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		  0,
		  NULL, NULL, _mcd_marshal_VOID__UINT_STRING,
		  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING);
#endif
#ifndef NO_NEW_PRESENCE_SIGNALS
    /* PresenceChanged signal */
    g_signal_new ("presence-changed",
		  G_OBJECT_CLASS_TYPE (self),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		  0,
		  NULL, NULL, _mcd_marshal_VOID__UINT_STRING,
		  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING);
#endif
}

McdService *
mcd_service_new (void)
{
    McdService *obj;
    DBusGConnection *dbus_connection;
    TpDBusDaemon *dbus_daemon;
    GError *error = NULL;

    /* Initialize DBus connection */
    dbus_connection = dbus_g_bus_get (DBUS_BUS_STARTER, &error);
    if (dbus_connection == NULL)
    {
	g_printerr ("Failed to open connection to bus: %s", error->message);
	g_error_free (error);
	return NULL;
    }
    dbus_daemon = tp_dbus_daemon_new (dbus_connection);
    obj = g_object_new (MCD_TYPE_SERVICE,
			"dbus-daemon", dbus_daemon,
			NULL);
    g_object_unref (dbus_daemon);
    return obj;
}

void
mcd_service_run (McdService * self)
{
    g_main_loop_run (self->main_loop);
}

