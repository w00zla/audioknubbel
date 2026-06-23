using System.Windows.Forms;

namespace AudioKnubbel.Companion;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        using var mutex = new Mutex(true, "AudioKnubbel.Companion.SingleInstance", out bool isNew);
        if (!isNew) return;   // bereits eine Instanz aktiv

        ApplicationConfiguration.Initialize();
        Application.Run(new TrayAppContext());
    }
}
