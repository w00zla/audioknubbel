namespace AudioKnubbel.Companion;

// Kern-Logik (testbar): spiegelt Audio-State auf den Serial-Sink.
// - Nichts senden, wenn nicht verbunden
// - Nur Senden bei tatsächlicher Änderung (Dedup pro Feld)
// - Reset() erzwingt beim nächsten Sync den vollen State (für Reconnect)
public sealed class SyncController
{
    private readonly ISerialSink _sink;
    private AudioState? _lastSent;

    public SyncController(ISerialSink sink) => _sink = sink;

    public void Reset() => _lastSent = null;

    public void Sync(AudioState s)
    {
        if (!_sink.Connected) return;

        if (_lastSent is { } last)
        {
            if (s.Volume != last.Volume) _sink.Send(Protocol.VolumeLine(s.Volume));
            if (s.Muted != last.Muted)   _sink.Send(Protocol.MuteLine(s.Muted));
        }
        else
        {
            _sink.Send(Protocol.VolumeLine(s.Volume));
            _sink.Send(Protocol.MuteLine(s.Muted));
        }
        _lastSent = s;
    }
}
