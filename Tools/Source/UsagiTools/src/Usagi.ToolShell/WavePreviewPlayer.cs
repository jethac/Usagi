using System.ComponentModel;
using System.Runtime.InteropServices;

namespace Usagi.ToolShell;

internal static class WavePreviewPlayer
{
    private const uint SoundAsync = 0x0001;
    private const uint SoundNoDefault = 0x0002;
    private const uint SoundFileName = 0x00020000;

    public static void Play(string path)
    {
        if (!OperatingSystem.IsWindows())
        {
            throw new InvalidOperationException("WAV preview is only available on Windows.");
        }

        if (!File.Exists(path))
        {
            throw new IOException($"WAV file was not found: {path}");
        }

        if (!PlaySound(path, IntPtr.Zero, SoundAsync | SoundNoDefault | SoundFileName))
        {
            throw new Win32Exception(Marshal.GetLastWin32Error());
        }
    }

    public static void Stop()
    {
        if (OperatingSystem.IsWindows())
        {
            PlaySound(null, IntPtr.Zero, 0);
        }
    }

    [DllImport("winmm.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool PlaySound(string? sound, IntPtr module, uint flags);
}
