# Build SavvyCAN and produce a Windows installer (SavvyCAN-<ver>-win64-setup.exe).
#
# Requires: qmake + MinGW (from the Qt online/aqt install) and NSIS (makensis).
# Example:
#     .\packaging\build-windows-installer.ps1
#
param(
    [string]$QtDir    = "C:\Qt\5.15.2\mingw81_64",
    [string]$MingwDir = "C:\Qt\Tools\mingw810_64",
    [string]$Jobs     = "8"
)
$ErrorActionPreference = "Continue"
$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"
$rel   = Join-Path $build "release"
$inst  = Join-Path $build "installer"
$stage = Join-Path $inst "app"

$env:Path = "$MingwDir\bin;$QtDir\bin;" + $env:Path

Write-Host "==> Building SavvyCAN (release)"
New-Item -ItemType Directory -Force $build | Out-Null
Push-Location $build
qmake "$root\SavvyCAN.pro" CONFIG+=release *> (Join-Path $build "setup_qmake.log")
if ($LASTEXITCODE -ne 0) { throw "qmake failed ($LASTEXITCODE)" }
mingw32-make "--jobs=$Jobs" *> (Join-Path $build "setup_make.log")
if ($LASTEXITCODE -ne 0) { throw "mingw32-make failed ($LASTEXITCODE)" }
Pop-Location

Write-Host "==> Compiling translations"
New-Item -ItemType Directory -Force "$rel\translations" | Out-Null
Get-ChildItem "$root\translations\*.ts" | ForEach-Object {
    lrelease $_.FullName 2>&1 | Out-Null
    $qm = [System.IO.Path]::ChangeExtension($_.FullName, ".qm")
    if (Test-Path $qm) { Copy-Item $qm "$rel\translations\" -Force }
}

Write-Host "==> Deploying Qt runtime"
# windeployqt fails to locate plugins with some aqt installs, so deploy explicitly.
$dlls = "Qt5Core","Qt5Gui","Qt5Widgets","Qt5Network","Qt5PrintSupport","Qt5Qml",
        "Qt5QmlModels","Qt5SerialPort","Qt5SerialBus","Qt5Sql","Qt5OpenGL",
        "Qt5Help","Qt5Svg","Qt5Concurrent"
foreach ($d in $dlls) { Copy-Item "$QtDir\bin\$d.dll" $rel -Force -ErrorAction SilentlyContinue }
foreach ($d in "libgcc_s_seh-1.dll","libstdc++-6.dll","libwinpthread-1.dll") {
    Copy-Item "$MingwDir\bin\$d" $rel -Force -ErrorAction SilentlyContinue
}
foreach ($p in "platforms","imageformats","iconengines","styles","printsupport",
               "canbus","sqldrivers","platformthemes","bearer","generic") {
    if (Test-Path "$QtDir\plugins\$p") { Copy-Item "$QtDir\plugins\$p" "$rel\$p" -Recurse -Force }
}
Copy-Item "$root\help" "$rel\help" -Recurse -Force

Write-Host "==> Staging installer payload"
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item "$rel\SavvyCAN.exe" $stage
Copy-Item "$rel\*.dll" $stage
foreach ($p in "platforms","imageformats","iconengines","styles","printsupport",
               "canbus","sqldrivers","platformthemes","bearer","generic") {
    if (Test-Path "$rel\$p") { Copy-Item "$rel\$p" "$stage\$p" -Recurse }
}
Copy-Item "$rel\translations" "$stage\translations" -Recurse
Copy-Item "$rel\help" "$stage\help" -Recurse

Write-Host "==> Compiling installer with NSIS"
New-Item -ItemType Directory -Force $inst | Out-Null
$nsiSrc = Join-Path $PSScriptRoot "installer.nsi"
$nsi    = Join-Path $inst "installer.nsi"
# makensis needs UTF-8 with BOM for non-ASCII (Chinese) text.
$text = Get-Content -Raw -Encoding UTF8 $nsiSrc
[System.IO.File]::WriteAllText($nsi, $text, (New-Object System.Text.UTF8Encoding($true)))
makensis.exe /V2 $nsi 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { throw "makensis failed ($LASTEXITCODE)" }

Write-Host "==> Done:" (Get-ChildItem "$inst\*.exe" | Select-Object -ExpandProperty FullName)
