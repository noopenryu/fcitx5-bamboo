// SPDX-License-Identifier: LGPL-2.1-or-later
import GLib from 'gi://GLib';
import {IndicatorClient} from '../extension/client.js';
import {PopupTimer, presentState} from '../extension/state.js';

function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

const en = ['keyboard-us', 'en', 'us', 'Keyboard - English (US)'];
const vi = ['bamboo', 'vi', 'VI', 'Bamboo'];

// Fake only the transport: exercise the actual asynchronous client, including
// replies delivered after cancellation and after the next connection starts.
class Bus {
    constructor() {
        this.signals = new Map();
        this.calls = [];
        this.next = 1;
    }

    signal_subscribe(_owner, _iface, _member, _path, _arg, _flags, callback) {
        const id = this.next++;
        this.signals.set(id, callback);
        return id;
    }

    signal_unsubscribe(id) {
        this.signals.delete(id);
    }

    call(owner, _path, _iface, method, parameters, _type, _flags, _timeout, cancel, callback) {
        this.calls.push({owner, method, parameters, cancel,
            reply: value => callback(this, new GLib.Variant('(ssss)', value))});
    }

    call_finish(result) {
        return result;
    }

    emit(state) {
        for (const callback of this.signals.values())
            callback(this, '', '', '', '', new GLib.Variant('(ssss)', state));
    }
}

const bus = new Bus();
const updates = [];
let unavailable = 0;
const client = new IndicatorClient((state, silent) => updates.push({state, silent}),
    () => unavailable++, error => { throw error; }, bus);
client.connect(':1.10');
const initial = bus.calls.at(-1);
bus.emit(vi);
initial.reply(en);
assert(updates.length === 1 && updates[0].state[0] === 'bamboo',
    'A stale startup snapshot replaced a newer state signal');
client.disconnect();
assert(initial.cancel.is_cancelled() && bus.signals.size === 0,
    'Disconnect leaked a subscription or pending call');
client.connect(':1.11');
initial.reply(en);
assert(updates.length === 1, 'Old daemon reply changed the new session');
bus.calls.at(-1).reply(en);
assert(updates.at(-1).silent, 'Reconnection snapshot must be silent');
client.sync();
const refresh = bus.calls.at(-1);
bus.emit(vi);
refresh.reply(en);
assert(updates.at(-1).state[0] === 'bamboo', 'Refresh raced with a state change');
client.select('bamboo');
assert(bus.calls.at(-1).method === 'SetCurrentIM' &&
    bus.calls.at(-1).parameters.deepUnpack()[0] === 'bamboo', 'Selection used wrong controller method');
for (let i = 0; i < 100; i++) {
    client.connect(`:1.${i + 20}`);
    assert(bus.signals.size === 1, 'Reconnect accumulated subscriptions');
    client.disconnect();
    assert(bus.signals.size === 0, 'Disconnect retained a subscription');
}
client.destroy();
const count = updates.length;
for (const call of bus.calls)
    call.reply(vi);
assert(updates.length === count && unavailable > 0, 'Late replies survived destruction');
assert(presentState(en).label === 'EN' && presentState(vi).name === 'Tiếng Việt',
    'Language presentation differs');
assert(presentState(['pinyin', 'zh_CN', '拼', 'Pinyin']).language === 'zh',
    'Other languages were incorrectly mapped to English');
assert(presentState(['', '', '', '']) === null, 'Unavailable state appeared active');

const sources = new Map();
let next = 0;
let dismissals = 0;
const timer = new PopupTimer((delay, callback) => {
    assert(delay === 900, 'Unexpected popup duration');
    sources.set(++next, callback);
    return next;
}, id => sources.delete(id), () => dismissals++);
for (let i = 0; i < 10000; i++) {
    timer.restart();
    assert(sources.size === 1, 'Rapid switching accumulated timers');
}
const callback = sources.get(next);
sources.delete(next);
callback();
assert(dismissals === 1, 'Popup failed to dismiss');
timer.restart();
timer.clear();
assert(sources.size === 0, 'Popup cleanup left a timer');
print('Client startup races, reconnects, cancellation, presentation, and timer stress passed');
