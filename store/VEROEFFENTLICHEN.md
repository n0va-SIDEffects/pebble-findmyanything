# Find My Anything im Pebble-Appstore veröffentlichen (Version 1.2)

Alles für die Listung liegt in `store/release/` und gebündelt in
`store/FindMyAnything_Store_Paket.zip`. Hochgeladen wird nichts automatisch, das
machst du im Portal, **am PC** (im Handy-Browser antwortet das Portal mit einem
nichtssagenden „Server error 400“).

| Datei | Wofür |
|---|---|
| `FindMyAnything-1.2.pbw` | die Watchapp. Dieselbe Datei wie im [GitHub-Release v1.2](https://github.com/n0va-SIDEffects/pebble-findmyanything/releases/tag/v1.2) (sha256 beginnt mit `6c17275d`) |
| `icon_80.png` | Store-Icon, 80 × 80, ohne Alphakanal (Pflichtfeld) |
| `icon_144.png`, `icon_48.png` | grosses und kleines Icon, falls das Portal danach fragt |
| `banner_720x320.png` | Marketing Banner der Asset Collection |
| `screenshots_emery/` | 5 Screenshots (Pebble Time 2), Reihenfolge nach Dateiname |
| `description_en.txt` | erste Zeile Kurzbeschreibung, darunter die Beschreibung (unter 1600 Zeichen, geprüft) |
| `RELEASE_NOTES.md` | Text für „Release Notes“: die Zeilen unter „1.2“ |

Screenshots in dieser Reihenfolge hochladen:
`1_very_close` (AirTag, grün, sehr nah), `2_device_list` (Liste),
`3_getting_closer` (Galaxy SmartTag, orange, Pfeil näher),
`4_trackers` (Filter Tracker), `5_far` (Tile, rot, Pfeil weg).

## Erste Listung

1. https://developer.repebble.com/dashboard öffnen, mit dem Konto der Pebble-App anmelden.
2. **Add Watchapp**, zuerst `FindMyAnything-1.2.pbw` hochladen. Name, Version 1.2,
   Plattform (nur emery) und `SIDEffect's` liest das Portal aus der Datei.
3. Grunddaten:
   - Title: `Find My Anything`
   - Category: `Tools & Utilities`
   - Support email: deine Adresse
   - Source code URL: `https://github.com/n0va-SIDEffects/pebble-findmyanything`
   - Website (falls gefragt): `https://github.com/n0va-SIDEffects/pebble-findmyanything/releases`
     (dort liegt die Android-App)
   - Icon: `icon_80.png`
4. **Create**, dann **Add a release** mit `FindMyAnything-1.2.pbw` und den Zeilen
   unter „1.2“ aus `RELEASE_NOTES.md`. Seite neu laden, **Publish** neben dem Release.
5. **Manage Asset Collections → Create**, Plattform **emery**:
   - Description: `description_en.txt` ab der zweiten Zeile
   - Screenshots: die fünf Bilder aus `screenshots_emery/` in der Reihenfolge oben
   - Marketing Banner: `banner_720x320.png`
   - **Create Asset Collection**
6. Oben zuerst **Publish Privately** und die Listung über den Link ansehen, dann **Publish**.

Nach einigen Minuten findet die Suche in der Pebble-App „Find My Anything“.
Kurz prüfen: aus dem Store installieren, Zahnrad (Einstellungen) erscheint, und
ohne installierte Handy-App zeigt die Pebble-App den Hinweis mit dem Link auf die
GitHub-Releases.

## Updates

1. `version` in `watch/package.json` erhöhen, immer `Major.Minor` (`1.3`, nicht `1.3.0`).
2. Release-Text `docs/releases/v1.3.md`, Zeilen unter „1.3“ in `store/RELEASE_NOTES.md`.
3. `cd watch && rm -rf build && pebble build`, committen, pushen.
4. GitHub: `( set -a; . ~/.hermes/.env; set +a; python3 tools/release.py )`.
5. Store-Paket: `python3 store/make_package.py`, im Portal **Add a release** mit
   der neuen `.pbw` und **Publish**. Screenshots, Banner und Beschreibung nur ersetzen,
   wenn sich etwas Sichtbares geändert hat (neue Asset Collection).

Alternativ per Kommandozeile: `pebble login` (einmalig), dann im Ordner `watch/`
`pebble publish --release-notes "…"`.

## Checkliste

- [x] UUID `cba78834-b48b-444a-b40e-0b10a7cbd608` (nie ändern)
- [x] Version `1.2` (Major.Minor)
- [x] Plattform nur `emery` (Pebble Time 2: Lautsprecher und Touch)
- [x] Einstellungen (`configurable`) im PBW
- [x] Beschreibung unter 1600 Zeichen, Englisch
- [x] Icon ohne Alphakanal, Launcher-Icon passend zum Store-Icon
- [x] Repo öffentlich, MIT-Lizenz (Source code URL)
- [x] Version 1.2 auf der echten Uhr getestet
