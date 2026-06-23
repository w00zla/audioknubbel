using Microsoft.Win32;

namespace AudioKnubbel.Companion;

// Verwaltet den Windows-Autostart über den HKCU "Run"-Key. Nutzerlokal,
// daher kein Admin nötig. Der Wert zeigt auf die aktuell laufende Exe
// (Environment.ProcessPath) — wer aus dist/ startet, registriert dist/.
public static class AutostartManager
{
    private const string RunKey = @"Software\Microsoft\Windows\CurrentVersion\Run";
    private const string ValueName = "AudioKnubbel.Companion";

    private static string ExePath => Environment.ProcessPath ?? "";

    // true, wenn ein Run-Eintrag existiert und auf die aktuelle Exe zeigt.
    public static bool IsEnabled
    {
        get
        {
            using var key = Registry.CurrentUser.OpenSubKey(RunKey);
            var value = key?.GetValue(ValueName) as string;
            return !string.IsNullOrEmpty(value)
                && string.Equals(Unquote(value), ExePath, StringComparison.OrdinalIgnoreCase);
        }
    }

    public static void Enable()
    {
        using var key = Registry.CurrentUser.CreateSubKey(RunKey);
        key.SetValue(ValueName, $"\"{ExePath}\"");
    }

    public static void Disable()
    {
        using var key = Registry.CurrentUser.OpenSubKey(RunKey, writable: true);
        key?.DeleteValue(ValueName, throwOnMissingValue: false);
    }

    private static string Unquote(string s) => s.Trim('"');
}
