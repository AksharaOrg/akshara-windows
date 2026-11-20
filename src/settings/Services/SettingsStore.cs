using Microsoft.Win32;

namespace Akshara.Settings.Services;

/// <summary>
/// The per-user settings the text service reads: DWORDs under HKCU\Software\Akshara\Settings.
/// Names and defaults must match src/common/AksharaPreferences.h; the text service picks up a change
/// the next time an application or Akshara profile receives focus.
/// </summary>
public static class SettingsStore
{
    private const string KeyPath = @"Software\Akshara\Settings";

    public sealed record Setting(string Name, bool Default);

    public static readonly Setting CommitOnPunctuation = new("CommitOnPunctuation", true);
    public static readonly Setting CommitOnEnter = new("CommitOnEnter", true);
    public static readonly Setting CommitOnTab = new("CommitOnTab", true);
    public static readonly Setting CommitOnCursorMovement = new("CommitOnCursorMovement", true);
    public static readonly Setting SmartPhoneticV2 = new("SmartPhoneticV2", true);
    public static readonly Setting V2Archaic = new("SmartPhoneticV2Archaic", false);
    public static readonly Setting V2RepayaZwj = new("SmartPhoneticV2RepayaZwj", false);
    public static readonly Setting V2Classical = new("SmartPhoneticV2Classical", false);
    public static readonly Setting V2RakaransayaU = new("SmartPhoneticV2RakaransayaU", false);
    public static readonly Setting V2RetroflexD = new("SmartPhoneticV2RetroflexD", false);
    public static readonly Setting DoubleSpacePeriod = new("DoubleSpacePeriod", true);

    public static readonly Setting[] All =
    [
        CommitOnPunctuation, CommitOnEnter, CommitOnTab, CommitOnCursorMovement, SmartPhoneticV2,
        V2Archaic, V2RepayaZwj, V2Classical, V2RakaransayaU, V2RetroflexD, DoubleSpacePeriod,
    ];

    public static bool Get(Setting setting)
    {
        using var key = Registry.CurrentUser.OpenSubKey(KeyPath);
        return key?.GetValue(setting.Name) is int value ? value != 0 : setting.Default;
    }

    public static bool Set(Setting setting, bool on)
    {
        try
        {
            using var key = Registry.CurrentUser.CreateSubKey(KeyPath);
            key.SetValue(setting.Name, on ? 1 : 0, RegistryValueKind.DWord);
            return true;
        }
        catch (Exception e) when (e is UnauthorizedAccessException or IOException or System.Security.SecurityException)
        {
            return false;
        }
    }

    /// <summary>Restores every setting to its default, as Android's "Reset keyboard settings" does.</summary>
    public static bool Reset() => All.Aggregate(true, (ok, setting) => Set(setting, setting.Default) && ok);
}
