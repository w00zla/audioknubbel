using System.Drawing;
using System.Windows.Forms;

namespace AudioKnubbel.Companion;

// Verdrahtet VolumeMonitor -> 40ms-Coalescing -> SyncController -> SerialLink,
// hält ein Tray-Icon und einen Reconnect-Timer.
public sealed class TrayAppContext : ApplicationContext
{
    private readonly VolumeMonitor _monitor = new();
    private readonly SerialLink _link = new();
    private readonly SyncController _sync;
    private readonly Icon _connectedIcon = TrayIconSet.LoadForConnectionState(connected: true);
    private readonly Icon _disconnectedIcon = TrayIconSet.LoadForConnectionState(connected: false);
    private readonly NotifyIcon _tray;
    private ToolStripMenuItem _statusItem = null!;   // zeigt Port + Verbindungsstatus
    private ToolStripMenuItem _brightnessMenu = null!;   // Untermenü „Helligkeit"
    private int _brightness = 100;                        // gecachter Board-Wert (Haken)
    private ToolStripMenuItem _standbyMenu = null!;       // Untermenü „Standby"
    private int _standby = 15;                            // gecachter Board-Wert (Haken)
    private readonly System.Windows.Forms.Timer _reconnect;
    private readonly System.Windows.Forms.Timer _debounce;
    private readonly System.Windows.Forms.Timer _heartbeat;
    private volatile bool _reconnecting;   // verhindert überlappende Connect-Versuche

    // Verstecktes Control nur fürs Thread-Marshalling: Handle wird im Ctor
    // erzwungen, damit BeginInvoke ab sofort auf den UI-Thread posten kann.
    // (SynchronizationContext.Current ist im Ctor noch null, da dieser vor
    //  Application.Run als dessen Argument ausgewertet wird.)
    private readonly Control _marshal = new();

    private AudioState _pending;
    private bool _hasPending;

    public TrayAppContext()
    {
        _ = _marshal.Handle;   // Handle-Erzeugung -> BeginInvoke wird nutzbar
        _sync = new SyncController(_link);

        _tray = new NotifyIcon
        {
            Icon = _disconnectedIcon,
            Text = "audioknubbel: suche Board…",
            Visible = true,
            ContextMenuStrip = BuildMenu(),
        };

        _link.ConnectionChanged += OnConnectionChanged;
        _monitor.StateChanged += OnVolumeStateChanged;

        _debounce = new System.Windows.Forms.Timer { Interval = 40 };
        _debounce.Tick += (_, _) =>
        {
            _debounce.Stop();
            if (_hasPending) { _hasPending = false; _sync.Sync(_pending); }
        };

        // TryConnect() blockiert (WMI-Discovery + Handshake mit Sleeps/ReadTimeouts,
        // bis zu ~4,75 s, wenn ein Espressif-Port da ist aber nicht antwortet). Der
        // Tick läuft auf dem UI-Thread -> liefe das synchron, fröre das Tray bei jedem
        // Versuch ein. Darum auf einen Background-Thread auslagern (wie QueryConfigAsync);
        // ConnectionChanged marshallt sich via Post selbst zurück. _reconnecting verhindert,
        // dass sich langsame Versuche stapeln, solange der 5-s-Timer weiterläuft.
        _reconnect = new System.Windows.Forms.Timer { Interval = 5000 };
        _reconnect.Tick += (_, _) => BeginReconnect();
        _reconnect.Start();

        // Heartbeat: hält den Disconnect-Punkt am Board fern, solange verbunden.
        // Board-Timeout liegt bei 3s -> 1s-Intervall toleriert einzelne Aussetzer.
        _heartbeat = new System.Windows.Forms.Timer { Interval = 1000 };
        _heartbeat.Tick += (_, _) => { if (_link.Connected) _link.Send(Protocol.PingLine()); };
        _heartbeat.Start();

        BeginReconnect();   // sofortiger erster Versuch (im Hintergrund)
    }

    // Stößt einen (Re)Connect-Versuch auf einem Background-Thread an. TryConnect()
    // blockiert (WMI-Discovery + Handshake mit Sleeps/ReadTimeouts, bis zu ~4,75 s,
    // wenn ein Espressif-Port da ist, aber nicht antwortet). Würde das auf dem
    // UI-Thread laufen (Timer-Tick / Ctor vor Application.Run), fröre das Tray bei
    // jedem Versuch ein. ConnectionChanged marshallt sich via Post selbst zurück;
    // _reconnecting verhindert, dass sich langsame Versuche stapeln.
    private void BeginReconnect()
    {
        if (_link.Connected || _reconnecting) return;
        _reconnecting = true;
        System.Threading.Tasks.Task.Run(() =>
        {
            try { _link.TryConnect(); }
            finally { _reconnecting = false; }
        });
    }

    private ContextMenuStrip BuildMenu()
    {
        var menu = new ContextMenuStrip();

        // Status-Zeile (deaktiviert): zeigt Port + ob verbunden, live aktualisiert.
        _statusItem = new ToolStripMenuItem("Status: …") { Enabled = false };
        menu.Items.Add(_statusItem);
        menu.Items.Add(new ToolStripSeparator());

        // Kein manuelles "Reconnect"-Item: der _reconnect-Timer (5 s) verbindet eh
        // automatisch neu, solange getrennt.
        var autostart = new ToolStripMenuItem("Mit Windows starten")
        {
            CheckOnClick = true,
        };
        autostart.Click += (_, _) =>
        {
            if (autostart.Checked) AutostartManager.Enable();
            else AutostartManager.Disable();
        };
        // Haken + Status bei jedem Öffnen frisch spiegeln.
        menu.Opening += (_, _) =>
        {
            autostart.Checked = AutostartManager.IsEnabled;
            UpdateStatus();
            _brightnessMenu.Enabled = _link.Connected;
            UpdateBrightnessChecks();
            _standbyMenu.Enabled = _link.Connected;
            UpdateStandbyChecks();
        };
        menu.Items.Add(autostart);

        menu.Items.Add(new ToolStripSeparator());
        _brightnessMenu = BuildBrightnessMenu();
        menu.Items.Add(_brightnessMenu);

        _standbyMenu = BuildStandbyMenu();
        menu.Items.Add(_standbyMenu);

#if DEVELOPER_MENU
        menu.Items.Add(new ToolStripSeparator());
        // Flash-Item bewusst eine Ebene tiefer unter „DEVELOPER", damit es nicht
        // versehentlich angeklickt wird. Nur kompiliert, wenn DEVELOPER_MENU gesetzt
        // ist (csproj: EnableDeveloperMenu, default true).
        var developer = new ToolStripMenuItem("DEVELOPER");
        developer.DropDownItems.Add("! In Flash-Mode versetzen", null, (_, _) => EnterBootloader());
        menu.Items.Add(developer);
#endif
        menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("Exit", null, (_, _) => ExitThread());
        return menu;
    }

    // Untermenü „Helligkeit": 5..100 % in 5er-Schritten; 5/25/50/75/100 fett.
    private ToolStripMenuItem BuildBrightnessMenu()
    {
        var root = new ToolStripMenuItem("Helligkeit");
        for (int pct = 5; pct <= 100; pct += 5)
        {
            int value = pct;
            var item = new ToolStripMenuItem($"{pct}%") { Tag = value };
            if (pct == 5 || pct == 25 || pct == 50 || pct == 75 || pct == 100)
                item.Font = new Font(item.Font, FontStyle.Bold);
            item.Click += (_, _) => SetBrightness(value);
            root.DropDownItems.Add(item);
        }
        return root;
    }

    // Setzt die Helligkeit am Board (BRIGHT:n) und spiegelt den Haken.
    private void SetBrightness(int pct)
    {
        _brightness = pct;
        _link.Send(Protocol.BrightnessLine(pct));
        UpdateBrightnessChecks();
    }

    // Haken auf den gecachten Wert setzen.
    private void UpdateBrightnessChecks()
    {
        foreach (ToolStripMenuItem item in _brightnessMenu.DropDownItems)
            item.Checked = (int)item.Tag! == _brightness;
    }

    // Untermenü „Standby": 5..60 s in 5er-Schritten; 5/15/30/60 fett.
    private ToolStripMenuItem BuildStandbyMenu()
    {
        var root = new ToolStripMenuItem("Standby");
        for (int sec = 5; sec <= 60; sec += 5)
        {
            int value = sec;
            var item = new ToolStripMenuItem($"{sec} s") { Tag = value };
            if (sec == 5 || sec == 15 || sec == 30 || sec == 60)
                item.Font = new Font(item.Font, FontStyle.Bold);
            item.Click += (_, _) => SetStandby(value);
            root.DropDownItems.Add(item);
        }
        return root;
    }

    // Setzt den Standby-Timeout am Board (STBY:n) und spiegelt den Haken.
    private void SetStandby(int sec)
    {
        _standby = sec;
        _link.Send(Protocol.StandbyLine(sec));
        UpdateStandbyChecks();
    }

    // Haken auf den gecachten Wert setzen.
    private void UpdateStandbyChecks()
    {
        foreach (ToolStripMenuItem item in _standbyMenu.DropDownItems)
            item.Checked = (int)item.Tag! == _standby;
    }

    // Beim Connect Helligkeit + Standby vom Board abfragen und die Haken setzen.
    // WICHTIG: streng sequenziell auf EINEM Background-Thread — niemals zwei parallele
    // Port-Leser. Sonst klauen sich die Abfragen gegenseitig die Antwort (concurrent
    // ReadLine + das DiscardInBuffer der zweiten Abfrage verwirft die erste), und das
    // Menü bleibt auf dem Default-Wert stehen. Board ist Quelle der Wahrheit (NVS).
    private void QueryConfigAsync()
    {
        System.Threading.Tasks.Task.Run(() =>
        {
            int? b = _link.QueryBrightness(700);
            if (b is int bv) Post(() => { _brightness = bv; UpdateBrightnessChecks(); });
            int? s = _link.QueryStandby(700);
            if (s is int sv) Post(() => { _standby = sv; UpdateStandbyChecks(); });
        });
    }

#if DEVELOPER_MENU
    // Versetzt das Board in den ROM-Bootloader — mit vorgeschaltetem Board-Countdown:
    // App schickt BOOT?, das Board spielt 5 s Countdown, bestätigt mit BOOTREADY und
    // löst den Download-Modus dann SELBST aus (usb_persist_restart in der Firmware).
    // Die App triggert nichts mehr am Port — der frühere Host-1200-Touch war auf
    // Native-USB nichtdeterministisch (mal Normal-, mal Download-Reset).
    private void EnterBootloader()
    {
        var answer = MessageBox.Show(
            "Board in den Boot-Modus (Flash) versetzen?\n\n" +
            "Das Board zeigt einen 5-Sekunden-Countdown und geht danach selbst in den " +
            "Flash-Modus; die Verbindung trennt sich dabei. Nach einem Reset " +
            "(Power-Cycle) läuft die Firmware wieder normal.",
            "audioknubbel",
            MessageBoxButtons.OKCancel,
            MessageBoxIcon.Warning);
        if (answer != DialogResult.OK) return;

        if (!_link.Connected)
        {
            MessageBox.Show(
                "Kein verbundenes Board — der Boot-Modus mit Countdown ist nur bei " +
                "aktiver Verbindung möglich.",
                "audioknubbel", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        // Port für die Sequenz exklusiv halten: Heartbeat/Reconnect dürfen nicht
        // dazwischenschreiben/-lesen. Auf Background-Thread, damit das Tray während
        // der ~5 s nicht einfriert; Ergebnis-Dialog zurück auf den UI-Thread.
        _heartbeat.Stop();
        _reconnect.Stop();
        System.Threading.Tasks.Task.Run(() =>
        {
            bool ready = _link.RequestBootCountdown(7000);
            if (ready) _link.Disconnect();   // Board rebootet sich selbst in den Bootloader
            Post(() =>
            {
                _reconnect.Start();
                // Heartbeat IMMER wieder anwerfen: solange getrennt ist Send() ein
                // No-op; sobald der Reconnect steht (Board zurück aus Bootloader/
                // Flash), halten die PINGs die Board-Liveness wach. Sonst zeigt das
                // Board trotz offener Verbindung den roten Dot, bis ein Volume-Event
                // wieder eine Zeile schickt.
                _heartbeat.Start();
                if (!ready)
                    MessageBox.Show(
                        "Board hat den Countdown nicht bestätigt (BOOTREADY ausgeblieben) " +
                        "— Boot-Modus abgebrochen.",
                        "audioknubbel", MessageBoxButtons.OK, MessageBoxIcon.Error);
            });
        });
    }
#endif

    // Marshallt eine Aktion auf den UI-Thread. Ereignisse kommen u.U. vom
    // NAudio-COM-Thread oder synchron aus dem Ctor -> immer über BeginInvoke.
    private void Post(Action action)
    {
        if (_marshal.IsHandleCreated) _marshal.BeginInvoke(action);
    }

    private void OnVolumeStateChanged(AudioState s) => Post(() =>
    {
        _pending = s;
        _hasPending = true;
        _debounce.Stop();
        _debounce.Start();
    });

    private void OnConnectionChanged(bool connected) => Post(() =>
    {
        UpdateStatus();
        if (connected)
        {
            _sync.Reset();
            _sync.Sync(_monitor.Current);    // voller State direkt nach Connect
            QueryConfigAsync();              // Helligkeit + Standby sequenziell vom Board holen
        }
    });

    // Tooltip + Status-Menüeintrag aus dem aktuellen Link-Zustand setzen.
    private void UpdateStatus()
    {
        bool conn = _link.Connected;
        string port = _link.PortName ?? "—";
        _tray.Icon       = conn ? _connectedIcon : _disconnectedIcon;
        _tray.Text       = conn ? $"audioknubbel: {port} verbunden" : "audioknubbel: getrennt (suche…)";
        _statusItem.Text = conn ? $"Verbunden: {port}" : "Getrennt – suche Board…";
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing)
        {
            _reconnect.Dispose();
            _debounce.Dispose();
            _heartbeat.Dispose();
            _tray.Visible = false;
            _tray.Dispose();
            _connectedIcon.Dispose();
            _disconnectedIcon.Dispose();
            _monitor.Dispose();
            _link.Dispose();
            _marshal.Dispose();
        }
        base.Dispose(disposing);
    }
}
