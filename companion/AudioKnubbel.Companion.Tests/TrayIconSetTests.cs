using AudioKnubbel.Companion;
using Xunit;

public class TrayIconSetTests
{
    [Theory]
    [InlineData(true, "AudioKnubbel.Companion.audioknubbel.ico")]
    [InlineData(false, "AudioKnubbel.Companion.audioknubbel-disconnected.ico")]
    public void ResourceNameForConnectionState_SelectsExpectedTrayIcon(bool connected, string expected)
    {
        Assert.Equal(expected, TrayIconSet.ResourceNameForConnectionState(connected));
    }

    [Theory]
    [InlineData(true)]
    [InlineData(false)]
    public void LoadForConnectionState_LoadsValidIcon(bool connected)
    {
        using var icon = TrayIconSet.LoadForConnectionState(connected);

        Assert.True(icon.Width > 0);
        Assert.True(icon.Height > 0);
    }
}
