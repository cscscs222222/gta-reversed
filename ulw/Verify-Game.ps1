param(
    [Parameter(Mandatory)]
    [string]$GameFolder
)
$ErrorActionPreference = 'Stop'
$folder = (Resolve-Path -LiteralPath $GameFolder).Path
$exe = Join-Path $folder 'gta_sa.exe'
$expected = 'A559AA772FD136379155EFA71F00C47AAD34BBFEAE6196B0FE1047D0645CBD26'
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $expected) {
    throw 'Bu surum dogrulanan GTA San Andreas Hoodlum 1.0 US dosyasiyla eslesmiyor.'
}
$loader = Get-Item -LiteralPath (Join-Path $folder 'vorbisFile.dll')
if ($loader.VersionInfo.FileDescription -ne 'Ultimate ASI Loader') {
    throw 'Bu paket Ultimate ASI Loader ile dogrulanmak uzere hazirlandi.'
}
$activeReversed = foreach ($path in @('gta_reversed.asi', 'scripts/gta_reversed.asi', 'plugins/gta_reversed.asi')) {
    if (Test-Path -LiteralPath (Join-Path $folder $path) -PathType Leaf) { $path }
}
if ($activeReversed) {
    Write-Warning ('Ayni kurulumda oyun testi yapilmamis eklenti bulundu: ' + ($activeReversed -join ', '))
}
Write-Output 'Hoodlum 1.0 US dosyasi ve Ultimate ASI Loader dogrulandi.'

$missionPath = Join-Path $folder 'data/script/main.scm'
if (!(Test-Path -LiteralPath $missionPath -PathType Leaf)) {
    throw 'Oyun klasorunde data/script/main.scm bulunamadi.'
}
$campaigns = @{
    '601DEF3BAAE766CE6A23E2F0B9B48F6B33C9A64E2FC32EB4F22DDEA8B868B0FA' = 'Orijinal GTA San Andreas'
    '81CF8DBA9331B8DBA1A59FFA437973DE86FDA299810B03A8A946A6E1CC2F91F8' = 'Ultimate Chaos Missions Zor'
    'BE0065CDE22611784CFF1C7FD707D4F62640AEBDDB430C4721AE16319A42C638' = 'Ultimate Chaos Missions Acimasiz'
}
$missionHash = (Get-FileHash -LiteralPath $missionPath -Algorithm SHA256).Hash
$campaign = $campaigns[$missionHash]
if (!$campaign) {
    Write-Warning 'main.scm ozeti taninan kampanyalarla eslesmedi. Oyun icinde genel gorev korumasi kullanilir.'
    return
}

$missionBytes = [IO.File]::ReadAllBytes($missionPath)
$info = 0xCFA4
$offsetTable = $info + 24
$mainSize = [BitConverter]::ToUInt32($missionBytes, $info + 8)
$largest = [BitConverter]::ToUInt32($missionBytes, $info + 12)
$count = [BitConverter]::ToUInt16($missionBytes, $info + 16)
if ($count -ne 135 -or $mainSize -gt 200000 -or $largest -gt 69000 -or $offsetTable + 4 * $count -gt $missionBytes.Length) {
    throw 'main.scm gorev tablosu ve motor sinirlari uyusmuyor.'
}
$offsets = for ($i = 0; $i -lt $count; ++$i) {
    [BitConverter]::ToUInt32($missionBytes, $offsetTable + 4 * $i)
}
if ($offsets[0] -lt [Math]::Max($mainSize, $offsetTable + 4 * $count)) {
    throw 'Ilk gorev blogu ana betik alaninin icinde.'
}
for ($i = 0; $i -lt $count; ++$i) {
    $end = if ($i + 1 -lt $count) { $offsets[$i + 1] } else { $missionBytes.Length }
    if ($end -gt $missionBytes.Length -or ([int64]$end - $offsets[$i]) -le 0 -or ([int64]$end - $offsets[$i]) -gt $largest) {
        throw ('Gecersiz gorev blogu {0}.' -f $i)
    }
}
Write-Output ('{0} kampanyasi ve {1} betik blogu dogrulandi.' -f $campaign, $count)
