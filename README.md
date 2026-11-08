# Find My Anything

Findet Bluetooth-Geräte in der Nähe über ihre Signalstärke. Die **Android-App sucht**,
die **Pebble-Watchapp zeigt an** und führt dich mit Farbe, Ton und Vibration zum Gerät
(wärmer/kälter). Das Handy kann dabei in der Tasche bleiben.

```
Uhr (Watchapp, emery)  ⇄  Pebble-App  ⇄  Find-My-Anything-App (Android, sucht per BLE + Classic)
                 AppMessage        PebbleKit Android 2
```

Farben überall als Ampel: **grün = direkt daneben**, gelb/orange = mittel, **rot = weit weg**.

Sprachen: Englisch, Deutsch, Französisch, Italienisch, Spanisch, Portugiesisch (Brasilien),
Niederländisch – Handy-App und Uhr, wählbar in den Einstellungen (Standard: Handy-Sprache).

## Installieren

Fertige Dateien unter [Releases](https://github.com/n0va-SIDEffects/pebble-findmyanything/releases):
`FindMyAnything-<Version>.apk` für das Handy, `FindMyAnything-<Version>.pbw` für die Uhr,
Anleitung im Release-Text. Kein Play Store nötig; Updates automatisch mit
[Obtainium](https://github.com/ImranR98/Obtainium) (Repo-Adresse hinzufügen).

## Bedienung auf der Uhr

**Liste** (ältere Einträge grau). Die Reihenfolge bleibt ruhig: Während du blätterst und
5 s danach sortiert nichts um; danach rückt ein Gerät nur auf, wenn es deutlich (6 dB)
stärker ist. Solange der Finger auf der Liste liegt oder sie nachläuft, ändert sich nichts.

| Taste / Touch | Wirkung |
|---|---|
| Hoch/Runter | Auswahl verschieben |
| Wischen | Liste scrollt unter dem Finger mit (mit Nachlauf und Scrollbalken), die Auswahl bleibt, wo sie ist |
| Select, Tippen auf eine Zeile | Gerät suchen |
| Nach rechts wischen | App beenden |
| Select lang | Filter wechseln: Alle → Tracker → Audio → Handy & PC → Wearables → Sonstige |
| Tippen auf die Kopfzeile | sofort neu sortieren |

Kopfzeile rechts: Handy-Symbol grün = alles gut, orange = Handy-Display aus (Suche langsamer),
rot = Problem (Text erscheint, wenn die Liste leer ist). Blauer Punkt = Classic-Suchlauf aktiv.

**Suche**: Der Rand füllt sich mit der Signalstärke, der weisse Strich ist der Bestwert.
Grüner Pfeil = du kommst näher, roter Pfeil = du entfernst dich. Die grosse Zahl zeigt
wahlweise 0–100 oder dBm.

| Taste | Touch | Wirkung |
|---|---|---|
| Select | Tippen auf die unterste Zeile | Signal: stumm → Ton → Vibration → Ton + Vibration |
| Hoch | Tippen auf die Zahl | Bestwert zurücksetzen |
| Runter | Tippen oben | Licht dauerhaft an/aus |
| Zurück | nach rechts wischen | zur Liste |

## Einstellungen

An zwei Stellen, mit denselben Einstellungen: in der Pebble-App (Find My Anything ›
Einstellungen) und in der Find-My-Anything-App («Einstellungen» oben rechts). Die Uhr
speichert sie; es gilt die letzte Änderung, die andere Seite übernimmt sie (Ablauf in
[docs/protokoll.md](docs/protokoll.md)).

- **Uhr – Liste:** Sortierung (Signal/Name/Neueste), Takt fürs Neusortieren (5–60 s),
  Filter beim Start, grosse Zahl als 0–100 oder dBm
- **Ton & Vibration:** Signal zu Beginn (stumm/Ton/Vibration/beides), Klang (weicher Piepton
  oder Geigerzähler-Klick), Lautstärke, Vibrationsstärke
- **Beleuchtung:** automatisch, an während der Suche, immer an in der App
- **Suche am Handy:** klassisches Bluetooth an/aus, schwache Signale ausblenden,
  Geräte ohne Namen ausblenden (Tracker bleiben), wie lange Geräte ausser Reichweite bleiben
- **Sprache**
- **Unterstützen:** Buy me a coffee (https://buymeacoffee.com/SIDEffects)

## Was gefunden wird

- **BLE** (alles, was ein Signal aussendet): AirTags und Find-My-Zubehör (inkl. Akkustand),
  Samsung SmartTag, Tile, Chipolo, Google-Find-Hub-Tracker, AirPods/Beats (Modell),
  iPhones/iPads/Macs, Windows-PCs, Fast-Pair-Kopfhörer, Uhren, Fitnessbänder, Sensoren,
  Beacons (iBeacon, Eddystone), TVs, Autos, Eingabegeräte. Hersteller über die komplette
  Bluetooth-SIG-Liste (über 4000 Firmen).
- **Classic Bluetooth**: nur Geräte im Kopplungsmodus/sichtbar; Suchlauf alle 45 s
  (dauert ca. 12 s), bei der Suche nach einem Classic-Gerät durchgehend.

Grenzen: Geräte, die aus sind, verbunden und still sind oder nur Classic ohne Sichtbarkeit
sprechen, findet niemand. Die Signalstärke gibt «wärmer/kälter», keine genaue Distanz.
Viele Geräte wechseln ihre Adresse; die App erkennt sie am Signalinhalt wieder.

## Einrichtung auf dem Handy (einmalig)

1. APK aus den [Releases](https://github.com/n0va-SIDEffects/pebble-findmyanything/releases) installieren.
2. App öffnen, unter «Einrichtung» alles auf ✓ bringen:
   Bluetooth-/Standort-Rechte, **Standort «Immer erlauben»**, **Akku-Optimierung aus**.
   Auf OPPO/ColorOS zusätzlich in den App-Infos «Hintergrundaktivität erlauben».
3. Watchapp `FindMyAnything.pbw` installieren und auf der Uhr öffnen.

Ohne «Immer erlauben» filtert Android die Ergebnisse, sobald die App im Hintergrund ist.
Mit ausgeschaltetem Handy-Display pausiert Android ungefilterte Suchen; die App sucht
dann mit Filtern weiter (alle Geräte mit Service-UUID, wichtige Hersteller, das Zielgerät),
aber seltener. Am schnellsten ist die Suche mit eingeschaltetem Handy-Display.

## Bauen

- **Watchapp**: `cd watch && pebble build` (SDK 4.33.1, nur `emery`).
  Demo-Daten für Screenshots: `FMA_DEMO` in `src/c/main.c` auf 1.
  Ton: Während der Suche bleibt ein PCM-Strom offen, die Pieptöne rechnet die Uhr selbst
  hinein (`src/c/beeper.c`, `src/c/audio_pump.c`). Grund: Auf der Time 2 schaltet die
  Firmware nach jedem einzelnen Ton Audio-Chip und Verstärker ab und beim nächsten wieder
  an, und sie verstimmt Samples ohne Interpolation – beides knackte und klang übersteuert.
  Tabellen neu erzeugen: `python3 tools/make_sounds.py`. Prüfen ohne Uhr:
  `tools/beeper_test.c` (rendert ein WAV) und `tools/beeper_check.py` (Sprünge, Oberwellen).
  Texte der Uhr: `src/c/i18n.c`.
- **Einstellungsseite** (Clay, `src/pkjs/`): `npm install` einmal, Texte kommen aus
  `android/tools/strings.py` (erzeugt `src/pkjs/i18n.js`). Tests: `node test/pkjs_test.js`,
  `node test/pkjs_flow_test.js`.
- **Android-App**: GitHub Actions (`.github/workflows/android.yml`), signiert mit dem
  Schlüssel aus den Secrets `FMA_KEYSTORE_B64` / `FMA_KS_PASS`.
  Der Schlüssel liegt nicht im Repo. Lokal: `cd android && ./gradlew assembleRelease`
  (dann Debug-Schlüssel). Texte: Tabelle in `android/tools/strings.py`, danach
  `python3 tools/strings.py` (erzeugt alle `values*/strings.xml`).

## Release

Eine Versionsnummer für alles: `version` in `watch/package.json` (die Android-App liest sie
beim Bauen, `versionCode` ist die Build-Nummer von GitHub Actions).

1. Version in `watch/package.json` erhöhen, Release-Text `docs/releases/v<Version>.md` schreiben.
2. `cd watch && pebble build`, committen, pushen; GitHub Actions baut die APK.
3. `( set -a; . ~/.hermes/.env; set +a; python3 tools/release.py )` – wartet auf den Build,
   legt den Release `v<Version>` an und lädt APK (Release-Schlüssel) und PBW hoch.
   `--prerelease` für Vorabversionen. Nochmals ausführen ersetzt Text und Dateien.

Protokoll zwischen Uhr und Handy: [docs/protokoll.md](docs/protokoll.md).

## Lizenz

[MIT](LICENSE) © 2026 SIDEffect's.

Mitgeliefert bzw. verwendet:
- [Clay](https://github.com/pebble-dev/clay) für die Einstellungsseite (MIT), im PBW gebündelt.
- [PebbleKit Android 2](https://github.com/pebble-dev/PebbleKitAndroid2) (Apache-2.0); das
  Android-Projekt ist aus dessen Beispiel entstanden.
- Hersteller-, Dienst- und Gerätetyp-Nummern aus den *Assigned Numbers* der Bluetooth SIG
  ([bluetooth-SIG/public](https://bitbucket.org/bluetooth-SIG/public)), in
  `android/app/src/main/assets/` aufbereitet.
