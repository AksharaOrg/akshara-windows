using System.Runtime.InteropServices;
using System.Runtime.InteropServices.Marshalling;
using Microsoft.Win32;

namespace Akshara.Settings.Services;

/// <summary>
/// The three Akshara input methods (TSF profiles; the identities in src/tsf/Globals.h): whether each is in the
/// user's language list, and adding or switching to one.
/// </summary>
public static partial class InputMethods
{
    public static readonly Guid TextService = new("4F06B8D9-27FC-4A9B-88A7-2503B8F075C4");
    private const ushort Sinhala = 0x045B;

    public sealed record Profile(string Name, string Description, string Glyph, Guid Id)
    {
        /// <summary>"0x045B:{clsid}{profile}", as InstallLayoutOrTip takes it.</summary>
        public string Tip => $"0x{Sinhala:X4}:{TextService.ToString("B").ToUpperInvariant()}{Id.ToString("B").ToUpperInvariant()}";
    }

    public static readonly Profile[] All =
    [
        new("Smart Phonetic", "Type Sinhala by sound, spelled by the rules of Sinhala orthography. Best for most people.",
            "", new("303B8D4E-BEFB-4708-95A8-99D79998688A")),
        new("Phonetic", "The classic romanized layout.", "", new("19C49470-8E7B-47F8-A15F-843E8AD5885F")),
        new("Wijesekara", "The SLS 1134 Sinhala typewriter layout.", "", new("F3594735-783B-4A9E-8415-4C2A3A5DDA63")),
    ];

    /// <summary>True when the profile is in the user's input method list (Windows + Space).</summary>
    public static bool IsAdded(Profile profile)
    {
        // Windows keeps the list under HKCU\Control Panel\International\User Profile\<language>, one value per
        // input method, named "<langid>:{clsid}{profile}".
        var id = $"{TextService:B}{profile.Id:B}";
        using var languages = Registry.CurrentUser.OpenSubKey(@"Control Panel\International\User Profile");
        if (languages is null) return false;
        foreach (var name in languages.GetSubKeyNames())
        {
            using var language = languages.OpenSubKey(name);
            if (language?.GetValueNames().Any(v => v.Contains(id, StringComparison.OrdinalIgnoreCase)) == true) return true;
        }
        return false;
    }

    public static bool IsAnyAdded() => All.Any(IsAdded);

    /// <summary>Adds the profile to the user's list and switches to it.</summary>
    public static bool Use(Profile profile)
    {
        if (!InstallLayoutOrTip(profile.Tip, 0)) return false;
        return Activate(profile) || IsAdded(profile);
    }

    public static void OpenLanguageSettings() => Launch("ms-settings:regionlanguage");

    public static void Launch(string uri)
    {
        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(uri) { UseShellExecute = true });
        }
        catch (Exception e) when (e is System.ComponentModel.Win32Exception or InvalidOperationException)
        {
        }
    }

    private static bool Activate(Profile profile)
    {
        const uint TF_PROFILETYPE_INPUTPROCESSOR = 1;
        const uint TF_IPPMF_FORSESSION = 0x20000000, TF_IPPMF_ENABLEPROFILE = 0x00000002,
            TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE = 0x00000004;
        const uint CLSCTX_INPROC_SERVER = 1;
        var clsid = new Guid("33C53A50-F456-4884-B049-85FD643ECFED");   // CLSID_TF_InputProcessorProfiles
        var iid = typeof(ITfInputProcessorProfileMgr).GUID;
        if (CoCreateInstance(clsid, 0, CLSCTX_INPROC_SERVER, iid, out var pointer) < 0 || pointer == 0) return false;
        try
        {
            var manager = (ITfInputProcessorProfileMgr)new StrategyBasedComWrappers()
                .GetOrCreateObjectForComInstance(pointer, CreateObjectFlags.UniqueInstance);
            var service = TextService;
            var id = profile.Id;
            return manager.ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, Sinhala, service, id, 0,
                TF_IPPMF_FORSESSION | TF_IPPMF_ENABLEPROFILE | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE) >= 0;
        }
        finally
        {
            Marshal.Release(pointer);
        }
    }

    [LibraryImport("input.dll", StringMarshalling = StringMarshalling.Utf16)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static partial bool InstallLayoutOrTip(string profile, uint flags);

    [LibraryImport("ole32.dll")]
    private static partial int CoCreateInstance(in Guid clsid, nint outer, uint context, in Guid iid, out nint instance);
}

// Only the first method of the interface is declared; the rest of its vtable is never called.
[GeneratedComInterface]
[Guid("71C6E74C-0F28-11D8-A82A-00065B84435C")]
internal partial interface ITfInputProcessorProfileMgr
{
    [PreserveSig]
    int ActivateProfile(uint profileType, ushort langId, in Guid clsid, in Guid profile, nint hkl, uint flags);
}
