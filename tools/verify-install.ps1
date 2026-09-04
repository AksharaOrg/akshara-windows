param([Parameter(Mandatory=$true)][string]$Bundle, [switch]$Uninstall)
$ErrorActionPreference = 'Stop'
if (-not $Uninstall) {
  $process = Start-Process -FilePath $Bundle -ArgumentList '/quiet','/norestart' -Wait -PassThru
  if ($process.ExitCode -notin 0, 3010) { throw "Silent installation failed: $($process.ExitCode)" }
  @("$env:ProgramFiles\Akshara\x64\AksharaIME.dll", "${env:ProgramFiles(x86)}\Akshara\x86\AksharaIME.dll", "$env:ProgramFiles\Akshara\help\AksharaHelp.exe") |
    ForEach-Object { if (-not (Test-Path $_)) { throw "Installed file missing: $_" } }
  $profiles = Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\CTF\TIP\{8B8E29C7-E118-4C77-9F58-525784EFB9C1}' -Recurse -ErrorAction Stop
  if (-not $profiles) { throw 'Akshara TSF registration missing' }
  & "$env:ProgramFiles\Akshara\x64\AksharaRegister.exe" probe
  if ($LASTEXITCODE -ne 0) { throw "Akshara TSF activation probe failed: $LASTEXITCODE" }
} else {
  $process = Start-Process -FilePath $Bundle -ArgumentList '/uninstall','/quiet','/norestart' -Wait -PassThru
  if ($process.ExitCode -notin 0, 3010) { throw "Silent uninstall failed: $($process.ExitCode)" }
  if (Test-Path "$env:ProgramFiles\Akshara\x64\AksharaIME.dll") { throw 'x64 payload remains after uninstall' }
  if (Test-Path 'HKLM:\SOFTWARE\Microsoft\CTF\TIP\{8B8E29C7-E118-4C77-9F58-525784EFB9C1}') { throw 'TSF registration remains after uninstall' }
}
