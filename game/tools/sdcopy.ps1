# Copies dist\STARLIFT.DAT (and dist\Starlift.CHG with -Chg) to the board's SD card: flashes the SD CARD READER app first,
# copies, compares SHA-256, ejects. Run from anywhere: powershell -File tools\sdcopy.ps1 [-Chg]
param([switch]$Chg)
$root = Split-Path -Parent $PSScriptRoot
$ard = Split-Path -Parent $root
Push-Location $ard
python chgame/tools/chgame.py uploader touch | Out-Null
Start-Sleep -Seconds 5
python chgame/tools/chgame.py uploader flash tools-sd/SDCARDRE.bin --verify --run | Select-Object -Last 1
Pop-Location
$v = $null
for ($i = 0; $i -lt 40 -and -not $v; $i++) { Start-Sleep -Seconds 2; $v = Get-Volume | Where-Object FileSystemLabel -eq 'CHGAME' }
if (-not $v) { 'no CHGAME drive'; exit 1 }
$d = "$($v.DriveLetter):"
$pairs = @(, @("$root\dist\STARLIFT.DAT", "$d\STARLIFT.DAT"))
if ($Chg) { $pairs += , @("$root\dist\Starlift.CHG", "$d\GAMES\GAMES\Starlift.CHG") }
foreach ($p in $pairs) {
    Copy-Item $p[0] $p[1] -Force
    $same = (Get-FileHash $p[0]).Hash -eq (Get-FileHash $p[1]).Hash
    "$($p[1]) $(if ($same) { 'MATCH' } else { 'DIFFER' })"
}
(New-Object -comObject Shell.Application).Namespace(17).ParseName($d).InvokeVerb("Eject")
Start-Sleep -Seconds 3
if (Get-Volume | Where-Object FileSystemLabel -eq 'CHGAME') { 'still mounted' } else { 'ejected' }
