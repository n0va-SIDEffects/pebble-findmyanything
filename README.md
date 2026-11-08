# Find My Anything

Findet Bluetooth-Geräte in der Nähe über ihre Signalstärke. Die **Android-App sucht**,
die **Pebble-Watchapp zeigt an** und führt dich mit Balken, Ton und Vibration zum Gerät
(wärmer/kälter). Das Handy kann dabei in der Tasche bleiben.

```
Uhr (Watchapp, emery)  ⇄  Pebble-App  ⇄  Find-My-Anything-App (Android, sucht per BLE + Classic)
                 AppMessage        PebbleKit Android 2
```

## Bedienung auf der Uhr

**Liste** (sortiert nach Signalstärke, ältere Einträge grau)

| Taste | Wirkung |
|---|---|
| Hoch/Runter | blättern |
| Select | Gerät suchen |
| Select lang | Filter wechseln: Alle → Tracker → Audio → Handy & PC → Wearables → Sonstige |

Kopfzeile rechts: Handy-Symbol grün = alles gut, orange = Handy-Display aus (Suche langsamer),
rot = Problem (Text erscheint, wenn die Liste leer ist). Blauer Punkt = Classic-Suchlauf aktiv.

**Suche**: Der Rand füllt sich mit der Signalstärke (0–100), der weisse Strich ist der Bestwert.
Pfeil hoch = du kommst näher, Pfeil runter = du entfernst dich.

| Taste | Wirkung |
|---|---|
| Select | Signal: stumm → Ton → Vibration → Ton + Vibration |
| Hoch | Bestwert zurücksetzen |
| Runter | Licht dauerhaft an/aus |
| Zurück | zur Liste |

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

1. APK installieren (aus GitHub Actions, Artefakt `findmyanything-apk`).
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
- **Android-App**: GitHub Actions (`.github/workflows/android.yml`), signiert mit dem
  Schlüssel aus den Secrets `FMA_KEYSTORE_B64` / `FMA_KS_PASS`.
  Der Schlüssel liegt nicht im Repo. Lokal: `cd android && ./gradlew assembleRelease`
  (dann Debug-Schlüssel).

Protokoll zwischen Uhr und Handy: [docs/protokoll.md](docs/protokoll.md).
