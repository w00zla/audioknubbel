using System.Drawing;

namespace AudioKnubbel.Companion;

public static class TrayIconSet
{
    public const string ConnectedResourceName = "AudioKnubbel.Companion.audioknubbel.ico";
    public const string DisconnectedResourceName = "AudioKnubbel.Companion.audioknubbel-disconnected.ico";

    public static string ResourceNameForConnectionState(bool connected) =>
        connected ? ConnectedResourceName : DisconnectedResourceName;

    public static Icon LoadForConnectionState(bool connected)
    {
        var asm = typeof(TrayIconSet).Assembly;
        using var stream = asm.GetManifestResourceStream(ResourceNameForConnectionState(connected));
        return stream is not null ? new Icon(stream) : SystemIcons.Application;
    }
}
