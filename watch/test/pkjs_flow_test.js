// node test/pkjs_flow_test.js  (from watch/) - index.js with a fake Pebble/localStorage:
// watch settings arrive -> page shows them -> user saves -> block goes to the watch.
var assert = require('assert');
var Module = require('module');
var path = require('path');

var store = {};
global.localStorage = {
  getItem: function (k) { return Object.prototype.hasOwnProperty.call(store, k) ? store[k] : null; },
  setItem: function (k, v) { store[k] = String(v); },
  removeItem: function (k) { delete store[k]; }
};
var handlers = {};
var sent = [];
var opened = [];
global.Pebble = {
  platform: 'phone',
  addEventListener: function (ev, fn) { (handlers[ev] = handlers[ev] || []).push(fn); },
  sendAppMessage: function (msg, ok) { sent.push(msg); if (ok) { ok(); } },
  openURL: function (u) { opened.push(u); },
  getActiveWatchInfo: function () { return { platform: 'emery' }; },
  getAccountToken: function () { return ''; },
  getWatchToken: function () { return ''; }
};
function fire(ev, e) { (handlers[ev] || []).forEach(function (fn) { fn(e || {}); }); }

// message_keys is provided by the Pebble build; stand-in for node
var origResolve = Module._resolveFilename;
Module._resolveFilename = function (req) {
  if (req === 'message_keys') { return path.join(__dirname, 'message_keys_stub.js'); }
  // the bundled build, as the Pebble build uses it
  if (req === '@rebble/clay') { return path.join(__dirname, '..', 'node_modules', '@rebble', 'clay', 'dist', 'js', 'index.js'); }
  return origResolve.apply(this, arguments);
};
require('fs').writeFileSync(path.join(__dirname, 'message_keys_stub.js'), 'module.exports = {Settings: 40, SettingsReq: 41};\n');

var S = require('../src/pkjs/settings');
require('../src/pkjs/index');

fire('ready');
assert.deepStrictEqual(sent[0], { SettingsReq: 1 }, 'asks the watch for its settings');

// Watch sends its settings (German, volume 45, stamp 1000)
var w = S.copy(S.DEFAULTS); w.lang = 1; w.volume = 45; w.stamp = 1000;
fire('appmessage', { payload: { Settings: S.encode(w) } });
assert.strictEqual(JSON.parse(store['clay-settings']).volume, 45, 'page storage follows the watch');

// An older block (stamp 999) must not overwrite it
var old = S.copy(w); old.volume = 10; old.stamp = 999;
fire('appmessage', { payload: { Settings: S.encode(old) } });
assert.strictEqual(JSON.parse(store['clay-settings']).volume, 45, 'older block ignored');

// Open the page: German texts, current values inside
fire('showConfiguration');
var html = decodeURIComponent(opened[0].replace(/^data:text\/html;charset=utf-8,/, ''));
assert.ok(html.indexOf('"lang":"de"') >= 0 || html.indexOf("lang:'de'") >= 0 || html.indexOf('lang:"de"') >= 0, 'page gets the language');
assert.ok(html.indexOf('Speichern') >= 0, 'German table embedded');

// User saves: volume 60, light always on
var response = { volume: { value: 60 }, light: { value: '2' }, lang: { value: 'de' }, sort: { value: '0' },
  resort: { value: '10' }, startFilter: { value: '0' }, bigNumber: { value: '0' }, feedback: { value: '1' },
  sound: { value: '0' }, vibe: { value: '0' }, classic: { value: true }, minRssi: { value: '-127' },
  hideUnnamed: { value: false }, maxAge: { value: '60' } };
sent.length = 0;
fire('webviewclosed', { response: encodeURIComponent(JSON.stringify(response)) });
assert.strictEqual(sent.length, 1, 'one message to the watch');
var b = S.decode(sent[0].Settings);
assert.strictEqual(b.volume, 60);
assert.strictEqual(b.light, 2);
assert.strictEqual(b.lang, 1);
assert.ok(b.stamp > 1000, 'newer stamp than the watch had');

// Saving without changes sends nothing
sent.length = 0;
fire('webviewclosed', { response: encodeURIComponent(JSON.stringify(response)) });
assert.strictEqual(sent.length, 0, 'unchanged settings are not sent');

require('fs').unlinkSync(path.join(__dirname, 'message_keys_stub.js'));
console.log('ok: settings request, sync from the watch, stale block ignored, page language, save -> watch');
