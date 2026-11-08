// settings.js - the settings block shared with the watch (src/c/main.c, Settings)
// and the phone app (Settings.kt): 19 bytes, version 2, see docs/protokoll.md.
// Pure functions, tested in node (test/pkjs_test.js).
//
// The watch keeps the settings. Each side stamps a change with the time (unix
// seconds); the watch takes the newer one and passes it on, so this page and
// the phone app stay in step.

var LEN = 19;
var VERSION = 2;
var LANG_AUTO = 255;
var LANGS = ['en', 'de', 'fr', 'it', 'es', 'pt', 'nl'];

var DEFAULTS = {
  sort: 0, resortSec: 10, startFilter: 0, feedback: 1, volume: 30, sound: 0, light: 0,
  bigNumber: 0, lang: LANG_AUTO, vibe: 0, classic: 1, minRssi: -127, hideUnnamed: 0,
  maxAge10: 6, stamp: 0
};

function copy(o) {
  var r = {};
  for (var k in o) {
    if (Object.prototype.hasOwnProperty.call(o, k)) { r[k] = o[k]; }
  }
  return r;
}

function clamp(v, lo, hi, dflt) {
  v = parseInt(v, 10);
  if (isNaN(v)) { return dflt; }
  return v < lo ? lo : (v > hi ? hi : v);
}

function bool(v) {
  return v === true || v === 1 || v === 'true' || v === '1';
}

// bytes: array of numbers 0..255 (AppMessage byte array). null if not version 2.
function decode(b) {
  if (!b || b.length < LEN || b[0] !== VERSION) { return null; }
  return {
    sort: b[1], resortSec: b[2], startFilter: b[3], feedback: b[4], volume: b[5], sound: b[6],
    light: b[7], bigNumber: b[8], lang: b[9], vibe: b[10], classic: b[11],
    minRssi: b[12] > 127 ? b[12] - 256 : b[12], hideUnnamed: b[13], maxAge10: b[14],
    stamp: b[15] + b[16] * 256 + b[17] * 65536 + b[18] * 16777216
  };
}

function encode(s) {
  var st = s.stamp >>> 0;
  return [VERSION, s.sort, s.resortSec, s.startFilter, s.feedback, s.volume, s.sound, s.light,
    s.bigNumber, s.lang, s.vibe, s.classic ? 1 : 0, (s.minRssi + 256) % 256, s.hideUnnamed ? 1 : 0,
    s.maxAge10, st % 256, (st >>> 8) % 256, (st >>> 16) % 256, (st >>> 24) % 256];
}

// Values for the Clay page (select values are strings)
function toClay(s) {
  return {
    sort: String(s.sort),
    resort: String(s.resortSec),
    startFilter: String(s.startFilter),
    bigNumber: String(s.bigNumber),
    feedback: String(s.feedback),
    sound: String(s.sound),
    volume: s.volume,
    vibe: String(s.vibe),
    light: String(s.light),
    classic: !!s.classic,
    minRssi: String(s.minRssi),
    hideUnnamed: !!s.hideUnnamed,
    maxAge: String(s.maxAge10 * 10),
    lang: s.lang < LANGS.length ? LANGS[s.lang] : 'auto'
  };
}

// raw: flattened Clay values ('clay-settings'); prev: settings before. Stamp stays.
function fromClay(raw, prev) {
  raw = raw || {};
  var p = prev || DEFAULTS;
  var li = LANGS.indexOf(raw.lang);
  return {
    sort: clamp(raw.sort, 0, 2, p.sort),
    resortSec: clamp(raw.resort, 2, 120, p.resortSec),
    startFilter: clamp(raw.startFilter, 0, 5, p.startFilter),
    feedback: clamp(raw.feedback, 0, 3, p.feedback),
    volume: clamp(raw.volume, 0, 100, p.volume),
    sound: clamp(raw.sound, 0, 1, p.sound),
    light: clamp(raw.light, 0, 2, p.light),
    bigNumber: clamp(raw.bigNumber, 0, 1, p.bigNumber),
    lang: li >= 0 ? li : (raw.lang === 'auto' ? LANG_AUTO : p.lang),
    vibe: clamp(raw.vibe, 0, 1, p.vibe),
    classic: raw.classic === undefined ? p.classic : (bool(raw.classic) ? 1 : 0),
    minRssi: clamp(raw.minRssi, -127, -40, p.minRssi),
    hideUnnamed: raw.hideUnnamed === undefined ? p.hideUnnamed : (bool(raw.hideUnnamed) ? 1 : 0),
    maxAge10: clamp(Math.round(parseInt(raw.maxAge, 10) / 10), 3, 30, p.maxAge10),
    stamp: p.stamp || 0
  };
}

function same(a, b) {
  return JSON.stringify(encode(a).slice(0, 15)) === JSON.stringify(encode(b).slice(0, 15));
}

// A new change must be newer than anything the watch has seen
function nextStamp(prev, nowMs) {
  return Math.max(Math.floor(nowMs / 1000), ((prev && prev.stamp) || 0) + 1);
}

module.exports = {
  LEN: LEN,
  LANGS: LANGS,
  LANG_AUTO: LANG_AUTO,
  DEFAULTS: DEFAULTS,
  copy: copy,
  decode: decode,
  encode: encode,
  toClay: toClay,
  fromClay: fromClay,
  same: same,
  nextStamp: nextStamp
};
