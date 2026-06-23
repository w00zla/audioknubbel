namespace AudioKnubbel.Companion;

public static class Protocol
{
    public static string VolumeLine(int volume) => $"VOL:{Math.Clamp(volume, 0, 100)}\n";
    public static string MuteLine(bool muted) => muted ? "MUTE:1\n" : "MUTE:0\n";
    public static string PingLine() => "PING\n";

    // Identifikations-Handshake: das Board antwortet auf "ID?" mit "AUDIOKNUBBEL <fw>".
    // Dient der Port-Discovery, den Firmware-Port vom Bootloader/Flash-Port (beide
    // Espressif-VID 303A) zu unterscheiden — nur das Board antwortet so.
    public static string IdentifyLine() => "ID?\n";

    public static bool IsIdentityReply(string? line)
        => line is not null && line.TrimStart().StartsWith("AUDIOKNUBBEL", StringComparison.Ordinal);

    // Flash-Vorbereitung: "BOOT?" bittet das Board, den Countdown zu starten; es
    // antwortet nach Ablauf mit "BOOTREADY" (-> dann löst die App den 1200-Baud-
    // Touch aus). Siehe protocol.h (ProtoCmd::EnterBoot).
    public static string BootRequestLine() => "BOOT?\n";

    public static bool IsBootReady(string? line)
        => line is not null && line.TrimStart().StartsWith("BOOTREADY", StringComparison.Ordinal);

    // Helligkeit (Backlight): App setzt 5..100; 0/aus ist dem Standby vorbehalten.
    public static string BrightnessLine(int v) => $"BRIGHT:{Math.Clamp(v, 5, 100)}\n";
    public static string QueryBrightnessLine() => "BRIGHT?\n";

    // Parst die Board-Antwort "BRIGHT:<n>" (auf BRIGHT?); clamp auf 5..100.
    // "BRIGHT?" selbst und andere Zeilen ergeben false.
    public static bool TryParseBrightness(string? line, out int value)
    {
        value = 0;
        if (line is null) return false;
        var s = line.Trim();
        const string prefix = "BRIGHT:";
        if (!s.StartsWith(prefix, StringComparison.Ordinal)) return false;
        if (!int.TryParse(s.AsSpan(prefix.Length), out int v)) return false;
        value = Math.Clamp(v, 5, 100);
        return true;
    }

    // Standby-Timeout in Sekunden: App setzt 5..60.
    public static string StandbyLine(int sec) => $"STBY:{Math.Clamp(sec, 5, 60)}\n";
    public static string QueryStandbyLine() => "STBY?\n";

    // Parst die Board-Antwort "STBY:<n>" (auf STBY?); clamp auf 5..60.
    // "STBY?" selbst und andere Zeilen ergeben false.
    public static bool TryParseStandby(string? line, out int value)
    {
        value = 0;
        if (line is null) return false;
        var s = line.Trim();
        const string prefix = "STBY:";
        if (!s.StartsWith(prefix, StringComparison.Ordinal)) return false;
        if (!int.TryParse(s.AsSpan(prefix.Length), out int v)) return false;
        value = Math.Clamp(v, 5, 60);
        return true;
    }
}
