param([Parameter(Mandatory=$true)][string]$Bundle, [switch]$Uninstall)
$ErrorActionPreference = 'Stop'
if (-not $Uninstall) {
  $process = Start-Process -FilePath $Bundle -ArgumentList '/quiet','/norestart' -Wait -PassThru
  if ($process.ExitCode -notin 0, 3010) { throw "Silent installation failed: $($process.ExitCode)" }
  @("$env:ProgramFiles\Akshara\x64\AksharaIME.dll", "${env:ProgramFiles(x86)}\Akshara\x86\AksharaIME.dll", "$env:ProgramFiles\Akshara\settings\AksharaSettings.exe") |
    ForEach-Object { if (-not (Test-Path $_)) { throw "Installed file missing: $_" } }
  if (-not (Test-Path "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\Akshara\Akshara.lnk")) {
    throw 'Akshara Start menu shortcut is missing'
  }
  $profiles = Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\CTF\TIP\{4F06B8D9-27FC-4A9B-88A7-2503B8F075C4}' -Recurse -ErrorAction Stop
  if (($profiles | Where-Object { $_.PSChildName -match '^\{(303B8D4E-BEFB-4708-95A8-99D79998688A|19C49470-8E7B-47F8-A15F-843E8AD5885F|F3594735-783B-4A9E-8415-4C2A3A5DDA63)\}$' }).Count -ne 3) {
    throw 'One or more Akshara TSF profiles are missing'
  }
} else {
  $process = Start-Process -FilePath $Bundle -ArgumentList '/uninstall','/quiet','/norestart' -Wait -PassThru
  if ($process.ExitCode -notin 0, 3010) { throw "Silent uninstall failed: $($process.ExitCode)" }
  if (Test-Path "$env:ProgramFiles\Akshara\x64\AksharaIME.dll") { throw 'x64 payload remains after uninstall' }
  if (Test-Path "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\Akshara\Akshara.lnk") { throw 'Start menu shortcut remains after uninstall' }
  if (Test-Path 'HKLM:\SOFTWARE\Microsoft\CTF\TIP\{4F06B8D9-27FC-4A9B-88A7-2503B8F075C4}') { throw 'TSF registration remains after uninstall' }
}
