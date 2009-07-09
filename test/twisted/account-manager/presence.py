# Copyright (C) 2009 Nokia Corporation
# Copyright (C) 2009 Collabora Ltd.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301 USA

import dbus
import dbus
import dbus.service

from servicetest import EventPattern, tp_name_prefix, tp_path_prefix, \
        call_async, sync_dbus
from mctest import exec_test, create_fakecm_account, enable_fakecm_account
import constants as cs

def test(q, bus, mc):
    account_manager = bus.get_object(cs.AM, cs.AM_PATH)
    account_manager_iface = dbus.Interface(account_manager, cs.AM)

    params = dbus.Dictionary({"account": "jc.denton@example.com",
        "password": "ionstorm"}, signature='sv')
    (cm_name_ref, account) = create_fakecm_account(q, bus, mc, params)

    account_iface = dbus.Interface(account, cs.ACCOUNT)
    account_props = dbus.Interface(account, cs.PROPERTIES_IFACE)

    presence = dbus.Struct((dbus.UInt32(cs.PRESENCE_TYPE_BUSY), 'busy',
            'Fighting conspiracies'), signature='uss')

    conn, e = enable_fakecm_account(q, bus, mc, account, params,
            has_presence=True,
            requested_presence=presence,
            expect_after_connect=[
                EventPattern('dbus-method-call',
                    interface=cs.CONN_IFACE_SIMPLE_PRESENCE,
                    method='SetPresence',
                    args=list(presence[1:]),
                    handled=True),
                ])

    q.dbus_emit(conn.object_path, cs.CONN_IFACE_SIMPLE_PRESENCE,
            'PresencesChanged', {conn.self_handle: presence},
            signature='a{u(uss)}')

    q.expect('dbus-signal', path=account.object_path,
            interface=cs.ACCOUNT, signal='AccountPropertyChanged',
            predicate=lambda e: e.args[0].get('CurrentPresence') == presence)

    # Change requested presence after going online
    presence = dbus.Struct((dbus.UInt32(cs.PRESENCE_TYPE_AWAY), 'away',
            'In Hong Kong'), signature='uss')
    call_async(q, account_props, 'Set', cs.ACCOUNT, 'RequestedPresence',
            presence)

    e = q.expect('dbus-method-call',
        interface=cs.CONN_IFACE_SIMPLE_PRESENCE, method='SetPresence',
        args=list(presence[1:]),
        handled=True)

    q.expect('dbus-signal', path=account.object_path,
            interface=cs.ACCOUNT, signal='AccountPropertyChanged',
            predicate=lambda e: e.args[0].get('CurrentPresence') == presence)

    # Setting RequestedPresence=RequestedPresence causes a (possibly redundant)
    # call to the CM, so we get any side-effects there might be, either in the
    # CM or in MC (e.g. asking connectivity services to go online). However,
    # AccountPropertyChanged is not emitted for RequestedPresence.

    sync_dbus(bus, q, mc)
    events = [EventPattern('dbus-signal', signal='AccountPropertyChanged',
        predicate=lambda e: e.args[0].get('RequestedPresence') is not None)]
    q.forbid_events(events)

    presence = dbus.Struct((dbus.UInt32(cs.PRESENCE_TYPE_AWAY), 'away',
            'In Hong Kong'), signature='uss')
    call_async(q, account_props, 'Set', cs.ACCOUNT, 'RequestedPresence',
            presence)

    e = q.expect('dbus-method-call',
        interface=cs.CONN_IFACE_SIMPLE_PRESENCE, method='SetPresence',
        args=list(presence[1:]),
        handled=True)

    sync_dbus(bus, q, mc)
    q.unforbid_events(events)

if __name__ == '__main__':
    exec_test(test, {})