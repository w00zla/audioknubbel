using AudioKnubbel.Companion;
using Xunit;

public class SyncControllerTests
{
    [Fact]
    public void FirstSync_SendsFullState()
    {
        var sink = new FakeSink();
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        Assert.Equal(new[] { "VOL:42\n", "MUTE:0\n" }, sink.Sent);
    }

    [Fact]
    public void RepeatSameState_SendsNothing()
    {
        var sink = new FakeSink();
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        sink.Sent.Clear();
        c.Sync(new AudioState(42, false));
        Assert.Empty(sink.Sent);
    }

    [Fact]
    public void VolumeChange_SendsOnlyVolume()
    {
        var sink = new FakeSink();
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        sink.Sent.Clear();
        c.Sync(new AudioState(43, false));
        Assert.Equal(new[] { "VOL:43\n" }, sink.Sent);
    }

    [Fact]
    public void MuteChange_SendsOnlyMute()
    {
        var sink = new FakeSink();
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        sink.Sent.Clear();
        c.Sync(new AudioState(42, true));
        Assert.Equal(new[] { "MUTE:1\n" }, sink.Sent);
    }

    [Fact]
    public void Disconnected_SendsNothing()
    {
        var sink = new FakeSink { Connected = false };
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        Assert.Empty(sink.Sent);
    }

    [Fact]
    public void AfterReset_SendsFullStateAgain()
    {
        var sink = new FakeSink();
        var c = new SyncController(sink);
        c.Sync(new AudioState(42, false));
        sink.Sent.Clear();
        c.Reset();
        c.Sync(new AudioState(42, false));
        Assert.Equal(new[] { "VOL:42\n", "MUTE:0\n" }, sink.Sent);
    }
}
