// node test/pkjs_test.js  (from watch/) - settings block and page translations
var assert = require('assert');
var S = require('../src/pkjs/settings');
var config = require('../src/pkjs/config');
var I18N = require('../src/pkjs/i18n');

// 1. Block layout matches the watch default (main.c s_cfg) byte for byte
assert.deepStrictEqual(S.encode(S.DEFAULTS), [2, 0, 10, 0, 1, 30, 0, 0, 0, 255, 0, 1, 129, 0, 6, 0, 0, 0, 0]);
assert.strictEqual(S.encode(S.DEFAULTS).length, S.LEN);

// 2. Round trip, including negative RSSI and a large stamp
var s = S.copy(S.DEFAULTS);
s.minRssi = -85; s.stamp = 1794000000; s.lang = 1; s.volume = 55; s.hideUnnamed = 1;
assert.deepStrictEqual(S.decode(S.encode(s)), s);
assert.strictEqual(S.decode([1, 0, 0]), null);

// 3. Clay values -> settings -> Clay values
var c = S.toClay(s);
assert.strictEqual(c.lang, 'de');
assert.strictEqual(c.maxAge, '60');
var back = S.fromClay(c, s);
assert.deepStrictEqual(back, s);
var auto = S.fromClay({ lang: 'auto', maxAge: '300', classic: false, volume: '35' }, s);
assert.strictEqual(auto.lang, 255);
assert.strictEqual(auto.maxAge10, 30);
assert.strictEqual(auto.classic, 0);
assert.strictEqual(auto.volume, 35);

// 4. Stamps only go forward
assert.strictEqual(S.nextStamp({ stamp: 5 }, 1000), 6);
assert.strictEqual(S.nextStamp({ stamp: 5 }, 1794000000123), 1794000000);

// 5. Every visible English string of the page has a translation in every language
var KEEP = ['Find My Anything', 'dBm', 'English', 'Deutsch', 'Français', 'Italiano', 'Español', 'Português',
  'Nederlands', '30 s', '1 min', '2 min', '5 min', '☕ Buy me a coffee'];
var texts = [];
(function walk(items) {
  items.forEach(function (it) {
    if (it.items) { walk(it.items); }
    ['defaultValue', 'label', 'description'].forEach(function (k) {
      if (typeof it[k] === 'string' && it.type !== 'select' || (k !== 'defaultValue' && typeof it[k] === 'string')) {
        texts.push(it[k]);
      }
    });
    (it.options || []).forEach(function (o) { texts.push(o.label); });
  });
})(config);
texts = texts.filter(function (t) { return KEEP.indexOf(t) < 0 && !/^-?\d+$/.test(t) && t !== 'auto'; });
var langs = Object.keys(I18N);
assert.deepStrictEqual(langs.sort(), ['de', 'es', 'fr', 'it', 'nl', 'pt']);
var missing = [];
langs.forEach(function (l) {
  texts.forEach(function (t) { if (!I18N[l][t]) { missing.push(l + ': ' + t); } });
});
assert.deepStrictEqual(missing, [], 'missing translations:\n' + missing.join('\n'));
console.log('ok: block layout, round trip, stamps, ' + texts.length + ' page texts x ' + langs.length + ' languages');
