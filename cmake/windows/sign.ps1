# SymStudio code-signing hook. No-op unless signing credentials are present.
# Usage: sign.ps1 <file1> [<file2> ...]
# Credentials (any one):
#   $env:SYMSTUDIO_SIGN_THUMBPRINT  - SHA1 thumbprint of a cert in the user/machine store
#   (extend here later for Azure Trusted Signing / SignPath)
param([Parameter(ValueFromRemainingArguments=$true)][string[]]$Files)

$thumb = $env:SYMSTUDIO_SIGN_THUMBPRINT
if (-not $thumb) {
  Write-Host "[sign] skipped - no SYMSTUDIO_SIGN_THUMBPRINT set"
  exit 0
}

$signtool = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" |
  Sort-Object FullName -Descending | Select-Object -First 1
if (-not $signtool) { Write-Error "[sign] signtool.exe not found"; exit 1 }

foreach ($f in $Files) {
  if (-not (Test-Path $f)) { Write-Error "[sign] missing file: $f"; exit 1 }
  & $signtool.FullName sign /sha1 $thumb /fd SHA256 `
    /tr http://timestamp.digicert.com /td SHA256 $f
  if ($LASTEXITCODE -ne 0) { Write-Error "[sign] failed: $f"; exit 1 }
  Write-Host "[sign] signed $f"
}
exit 0
