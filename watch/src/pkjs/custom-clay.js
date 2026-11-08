// custom-clay.js - runs inside the settings page (Clay customFn).
//
// Clay writes this function into the page as source text, so it cannot use
// anything from outside; translations and the chosen language arrive as
// clayConfig.meta.userData (set in index.js).
//
// 1. Translation: language from the app settings, or the phone language when
//    set to automatic. Every element whose whole text is an English key of the
//    table gets the translation (text only, so "&" and markup are safe).
// 2. Support button: Buy Me a Coffee yellow (#FFDD00, black text) instead of
//    Clay's orange, which would look like a second Save button.
module.exports = function (minified) {
  var clayConfig = this;
  var DONATE_URL = 'https://buymeacoffee.com/SIDEffects';
  var BMC_CSS = [
    'button[data-donate="bmc"] {',
    '  background-color: #ffdd00 !important;',
    '  color: #000000 !important;',
    '  text-transform: none !important;',
    '  -webkit-tap-highlight-color: rgba(255, 221, 0, 0.4) !important;',
    '}',
    'button[data-donate="bmc"]:active {',
    '  background-color: #f2d100 !important;',
    '}'
  ].join('\n');

  function phoneLanguage() {
    try {
      return String(navigator.language || navigator.userLanguage || 'en').toLowerCase().substring(0, 2);
    } catch (e) {
      return 'en';
    }
  }

  function trim(s) {
    return String(s).replace(/^\s+|\s+$/g, '');
  }

  function translate(dict) {
    var els = document.querySelectorAll(
      '.label, .description, .value, h1, h2, h3, h4, h5, h6, p, button, option, span, div');
    for (var i = 0; i < els.length; i++) {
      if (els[i].children.length) { continue; }   // only plain text elements
      var key = trim(els[i].textContent);
      if (Object.prototype.hasOwnProperty.call(dict, key)) {
        els[i].textContent = dict[key];
      }
    }
  }

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function () {
    var data = (clayConfig.meta && clayConfig.meta.userData) || {};
    var tables = data.i18n || {};
    var lang = data.lang && data.lang !== 'auto' ? data.lang : phoneLanguage();
    if (tables[lang]) {
      translate(tables[lang]);
      document.documentElement.setAttribute('lang', lang);
    }

    var button = clayConfig.getItemById('donate');
    if (!button) {
      return;
    }
    if (!document.getElementById('bmc-style')) {
      var style = document.createElement('style');
      style.id = 'bmc-style';
      style.appendChild(document.createTextNode(BMC_CSS));
      document.head.appendChild(style);
    }
    button.on('click', function () {
      var w = null;
      try {
        w = window.open(DONATE_URL, '_blank');
      } catch (e) {
        w = null;
      }
      if (!w) {
        window.location.href = DONATE_URL;
      }
    });
  });
};
