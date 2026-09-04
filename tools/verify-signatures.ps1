param([Parameter(Mandatory=$true)][string[]]$Paths)
$ErrorActionPreference = 'Stop'
foreach ($path in $Paths) {
  if (-not (Test-Path $path)) { throw "Missing signed artifact: $path" }
  & signtool verify /pa /all /v $path
  if ($LASTEXITCODE -ne 0) { throw "Authenticode verification failed: $path" }
  $signature = Get-AuthenticodeSignature $path
  if ($signature.Status -ne 'Valid') { throw "Invalid signature status $($signature.Status): $path" }
  if (-not $signature.TimeStamperCertificate) { throw "Missing trusted timestamp: $path" }
}

