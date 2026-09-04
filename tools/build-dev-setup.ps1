param(
  [ValidatePattern('^\d+\.\d+\.\d+$')]
  [string]$Version = '0.1.0',
  [string]$OutputDirectory = 'dist/dev',
  [string]$AcceptEula = $env:WIX_ACCEPT_EULA
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot

if ($AcceptEula -ne 'wix7') {
  throw 'WiX 7 packaging requires WIX_ACCEPT_EULA=wix7 (or -AcceptEula wix7).'
}

Push-Location $repositoryRoot
try {
  cmake --preset windows-x86 "-DAKSHARA_VERSION=$Version"
  cmake --build --preset windows-x86
  cmake --preset windows-x64 "-DAKSHARA_VERSION=$Version"
  cmake --build --preset windows-x64

  $payloadRoot = Join-Path $repositoryRoot 'build/payload'
  New-Item -ItemType Directory -Force "$payloadRoot/x86", "$payloadRoot/x64" | Out-Null
  Copy-Item build/x86/Release/AksharaIME.dll, build/x86/Release/AksharaRegister.exe "$payloadRoot/x86/" -Force
  Copy-Item build/x64/Release/AksharaIME.dll, build/x64/Release/AksharaRegister.exe, build/x64/Release/AksharaHelp.exe "$payloadRoot/x64/" -Force

  $outputPath = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $repositoryRoot $OutputDirectory }
  $resolvedOutput = [IO.Path]::GetFullPath($outputPath)
  & "$PSScriptRoot/package.ps1" -Version $Version -PayloadRoot $payloadRoot -OutputDirectory $resolvedOutput -AcceptEula $AcceptEula

  $artifacts = @(
    "$resolvedOutput/Akshara-Windows-v$Version-Setup.exe",
    "$resolvedOutput/Akshara-Windows-v$Version.msi"
  )
  foreach ($artifact in $artifacts) {
    if (-not (Test-Path $artifact)) { throw "Missing developer installer artifact: $artifact" }
  }

  Get-FileHash $artifacts -Algorithm SHA256 |
    ForEach-Object { "$($_.Hash.ToLower())  $([IO.Path]::GetFileName($_.Path))" } |
    Set-Content "$resolvedOutput/SHA256SUMS.txt"

  Write-Host "Unsigned developer setup: $($artifacts[0])"
  Write-Warning 'This build is intentionally unsigned. Windows may show SmartScreen and publisher warnings.'
}
finally {
  Pop-Location
}
