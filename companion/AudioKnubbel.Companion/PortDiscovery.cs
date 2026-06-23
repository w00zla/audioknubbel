using System.Management;

namespace AudioKnubbel.Companion;

// Findet den COM-Port des audioknubbel über die Espressif-VID (0x303A).
public static class PortDiscovery
{
    private const string Vid = "VID_303A";
    // Der ESP32-S3-Bootloader (USB-Serial/JTAG) trägt dieselbe VID, aber sein
    // CDC sitzt auf Interface 0 (MI_00). Die Firmware-CDC ist MI_01 (HID
    // begin()-t in hid.cpp vor CDC -> CDC nie Interface 0). MI_00 überspringen,
    // damit die Companion den Flash-Port nicht öffnet/per DTR-RTS stört.
    private const string BootloaderInterface = "MI_00";

    public static string? FindPort()
    {
        using var searcher = new ManagementObjectSearcher(
            "SELECT Name, PNPDeviceID FROM Win32_PnPEntity WHERE Name LIKE '%(COM%)'");
        foreach (ManagementBaseObject device in searcher.Get())
        {
            var pnpId = device["PNPDeviceID"]?.ToString() ?? "";
            if (!pnpId.Contains(Vid, StringComparison.OrdinalIgnoreCase)) continue;
            if (pnpId.Contains(BootloaderInterface, StringComparison.OrdinalIgnoreCase)) continue;

            var name = device["Name"]?.ToString() ?? "";
            int open = name.LastIndexOf("(COM", StringComparison.OrdinalIgnoreCase);
            int close = name.LastIndexOf(')');
            if (open >= 0 && close > open)
                return name.Substring(open + 1, close - open - 1); // z.B. "COM6"
        }
        return null;
    }
}
