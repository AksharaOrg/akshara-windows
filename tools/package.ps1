param(
  [Parameter(Mandatory=$true)][string]$Version,
  [Parameter(Mandatory=$true)][string]$PayloadRoot,
  [Parameter(Mandatory=$true)][string]$OutputDirectory,
  [Parameter(Mandatory=$true)][ValidateSet('wix7')][string]$AcceptEula
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
dotnet build installer/Product.wixproj -c Release -p:Version=$Version -p:AcceptEula=$AcceptEula -p:PayloadRoot=$PayloadRoot -o "$OutputDirectory\msi"
$msi = Get-ChildItem "$OutputDirectory\msi\*.msi" | Select-Object -First 1
if (-not $msi) { throw 'WiX did not produce an MSI' }
Copy-Item $msi.FullName "$OutputDirectory\Akshara-Windows-v$Version.msi"
dotnet build installer/Bundle.wixproj -c Release -p:Version=$Version -p:AcceptEula=$AcceptEula -p:MsiPath="$OutputDirectory\Akshara-Windows-v$Version.msi" -o "$OutputDirectory\bundle"
$bundle = Get-ChildItem "$OutputDirectory\bundle\*.exe" | Select-Object -First 1
if (-not $bundle) { throw 'WiX did not produce a bundle EXE' }
Copy-Item $bundle.FullName "$OutputDirectory\Akshara-Windows-v$Version-Setup.exe"
