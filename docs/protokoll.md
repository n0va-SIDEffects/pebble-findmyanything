# Protokoll Uhr ⇄ Handy

AppMessage über PebbleKit Android 2. Watchapp-UUID `cba78834-b48b-444a-b40e-0b10a7cbd608`,
Android-Paket `ch.sideffects.findmyanything`. Schlüssel fest vergeben in `watch/package.json`
(`messageKeys`) und `android/.../Protocol.kt` – beide gleich halten.

## Uhr → Handy

| Key | Name | Typ | Bedeutung |
|---|---|---|---|
| 1 | Cmd | uint8 | 1 = Liste, 2 = Suche, 3 = Stopp |
| 2 | Target | uint32 | Geräte-ID (bei Cmd 2) |

Die Uhr schickt beim Start `Cmd=1`, beim Öffnen der Suche `Cmd=2`, zurück `Cmd=1`
und alle 60 s den aktuellen Befehl erneut (Keepalive). Hört das Handy 150 s nichts,
stoppt es die Suche. Schliesst die Watchapp, meldet die Pebble-App das zusätzlich
(`onAppClosed`).

## Handy → Uhr

Alle Zahlen als int32.

| Key | Name | Inhalt |
|---|---|---|
| 10 | Type | 1 = Liste, 2 = Suche |
| 11–14 | Seq, Part, Parts, Total | Liste: Schnappschuss-Nummer, Teil, Anzahl Teile, Geräte gesamt |
| 15 | Data | Liste: gepackte Einträge (Bytes) |
| 20 | Rssi | geglättet, dBm |
| 21 | Peak | Bestwert dBm (Uhr führt eigenen) |
| 22 | Age | Zehntelsekunden seit letzter Messung (9999 = nie) |
| 23 | Dist | cm, −1 = unbekannt (nur mit TX-Power im Signal) |
| 24 | Trend | dB/s × 10, positiv = näher |
| 25, 26 | Name, Detail | Text |
| 27 | Raw | letzter Rohwert dBm |
| 28 | Rate | Messungen/s × 10 |
| 29 | Category | Kategorie |
| 30 | Status | Bits: 0 BT aus, 1 Rechte fehlen, 2 Display aus, 3 Classic-Suchlauf, 4 Standort aus, 5 kein Hintergrund-Standort, 6 Scan-Fehler |
| 31 | StatusText | Fehlertext für die Uhr oder leer |

Liste alle 2 s, Suche alle 250 ms. Ein Listeneintrag in `Data`:

```
id u32 LE | rssi i8 | category u8 | flags u8 | age_s u8 | name_len u8 | name | detail_len u8 | detail
```

Flags: Bit 0 nur Classic, 1 verbindbar, 2 gekoppelt, 3 wechselnde Adresse. Texte UTF-8,
höchstens 24 Byte. Bis 40 Geräte, Teile bis 900 Byte.

Kategorien: 0 Gerät, 1 Tracker, 2 Handy/Tablet, 3 Computer, 4 Audio, 5 Wearable,
6 TV/Medien, 7 Beacon, 8 Smart Home, 9 Eingabe, 10 Fahrzeug, 11 Gesundheit.
