param([Parameter(Mandatory=$true)][string]$Tag)
$ErrorActionPreference = 'Stop'
if ($Tag -notmatch '^v(?<major>0|[1-9]\d*)\.(?<minor>0|[1-9]\d*)\.(?<patch>0|[1-9]\d*)$') {
  throw "Release tag '$Tag' is not canonical SemVer (expected vMAJOR.MINOR.PATCH)."
}
$version = "$($Matches.major).$($Matches.minor).$($Matches.patch)"
"version=$version" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
"file_version=$version.0" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8
"artifact_prefix=Akshara-Windows-v$version" | Out-File -FilePath $env:GITHUB_OUTPUT -Append -Encoding utf8

