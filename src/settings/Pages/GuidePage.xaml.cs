using CommunityToolkit.WinUI.Controls;
using Microsoft.UI.Text;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

namespace Akshara.Settings.Pages;

/// <summary>
/// The typing guide for Grammar-correct Smart Phonetic, as in the macOS app (SmartPhoneticV2Guide.swift). The
/// Sinhala column is to_sinhala() from the Sinhala-Phonetic-Orthography research repo with the default options;
/// the dictionary examples are what Space picks (checked by tests/core/SmartPhoneticV2Tests.cpp).
/// </summary>
public sealed partial class GuidePage : Page
{
    private sealed record Section(string Title, string Glyph, string Note, (string Keys, string Sinhala)[] Rows);

    private static readonly Section[] Guide =
    [
        new("ස්වර අක්ෂර · Vowels", "",
            "දීර්ඝ ස්වර සඳහා අකුර දෙවරක් යොදන්න. ඇ, ඈ, ඍ, ඓ සහ ඖ සඳහා කැපිටල් අකුරු යොදන්න. ai සහ au ලියැවෙන්නේ අයි සහ අවු ලෙසයි.",
            [("a", "අ"), ("aa", "ආ"), ("A / ae", "ඇ"), ("Aa / AA", "ඈ"), ("i", "ඉ"), ("ii / I", "ඊ"), ("u / U", "උ"), ("uu / UU", "ඌ"),
             ("R", "ඍ"), ("e", "එ"), ("ee", "ඒ"), ("E", "ඓ"), ("o / O", "ඔ"), ("oo / OO", "ඕ"), ("Au / AU", "ඖ"), ("ai", "අයි")]),
        new("ව්‍යංජන අක්ෂර · Consonants", "",
            "d යනු ද, කැපිටල් D යනු ඩ. මූර්ධජ ණ, ළ සහ ෂ සඳහා කැපිටල් අකුරු යොදන්න.",
            [("ka", "ක"), ("ga", "ග"), ("cha", "ච"), ("ja", "ජ"), ("ta", "ට"), ("Da", "ඩ"), ("tha", "ත"), ("da / qa", "ද"),
             ("na", "න"), ("Na", "ණ"), ("pa", "ප"), ("ba", "බ"), ("ma", "ම"), ("ya", "ය"), ("ra", "ර"), ("la", "ල"), ("La", "ළ"),
             ("wa / va", "ව"), ("sa", "ස"), ("sha", "ශ"), ("Sha / Sa", "ෂ"), ("ha", "හ"), ("fa", "ෆ")]),
        new("මහප්‍රාණ අක්ෂර · Aspirated letters", "",
            "අකුරට h එක් කරන්න. ඨ සඳහා T සහ ඪ සඳහා Dh යොදන්න.",
            [("kha / Ka", "ඛ"), ("gha / Ga", "ඝ"), ("chha", "ඡ"), ("jha / Ja", "ඣ"), ("Ta", "ඨ"), ("Dha", "ඪ"), ("thha", "ථ"),
             ("dha", "ධ"), ("pha / Pa", "ඵ"), ("bha", "භ")]),
        new("සඤ්ඤක සහ වෙනත් අක්ෂර · Sanyaka and other letters", "",
            "සඤ්ඤක අකුරු සඳහා z යොදන්න (ඹ සඳහා B). සඤ්ඤක අකුරකින් වචනයක් ආරම්භ නොවේ. ඤ සඳහා zk, ඥ සඳහා zh යොදන්න.",
            [("gazga", "ගඟ"), ("kazda", "කඳ"), ("kazDa", "කඬ"), ("aBa", "අඹ"), ("zka", "ඤ"), ("zha", "ඥ"), ("aXka", "අඞ්ක")]),
        new("පිළි · Vowel signs", "",
            "ස්වරයක් නැති ව්‍යංජනයකට හල් කිරීම ස්වයංක්‍රීයව යෙදේ. kru ලියැවෙන්නේ කෘ ලෙසයි. ං සඳහා x හෝ M, ඃ සඳහා H යොදන්න.",
            [("k", "ක්"), ("ka", "ක"), ("kaa", "කා"), ("kA", "කැ"), ("kAa", "කෑ"), ("ki", "කි"), ("kii", "කී"), ("ku", "කු"),
             ("kuu", "කූ"), ("kru / kR", "කෘ"), ("kruu", "කෲ"), ("ke", "කෙ"), ("kee", "කේ"), ("kE", "කෛ"), ("ko", "කො"),
             ("koo", "කෝ"), ("kAu", "කෞ"), ("kax / kaM", "කං"), ("kaH", "කඃ")]),
        new("බැඳි අක්ෂර · Joined letters", "",
            "යංශය සහ රකාරාංශය ස්වයංක්‍රීයව යෙදේ. ක, ග වැනි අකුරකට පෙර n ලියූ විට ං ලියැවේ. බැඳි අක්ෂර සඳහා Typing හි Classical conjuncts සක්‍රිය කරන්න.",
            [("kya", "ක්‍ය"), ("kra", "ක්‍ර"), ("vidyaava", "විද්‍යාව"), ("shrii", "ශ්‍රී"), ("karma", "කර්ම"), ("ganga", "ගංග"),
             ("ingriisi", "ඉංග්‍රීසි"), ("dumriya", "දුම්රිය")]),
        new("ශබ්දකෝෂයෙන් නිවැරදි අක්ෂර වින්‍යාසය · Dictionary spelling", "",
            "එක හා සමාන ශබ්ද ඇති අකුරු (න/ණ, ල/ළ, ද/ඩ …) සඳහා Space එබූ විට ශබ්දකෝෂයේ ඇති වචනය යෙදේ. වහාම Backspace එබීමෙන් ඔබ ලියූ ආකාරයටම ලැබේ.",
            [("honda ␣", "හොඳ"), ("sinhala ␣", "සිංහල"), ("bada ␣", "බඩ"), ("lamaya ␣", "ළමයා"), ("pilithura ␣", "පිළිතුර"),
             ("kalu ␣", "කළු"), ("keema ␣", "කෑම"), ("amma ␣", "අම්මා")]),
    ];

    public GuidePage()
    {
        InitializeComponent();
        var first = true;
        foreach (var section in Guide)
        {
            Sections.Children.Add(CreateSection(section, first));
            first = false;
        }
    }

    private static SettingsExpander CreateSection(Section section, bool expanded)
    {
        var grid = new VariableSizedWrapGrid { Orientation = Orientation.Horizontal, ItemWidth = 168, ItemHeight = 64 };
        foreach (var (keys, sinhala) in section.Rows) grid.Children.Add(CreateExample(keys, sinhala));

        var body = new StackPanel { Spacing = 12, Padding = new Thickness(0, 4, 0, 8) };
        body.Children.Add(new TextBlock
        {
            Text = section.Note,
            TextWrapping = TextWrapping.Wrap,
            Foreground = Brush("TextFillColorSecondaryBrush"),
        });
        body.Children.Add(grid);

        var expander = new SettingsExpander
        {
            Header = section.Title,
            HeaderIcon = new FontIcon { Glyph = section.Glyph },
            IsExpanded = expanded,
        };
        expander.Items.Add(new SettingsCard { Content = body, ContentAlignment = ContentAlignment.Vertical, HorizontalContentAlignment = HorizontalAlignment.Stretch });
        return expander;
    }

    private static Border CreateExample(string keys, string sinhala)
    {
        var row = new Grid { ColumnSpacing = 12, VerticalAlignment = VerticalAlignment.Center };
        row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        var key = new TextBlock
        {
            Text = keys,
            FontFamily = new FontFamily("Cascadia Mono, Consolas"),
            VerticalAlignment = VerticalAlignment.Center,
            Foreground = Brush("TextFillColorSecondaryBrush"),
        };
        var letter = new TextBlock { Text = sinhala, FontSize = 22, FontWeight = FontWeights.SemiBold, VerticalAlignment = VerticalAlignment.Center };
        Grid.SetColumn(letter, 1);
        row.Children.Add(key);
        row.Children.Add(letter);
        return new Border
        {
            Margin = new Thickness(0, 0, 8, 8),
            Padding = new Thickness(14, 6, 14, 6),
            CornerRadius = new CornerRadius(6),
            Background = Brush("SubtleFillColorSecondaryBrush"),
            Child = row,
        };
    }

    private static Brush Brush(string key) => (Brush)Application.Current.Resources[key];
}
