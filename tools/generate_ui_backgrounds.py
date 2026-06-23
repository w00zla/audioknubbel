import math
from pathlib import Path

SIZE = 240
CENTER = 119.5
OUT = Path("src/ui_backgrounds.cpp")


def smoothstep(edge0, edge1, x):
    if edge0 == edge1:
        return 1.0 if x >= edge1 else 0.0
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def angle_distance(a, b):
    d = abs((a - b + 180.0) % 360.0 - 180.0)
    return d


def plasma_noise(x, y):
    return (
        0.50
        + 0.22 * math.sin(x * 0.105 + y * 0.031)
        + 0.18 * math.sin(x * 0.047 - y * 0.088 + 1.7)
        + 0.12 * math.sin((x + y) * 0.074 + math.sin(x * 0.025) * 2.0)
    )


def aurora_pattern(r, angle):
    a = math.radians(angle)
    tangential = (
        0.52
        + 0.24 * math.sin(5.0 * a + 0.055 * r)
        + 0.16 * math.sin(9.0 * a - 0.038 * r + 1.2)
        + 0.10 * math.sin(15.0 * a + 0.7)
    )
    radial_wave = 0.64 + 0.36 * math.sin((r - 76.0) * 0.33 + 2.8 * math.sin(3.0 * a))
    striation = 0.72 + 0.28 * math.sin(34.0 * a + r * 0.09)
    return max(0.0, min(1.0, tangential * radial_wave * striation))


def pixel(x, y, strength):
    dx = x - CENTER
    dy = y - CENTER
    r = math.sqrt(dx * dx + dy * dy)
    if r > 119.2:
        return 0, 0, 0

    angle = (math.degrees(math.atan2(dy, dx)) + 360.0) % 360.0
    rim = smoothstep(64.0, 91.0, r) * (1.0 - smoothstep(118.0, 121.0, r))
    outer_bias = 0.42 + 0.58 * smoothstep(88.0, 116.0, r)

    axis_guard = max(
        math.exp(-((angle_distance(angle, 0.0) / 13.0) ** 2)),
        math.exp(-((angle_distance(angle, 90.0) / 14.0) ** 2)),
        math.exp(-((angle_distance(angle, 180.0) / 13.0) ** 2)),
        math.exp(-((angle_distance(angle, 270.0) / 14.0) ** 2)),
    )

    plasma = 0.58 * aurora_pattern(r, angle) + 0.42 * max(0.0, plasma_noise(x, y))
    banding = 0.65 + 0.35 * smoothstep(0.42, 0.86, plasma)
    glow = strength * rim * outer_bias * (1.0 - 0.36 * axis_guard) * banding * plasma

    vignette = 0.035 * smoothstep(26.0, 116.0, r)
    base = int(2 + 7 * vignette)
    red = min(255, int(base + 126 * glow))
    green = min(255, int(base + 34 * glow))
    blue = min(255, int(base + 205 * glow))

    border = smoothstep(116.5, 119.0, r) * (1.0 - smoothstep(119.0, 120.0, r))
    red = min(255, red + int(11 * border))
    green = min(255, green + int(8 * border))
    blue = min(255, blue + int(16 * border))
    return red, green, blue


def rgb565_bytes(red, green, blue):
    value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
    return value & 0xFF, value >> 8


def emit_array(lines, name, strength):
    lines.append(f"const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t {name}_map[] = {{")
    row = []
    for y in range(SIZE):
        for x in range(SIZE):
            lo, hi = rgb565_bytes(*pixel(x, y, strength))
            row.extend((lo, hi))
        lines.append("  " + ", ".join(f"0x{b:02x}" for b in row) + ",")
        row.clear()
    lines.append("};")
    lines.append("")
    lines.append(f"const lv_img_dsc_t {name} = {{")
    lines.append("  UI_BG_HEADER,")
    lines.append(f"  {SIZE} * {SIZE} * LV_COLOR_SIZE / 8,")
    lines.append(f"  {name}_map,")
    lines.append("};")
    lines.append("")


def main():
    lines = [
        "#include \"ui_backgrounds.h\"",
        "",
        "#if LV_COLOR_DEPTH != 16",
        "#error \"UI backgrounds are generated as RGB565 and require LV_COLOR_DEPTH 16\"",
        "#endif",
        "",
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN",
        "#define LV_ATTRIBUTE_MEM_ALIGN",
        "#endif",
        "",
        "#ifndef LV_ATTRIBUTE_LARGE_CONST",
        "#define LV_ATTRIBUTE_LARGE_CONST",
        "#endif",
        "",
        "#if LV_BIG_ENDIAN_SYSTEM",
        "#define UI_BG_HEADER {240, 240, 0, 0, LV_IMG_CF_TRUE_COLOR}",
        "#else",
        "#define UI_BG_HEADER {LV_IMG_CF_TRUE_COLOR, 0, 0, 240, 240}",
        "#endif",
        "",
    ]

    emit_array(lines, "ui_bg_low", 0.58)
    emit_array(lines, "ui_bg_mid", 0.88)
    emit_array(lines, "ui_bg_high", 1.24)

    OUT.write_text("\n".join(lines), encoding="ascii", newline="\n")


if __name__ == "__main__":
    main()
