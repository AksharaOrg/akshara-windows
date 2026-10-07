using Akshara.Settings.Services;
using CommunityToolkit.WinUI.Controls;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Documents;

namespace Akshara.Settings.Pages;

public sealed partial class AboutPage : Page
{
    private sealed record Paragraph(string Title, string Body, string? Link = null);

    // PRIVACY.md, for people reading it in the app.
    private static readonly Paragraph[] Privacy =
    [
        new("On your PC", "Typing stays on this PC. Akshara does not send keystrokes, suggestions or analytics anywhere."),
        new("Input method access", "Like every Windows input method, Akshara receives the keys you type while one of its input methods "
            + "is selected, to turn them into Sinhala. It does not store or log them. The word being typed exists only in memory "
            + "until it is finished."),
        new("Network", "Akshara makes no network requests. Links in Settings open your browser only when you click them."),
        new("Dictionary spelling", "Grammar-correct Smart Phonetic picks spellings from a word list installed with Akshara. "
            + "Akshara does not learn or save what you type."),
        new("No tracking", "Akshara has no account, no advertising identifiers, and no third-party analytics or tracking."),
        new("Contact", "Questions and reports:", AppInfo.Issues),
    ];

    private static readonly Paragraph[] Notices =
    [
        new("Akshara", "Copyright (c) 2026 Akshara contributors. Licensed under the MIT License.", AppInfo.Repository),
        new("Sinhala Phonetic Orthography", "Copyright (c) 2026 Srilal Siriwardhana. Licensed under the MIT License. "
            + "Grammar-correct Smart Phonetic and the dictionary sound matching are ports of its reference code, and follow its "
            + "orthographic rules.", AppInfo.Research),
        new("A Word Frequency List for Sinhala", "The bundled word list is a compact derivative of the University of Moratuwa "
            + "National Languages Processing Centre word-frequency list. It retains the first 40,000 high-frequency entries and "
            + "filters malformed or overlong tokens.\n\nCitation: Aloka Fernando and Gihan Dias (2021), “Building a Linguistic "
            + "Resource: A Word Frequency List for Sinhala,” ICON 2021, pages 606–610.", AppInfo.FrequencyList),
        new("Windows App SDK and .NET", "Copyright (c) Microsoft Corporation. Licensed under the MIT License."),
        new("Windows Community Toolkit", "Copyright (c) .NET Foundation and Contributors. Licensed under the MIT License."),
    ];

    public AboutPage()
    {
        InitializeComponent();
        VersionText.Text = $"Version {AppInfo.Version}";
        foreach (var person in AppInfo.Contributors) People.Children.Add(CreatePerson(person));
    }

    private static SettingsCard CreatePerson(Contributor person)
    {
        var card = new SettingsCard
        {
            Header = person.Name,
            Description = person.Role,
            HeaderIcon = new FontIcon { Glyph = "" },
        };
        if (Uri.TryCreate(person.Link, UriKind.Absolute, out var link) && link.Scheme == Uri.UriSchemeHttps)
        {
            card.Content = new HyperlinkButton { Content = "Profile", NavigateUri = link };
        }
        return card;
    }

    private void OnGitHubClick(object sender, RoutedEventArgs e) => InputMethods.Launch(AppInfo.Repository);

    private async void OnPrivacyClick(object sender, RoutedEventArgs e) => await ShowDocument("Privacy policy", Privacy);

    private async void OnNoticesClick(object sender, RoutedEventArgs e) => await ShowDocument("Open source notices", Notices);

    private async Task ShowDocument(string title, Paragraph[] paragraphs)
    {
        var text = new RichTextBlock { IsTextSelectionEnabled = true };
        foreach (var paragraph in paragraphs)
        {
            var block = new Microsoft.UI.Xaml.Documents.Paragraph { Margin = new Thickness(0, 0, 0, 14) };
            block.Inlines.Add(new Run { Text = paragraph.Title, FontWeight = Microsoft.UI.Text.FontWeights.SemiBold });
            block.Inlines.Add(new LineBreak());
            block.Inlines.Add(new Run { Text = paragraph.Body });
            if (paragraph.Link is { } link)
            {
                block.Inlines.Add(new LineBreak());
                var hyperlink = new Hyperlink { NavigateUri = new Uri(link) };
                hyperlink.Inlines.Add(new Run { Text = link });
                block.Inlines.Add(hyperlink);
            }
            text.Blocks.Add(block);
        }
        await new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = title,
            Content = new ScrollViewer { Content = text, MaxHeight = 460 },
            CloseButtonText = "Close",
            DefaultButton = ContentDialogButton.Close,
        }.ShowAsync();
    }

    private async void OnResetClick(object sender, RoutedEventArgs e)
    {
        var result = await new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = "Reset settings?",
            Content = "This restores typing and correction options to their defaults.",
            PrimaryButtonText = "Reset",
            CloseButtonText = "Cancel",
            DefaultButton = ContentDialogButton.Close,
        }.ShowAsync();
        if (result == ContentDialogResult.Primary && !SettingsStore.Reset())
        {
            await new ContentDialog { XamlRoot = XamlRoot, Title = "Akshara couldn't save the settings", CloseButtonText = "OK" }.ShowAsync();
        }
    }
}
