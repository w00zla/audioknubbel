using AudioKnubbel.Companion;
using Xunit;

public class ProtocolTests
{
    [Theory]
    [InlineData(42, "VOL:42\n")]
    [InlineData(0, "VOL:0\n")]
    [InlineData(100, "VOL:100\n")]
    [InlineData(150, "VOL:100\n")]
    [InlineData(-5, "VOL:0\n")]
    public void VolumeLine_ClampsAndFormats(int input, string expected)
        => Assert.Equal(expected, Protocol.VolumeLine(input));

    [Theory]
    [InlineData(true, "MUTE:1\n")]
    [InlineData(false, "MUTE:0\n")]
    public void MuteLine_Formats(bool muted, string expected)
        => Assert.Equal(expected, Protocol.MuteLine(muted));

    [Fact]
    public void PingLine_IsNewlineTerminated()
        => Assert.Equal("PING\n", Protocol.PingLine());

    [Fact]
    public void IdentifyLine_IsNewlineTerminated()
        => Assert.Equal("ID?\n", Protocol.IdentifyLine());

    [Theory]
    [InlineData("AUDIOKNUBBEL", true)]
    [InlineData("AUDIOKNUBBEL 1.0", true)]
    [InlineData("AUDIOKNUBBEL M3\r", true)]
    [InlineData("  AUDIOKNUBBEL", true)]            // führende Whitespaces tolerieren
    [InlineData("[BOOT] audioknubbel ready", false)] // Boot-Zeile ist keine ID-Antwort
    [InlineData("", false)]
    [InlineData(null, false)]
    public void IsIdentityReply_RecognizesBoardOnly(string? line, bool expected)
        => Assert.Equal(expected, Protocol.IsIdentityReply(line));

    [Fact]
    public void BootRequestLine_IsNewlineTerminated()
        => Assert.Equal("BOOT?\n", Protocol.BootRequestLine());

    [Theory]
    [InlineData("BOOTREADY", true)]
    [InlineData("BOOTREADY\r", true)]
    [InlineData("  BOOTREADY", true)]              // führende Whitespaces tolerieren
    [InlineData("AUDIOKNUBBEL M3", false)]             // ID-Antwort ist kein BOOTREADY
    [InlineData("BOOT?", false)]                   // das Kommando selbst nicht
    [InlineData("", false)]
    [InlineData(null, false)]
    public void IsBootReady_RecognizesReadyOnly(string? line, bool expected)
        => Assert.Equal(expected, Protocol.IsBootReady(line));

    [Theory]
    [InlineData(55, "BRIGHT:55\n")]
    [InlineData(5, "BRIGHT:5\n")]
    [InlineData(100, "BRIGHT:100\n")]
    [InlineData(0, "BRIGHT:5\n")]      // unter Minimum -> 5
    [InlineData(4, "BRIGHT:5\n")]      // unter Minimum -> 5
    [InlineData(150, "BRIGHT:100\n")]  // über Maximum -> 100
    [InlineData(-10, "BRIGHT:5\n")]
    public void BrightnessLine_ClampsAndFormats(int input, string expected)
        => Assert.Equal(expected, Protocol.BrightnessLine(input));

    [Fact]
    public void QueryBrightnessLine_IsNewlineTerminated()
        => Assert.Equal("BRIGHT?\n", Protocol.QueryBrightnessLine());

    [Theory]
    [InlineData("BRIGHT:55", true, 55)]
    [InlineData("BRIGHT:5\r", true, 5)]
    [InlineData("  BRIGHT:100", true, 100)]   // führende Whitespaces tolerieren
    [InlineData("BRIGHT:0", true, 5)]         // Antwort unter Minimum -> 5
    [InlineData("BRIGHT:150", true, 100)]     // Antwort über Maximum -> 100
    [InlineData("BRIGHT?", false, 0)]         // das Query-Kommando ist keine Antwort
    [InlineData("VOL:55", false, 0)]
    [InlineData("", false, 0)]
    [InlineData(null, false, 0)]
    public void TryParseBrightness_ParsesReply(string? line, bool ok, int expected)
    {
        bool result = Protocol.TryParseBrightness(line, out int value);
        Assert.Equal(ok, result);
        if (ok) Assert.Equal(expected, value);
    }

    [Theory]
    [InlineData(15, "STBY:15\n")]
    [InlineData(5, "STBY:5\n")]
    [InlineData(60, "STBY:60\n")]
    [InlineData(0, "STBY:5\n")]      // unter Minimum -> 5
    [InlineData(4, "STBY:5\n")]      // unter Minimum -> 5
    [InlineData(90, "STBY:60\n")]    // über Maximum -> 60
    [InlineData(-10, "STBY:5\n")]
    public void StandbyLine_ClampsAndFormats(int input, string expected)
        => Assert.Equal(expected, Protocol.StandbyLine(input));

    [Fact]
    public void QueryStandbyLine_IsNewlineTerminated()
        => Assert.Equal("STBY?\n", Protocol.QueryStandbyLine());

    [Theory]
    [InlineData("STBY:15", true, 15)]
    [InlineData("STBY:5\r", true, 5)]
    [InlineData("  STBY:60", true, 60)]      // führende Whitespaces tolerieren
    [InlineData("STBY:0", true, 5)]          // Antwort unter Minimum -> 5
    [InlineData("STBY:90", true, 60)]        // Antwort über Maximum -> 60
    [InlineData("STBY?", false, 0)]          // das Query-Kommando ist keine Antwort
    [InlineData("VOL:15", false, 0)]
    [InlineData("", false, 0)]
    [InlineData(null, false, 0)]
    public void TryParseStandby_ParsesReply(string? line, bool ok, int expected)
    {
        bool result = Protocol.TryParseStandby(line, out int value);
        Assert.Equal(ok, result);
        if (ok) Assert.Equal(expected, value);
    }
}
