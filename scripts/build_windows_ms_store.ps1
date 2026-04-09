# build_windows_ms_store.ps1
# PowerShell script to compile and package ORKA for the Microsoft Store (MSIX format).
# NOTE: Run this on a Windows machine with Visual Studio Code / MSBuild and the Windows 10/11 SDK installed.

Write-Host "╔══════════════════════════════════════════╗"
Write-Host "║  Windows MSIX Package Build — ORKA       ║"
Write-Host "╚══════════════════════════════════════════╝"

$ProjectPath = (Resolve-Path "$PSScriptRoot\..").Path
$StagingDir = "$ProjectPath\build\MSIX_Staging"
$AssetsDir = "$StagingDir\Assets"

# 1. Compile via CMake (requires MSVC)
Write-Host "`n[1/4] Compiling Orka for Windows using CMake..."
cd $ProjectPath
if (!(Test-Path -Path build)) { New-Item -ItemType Directory build }
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release

if (!(Test-Path -Path "Release\orka.exe")) {
    Write-Error "Compilation failed. orka.exe not found."
    exit 1
}

# 2. Prepare MSIX staging directory
Write-Host "`n[2/4] Preparing MSIX Staging Directory..."
if (Test-Path -Path $StagingDir) { Remove-Item -Recurse -Force $StagingDir }
New-Item -ItemType Directory $StagingDir | Out-Null
New-Item -ItemType Directory $AssetsDir | Out-Null

Copy-Item "Release\orka.exe" "$StagingDir\orka.exe"
Copy-Item "..\packaging\windows\AppxManifest.xml" "$StagingDir\AppxManifest.xml"

# Copy logo (Assuming it exists, or create dummy)
if (Test-Path "..\imege.png") {
    Copy-Item "..\imege.png" "$AssetsDir\StoreLogo.png"
    Copy-Item "..\imege.png" "$AssetsDir\Square150x150Logo.png"
    Copy-Item "..\imege.png" "$AssetsDir\Square44x44Logo.png"
    Copy-Item "..\imege.png" "$AssetsDir\Wide310x150Logo.png"
} else {
    Write-Warning "Images not found. Microsoft Store packaging requires images inside Assets folder"
}

# 3. Create MSIX Package using MakeAppx.exe (from Windows SDK)
Write-Host "`n[3/4] Packaging MSIX container..."
# Try to dynamically locate Windows 10 SDK MakeAppx.exe
$sdkPath = (Get-ChildItem -Path "C:\Program Files (x86)\Windows Kits\10\bin" -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$makeAppx = "$sdkPath\x64\makeappx.exe"

if (!(Test-Path $makeAppx)) {
    Write-Error "MakeAppx.exe not found at $makeAppx"
    exit 1
}

Write-Host "Found MakeAppx at: $makeAppx"
& $makeAppx pack /d $StagingDir /p "Orka_1.0.0.0_x64.msix"

if ($LASTEXITCODE -ne 0) {
    Write-Error "MakeAppx execution failed"
    exit $LASTEXITCODE
}

# 4. Success note
Write-Host "`n[4/4] Success! MSIX Package generated at $ProjectPath\build\Orka_1.0.0.0_x64.msix"
Write-Host "Reminder: To test locally, you must sign the MSIX using SignTool.exe and a test certificate."
Write-Host "To submit to Microsoft Store, use Partner Center."
