using AudioKnubbel.Companion;

public sealed class FakeSink : ISerialSink
{
    public bool Connected { get; set; } = true;
    public List<string> Sent { get; } = new();
    public void Send(string line) => Sent.Add(line);
}
