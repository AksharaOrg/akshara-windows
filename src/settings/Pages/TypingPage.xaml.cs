using Akshara.Settings.Services;
using CommunityToolkit.WinUI.Controls;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace Akshara.Settings.Pages;

/// <summary>Each ToggleSwitch's Tag names its setting in SettingsStore.</summary>
public sealed partial class TypingPage : Page
{
    private bool loading;

    public TypingPage()
    {
        InitializeComponent();
        Loaded += (_, _) => Load();
    }

    private IEnumerable<ToggleSwitch> Toggles =>
    [
        GrammarToggle, RetroflexDToggle, RakaransayaUToggle, RepayaZwjToggle, ClassicalToggle, ArchaicToggle, DoubleSpaceToggle,
        PunctuationToggle, EnterToggle, TabToggle, CursorToggle,
    ];

    private static SettingsStore.Setting? SettingFor(ToggleSwitch toggle) =>
        SettingsStore.All.FirstOrDefault(s => s.Name == toggle.Tag as string);

    private void Load()
    {
        loading = true;
        foreach (var toggle in Toggles)
        {
            if (SettingFor(toggle) is { } setting) toggle.IsOn = SettingsStore.Get(setting);
        }
        loading = false;
        UpdateOptions();
    }

    private void OnToggled(object sender, RoutedEventArgs e)
    {
        if (loading || sender is not ToggleSwitch toggle || SettingFor(toggle) is not { } setting) return;
        SettingsStore.Set(setting, toggle.IsOn);
        if (setting == SettingsStore.SmartPhoneticV2) UpdateOptions();
    }

    // The spelling options apply to grammar-correct Smart Phonetic only.
    private void UpdateOptions()
    {
        foreach (var item in GrammarCard.Items.OfType<SettingsCard>()) item.IsEnabled = GrammarToggle.IsOn;
    }

    private void OnRomanizationClick(object sender, RoutedEventArgs e) => InputMethods.Launch(AppInfo.Romanization);
}
