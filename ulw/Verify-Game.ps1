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
