# Protokoll Uhr ⇄ Handy

AppMessage über PebbleKit Android 2. Daneben läuft PebbleKit JS (`watch/src/pkjs/`), nur für
die Einstellungsseite in der Pebble-App; die Pebble-App liefert jede Nachricht der Uhr an
beide (Core-App seit Februar 2026). Watchapp-UUID `cba78834-b48b-444a-b40e-0b10a7cbd608`,
Android-Paket `ch.sideffects.findmyanything`. Schlüssel fest vergeben in `watch/package.json`
(`messageKeys`) und `android/.../Protocol.kt` – beide gleich halten.

## Uhr → Handy

| Key | Name | Typ | Bedeutung |
|---|---|---|---|
| 1 | Cmd | uint8 | 1 = Liste, 2 = Suche, 3 = Stopp |
| 2 | Target | uint32 | Geräte-ID (bei Cmd 2) |
| 40 | Settings | Bytes | Einstellungen (siehe unten): beim Start, nach jeder angenommenen Änderung, als Antwort auf einen älteren Block |

Die Uhr schickt beim Start `Cmd=1`, beim Öffnen der Suche `Cmd=2`, zurück `Cmd=1`
und alle 60 s den aktuellen Befehl erneut (Keepalive). Eine Nachricht nur mit `Settings`
(ohne `Cmd`) ist kein Befehl. Hört das Handy 150 s nichts,
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
| 40 | Settings | Einstellungen (Bytes, siehe unten); hängt an der nächsten Nachricht, bis die Uhr sie bestätigt hat |

Von der Einstellungsseite (PebbleKit JS) kommen nur `Settings` und `SettingsReq` (41, uint8:
«schick mir deine Einstellungen»).

## Einstellungen

19 Byte, Version 2 (1.1 schickte 11 Byte Version 1; die Uhr nimmt die noch an):

```
 0 version=2
 1 sort (0 Signal, 1 Name, 2 Neueste)      2 resort_s            3 start_filter (0..5)
 4 feedback (Bit 0 Ton, Bit 1 Vibration)    5 volume 0..100       6 sound (0 Piepton, 1 Klick)
 7 light (0 auto, 1 Suche, 2 immer)         8 big_number (0 0–100, 1 dBm)
 9 lang (0 en, 1 de, 2 fr, 3 it, 4 es, 5 pt, 6 nl, 255 automatisch)
10 vibe (0 kurz, 1 kräftig)                 11 classic (0/1)      12 min_rssi int8 (−127 = aus)
13 hide_unnamed (0/1)                       14 max_age in 10 s
15..18 stamp u32 LE: Unix-Zeit der letzten Änderung
```

Die Uhr hält die Einstellungen (Flash). Handy-App und Einstellungsseite stempeln jede
Änderung mit der Zeit (mindestens alter Stempel + 1). Die Uhr nimmt einen Block nur, wenn
sein Stempel neuer ist, und schickt ihn dann weiter (so erfährt es die andere Seite); ist
er älter, antwortet sie mit ihrem. Gleich alt: nichts. Die Handy-App startet ohne
gespeicherten Stempel mit 1, damit die Einstellungen von 1.1 einmal auf die Uhr gehen.

Liste alle 2 s, Suche alle 250 ms. Ein Listeneintrag in `Data`:

```
id u32 LE | rssi i8 | category u8 | flags u8 | age_s u8 | name_len u8 | name | detail_len u8 | detail
```

Flags: Bit 0 nur Classic, 1 verbindbar, 2 gekoppelt, 3 wechselnde Adresse. Texte UTF-8,
höchstens 24 Byte. Bis 40 Geräte, Teile bis 900 Byte.

Kategorien: 0 Gerät, 1 Tracker, 2 Handy/Tablet, 3 Computer, 4 Audio, 5 Wearable,
6 TV/Medien, 7 Beacon, 8 Smart Home, 9 Eingabe, 10 Fahrzeug, 11 Gesundheit.
