# Volume-%-Anzeige auf dem Knob — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eine zentrierte `<pct>%`-Zahl (Montserrat 48, weiß) in der Arc-Mitte des GC9A01-Displays anzeigen, die synchron zum Arc läuft und bei Mute durch „MUTE" ersetzt wird.

**Architecture:** Reine LVGL-Display-Änderung in `src/ui.cpp`. Neues Label `s_vol_label` parallel zum bestehenden Arc und „MUTE"-Label. `ui_set_volume()` aktualisiert zusätzlich den Label-Text, `ui_set_mute()` togglet Sichtbarkeit zwischen Zahl und „MUTE". Keine API-Änderung in `ui.h`, keine pure Logik betroffen.

**Tech Stack:** LVGL 8.3, LovyanGFX, PlatformIO (`pioarduino`-Fork, env `crowpanel-s3`).

**Verifikation:** Kein Host-gcc → keine Unit-Tests. Build ersetzt automatisierte Tests; Funktionsprüfung on-device nach Flash. **Flash erst nach explizitem „go" vom User.**

---

### Task 1: Montserrat-48-Font aktivieren

**Files:**
- Modify: `src/lv_conf.h:106`

- [ ] **Step 1: Font einschalten**

In `src/lv_conf.h` die Zeile

```c
#define LV_FONT_MONTSERRAT_48  0
```

ändern zu

```c
#define LV_FONT_MONTSERRAT_48  1
```

- [ ] **Step 2: Build verifizieren**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
Expected: `SUCCESS`, keine Fehler. (Flash-Verbrauch steigt durch die Glyphen.)

- [ ] **Step 3: Commit**

```bash
git add src/lv_conf.h
git commit -m "feat(ui): Montserrat 48 Font aktivieren für Volume-Anzeige"
```

---

### Task 2: Volume-Label in der UI ergänzen

**Files:**
- Modify: `src/ui.cpp`

- [ ] **Step 1: Statische Variable für das Label deklarieren**

In `src/ui.cpp` die Deklarationen oben erweitern:

```c
static lv_obj_t* s_arc;
static lv_obj_t* s_mute_label;
static lv_obj_t* s_vol_label;
```

- [ ] **Step 2: Label in `ui_init()` erstellen**

In `ui_init()`, **vor** der Erstellung von `s_mute_label`, einfügen:

```c
    // Volume-Label: große zentrierte Zahl in der Arc-Mitte
    s_vol_label = lv_label_create(scr);
    lv_label_set_text(s_vol_label, "50%");
    lv_obj_set_style_text_font(s_vol_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_vol_label, lv_color_white(), 0);
    lv_obj_center(s_vol_label);
```

- [ ] **Step 3: `ui_set_volume()` erweitern**

`ui_set_volume()` ersetzen durch:

```c
void ui_set_volume(int pct) {
    lv_arc_set_value(s_arc, pct);
    lv_label_set_text_fmt(s_vol_label, "%d%%", pct);
}
```

- [ ] **Step 4: `ui_set_mute()` Sichtbarkeit ergänzen**

`ui_set_mute()` ersetzen durch:

```c
void ui_set_mute(bool muted) {
    if (muted) {
        lv_obj_set_style_arc_color(s_arc, lv_color_make(80, 80, 80), LV_PART_INDICATOR);
        lv_obj_add_flag(s_vol_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_mute_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_style_arc_color(s_arc, lv_color_make(0, 200, 255), LV_PART_INDICATOR);
        lv_obj_clear_flag(s_vol_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_mute_label, LV_OBJ_FLAG_HIDDEN);
    }
}
```

- [ ] **Step 5: Build verifizieren**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3`
Expected: `SUCCESS`, keine Fehler/Warnings zu `s_vol_label`.

- [ ] **Step 6: Commit**

```bash
git add src/ui.cpp
git commit -m "feat(ui): zentrierte Volume-%-Anzeige, bei Mute durch MUTE ersetzt"
```

---

### Task 3: On-Device-Verifikation (nach „go")

**Files:** keine

- [ ] **Step 1: Build melden** — „ready to flash" an User; auf „go" warten.

- [ ] **Step 2: Flash (nach „go")**

Run: `C:\Users\w00zla\.platformio\penv\Scripts\pio.exe run -e crowpanel-s3 -t upload`
Expected: Upload über COM5 erfolgreich.

- [ ] **Step 3: Funktion prüfen**

  1. Start zeigt `50%` zentriert, Arc bei 50%.
  2. Encoder drehen → Zahl + Arc ändern sich synchron, 0–100 geclamped.
  3. Mute → Zahl weg, „MUTE" sichtbar, Arc grau.
  4. Unmute → Zahl zurück, Arc cyan.
