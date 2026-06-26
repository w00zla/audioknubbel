using AudioKnubbel.Companion;
using Xunit;

public class PortDiscoveryTests
{
    [Fact]
    public void IsAudioknubbelCandidate_RejectsEspressifCdcWithoutMatchingHidSibling()
    {
        const string pnpId = @"USB\VID_303A&PID_1001&MI_01\7&123456&0&0001";

        Assert.False(PortDiscovery.IsAudioknubbelCandidate(pnpId, [pnpId], "audioknubbel"));
    }

    [Fact]
    public void IsAudioknubbelCandidate_AcceptsExactAudioknubbelCompositeFirmwareCdc()
    {
        const string cdcId = @"USB\VID_303A&PID_1001&MI_01\D&547346B&0&0001";
        const string hidId = @"USB\VID_303A&PID_1001&MI_00\D&547346B&0&0000";

        Assert.True(PortDiscovery.IsAudioknubbelCandidate(cdcId, [cdcId, hidId], "audioknubbel"));
    }

    [Fact]
    public void IsAudioknubbelCandidate_RejectsAudioknubbel2CompositeFirmwareCdc()
    {
        const string cdcId = @"USB\VID_303A&PID_1001&MI_01\D&547346B&0&0001";
        const string hidId = @"USB\VID_303A&PID_1001&MI_00\D&547346B&0&0000";

        Assert.False(PortDiscovery.IsAudioknubbelCandidate(cdcId, [cdcId, hidId], "audioknubbel2"));
    }

    [Fact]
    public void IsAudioknubbelCandidate_RejectsBootloaderInterface()
    {
        const string bootloaderId = @"USB\VID_303A&PID_1001&MI_00\D&547346B&0&0000";

        Assert.False(PortDiscovery.IsAudioknubbelCandidate(bootloaderId, [bootloaderId], "audioknubbel"));
    }
}
