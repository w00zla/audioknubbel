# Handoff: Flash-Modus per Tray (Board-Countdown + Firmware-Self-Reboot)

Datum: 2026-06-14 · Branch: `feature/board-boot-countdown` → gemergt nach `master`

## Was das Feature macht

Tray-Menüpunkt **„! In Flash-Mode versetzen"** versetzt das Board zuverlässig in
den ROM-Download-Modus (zum Flashen), mit einem 5-Sekunden-Countdown auf dem
Display als Vorwarnung.

```
App (Tray) ──"BOOT?"──────▶ Board   5s-Countdown (Arc aus, große Ziffer 5→1)
App ◀──"BOOTREADY"───────── Board   am Ende
App ──Disconnect            Board   → ui_boot_flashing() "FLASH-MODUS ⬇"
                            Board   → usb_persist_restart(RESTART_BOOTLOADER)
                                      = ROM USB-Serial/JTAG, Flash-Port COM5
```

Der eingefrorene „FLASH-MODUS ⬇"-Screen **ist** der Erfolgsfall (der Bootloader
hat kein UI). Nach einem Power-Cycle / Reflash läuft die Firmware wieder normal.

## Wichtigste Design-Entscheidung

Der ursprünglich geplante host-seitige **1200-Baud-Touch** wurde verworfen: auf
ESP32-S3-Native-USB ist er nichtdeterministisch (mal Normal-, mal Download-Reset),
und eine host-seitige Erfolgsprüfung kann den Normal-Reboot-Transient nicht vom
Bootloader unterscheiden. Stattdessen ruft die **Firmware** am Countdown-Ende
`usb_persist_restart(RESTART_BOOTLOADER)` selbst auf — exakt der Aufruf, den auch
der Touch-Handler intern macht, nur ohne Host-Race. Hardware-verifiziert
(esptool: „Staying in bootloader", COM5).

Volle Begründung + Debugging-Verlauf:
`docs/superpowers/specs/2026-06-14-board-boot-countdown-design.md`.

## Berührte Dateien

- **Firmware:** `src/protocol.h` (`ProtoCmd::EnterBoot`), `src/protocol.cpp`
  (Dispatch), `src/main.cpp` (Countdown-State-Machine + Self-Reboot),
  `src/ui.cpp` / `src/ui.h` (`ui_boot_countdown`, `ui_boot_flashing`; Arc im
  Overlay ausgeblendet).
- **Companion:** `Protocol.cs` (`BootRequestLine`/`IsBootReady`), `SerialLink.cs`
  (`RequestBootCountdown`, `Disconnect`), `TrayAppContext.cs` (Sequenz; „Reconnect"-
  Menüitem entfernt).
- **Protokoll:** neue Zeilen `BOOT?` (PC→Board) und `BOOTREADY` (Board→PC).

## How to test

1. Firmware bauen/flashen: `pio.exe run -e crowpanel-s3 -t upload --upload-port COM5`
   (nur möglich, wenn das Board im Download-Modus ist — siehe unten).
2. Companion bauen + starten:
   `dotnet publish companion/AudioKnubbel.Companion -p:PublishProfile=dist` →
   `dist/AudioKnubbel.Companion.exe` (Tray, verbindet auf COM6).
3. Tray-Rechtsklick → „! In Flash-Mode versetzen" → bestätigen. Erwartet:
   Countdown 5→1 (kein Arc) → eingefrorener „FLASH-MODUS ⬇" → Board auf COM5.
4. C#-Tests: `dotnet test companion/AudioKnubbel.Companion.Tests` (30/30).

## Offene Punkte / Hinweise

- **LED-Ring-WIP** (`src/led_ring.*`, plus Anteile in `touch.*`, `lv_conf.h`,
  `ui.*`) ist im selben `feat(boot)`-Commit mit eingeflossen — er war mit den
  Boot-Dateien verflochten und nicht sauber trennbar. **Noch unfertig**, falls
  jemand daran weiterarbeitet.
- **Flash-Port-Asymmetrie:** Firmware-CDC = **COM6** (MI_01), ROM-Bootloader =
  **COM5** (MI_00 + „USB JTAG/serial debug unit" MI_02). `PortDiscovery` skippt
  MI_00, findet also nur die Firmware. Modus prüfen ohne Rauskicken:
  `esptool --chip esp32s3 --port COM5 --before no_reset --after no_reset chip_id`.
- **Untracked Clutter** im Repo (`.superpowers/`, `.vscode/`, `dist/`, `AGENTS.md`,
  `docs/ui-preview-media-gestures.html`) wurde bewusst nicht committet/ignoriert.
