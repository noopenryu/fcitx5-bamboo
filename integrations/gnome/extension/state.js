// SPDX-License-Identifier: LGPL-2.1-or-later

export function presentState(state) {
    if (!Array.isArray(state) || state.length !== 4 ||
        !state.every(value => typeof value === 'string') || !state[0])
        return null;

    const [id, language, label, name] = state;
    const code = language.toLowerCase().split(/[_-]/)[0];
    if (id === 'keyboard-us' || code === 'en')
        return {id, language: 'en', label: 'EN', name: 'English'};
    if (id === 'bamboo' || code === 'vi')
        return {id, language: 'vi', label: 'VI', name: 'Tiếng Việt'};
    return {id, language: code || id, label: label || code.toUpperCase() || '?', name: name || id};
}

// Owns the only dismissal source. The injected scheduler also lets the tests
// check cleanup and rapid switching without loading GNOME Shell.
export class PopupTimer {
    constructor(schedule, cancel, dismiss) {
        this._schedule = schedule;
        this._cancel = cancel;
        this._dismiss = dismiss;
        this._source = null;
    }

    restart() {
        this.clear();
        this._source = this._schedule(900, () => {
            this._source = null;
            this._dismiss();
        });
    }

    clear() {
        if (this._source !== null)
            this._cancel(this._source);
        this._source = null;
    }
}
