# Spec: „Boot-Modus" mit Board-Countdown

Datum: 2026-06-14 · Branch: `feature/board-boot-countdown`

> **Designänderung (nach Hardware-Debugging):** Der ursprünglich geplante
> host-seitige **1200-Baud-Touch** zum Auslösen des Bootloaders ist auf
> ESP32-S3-Native-USB **nichtdeterministisch** (mal Normal-, mal Download-Reset).
> Stattdessen löst die **Firmware den Download-Modus selbst** aus
> (`usb_persist_restart(RESTART_BOOTLOADER)`). Diese Spec beschreibt den finalen,
> hardware-verifizierten Stand. Details siehe Abschnitt „Warum kein Host-Touch".

## Ziel

Der Tray-App-Menüpunkt „! In Flash-Mode versetzen" zeigt dem User zuerst eine
5-Sekunden-Countdown-Animation auf dem Board und versetzt das Board danach
zuverlässig in den ROM-Download-Modus (Flash-Modus).

## Ablauf

```
App (Boot-Menü) ──"BOOT?"──────▶ Board   spielt 5s-Countdown-Animation
App ◀──"BOOTREADY"────────────── Board   (einmal, am Ende)  ───┐
App ──Disconnect (Port frei)              Board löst SELBST ◀──┘
                                          usb_persist_restart(RESTART_BOOTLOADER)
                                          → ROM USB-Serial/JTAG (Flash-Modus)
```

Die App **wartet aktiv** auf `BOOTREADY`. Kommt es nicht (Timeout) oder ist beim
Klick kein Board verbunden, wird **abgebrochen** und eine Fehlermeldung gezeigt.
Bei `BOOTREADY` gibt die App nur ihren Port frei (`Disconnect`) — den Reset macht
das Board selbst.

## Protokoll (2 neue Zeilen)

Zeilenbasiert, ASCII, `\n`-terminiert (wie bisher).

```
PC → Board:   BOOT?\n        (Bitte Countdown starten)
Board → PC:   BOOTREADY\n     (Countdown fertig; Board geht jetzt selbst in Flash)
```

- `BOOT?` reiht sich in den pure-Parser `protocolParseLine` ein → neuer
  `ProtoCmd::EnterBoot`. Toleranzregeln (führende Whitespaces, trailing `\r`,
  unbekannt/leer → `None`) bleiben.
- `BOOTREADY` ist — neben `AUDIOKNUBBEL <fw>` — die einzige Zeile, die das Board
  **aktiv** sendet (sonst nur Antwort auf `ID?`). Rein informativ fürs App-UI;
  der Reset hängt nicht davon ab.

## Firmware

### `protocol.h` / `protocol.cpp`
- `ProtoCmd::EnterBoot` ergänzen; Parser erkennt `BOOT?`.
- `handleLine` dispatcht `EnterBoot` an neues `extern void protocolEnterBoot()`
  (definiert in `main.cpp`, analog zu `protocolApplyVolume`).

### `main.cpp` — nicht-blockierende Countdown-State-Machine
- Forward-Declaration `extern "C" void usb_persist_restart(restart_type_t)` (aus
  `esp32-hal-tinyusb.h`, direkt deklariert statt das TinyUSB-Header mitzuziehen).
- Statics: `s_boot_pending` (bool), `s_boot_deadline` (uint32_t),
  `s_boot_last_shown` (int).
- `protocolEnterBoot()`: ignorieren wenn schon `pending`; sonst `wakeUp()`,
  `s_boot_pending = true`, `s_boot_deadline = millis() + 5000`,
  `s_boot_last_shown = -1`.
- In `loop()`: solange `s_boot_pending`
  - Standby unterdrücken (`s_last_activity_ms = millis()`),
  - verbleibende Sekunden per Ceil; bei Änderung `ui_boot_countdown(secondsLeft)`,
  - Encoder-/Touch-Eingaben in diesem Zyklus ignorieren,
  - bei `millis() >= s_boot_deadline`: `ui_boot_flashing()` + ein paar
    `lv_timer_handler()`-Durchläufe (klarer Endframe), dann
    `USBSerial.println("BOOTREADY")` + `flush()` + `delay(150)` (BOOTREADY drainen)
    und schließlich **`usb_persist_restart(RESTART_BOOTLOADER)`** — kehrt nicht
    zurück (auf S3: `usb_switch_to_cdc_jtag()` + `esp_restart()`). `s_boot_pending
    = false` nur als Fallback, falls der Reset wider Erwarten scheitert.

### `ui.h` / `ui.cpp`
- `void ui_boot_countdown(int secondsLeft)`: Overlay mit großer zentrierter Ziffer
  (`lv_font_montserrat_48`) + Caption „FLASH-MODUS"; Vol-Label/Mute/Hints aus,
  Tick-Ring gedimmt.
- `void ui_boot_flashing()`: Endframe vor dem Reboot — ersetzt die Ziffer durch
  `LV_SYMBOL_DOWNLOAD`, damit der eingefrorene Bootloader-Screen eindeutig
  „FLASH-MODUS ⬇" zeigt (statt der letzten Countdown-Ziffer). Kein Restore nötig.

## Companion (.NET)

### `Protocol.cs`
- `BootRequestLine() => "BOOT?\n"`.
- `IsBootReady(string? line)` → `line` startet (nach TrimStart) mit `BOOTREADY`.

### `SerialLink.cs`
- `bool RequestBootCountdown(int timeoutMs)`: schreibt `BOOT?` auf den offenen
  Firmware-Port, liest Zeilen bis `BOOTREADY` oder Gesamt-Timeout; `true` nur bei
  `BOOTREADY`.
- `void Disconnect()`: gibt den Firmware-Port frei und meldet getrennt. (Kein
  Host-1200-Touch mehr — das Board rebootet sich selbst.)

### `TrayAppContext.cs` — Sequenz
`EnterBootloader()`:
1. Bestätigungs-Dialog.
2. Wenn **nicht verbunden** → Fehlermeldung, Abbruch.
3. Heartbeat- und Reconnect-Timer stoppen (kein konkurrierender Port-Zugriff).
4. Auf **Background-Thread** (Tray friert in den ~5s nicht ein):
   - `RequestBootCountdown(7000)`;
   - bei `true` → `_link.Disconnect()` (Board rebootet selbst);
   - bei `false` → Heartbeat wieder an + Fehlermeldung.
5. Reconnect-Timer wieder starten (findet im Bootloader nichts → harmlos).

Kein manuelles „Reconnect"-Menüitem (der 2-s-Auto-Reconnect deckt das ab). Menü:
Status · Autostart · ─ · „! In Flash-Mode versetzen" · ─ · Exit.

## Warum kein Host-Touch (Debugging-Erkenntnis)

- Der 1200-Baud-Touch ist auf Native-USB gated durch `reboot_enable` +
  bit_rate-Change-Detection + DTR/RTS-State-Machine (`USBCDC.cpp`) → ein einzelner
  Touch macht mal Normal-, mal Download-Reset.
- Host-seitige Erfolgsprüfung „Firmware-Port verschwunden" (`FindPort()==null`)
  ist zweideutig: ein normaler Reboot zeigt dasselbe kurze GONE-Fenster (MI_01
  weg) wie der Bootloader → False Positive.
- `usb_persist_restart(RESTART_BOOTLOADER)` ist exakt der Aufruf, den der
  Touch-Handler intern macht — firmware-seitig direkt aufgerufen entfällt der
  Host-Race komplett.

## Tests

- **`ProtocolTests`** (C#, dotnet): `BootRequestLine` liefert `"BOOT?\n"`;
  `IsBootReady` akzeptiert `"BOOTREADY"`/`" BOOTREADY\r"`, lehnt anderes ab.
- **Firmware-Parser** (`protocolParseLine` → `EnterBoot`): on-device verifiziert
  (kein Host-gcc auf dieser Maschine — Konvention aus M1).
- **End-to-End hardware-verifiziert:** Countdown → „FLASH-MODUS ⬇" → Board
  deterministisch im ROM-Download-Modus (esptool: „Staying in bootloader", COM5).

## Out of Scope / YAGNI

- Kein Abbruch-/Cancel-Pfad für den laufenden Countdown am Board.
- Keine Fortschrittsanzeige in der Tray-App selbst (Countdown lebt am Board).
