import math
from pathlib import Path

OUT = Path("docs/mockups")
SIZE = 240
CENTER = 120
TICKS = 37
ARC_START = 135
ARC_SWEEP = 270


def zone_color(i):
    if i < TICKS // 3:
        return "#00dc5a"
    if i < 2 * (TICKS // 3):
        return "#ffaa00"
    return "#ff3232"


def plasma_defs(strength):
    alpha = {
        "low": (0.22, 0.15),
        "mid": (0.36, 0.25),
        "high": (0.54, 0.38),
    }[strength]
    return f"""
  <filter id="softGlow" x="-30%" y="-30%" width="160%" height="160%">
    <feGaussianBlur stdDeviation="5"/>
  </filter>
  <radialGradient id="bg" cx="50%" cy="50%" r="58%">
    <stop offset="0%" stop-color="#030404"/>
    <stop offset="62%" stop-color="#050606"/>
    <stop offset="100%" stop-color="#101012"/>
  </radialGradient>
  <linearGradient id="aurora" x1="0%" y1="0%" x2="100%" y2="0%">
    <stop offset="0%" stop-color="#40105f" stop-opacity="0"/>
    <stop offset="14%" stop-color="#9a35ff" stop-opacity="{alpha[0]}"/>
    <stop offset="28%" stop-color="#ea4aff" stop-opacity="{alpha[1]}"/>
    <stop offset="42%" stop-color="#6b26ff" stop-opacity="{alpha[0]}"/>
    <stop offset="56%" stop-color="#c348ff" stop-opacity="{alpha[1]}"/>
    <stop offset="72%" stop-color="#7f2dff" stop-opacity="{alpha[0]}"/>
    <stop offset="88%" stop-color="#d04aff" stop-opacity="{alpha[1]}"/>
    <stop offset="100%" stop-color="#40105f" stop-opacity="0"/>
  </linearGradient>
"""


def plasma_shapes():
    parts = []
    for idx, radius in enumerate((112, 105, 98, 91)):
        dash = "9 7" if idx % 2 == 0 else "15 10"
        opacity = 0.46 - idx * 0.065
        parts.append(
            f'    <circle cx="120" cy="120" r="{radius}" fill="none" stroke="url(#aurora)" '
            f'stroke-width="{12 - idx * 2}" stroke-dasharray="{dash}" stroke-linecap="round" '
            f'opacity="{opacity:.2f}" filter="url(#softGlow)" transform="rotate({idx * 17} 120 120)"/>'
        )
    for deg in range(22, 360, 18):
        if min(abs((deg - axis + 180) % 360 - 180) for axis in (0, 90, 180, 270)) < 15:
            continue
        rad = math.radians(deg)
        x1 = 120 + 88 * math.cos(rad)
        y1 = 120 + 88 * math.sin(rad)
        x2 = 120 + 116 * math.cos(rad)
        y2 = 120 + 116 * math.sin(rad)
        parts.append(
            f'    <line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
            f'stroke="#d65cff" stroke-width=".7" opacity=".18"/>'
        )
    return "\n".join(parts)


def ticks(value):
    lit = round(value * (TICKS - 1) / 100)
    parts = []
    for i in range(TICKS):
        frac = i / (TICKS - 1)
        deg = ARC_START + frac * ARC_SWEEP
        rad = math.radians(deg)
        x1 = CENTER + 86 * math.cos(rad)
        y1 = CENTER + 86 * math.sin(rad)
        x2 = CENTER + 100 * math.cos(rad)
        y2 = CENTER + 100 * math.sin(rad)
        color = zone_color(i) if i <= lit else "#2d2d2d"
        glow = ""
        if i <= lit:
            glow = f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{color}" stroke-width="8" stroke-linecap="round" opacity=".20"/>'
        parts.append(glow)
        parts.append(
            f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" '
            f'stroke="{color}" stroke-width="3" stroke-linecap="round"/>'
        )
    return "\n    ".join(p for p in parts if p)


def media_icons():
    return """
  <g fill="#d9ffff" opacity=".82" filter="url(#iconGlow)">
    <path d="M109 20 L109 36 L123 28 Z"/>
    <rect x="128" y="20" width="4" height="16" rx="1"/>
    <rect x="136" y="20" width="4" height="16" rx="1"/>
    <rect x="16" y="113" width="3" height="14" rx="1"/>
    <path d="M36 111 L24 120 L36 129 Z"/>
    <path d="M25 111 L13 120 L25 129 Z"/>
    <rect x="221" y="113" width="3" height="14" rx="1"/>
    <path d="M204 111 L216 120 L204 129 Z"/>
    <path d="M215 111 L227 120 L215 129 Z"/>
  </g>
"""


def render(name, label, value, strength):
    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" width="720" height="720" viewBox="0 0 240 240">
<defs>
{plasma_defs(strength)}
  <filter id="iconGlow" x="-80%" y="-80%" width="260%" height="260%">
    <feGaussianBlur stdDeviation="1.4" result="blur"/>
    <feMerge><feMergeNode in="blur"/><feMergeNode in="SourceGraphic"/></feMerge>
  </filter>
  <filter id="textGlow" x="-80%" y="-80%" width="260%" height="260%">
    <feGaussianBlur stdDeviation="1.2" result="blur"/>
    <feMerge><feMergeNode in="blur"/><feMergeNode in="SourceGraphic"/></feMerge>
  </filter>
</defs>
<rect width="240" height="240" fill="#000"/>
<circle cx="120" cy="120" r="118" fill="url(#bg)" stroke="#262629" stroke-width=".8"/>
<g clip-path="circle(118px at 120px 120px)">
{plasma_shapes()}
</g>
<circle cx="120" cy="120" r="118" fill="none" stroke="#333338" stroke-width=".4" opacity=".65"/>
<circle cx="120" cy="120" r="47" fill="#060707" stroke="#27272b" stroke-width=".9"/>
<g>
    {ticks(value)}
</g>
{media_icons()}
<text x="120" y="130" text-anchor="middle" font-family="Segoe UI, Arial, sans-serif" font-size="34" font-weight="300" fill="#ddffff" filter="url(#textGlow)">{value}%</text>
<circle cx="120" cy="216" r="3.2" fill="#ff2828" filter="url(#iconGlow)"/>
<text x="120" y="235" text-anchor="middle" font-family="Segoe UI, Arial, sans-serif" font-size="5" fill="#8f8f9a">{label}</text>
</svg>
"""
    (OUT / name).write_text(svg, encoding="ascii", newline="\n")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    render("volume-bg-low.svg", "LOW 0-33% weak plasma", 20, "low")
    render("volume-bg-mid.svg", "MID 34-66% medium plasma", 50, "mid")
    render("volume-bg-high.svg", "HIGH 67-100% strong plasma", 85, "high")


if __name__ == "__main__":
    main()
