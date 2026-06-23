# Handoff — Companion: Handshake-Settle + Tray-Port-Anzeige

**Datum:** 2026-06-14 · **Status:** dist-Build vom User verifiziert (Sync läuft wieder).

## Problem
„Board zeigt connected, ist aber desync." Per Diagnose-Log nachgewiesen:
`Handshake FAIL on COM6` bei jedem Reconnect-Versuch → die Companion verband nie
→ sendete nie `VOL:` (nur die `ID?`-Probes erreichten das Board und ließen den
roten Punkt verschwinden → trügerisches „connected").

## Ursache
Das Board sendet nicht-blockierend (`setTxTimeoutMs(0)`) und **verwirft seine
`AUDIOKNUBBEL`-Antwort, solange es DTR noch nicht erkannt hat**. Der Handshake las
sofort nach dem Öffnen → verpasste die Antwort. (Meine PS-Tests hatten zufällig
ein `Sleep` vor dem Lesen und klappten deshalb.)

## Fix (`SerialLink.cs`)
`Handshake()` wartet nach dem Öffnen **250 ms** und sendet `ID?` in **bis zu 3
Runden** erneut, bevor es aufgibt. Robust gegen das DTR-/TX-Timing.

## Zusatzfeature (`SerialLink.cs` + `TrayAppContext.cs`)
- `SerialLink.PortName` exponiert den aktuell verbundenen Port.
- Tray zeigt jetzt **Port + Status**: Tooltip „audioknubbel: COM6 verbunden" bzw.
  „getrennt (suche…)", plus eine (deaktivierte) Status-Zeile oben im Kontextmenü,
  die beim Öffnen live aktualisiert wird.

## ⚠️ Stolperstein für die Zukunft (teuer erkauft)
**Nicht im Sekundentakt fremd auf den Board-COM-Port zugreifen.** Viele schnelle
Open/Close-/DTR-Zyklen von außen haben die **CDC-TX des Boards verklemmt** (RX
lief weiter, TX tot) — nur ein **Board-Reset/Replug** hat es behoben. Diagnose
am besten über das Companion-eigene Log/Tray, nicht durch paralleles Port-Öffnen
(der Port ist exklusiv).

## Verifizierung
- xUnit grün (22). dist-Single-File publiziert (`-p:PublishProfile=dist`), läuft.
- Log zeigte nach Board-Reset: `CONNECTED COM6` → `TX VOL:48` → `TX MUTE:0` → PING.
- Diagnose-Log (`DebugLog`) nach der Fehlersuche wieder entfernt.

## Hinweis
`dist/` ist ein Build-Artefakt (nicht in git). Publish-Profil: `dist.pubxml`
(Framework-dependent Single-File nach `dist/`).
