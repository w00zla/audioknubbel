using System.IO.Ports;

namespace AudioKnubbel.Companion;

// SerialPort-Wrapper mit VID/PID-Discovery + Auto-Reconnect.
public sealed class SerialLink : ISerialSink, IDisposable
{
    private readonly object _gate = new();
    private SerialPort? _port;

    public event Action<bool>? ConnectionChanged;

    public bool Connected
    {
        get { lock (_gate) return _port?.IsOpen == true; }
    }

    // Name des aktuell verbundenen Ports (z. B. "COM6"), sonst null.
    public string? PortName { get; private set; }

    // Versucht zu (re)connecten; true bei Erfolg. Feuert ConnectionChanged(true).
    public bool TryConnect()
    {
        lock (_gate)
        {
            if (_port?.IsOpen == true) return true;

            var portName = PortDiscovery.FindPort();
            if (portName is null) return false;
            try
            {
                var p = new SerialPort(portName, 115200)
                {
                    NewLine = "\n",
                    WriteTimeout = 500,
                    ReadTimeout = 500,
                    // ESP32-S3-USB-CDC sendet nur (z. B. die AUDIOKNUBBEL-Antwort auf
                    // ID?), wenn der Host DTR setzt. Ohne das schlägt der Handshake
                    // fehl und es wird nie verbunden. Löst keinen Reset aus.
                    DtrEnable = true,
                    RtsEnable = true,
                };
                p.Open();

                // Handshake: nur behalten, wenn das Board mit AUDIOKNUBBEL antwortet.
                // So latcht die Discovery nicht auf dem Bootloader-/Flash-Port, der
                // dieselbe Espressif-VID (303A) trägt, aber nicht auf ID? antwortet.
                if (!Handshake(p))
                {
                    try { p.Close(); } catch { }
                    p.Dispose();
                    _port = null;
                    return false;
                }
                _port = p;
                PortName = portName;
            }
            catch
            {
                _port = null;
                return false;
            }
        }
        ConnectionChanged?.Invoke(true);
        return true;
    }

    // Sendet ID? und prüft, ob eine AUDIOKNUBBEL-Antwort kommt. Überspringt dabei
    // bis zu ein paar Fremdzeilen (z. B. ein verspätetes [BOOT]…), bricht bei
    // Timeout/Fehler ab (= falscher Port, etwa der Bootloader).
    private static bool Handshake(SerialPort p)
    {
        try
        {
            // Das Board sendet nicht-blockierend (setTxTimeoutMs(0)) und verwirft
            // die Antwort, solange es DTR noch nicht erkannt hat. Daher nach dem
            // Öffnen kurz settlen und ID? notfalls mehrfach senden.
            System.Threading.Thread.Sleep(250);
            for (int round = 0; round < 3; round++)
            {
                p.DiscardInBuffer();
                p.Write(Protocol.IdentifyLine());
                try
                {
                    for (int i = 0; i < 3; i++)
                        if (Protocol.IsIdentityReply(p.ReadLine())) return true;
                }
                catch { /* Timeout -> nächste Runde, ID? erneut senden */ }
            }
        }
        catch { /* kein Board an diesem Port */ }
        return false;
    }

    // Bittet das Board, den Flash-Countdown zu starten ("BOOT?"), und wartet bis
    // zu timeoutMs auf die "BOOTREADY"-Antwort. true nur bei BOOTREADY. Erwartet,
    // dass parallele Port-Nutzer (Heartbeat/Reconnect) währenddessen pausiert sind.
    public bool RequestBootCountdown(int timeoutMs)
    {
        SerialPort? p;
        lock (_gate)
        {
            p = _port;
            if (p?.IsOpen != true) return false;
        }
        try
        {
            var deadline = DateTime.UtcNow.AddMilliseconds(timeoutMs);
            p.DiscardInBuffer();
            p.Write(Protocol.BootRequestLine());
            while (DateTime.UtcNow < deadline)
            {
                int remaining = (int)(deadline - DateTime.UtcNow).TotalMilliseconds;
                p.ReadTimeout = Math.Clamp(remaining, 50, timeoutMs);
                try
                {
                    if (Protocol.IsBootReady(p.ReadLine())) return true;
                }
                catch (TimeoutException) { break; }
            }
        }
        catch { /* Port-Fehler -> Fehlschlag */ }
        return false;
    }

    // Fragt die aktuelle Board-Helligkeit ab ("BRIGHT?") und wartet bis timeoutMs
    // auf die "BRIGHT:n"-Antwort. Gibt den Wert (5..100) zurück oder null. Der
    // Schreibvorgang läuft unter _gate (kein Byte-Interleave mit dem Heartbeat-PING),
    // das Lesen danach außerhalb (einziger Reader im Normalbetrieb).
    public int? QueryBrightness(int timeoutMs)
    {
        SerialPort? p;
        lock (_gate)
        {
            p = _port;
            if (p?.IsOpen != true) return null;
            try { p.DiscardInBuffer(); p.Write(Protocol.QueryBrightnessLine()); }
            catch { return null; }
        }
        try
        {
            var deadline = DateTime.UtcNow.AddMilliseconds(timeoutMs);
            while (DateTime.UtcNow < deadline)
            {
                int remaining = (int)(deadline - DateTime.UtcNow).TotalMilliseconds;
                p.ReadTimeout = Math.Clamp(remaining, 50, timeoutMs);
                try
                {
                    if (Protocol.TryParseBrightness(p.ReadLine(), out int v)) return v;
                }
                catch (TimeoutException) { break; }
            }
        }
        catch { /* Port-Fehler -> kein Wert */ }
        return null;
    }

    // Fragt den aktuellen Standby-Timeout ab ("STBY?") und wartet bis timeoutMs auf
    // die "STBY:n"-Antwort. Gibt den Wert (5..60) zurück oder null. Schreibvorgang
    // unter _gate (kein Byte-Interleave mit dem Heartbeat-PING), Lesen danach.
    public int? QueryStandby(int timeoutMs)
    {
        SerialPort? p;
        lock (_gate)
        {
            p = _port;
            if (p?.IsOpen != true) return null;
            try { p.DiscardInBuffer(); p.Write(Protocol.QueryStandbyLine()); }
            catch { return null; }
        }
        try
        {
            var deadline = DateTime.UtcNow.AddMilliseconds(timeoutMs);
            while (DateTime.UtcNow < deadline)
            {
                int remaining = (int)(deadline - DateTime.UtcNow).TotalMilliseconds;
                p.ReadTimeout = Math.Clamp(remaining, 50, timeoutMs);
                try
                {
                    if (Protocol.TryParseStandby(p.ReadLine(), out int v)) return v;
                }
                catch (TimeoutException) { break; }
            }
        }
        catch { /* Port-Fehler -> kein Wert */ }
        return null;
    }

    // Gibt den Firmware-Port frei (Handle schließen) und meldet getrennt. Genutzt,
    // nachdem das Board den Flash-Countdown bestätigt hat: das Board löst den
    // ROM-Download-Modus selbst aus (usb_persist_restart in der Firmware) und
    // verschwindet von diesem Port — wir müssen den Handle nur sauber loslassen.
    public void Disconnect()
    {
        bool wasConnected;
        lock (_gate)
        {
            wasConnected = _port?.IsOpen == true;
            try { _port?.Close(); } catch { }
            _port?.Dispose();
            _port = null;
            PortName = null;
        }
        if (wasConnected) ConnectionChanged?.Invoke(false);
    }

    public void Send(string line)
    {
        bool dropped = false;
        lock (_gate)
        {
            if (_port?.IsOpen != true) return;
            try { _port.Write(line); }
            catch
            {
                try { _port.Close(); } catch { }
                _port.Dispose();
                _port = null;
                PortName = null;
                dropped = true;
            }
        }
        if (dropped) ConnectionChanged?.Invoke(false);
    }

    public void Dispose()
    {
        lock (_gate)
        {
            try { _port?.Close(); } catch { }
            _port?.Dispose();
            _port = null;
            PortName = null;
        }
    }
}
