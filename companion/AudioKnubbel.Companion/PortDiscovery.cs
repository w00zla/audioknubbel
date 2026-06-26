using System.Management;
using System.Runtime.InteropServices;
using System.Text;

namespace AudioKnubbel.Companion;

// Findet den COM-Port des audioknubbel passiv über die USB-Composite-Topologie.
// Die Espressif-VID allein ist zu breit: andere ESP32-S3-Firmware-CDC-Ports
// tragen dieselbe VID und dürfen nicht einmal kurz geöffnet werden.
public static class PortDiscovery
{
    private const string Vid = "VID_303A";
    private const string ProductName = "audioknubbel";
    private const string HidInterface = "MI_00";
    private const string CdcInterface = "MI_01";

    public static string? FindPort()
    {
        using var searcher = new ManagementObjectSearcher(
            "SELECT Name, PNPDeviceID FROM Win32_PnPEntity WHERE PNPDeviceID LIKE '%VID_303A%' OR Name LIKE '%(COM%)'");

        var devices = searcher.Get()
            .Cast<ManagementBaseObject>()
            .Select(d => new
            {
                Name = d["Name"]?.ToString() ?? "",
                PnpId = d["PNPDeviceID"]?.ToString() ?? "",
            })
            .ToArray();
        var pnpIds = devices.Select(d => d.PnpId).ToArray();

        foreach (var device in devices)
        {
            if (!device.Name.Contains("(COM", StringComparison.OrdinalIgnoreCase)) continue;
            string parentProductName = GetParentProductName(device.PnpId) ?? "";
            if (!IsAudioknubbelCandidate(device.PnpId, pnpIds, parentProductName)) continue;

            int open = device.Name.LastIndexOf("(COM", StringComparison.OrdinalIgnoreCase);
            int close = device.Name.LastIndexOf(')');
            if (open >= 0 && close > open)
                return device.Name.Substring(open + 1, close - open - 1); // z.B. "COM6"
        }
        return null;
    }

    public static bool IsAudioknubbelCandidate(
        string pnpDeviceId,
        IEnumerable<string> presentPnpDeviceIds,
        string parentProductName)
    {
        if (!string.Equals(parentProductName, ProductName, StringComparison.OrdinalIgnoreCase)) return false;
        if (!pnpDeviceId.Contains(Vid, StringComparison.OrdinalIgnoreCase)) return false;
        if (!pnpDeviceId.Contains(CdcInterface, StringComparison.OrdinalIgnoreCase)) return false;

        string marker = "&" + CdcInterface + "\\";
        int markerAt = pnpDeviceId.IndexOf(marker, StringComparison.OrdinalIgnoreCase);
        if (markerAt < 0) return false;

        string prefix = pnpDeviceId[..markerAt];
        string instance = pnpDeviceId[(markerAt + marker.Length)..];
        const string cdcSuffix = "&0001";
        if (!instance.EndsWith(cdcSuffix, StringComparison.OrdinalIgnoreCase)) return false;

        string siblingInstance = instance[..^cdcSuffix.Length] + "&0000";
        string hidSibling = prefix + "&" + HidInterface + "\\" + siblingInstance;
        return presentPnpDeviceIds.Any(id => string.Equals(id, hidSibling, StringComparison.OrdinalIgnoreCase));
    }

    private static string? GetParentProductName(string pnpDeviceId)
    {
        string? parentId = DevicePropertyReader.GetString(pnpDeviceId, DevicePropertyKeys.Parent);
        return parentId is null
            ? null
            : DevicePropertyReader.GetString(parentId, DevicePropertyKeys.BusReportedDeviceDesc);
    }

    private static class DevicePropertyReader
    {
        private const uint DigcfAllClasses = 0x00000004;
        private const uint DigcfPresent = 0x00000002;
        private static readonly IntPtr InvalidHandleValue = new(-1);

        public static string? GetString(string deviceInstanceId, DevPropKey key)
        {
            IntPtr deviceInfoSet = SetupDiGetClassDevs(
                IntPtr.Zero,
                null,
                IntPtr.Zero,
                DigcfAllClasses | DigcfPresent);
            if (deviceInfoSet == InvalidHandleValue) return null;

            try
            {
                var deviceInfoData = new SpDevinfoData
                {
                    CbSize = Marshal.SizeOf<SpDevinfoData>(),
                };
                if (!SetupDiOpenDeviceInfo(deviceInfoSet, deviceInstanceId, IntPtr.Zero, 0, ref deviceInfoData))
                    return null;

                var propertyKey = key;
                SetupDiGetDeviceProperty(
                    deviceInfoSet,
                    ref deviceInfoData,
                    ref propertyKey,
                    out _,
                    null,
                    0,
                    out uint requiredSize,
                    0);
                if (requiredSize == 0) return null;

                var buffer = new byte[requiredSize];
                propertyKey = key;
                if (!SetupDiGetDeviceProperty(
                    deviceInfoSet,
                    ref deviceInfoData,
                    ref propertyKey,
                    out _,
                    buffer,
                    requiredSize,
                    out _,
                    0))
                    return null;

                return Encoding.Unicode.GetString(buffer).TrimEnd('\0');
            }
            finally
            {
                SetupDiDestroyDeviceInfoList(deviceInfoSet);
            }
        }

        [DllImport("setupapi.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr SetupDiGetClassDevs(
            IntPtr classGuid,
            string? enumerator,
            IntPtr hwndParent,
            uint flags);

        [DllImport("setupapi.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool SetupDiOpenDeviceInfo(
            IntPtr deviceInfoSet,
            string deviceInstanceId,
            IntPtr hwndParent,
            uint openFlags,
            ref SpDevinfoData deviceInfoData);

        [DllImport("setupapi.dll", EntryPoint = "SetupDiGetDevicePropertyW", SetLastError = true)]
        private static extern bool SetupDiGetDeviceProperty(
            IntPtr deviceInfoSet,
            ref SpDevinfoData deviceInfoData,
            ref DevPropKey propertyKey,
            out uint propertyType,
            byte[]? propertyBuffer,
            uint propertyBufferSize,
            out uint requiredSize,
            uint flags);

        [DllImport("setupapi.dll", SetLastError = true)]
        private static extern bool SetupDiDestroyDeviceInfoList(IntPtr deviceInfoSet);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct SpDevinfoData
    {
        public int CbSize;
        public Guid ClassGuid;
        public uint DevInst;
        public IntPtr Reserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct DevPropKey
    {
        public Guid FmtId;
        public uint Pid;

        public DevPropKey(Guid fmtId, uint pid)
        {
            FmtId = fmtId;
            Pid = pid;
        }
    }

    private static class DevicePropertyKeys
    {
        public static readonly DevPropKey Parent = new(
            new Guid("4340A6C5-93FA-4706-972C-7B648008A5A7"),
            8);

        public static readonly DevPropKey BusReportedDeviceDesc = new(
            new Guid("540B947E-8B40-45BC-A8A2-6A0B894CBDA2"),
            4);
    }
}
