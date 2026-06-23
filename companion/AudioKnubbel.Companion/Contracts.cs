namespace AudioKnubbel.Companion;

public readonly record struct AudioState(int Volume, bool Muted);

public interface IVolumeSource
{
    AudioState Current { get; }
    event Action<AudioState>? StateChanged;
}

public interface ISerialSink
{
    bool Connected { get; }
    void Send(string line);
}
