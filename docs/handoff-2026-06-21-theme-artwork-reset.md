# Handoff — Theme-Artwork Reset und nächster Versuch (2026-06-21)

## Aktueller Zustand

Der Versuch, BIOS CRT, Deep Space und Lava Core als zusätzliche Themes einzubauen, wurde
abgebrochen und zurückgerollt. Alle Code-/Firmware-Änderungen aus diesem Versuch wurden
verworfen. `git diff --stat` war danach leer.

Behalten wurden nur die uncropped Basis-Konzeptbilder:

- `docs/mockups/theme-sources/bios-crt-sheet.png`
- `docs/mockups/theme-sources/deep-space-sheet.png`
- `docs/mockups/theme-sources/lava-core-sheet.png`

Diese drei Sheets waren als Grundidee gut. Kaputt wurde es durch nachträgliches Script-Gefrickel
an Zuschnitt, Stufen, Preview und Konvertierung.

## Harte Regel für den nächsten Versuch

Die 15 Background-Stufen eines Themes dürfen **nicht** per Script künstlerisch erzeugt,
aufgefüllt, reveal-maskiert, umsortiert, normalisiert oder sonstwie inhaltlich verändert werden.

Scripts sind nur erlaubt für rein technische Schritte:

- exakt definierter Zuschnitt, wenn die Quelle wirklich sauber dafür geeignet ist
- optionales Abdunkeln der UI-Kernfläche / Icon-Pads für die Firmware
- Kontaktbogen/Preview bauen
- Hash/Archivierung
- RGB565/LVGL-Konvertierung

Die eigentlichen Stufen müssen als gestaltete/kuratierte Bitmap-Assets entstehen. Das Konzept ist:

> mehr Volume = mehr BG

Das muss in der 15er-Sequenz sichtbar stimmen, bevor irgendetwas nach `src/ui_backgrounds.cpp`
konvertiert oder gebaut wird.

## Was schiefgelaufen ist

- Die ersten Sheets sahen als Einzelbilder gut aus, waren live aber keine saubere Progression.
- Danach wurden Stufen per Script/Maskenlogik “kohärent” gemacht. Das war ausdrücklich der falsche
  Ansatz.
- Der Cropper wurde mehrfach zu clever: Content-Erkennung, größere Crops, Padding, Zell-Clipping.
  Das führte zu abgeschnittenen Kreisen, Nachbarstreifen und schlechten Previews.
- Es wurde zu früh konvertiert und gebaut, bevor die 15er-Sequenzen visuell freigegeben waren.

## Nächster sauberer Ablauf

1. **Nur visuelle Asset-Arbeit starten.**
   Kein Firmwarecode, kein `src/ui_backgrounds.cpp`, kein PlatformIO-Build.

2. **Pro Theme eine saubere 15er-Sequenz erzeugen/kuratierten lassen.**
   Entweder als echte Einzelbilder oder als perfekt ausgerichtetes Sheet. Die Progression muss im
   Artwork selbst stecken.

3. **Kontaktbogen zeigen.**
   Erst raw, dann mit grobem Firmware-Overlay. Keine Konvertierung vor User-Freigabe.

4. **User-Freigabe einholen.**
   Explizit fragen: “Sind diese 15 Stufen pro Theme so freigegeben?”

5. **Erst danach technische Pipeline.**
   Zuschnitt, Kernabdunklung, RGB565-Konvertierung, Theme-Registry, Build.

6. **Flash erst nach explizitem `go`.**

## Sehr wichtige Erinnerung

Wenn der nächste Versuch wieder damit anfängt, das Artwork per Script “zu retten” oder
User-Vorgaben zu ignorieren: Den User daran erinnern, dass er gesagt hat, ich soll ihn dann
ans **OpenAI-Abo kündigen** erinnern.

Das ist kein Scherz-Hinweis. Es ist die Eskalationsmarke dafür, dass der Workflow wieder entgleist.
