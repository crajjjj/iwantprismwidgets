# Builds the release zip for iWant Widgets - Prisma Edition.
#
# Usage:
#   cd plugin; xmake; cd ..        # build the DLL first (game must be closed
#                                  #  or the auto-deploy copy step will fail;
#                                  #  the DLL itself still compiles)
#   .\package.ps1                  # version read from plugin\xmake.lua
#   .\package.ps1 -Version 1.0.0   # explicit version
#
# The zip root is a Data-root mod layout, installable directly in MO2. It
# intentionally ships NO DDS assets: the original iWant Widgets mod stays
# enabled below this one and keeps providing the icon library + the esl slot.
param(
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot

if (-not $Version) {
    $m = Select-String -Path (Join-Path $root 'plugin\xmake.lua') -Pattern 'set_version\("([^"]+)"\)' | Select-Object -First 1
    if (-not $m) { throw 'No -Version given and set_version not found in plugin\xmake.lua' }
    $Version = $m.Matches[0].Groups[1].Value
}

$dll = Join-Path $root 'plugin\build\windows\x64\release\iWantWidgetsPrisma.dll'

# Everything the mod ships, staged path -> source path.
$files = [ordered]@{
    'iWant Widgets.esl'                        = Join-Path $root 'iWant Widgets.esl'
    'SEQ\iWant Widgets.seq'                    = Join-Path $root 'SEQ\iWant Widgets.seq'
    'SKSE\Plugins\iWantWidgetsPrisma.dll'      = $dll
    'PrismaUI\views\iwantwidgets\index.html'   = Join-Path $root 'PrismaUI\views\iwantwidgets\index.html'
    'Scripts\iwant_widgets.pex'                = Join-Path $root 'Scripts\iwant_widgets.pex'
    'Scripts\iWantWidgetsNative.pex'           = Join-Path $root 'Scripts\iWantWidgetsNative.pex'
    'Scripts\iwant_widgets_prisma_alias.pex'   = Join-Path $root 'Scripts\iwant_widgets_prisma_alias.pex'
    'Source\Scripts\iwant_widgets.psc'         = Join-Path $root 'Source\Scripts\iwant_widgets.psc'
    'Source\Scripts\iWantWidgetsNative.psc'    = Join-Path $root 'Source\Scripts\iWantWidgetsNative.psc'
    'Source\Scripts\iwant_widgets_prisma_alias.psc' = Join-Path $root 'Source\Scripts\iwant_widgets_prisma_alias.psc'
    'LICENSE'                                  = Join-Path $root 'LICENSE'
    'README.md'                                = Join-Path $root 'README.md'
}

foreach ($src in $files.Values) {
    if (-not (Test-Path $src)) { throw "Missing release input: $src" }
}

# The DLL is a build output, not tracked - surface its age so a stale build
# does not slip into a release unnoticed.
$dllTime = (Get-Item $dll).LastWriteTime
Write-Host ("DLL: built {0}" -f $dllTime)

$stage = Join-Path $env:TEMP ("iwwp-package-" + [guid]::NewGuid().ToString('N'))
try {
    foreach ($e in $files.GetEnumerator()) {
        $dst = Join-Path $stage $e.Key
        New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
        Copy-Item $e.Value $dst
    }

    $outDir = Join-Path $root 'dist'
    New-Item -ItemType Directory -Force $outDir | Out-Null
    $zip = Join-Path $outDir ("iWantWidgetsPrisma-{0}.zip" -f $Version)
    if (Test-Path $zip) { Remove-Item $zip }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip

    Write-Host ("Release: {0} ({1:N0} KB)" -f $zip, ((Get-Item $zip).Length / 1KB))
} finally {
    Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
}
