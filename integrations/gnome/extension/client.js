// SPDX-License-Identifier: LGPL-2.1-or-later
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

const SERVICE = 'org.fcitx.Fcitx5';
const PATH = '/bamboo/indicator';
const IFACE = 'org.fcitx.Fcitx.Bamboo.Indicator1';

// No polling: name-owner changes reconnect the client, and state arrives through
// one signal subscription. All calls have a finite timeout and are cancellable.
export class IndicatorClient {
    constructor(onState, onUnavailable, onError, bus = Gio.DBus.session) {
        this._onState = onState;
        this._onUnavailable = onUnavailable;
        this._onError = onError;
        this._bus = bus;
        this._generation = 0;
        this._revision = 0;
        this._watch = 0;
        this._subscription = 0;
        this._owner = null;
        this._cancel = null;
    }

    start() {
        if (this._watch)
            return;
        this._watch = Gio.bus_watch_name_on_connection(this._bus, SERVICE,
            Gio.BusNameWatcherFlags.NONE,
            (_bus, _name, owner) => this.connect(owner),
            () => this.disconnect());
    }

    connect(owner) {
        this.disconnect();
        this._owner = owner;
        this._cancel = new Gio.Cancellable();
        const generation = this._generation;
        this._subscription = this._bus.signal_subscribe(owner, IFACE,
            'StateChanged', PATH, null, Gio.DBusSignalFlags.NONE,
            (_bus, _sender, _path, _iface, _signal, parameters) => {
                if (generation !== this._generation)
                    return;
                this._revision++;
                this._onState(parameters.deepUnpack(), false);
            });
        this.sync();
    }

    sync() {
        if (!this._owner)
            return;
        const revision = this._revision;
        this._call(PATH, IFACE, 'GetState', null, '(ssss)', result => {
            if (revision === this._revision)
                this._onState(result.deepUnpack(), true);
        });
    }

    select(id) {
        this._call('/controller', 'org.fcitx.Fcitx.Controller1', 'SetCurrentIM',
            new GLib.Variant('(s)', [id]), '()', () => {});
    }

    configure() {
        this._call('/controller', 'org.fcitx.Fcitx.Controller1', 'ConfigureAddon',
            new GLib.Variant('(s)', ['bamboo']), '()', () => {});
    }

    _call(path, iface, method, parameters, signature, done) {
        if (!this._owner)
            return;
        const generation = this._generation;
        this._bus.call(this._owner, path, iface, method, parameters,
            new GLib.VariantType(signature), Gio.DBusCallFlags.NONE, 3000,
            this._cancel, (bus, result) => {
                let value;
                try {
                    value = bus.call_finish(result);
                } catch (error) {
                    if (generation !== this._generation ||
                        error.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                        return;
                    if (method === 'GetState')
                        this._onUnavailable();
                    this._onError(error);
                    return;
                }
                if (generation === this._generation)
                    done(value);
            });
    }

    disconnect() {
        this._generation++;
        this._cancel?.cancel();
        this._cancel = null;
        this._owner = null;
        if (this._subscription)
            this._bus.signal_unsubscribe(this._subscription);
        this._subscription = 0;
        this._onUnavailable();
    }

    destroy() {
        if (this._watch)
            Gio.bus_unwatch_name(this._watch);
        this._watch = 0;
        this.disconnect();
    }
}
