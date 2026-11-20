using Akshara.Settings.Services;
using CommunityToolkit.WinUI.Controls;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Navigation;

namespace Akshara.Settings.Pages;

public sealed partial class HomePage : Page
{
    public HomePage()
    {
        InitializeComponent();
        Subtitle.Text = $"Sinhala input for Windows · Version {AppInfo.Version}";
    }

    protected override void OnNavigatedTo(NavigationEventArgs e)
    {
        base.OnNavigatedTo(e);
        Refresh();
    }

    private void Refresh()
    {
        var ready = InputMethods.IsAnyAdded();
        Status.Severity = ready ? InfoBarSeverity.Success : InfoBarSeverity.Warning;
        Status.Title = ready ? "Akshara is ready" : "Add an Akshara input method";
        Status.Message = ready
            ? "Press Windows + Space and choose Akshara to type Sinhala."
            : "Akshara is installed, but none of its input methods is in your list yet. Add Smart Phonetic below.";

        ProfileCards.Children.Clear();
        foreach (var profile in InputMethods.All) ProfileCards.Children.Add(CreateCard(profile));
    }

    private SettingsCard CreateCard(InputMethods.Profile profile)
    {
        var added = InputMethods.IsAdded(profile);
        var button = new Button { Content = added ? "Switch to" : "Add", MinWidth = 96 };
        if (!added) button.Style = (Style)Application.Current.Resources["AccentButtonStyle"];
        button.Click += async (_, _) =>
        {
            if (!InputMethods.Use(profile))
            {
                await new ContentDialog
                {
                    XamlRoot = XamlRoot,
                    Title = "Windows couldn't add this input method",
                    Content = "Reinstall Akshara, or add it in Language & region settings: Sinhala → Language options → Add a keyboard.",
                    CloseButtonText = "OK",
                }.ShowAsync();
            }
            Refresh();
        };

        var content = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 12, VerticalAlignment = VerticalAlignment.Center };
        if (added)
        {
            content.Children.Add(new FontIcon { Glyph = "", FontSize = 14, Foreground = (Microsoft.UI.Xaml.Media.Brush)Application.Current.Resources["SystemFillColorSuccessBrush"], VerticalAlignment = VerticalAlignment.Center });
            content.Children.Add(new TextBlock { Text = "Added", VerticalAlignment = VerticalAlignment.Center, Foreground = (Microsoft.UI.Xaml.Media.Brush)Application.Current.Resources["TextFillColorSecondaryBrush"] });
        }
        content.Children.Add(button);

        return new SettingsCard
        {
            Header = profile.Name,
            Description = profile.Description,
            HeaderIcon = new FontIcon { Glyph = profile.Glyph },
            Content = content,
        };
    }

    private void OnGuideClick(object sender, RoutedEventArgs e) => Frame.Navigate(typeof(GuidePage));

    private void OnLanguageSettingsClick(object sender, RoutedEventArgs e) => InputMethods.OpenLanguageSettings();
}
