// index.js - PebbleKit JS of Find My Anything: only the settings page.
//
// The Android companion app does all the Bluetooth work and talks to the
// watch. The Pebble app delivers every message from the watch to both, this
// script and the companion app (Core app since February 2026), so the two
// coexist. This script never sends list or search data.
//
// Flow: the watch keeps the settings and sends them at start and after every
// change; we mirror them into Clay's storage so the page shows them. On save we
// stamp the change and send the 19-byte block; the watch passes it on to the
// phone app.

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var customClay = require('./custom-clay');
var I18N = require('./i18n');
var S = require('./settings');

var K_SETTINGS = 'fma.settings';   // last known settings (decoded block)

var clay = new Clay(clayConfig, customClay, {
  autoHandleEvents: false,
  userData: { i18n: I18N, lang: 'auto' }
});

function load() {
  try {
    var s = JSON.parse(localStorage.getItem(K_SETTINGS));
    if (s && typeof s.stamp === 'number') { return s; }
  } catch (e) { /* fall through */ }
  return S.copy(S.DEFAULTS);
}

function store(s) {
  localStorage.setItem(K_SETTINGS, JSON.stringify(s));
  clay.setSettings(S.toClay(s));
}

function payloadValue(payload, name, key) {
  if (!payload) { return undefined; }
  if (payload[name] !== undefined) { return payload[name]; }
  return payload[key];
}

function send(msg, tries) {
  Pebble.sendAppMessage(msg, function () {}, function () {
    if (tries > 0) {
      setTimeout(function () { send(msg, tries - 1); }, 1500);
    }
  });
}

Pebble.addEventListener('ready', function () {
  // The watch sends its settings at start, maybe before we were ready: ask again
  send({ SettingsReq: 1 }, 2);
});

Pebble.addEventListener('appmessage', function (e) {
  var bytes = payloadValue(e.payload, 'Settings', '40');
  var s = S.decode(bytes);
  if (!s) { return; }
  if (s.stamp >= load().stamp) { store(s); }
});

// Only the Time 2 (emery) and the Pebble 2 Duo (flint) have a speaker
function hasSpeaker() {
  try {
    var info = Pebble.getActiveWatchInfo ? Pebble.getActiveWatchInfo() : null;
    var p = info && info.platform;
    return !p || p === 'emery' || p === 'flint';
  } catch (e) {
    return true;
  }
}

Pebble.addEventListener('showConfiguration', function () {
  var s = load();
  clay.setSettings(S.toClay(s));
  clay.meta.userData = { i18n: I18N, lang: S.toClay(s).lang, speaker: hasSpeaker() };
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) { return; }          // cancelled
  try {
    clay.getSettings(e.response, false);      // stores 'clay-settings'
  } catch (err) {
    return;
  }
  var prev = load();
  var raw = null;
  try { raw = JSON.parse(localStorage.getItem('clay-settings')); } catch (err2) { raw = null; }
  var s = S.fromClay(raw, prev);
  if (S.same(s, prev)) { return; }
  s.stamp = S.nextStamp(prev, Date.now());
  store(s);
  send({ Settings: S.encode(s) }, 3);
});
