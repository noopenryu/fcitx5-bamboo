// SPDX-License-Identifier: LGPL-2.1-or-later
// This test must run under dbus-run-session, never on the desktop session bus.
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import System from 'system';
import {IndicatorClient} from '../extension/client.js';

const XML = `<node><interface name="org.fcitx.Fcitx.Bamboo.Indicator1">
<method name="GetState"><arg direction="out" type="s"/><arg direction="out" type="s"/>
<arg direction="out" type="s"/><arg direction="out" type="s"/></method>
<signal name="StateChanged"><arg type="s"/><arg type="s"/><arg type="s"/><arg type="s"/></signal>
</interface></node>`;
const en = ['keyboard-us', 'en', 'us', 'English'];
const vi = ['bamboo', 'vi', 'VI', 'Bamboo'];

function assert(condition, message) {
    if (!condition)
        throw new Error(message);
}

function waitFor(condition) {
    const deadline = GLib.get_monotonic_time() + 3000000;
    return new Promise((resolve, reject) => {
        GLib.timeout_add(GLib.PRIORITY_DEFAULT, 5, () => {
            if (condition()) {
                resolve();
                return GLib.SOURCE_REMOVE;
            }
            if (GLib.get_monotonic_time() >= deadline) {
                reject(new Error('D-Bus test timed out'));
                return GLib.SOURCE_REMOVE;
            }
            return GLib.SOURCE_CONTINUE;
        });
    });
}

async function run() {
    const bus = Gio.DBus.session;
    const pending = [];
    const object = Gio.DBusExportedObject.wrapJSObject(XML, {
        GetStateAsync(_parameters, invocation) {
            pending.push(invocation);
        },
    });
    object.export(bus, '/bamboo/indicator');
    const own = () => Gio.bus_own_name_on_connection(bus, 'org.fcitx.Fcitx5',
        Gio.BusNameOwnerFlags.NONE, null, null);
    let owner = own();
    const updates = [];
    const errors = [];
    let unavailable = 0;
    const client = new IndicatorClient((state, silent) => updates.push({state, silent}),
        () => unavailable++, error => errors.push(error), bus);
    client.start();
    await waitFor(() => pending.length === 1);
    object.emit_signal('StateChanged', new GLib.Variant('(ssss)', vi));
    await waitFor(() => updates.length === 1);
    pending.shift().return_value(new GLib.Variant('(ssss)', en));
    // A second query's reply establishes that the earlier reply was processed.
    client.sync();
    await waitFor(() => pending.length === 1);
    assert(updates.length === 1 && updates[0].state[0] === 'bamboo',
        'Live D-Bus startup query overwrote a newer signal');
    const beforeLoss = unavailable;
    Gio.bus_unown_name(owner);
    await waitFor(() => unavailable > beforeLoss);
    assert(!client._subscription && !client._owner,
        'Daemon loss retained a subscription');
    pending.shift().return_value(new GLib.Variant('(ssss)', en));
    owner = own();
    await waitFor(() => pending.length === 1);
    pending.shift().return_value(new GLib.Variant('(ssss)', en));
    await waitFor(() => updates.length === 2);
    assert(updates[1].silent && updates[1].state[0] === 'keyboard-us',
        'Daemon reconnection did not synchronize silently');
    client.sync();
    await waitFor(() => pending.length === 1);
    client.destroy();
    pending.shift().return_value(new GLib.Variant('(ssss)', vi));
    assert(!client._watch && !client._subscription && !client._cancel,
        'Client destruction retained resources');
    object.unexport();
    client.connect(bus.get_unique_name());
    await waitFor(() => errors.length === 1);
    assert(updates.length === 2, 'Cancelled request updated the UI');
    client.destroy();
    Gio.bus_unown_name(owner);
    print('Live D-Bus name ownership, signal/query races, cancellation, and missing bridge passed');
}

const loop = new GLib.MainLoop(null, false);
let result = 0;
run().catch(error => {
    console.error(error);
    result = 1;
}).finally(() => loop.quit());
loop.run();
System.exit(result);
