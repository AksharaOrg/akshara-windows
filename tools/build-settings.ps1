param(
  [ValidatePattern('^\d+\.\d+\.\d+$')]
  [string]$Version = '0.0.6',
  [string]$OutputDirectory = 'build/settings-publish'
)

# Publishes Akshara Settings (WinUI 3, self-contained, trimmed) for x64. The installer takes the whole folder.
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$project = Join-Path $repositoryRoot 'src/settings/AksharaSettings.csproj'
$output = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $repositoryRoot $OutputDirectory }

if (Test-Path $output) { Remove-Item -Recurse -Force $output }
# Restore on its own: the Windows App SDK's MSBuild props must be in place before the build that uses them.
dotnet restore $project -r win-x64 -p:Platform=x64
if ($LASTEXITCODE) { throw 'dotnet restore failed' }
dotnet publish $project --no-restore -c Release -r win-x64 -p:Platform=x64 "-p:Version=$Version" -o $output
if ($LASTEXITCODE) { throw 'dotnet publish failed' }
foreach ($required in 'AksharaSettings.exe', 'AksharaSettings.pri', 'contributors.json') {
  if (-not (Test-Path (Join-Path $output $required))) { throw "Akshara Settings publish is missing $required" }
}
Write-Host "Akshara Settings: $output"
