using NAudio.CoreAudioApi;
using NAudio.CoreAudioApi.Interfaces;

namespace AudioKnubbel.Companion;

// Liest Master-Volume + Mute des Default-Render-Geräts und meldet Änderungen.
// Folgt automatisch dem Default-Device (Kopfhörer rein/raus) via IMMNotificationClient.
public sealed class VolumeMonitor : IVolumeSource, IMMNotificationClient, IDisposable
{
    private readonly MMDeviceEnumerator _enumerator = new();
    private readonly object _gate = new();
    private MMDevice? _device;

    public event Action<AudioState>? StateChanged;

    public VolumeMonitor()
    {
        _enumerator.RegisterEndpointNotificationCallback(this);
        Attach();
    }

    public AudioState Current
    {
        get
        {
            lock (_gate)
            {
                if (_device is null) return new AudioState(0, false);
                var vol = (int)Math.Round(
                    _device.AudioEndpointVolume.MasterVolumeLevelScalar * 100);
                return new AudioState(vol, _device.AudioEndpointVolume.Mute);
            }
        }
    }

    private void Attach()
    {
        lock (_gate)
        {
            Detach();
            try
            {
                _device = _enumerator.GetDefaultAudioEndpoint(DataFlow.Render, Role.Multimedia);
                _device.AudioEndpointVolume.OnVolumeNotification += OnVolumeNotification;
            }
            catch { _device = null; }
        }
        StateChanged?.Invoke(Current);
    }

    private void Detach()
    {
        if (_device is null) return;
        try { _device.AudioEndpointVolume.OnVolumeNotification -= OnVolumeNotification; } catch { }
        try { _device.Dispose(); } catch { }
        _device = null;
    }

    private void OnVolumeNotification(AudioVolumeNotificationData data)
        => StateChanged?.Invoke(Current);

    // --- IMMNotificationClient ---
    public void OnDefaultDeviceChanged(DataFlow flow, Role role, string defaultDeviceId)
    {
        // WICHTIG: Aus einem IMMNotificationClient-Callback NICHT synchron zurück in
        // den MMDeviceEnumerator rufen (GetDefaultAudioEndpoint) — das deadlockt auf
        // dem Notification-Thread. Re-Attach daher auf einen Threadpool-Thread auslagern.
        if (flow == DataFlow.Render && role == Role.Multimedia)
            Task.Run(Attach);
    }
    public void OnDeviceStateChanged(string deviceId, DeviceState newState) { }
    public void OnDeviceAdded(string pwstrDeviceId) { }
    public void OnDeviceRemoved(string deviceId) { }
    public void OnPropertyValueChanged(string pwstrDeviceId, PropertyKey key) { }

    public void Dispose()
    {
        try { _enumerator.UnregisterEndpointNotificationCallback(this); } catch { }
        lock (_gate) Detach();
        _enumerator.Dispose();
    }
}
