# Find My Anything im Pebble-Appstore veröffentlichen (Version 1.3)

Alles für die Listung liegt in `store/release/` und gebündelt in
`store/FindMyAnything_Store_Paket.zip`. Hochgeladen wird nichts automatisch, das
machst du im Portal, **am PC** (im Handy-Browser antwortet das Portal mit einem
nichtssagenden „Server error 400“).

| Datei | Wofür |
|---|---|
| `FindMyAnything-1.3.pbw` | die Watchapp für alle sechs Uhren. Dieselbe Datei wie im [GitHub-Release v1.3](https://github.com/n0va-SIDEffects/pebble-findmyanything/releases/tag/v1.3) (sha256 beginnt mit `bfd9b3a8`) |
| `icon_80.png` | Store-Icon, 80 × 80, ohne Alphakanal (Pflichtfeld) |
| `icon_144.png`, `icon_48.png` | grosses und kleines Icon, falls das Portal danach fragt |
| `screenshots_<plattform>/` | pro Uhr eine Asset Collection: 5 Screenshots und `banner_720x320.png` mit der passenden Uhr |
| `description_en.txt` | erste Zeile Kurzbeschreibung, darunter die Beschreibung (unter 1600 Zeichen, geprüft, mit KI-Hinweis) |
| `RELEASE_NOTES.md` | Text für „Release Notes“: die Zeilen unter „1.3“ |

| Ordner | Plattform | Uhr |
|---|---|---|
| `screenshots_emery` | emery | Pebble Time 2 |
| `screenshots_basalt` | basalt | Pebble Time, Time Steel |
| `screenshots_chalk` | chalk | Pebble Time Round |
| `screenshots_diorite` | diorite | Pebble 2 |
| `screenshots_flint` | flint | Pebble 2 Duo |
| `screenshots_gabbro` | gabbro | Pebble Round 2 |

Screenshots in jedem Ordner in dieser Reihenfolge hochladen:
`1_very_close` (AirTag, sehr nah), `2_device_list` (Liste),
`3_getting_closer` (Galaxy SmartTag, Pfeil näher), `4_trackers` (Filter Tracker),
`5_far` (Tile, weit weg, Pfeil weg).

## Erste Listung

1. https://developer.repebble.com/dashboard öffnen, mit dem Konto der Pebble-App anmelden.
2. **Add Watchapp**, zuerst `FindMyAnything-1.3.pbw` hochladen. Name, Version 1.3,
   die sechs Plattformen und `SIDEffect's` liest das Portal aus der Datei.
3. Grunddaten:
   - Title: `Find My Anything`
   - Category: `Tools & Utilities`
   - Support email: deine Adresse
   - Source code URL: `https://github.com/n0va-SIDEffects/pebble-findmyanything`
   - Website (falls gefragt): `https://github.com/n0va-SIDEffects/pebble-findmyanything/releases`
     (dort liegt die Android-App)
   - Icon: `icon_80.png`
4. **Create**, dann **Add a release** mit `FindMyAnything-1.3.pbw` und den Zeilen
   unter „1.3“ aus `RELEASE_NOTES.md`. Seite neu laden, **Publish** neben dem Release.
5. **Manage Asset Collections → Create**, einmal pro Plattform (Tabelle oben):
   - Description: `description_en.txt` ab der zweiten Zeile (für alle gleich)
   - Screenshots: die fünf Bilder aus `screenshots_<plattform>/` in der Reihenfolge oben
   - Marketing Banner: `banner_720x320.png` aus demselben Ordner
   - **Create Asset Collection**
6. Oben zuerst **Publish Privately** und die Listung über den Link ansehen, dann **Publish**.

Nach einigen Minuten findet die Suche in der Pebble-App „Find My Anything“.
Kurz prüfen: aus dem Store installieren, Zahnrad (Einstellungen) erscheint, oben
steht der Download-Knopf für die Android-App.

## Updates

1. `version` in `watch/package.json` erhöhen, immer `Major.Minor` (`1.4`, nicht `1.4.0`).
2. Release-Text `docs/releases/v1.4.md`, Zeilen unter „1.4“ in `store/RELEASE_NOTES.md`.
3. `cd watch && rm -rf build && pebble build`, committen, pushen.
4. GitHub: `( set -a; . ~/.hermes/.env; set +a; python3 tools/release.py )`.
5. Wenn sich etwas Sichtbares geändert hat: `python3 store/make_screenshots.py`
   (Emulator, alle Plattformen, ~25 min), `python3 store/banner/make_banner.py`.
6. Store-Paket: `python3 store/make_package.py`, im Portal **Add a release** mit
   der neuen `.pbw` und **Publish**; neue Asset Collections nur bei neuen Bildern.

Alternativ per Kommandozeile: `pebble login` (einmalig), dann im Ordner `watch/`
`pebble publish --release-notes "…"`.

## Checkliste

- [x] UUID `cba78834-b48b-444a-b40e-0b10a7cbd608` (nie ändern)
- [x] Version `1.3` (Major.Minor)
- [x] Plattformen emery, basalt, chalk, diorite, flint, gabbro (aplite: zu wenig Speicher)
- [x] Einstellungen (`configurable`) im PBW, Ton-Optionen nur auf Uhren mit Lautsprecher
- [x] Beschreibung unter 1600 Zeichen, Englisch, mit KI-Hinweis
- [x] Icon ohne Alphakanal, Launcher-Icon passend zum Store-Icon
- [x] Repo öffentlich, MIT-Lizenz (Source code URL)
- [x] Time 2 auf der echten Uhr getestet (1.2); die anderen fünf Uhren nur im Emulator
