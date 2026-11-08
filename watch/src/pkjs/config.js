// config.js - Clay settings page in the Pebble app (English source texts).
//
// The same settings as in the phone app (Settings.kt). Translations come from
// i18n.js, which android/tools/strings.py generates from the shared text table;
// custom-clay.js swaps them in, so every visible string here must be an English
// text of that table (test/pkjs_test.js checks it). Names, units and the
// language names stay as they are.
//
// Clay keys are storage names only; index.js turns the values into the
// 19-byte settings block (settings.js).

function opts(labels, values) {
  return labels.map(function (l, i) {
    return { label: l, value: String(values ? values[i] : i) };
  });
}

module.exports = [
  { type: 'heading', defaultValue: 'Find My Anything' },
  { type: 'text', defaultValue: 'The same settings are in the Find My Anything phone app. The last change counts.' },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Android app' },
      {
        type: 'text',
        defaultValue: 'Find My Anything needs its free Android app: it does the Bluetooth search. ' +
          'The app is not in the Play Store, it is on GitHub.'
      },
      { type: 'button', id: 'download', defaultValue: 'Download the Android app', attributes: { 'data-download': 'apk' } }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Watch: list' },
      {
        type: 'select', messageKey: 'sort', label: 'Sort by', defaultValue: '0',
        options: opts(['Signal strength', 'Name', 'Newest first'])
      },
      {
        type: 'select', messageKey: 'resort', label: 'Re-sort by signal', defaultValue: '10',
        description: 'The list holds still while you scroll and for 5 s after. ' +
          'Tap the header on the watch to sort right away.',
        options: opts(['every 5 s', 'every 10 s', 'every 30 s', 'every 60 s'], [5, 10, 30, 60])
      },
      {
        type: 'select', messageKey: 'startFilter', label: 'Filter at start', defaultValue: '0',
        options: opts(['All', 'Trackers', 'Audio', 'Phone & PC', 'Wearables', 'Other'])
      },
      {
        type: 'select', messageKey: 'bigNumber', label: 'Big number in the search', defaultValue: '0',
        options: opts(['Signal 0–100', 'dBm'])
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Sound & vibration' },
      {
        type: 'select', messageKey: 'feedback', label: 'Signal at the start of a search', defaultValue: '1',
        description: 'On the watch, Select or a tap on the bottom line switches it during a search.',
        options: opts(['Silent', 'Sound', 'Vibration', 'Sound + vibration'])
      },
      {
        type: 'select', messageKey: 'sound', label: 'Sound', defaultValue: '0',
        options: opts(['Soft beep', 'Geiger counter click'])
      },
      {
        type: 'slider', messageKey: 'volume', label: 'Volume', defaultValue: 30, min: 0, max: 100, step: 5
      },
      {
        type: 'select', messageKey: 'vibe', label: 'Vibration strength', defaultValue: '0',
        options: opts(['Short', 'Strong'])
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Backlight' },
      {
        type: 'select', messageKey: 'light', label: 'Backlight', defaultValue: '0',
        description: 'During a search, Down or a tap on the top of the watch screen turns the light on and off.',
        options: opts(['Automatic', 'On during a search', 'Always on in the app'])
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Search on the phone' },
      {
        type: 'toggle', messageKey: 'classic', label: 'Also search classic Bluetooth', defaultValue: true,
        description: 'Finds older devices in pairing mode. Slows the BLE search down briefly every 45 s.'
      },
      {
        type: 'select', messageKey: 'minRssi', label: 'Hide weak signals', defaultValue: '-127',
        options: opts(['Off', 'below -95 dBm', 'below -90 dBm', 'below -85 dBm', 'below -80 dBm'],
          [-127, -95, -90, -85, -80])
      },
      {
        type: 'toggle', messageKey: 'hideUnnamed', label: 'Hide devices without a name', defaultValue: false,
        description: 'Trackers are always shown.'
      },
      {
        type: 'select', messageKey: 'maxAge', label: 'Keep devices out of reach for', defaultValue: '60',
        options: opts(['30 s', '1 min', '2 min', '5 min'], [30, 60, 120, 300])
      }
    ]
  },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Language' },
      {
        type: 'select', messageKey: 'lang', label: 'Language', defaultValue: 'auto',
        description: 'Applies to the phone app and the watch.',
        options: opts(['Automatic (phone)', 'English', 'Deutsch', 'Français', 'Italiano', 'Español', 'Português',
          'Nederlands'], ['auto', 'en', 'de', 'fr', 'it', 'es', 'pt', 'nl'])
      }
    ]
  },
  { type: 'submit', defaultValue: 'Save' },
  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Support' },
      {
        type: 'text',
        defaultValue: 'Find My Anything is free and has no ads. If you enjoy it, a coffee is much appreciated.'
      },
      { type: 'button', id: 'donate', defaultValue: '☕ Buy me a coffee', attributes: { 'data-donate': 'bmc' } }
    ]
  }
];
