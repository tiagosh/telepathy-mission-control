
"""
Infrastructure code for testing Mission Control
"""

from twisted.internet import glib2reactor
from twisted.internet.protocol import Protocol, Factory, ClientFactory
glib2reactor.install()

import pprint
import unittest

import dbus
import dbus.lowlevel
import dbus.glib

from twisted.internet import reactor

tp_name_prefix = 'org.freedesktop.Telepathy'
tp_path_prefix = '/org/freedesktop/Telepathy'

class Event:
    def __init__(self, type, **kw):
        self.__dict__.update(kw)
        self.type = type

def format_event(event):
    ret = ['- type %s' % event.type]

    for key in dir(event):
        if key != 'type' and not key.startswith('_'):
            ret.append('- %s: %s' % (
                key, pprint.pformat(getattr(event, key))))

            if key == 'error':
                ret.append('%s' % getattr(event, key))

    return ret

class EventPattern:
    def __init__(self, type, **properties):
        self.type = type
        self.predicate = lambda x: True
        if 'predicate' in properties:
            self.predicate = properties['predicate']
            del properties['predicate']
        self.properties = properties

    def match(self, event):
        if event.type != self.type:
            return False

        for key, value in self.properties.iteritems():
            try:
                if getattr(event, key) != value:
                    return False
            except AttributeError:
                return False

        if self.predicate(event):
            return True

        return False


class TimeoutError(Exception):
    pass

class BaseEventQueue:
    """Abstract event queue base class.

    Implement the wait() method to have something that works.
    """

    def __init__(self, timeout=None):
        self.verbose = False
        self.past_events = []
        self.forbidden_events = set()

        if timeout is None:
            self.timeout = 5
        else:
            self.timeout = timeout

    def log(self, s):
        if self.verbose:
            print s

    def flush_past_events(self):
        self.past_events = []

    def expect_racy(self, type, **kw):
        pattern = EventPattern(type, **kw)

        for event in self.past_events:
            if pattern.match(event):
                self.log('past event handled')
                map(self.log, format_event(event))
                self.log('')
                self.past_events.remove(event)
                return event

        return self.expect(type, **kw)

    def forbid_events(self, patterns):
        """
        Add patterns (an iterable of EventPattern) to the set of forbidden
        events. If a forbidden event occurs during an expect or expect_many,
        the test will fail.
        """
        self.forbidden_events.update(set(patterns))

    def unforbid_events(self, patterns):
        """
        Remove 'patterns' (an iterable of EventPattern) from the set of
        forbidden events. These must be the same EventPattern pointers that
        were passed to forbid_events.
        """
        self.forbidden_events.difference_update(set(patterns))

    def _check_forbidden(self, event):
        for e in self.forbidden_events:
            if e.match(event):
                print "forbidden event occurred:"
                for x in format_event(event):
                    print x
                assert False

    def expect(self, type, **kw):
        pattern = EventPattern(type, **kw)

        while True:
            event = self.wait()
            self.log('got event:')
            map(self.log, format_event(event))

            self._check_forbidden(event)

            if pattern.match(event):
                self.log('handled')
                self.log('')
                return event

            self.past_events.append(event)
            self.log('not handled')
            self.log('')

    def expect_many(self, *patterns):
        ret = [None] * len(patterns)

        while None in ret:
            event = self.wait()
            self.log('got event:')
            map(self.log, format_event(event))

            self._check_forbidden(event)

            for i, pattern in enumerate(patterns):
                if pattern.match(event):
                    self.log('handled')
                    self.log('')
                    ret[i] = event
                    break
            else:
                self.past_events.append(event)
                self.log('not handled')
                self.log('')

        return ret

    def demand(self, type, **kw):
        pattern = EventPattern(type, **kw)

        event = self.wait()
        self.log('got event:')
        map(self.log, format_event(event))

        if pattern.match(event):
            self.log('handled')
            self.log('')
            return event

        self.log('not handled')
        raise RuntimeError('expected %r, got %r' % (pattern, event))

class IteratingEventQueue(BaseEventQueue):
    """Event queue that works by iterating the Twisted reactor."""

    def __init__(self, timeout=None):
        BaseEventQueue.__init__(self, timeout)
        self.events = []
        self._dbus_method_impls = []
        self._bus = None

    def wait(self):
        stop = [False]

        def later():
            stop[0] = True

        delayed_call = reactor.callLater(self.timeout, later)

        while (not self.events) and (not stop[0]):
            reactor.iterate(0.1)

        if self.events:
            delayed_call.cancel()
            return self.events.pop(0)
        else:
            raise TimeoutError

    def append(self, event):
        self.events.append(event)

    # compatibility
    handle_event = append

    def add_dbus_method_impl(self, cb, **kwargs):
        self._dbus_method_impls.append(
                (EventPattern('dbus-method-call', **kwargs), cb))

    def dbus_emit(self, path, iface, name, *a, **k):
        assert 'signature' in k, k
        message = dbus.lowlevel.SignalMessage(path, iface, name)
        message.append(*a, **k)
        self._bus.send_message(message)

    def dbus_return(self, in_reply_to, *a, **k):
        assert 'signature' in k, k
        reply = dbus.lowlevel.MethodReturnMessage(in_reply_to)
        reply.append(*a, **k)
        self._bus.send_message(reply)

    def dbus_raise(self, in_reply_to, name, message=None):
        reply = dbus.lowlevel.ErrorMessage(in_reply_to, name, message)
        self._bus.send_message(reply)

    def attach_to_bus(self, bus):
        assert self._bus is None, self._bus
        self._bus = bus
        self._dbus_filter_bound_method = self._dbus_filter
        self._bus.add_message_filter(self._dbus_filter_bound_method)

    def cleanup(self):
        if self._bus is not None:
            self._bus.remove_message_filter(self._dbus_filter_bound_method)
            self._bus = None
        self._dbus_method_impls = []

    def _dbus_filter(self, bus, message):
        if isinstance(message, dbus.lowlevel.MethodCallMessage):

            e = Event('dbus-method-call', message=message,
                interface=message.get_interface(), path=message.get_path(),
                args=map(unwrap, message.get_args_list(byte_arrays=True)),
                destination=message.get_destination(),
                method=message.get_member(),
                sender=message.get_sender(),
                handled=False)

            for pair in self._dbus_method_impls:
                pattern, cb = pair
                if pattern.match(e):
                    cb(e)
                    e.handled = True
                    break

            self.append(e)

            return dbus.lowlevel.HANDLER_RESULT_HANDLED

        return dbus.lowlevel.HANDLER_RESULT_NOT_YET_HANDLED

class TestEventQueue(BaseEventQueue):
    def __init__(self, events):
        BaseEventQueue.__init__(self)
        self.events = events

    def wait(self):
        if self.events:
            return self.events.pop(0)
        else:
            raise TimeoutError

class EventQueueTest(unittest.TestCase):
    def test_expect(self):
        queue = TestEventQueue([Event('foo'), Event('bar')])
        assert queue.expect('foo').type == 'foo'
        assert queue.expect('bar').type == 'bar'

    def test_expect_many(self):
        queue = TestEventQueue([Event('foo'), Event('bar')])
        bar, foo = queue.expect_many(
            EventPattern('bar'),
            EventPattern('foo'))
        assert bar.type == 'bar'
        assert foo.type == 'foo'

    def test_timeout(self):
        queue = TestEventQueue([])
        self.assertRaises(TimeoutError, queue.expect, 'foo')

    def test_demand(self):
        queue = TestEventQueue([Event('foo'), Event('bar')])
        foo = queue.demand('foo')
        assert foo.type == 'foo'

    def test_demand_fail(self):
        queue = TestEventQueue([Event('foo'), Event('bar')])
        self.assertRaises(RuntimeError, queue.demand, 'bar')

def unwrap(x):
    """Hack to unwrap D-Bus values, so that they're easier to read when
    printed."""

    if isinstance(x, list):
        return map(unwrap, x)

    if isinstance(x, tuple):
        return tuple(map(unwrap, x))

    if isinstance(x, dict):
        return dict([(unwrap(k), unwrap(v)) for k, v in x.iteritems()])

    for t in [unicode, str, long, int, float, bool]:
        if isinstance(x, t):
            return t(x)

    return x

def call_async(test, proxy, method, *args, **kw):
    """Call a D-Bus method asynchronously and generate an event for the
    resulting method return/error."""

    def reply_func(*ret):
        test.handle_event(Event('dbus-return', method=method,
            value=unwrap(ret)))

    def error_func(err):
        test.handle_event(Event('dbus-error', method=method, error=err))

    method_proxy = getattr(proxy, method)
    kw.update({'reply_handler': reply_func, 'error_handler': error_func})
    method_proxy(*args, **kw)

def sync_dbus(bus, q, proxy):
    # Dummy D-Bus method call
    call_async(q, dbus.Interface(proxy, dbus.PEER_IFACE), "Ping")
    event = q.expect('dbus-return', method='Ping')

class ProxyWrapper:
    def __init__(self, object, default, others):
        self.object = object
        self.default_interface = dbus.Interface(object, default)
        self.interfaces = dict([
            (name, dbus.Interface(object, iface))
            for name, iface in others.iteritems()])

    def __getattr__(self, name):
        if name in self.interfaces:
            return self.interfaces[name]

        if name in self.object.__dict__:
            return getattr(self.object, name)

        return getattr(self.default_interface, name)

def make_mc(bus, event_func, params):
    mc = bus.get_object(
        tp_name_prefix + '.MissionControl',
        tp_path_prefix + '/MissionControl')
    assert mc is not None

    bus.add_signal_receiver(
        lambda *args, **kw:
            event_func(
                Event('dbus-signal',
                    path=unwrap(kw['path']),
                    signal=kw['member'], args=map(unwrap, args),
                    interface=kw['interface'])),
        None,       # signal name
        None,       # interface
        mc._named_service,
        path_keyword='path',
        member_keyword='member',
        interface_keyword='interface',
        byte_arrays=True
        )

    return mc


class EventProtocol(Protocol):
    def __init__(self, queue=None):
        self.queue = queue

    def dataReceived(self, data):
        if self.queue is not None:
            self.queue.handle_event(Event('socket-data', protocol=self,
                data=data))

    def sendData(self, data):
        self.transport.write(data)

class EventProtocolFactory(Factory):
    def __init__(self, queue):
        self.queue = queue

    def buildProtocol(self, addr):
        proto =  EventProtocol(self.queue)
        self.queue.handle_event(Event('socket-connected', protocol=proto))
        return proto

class EventProtocolClientFactory(EventProtocolFactory, ClientFactory):
    pass

if __name__ == '__main__':
    unittest.main()

