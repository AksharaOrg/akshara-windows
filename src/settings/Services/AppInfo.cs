using System.Reflection;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Akshara.Settings.Services;

/// <summary>A person shown under About → People: contributors.json, shared with the Android and macOS apps.</summary>
public sealed record Contributor(string Name, string Role, string? Link);

[JsonSourceGenerationOptions(PropertyNameCaseInsensitive = true)]
[JsonSerializable(typeof(Contributor[]))]
internal sealed partial class ContributorJson : JsonSerializerContext;

public static class AppInfo
{
    public const string Repository = "https://github.com/AksharaOrg/akshara-windows";
    public const string Issues = "https://github.com/AksharaOrg/akshara-windows/issues";
    public const string Research = "https://srilals.github.io/Sinhala-Phonetic-Orthography/";
    public const string Romanization = "https://srilals.github.io/Sinhala-Phonetic-Orthography/research/phonetic-romanization";
    public const string FrequencyList = "https://github.com/nlpcuom/Word-Frequency-List-for-Sinhala";

    public static string Version
    {
        get
        {
            var version = Assembly.GetExecutingAssembly().GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion ?? "?";
            var plus = version.IndexOf('+');   // drop the source revision the SDK appends
            return plus < 0 ? version : version[..plus];
        }
    }

    public static IReadOnlyList<Contributor> Contributors { get; } = LoadContributors();

    private static Contributor[] LoadContributors()
    {
        try
        {
            var path = Path.Combine(AppContext.BaseDirectory, "contributors.json");
            return JsonSerializer.Deserialize(File.ReadAllText(path), ContributorJson.Default.ContributorArray) ?? [];
        }
        catch (Exception e) when (e is IOException or JsonException or UnauthorizedAccessException)
        {
            return [];
        }
    }
}
