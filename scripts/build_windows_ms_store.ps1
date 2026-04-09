# build_windows_ms_store.ps1
# PowerShell script to compile and package ORKA for the Microsoft Store (MSIX format).
# NOTE: Run this on a Windows machine with Visual Studio / MSBuild and the Windows 10/11 SDK installed.

# ── Strict error handling ───────────────────────────────────────────
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

Write-Host "╔══════════════════════════════════════════╗"
Write-Host "║  Windows MSIX Package Build — ORKA       ║"
Write-Host "╚══════════════════════════════════════════╝"

# ── Resolve project root ────────────────────────────────────────────
# $PSScriptRoot may be empty when piped or run inline; fall back to CWD
if ($PSScriptRoot -and (Test-Path $PSScriptRoot)) {
    $ProjectPath = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $ProjectPath = (Get-Location).Path
}
Write-Host "Project root: $ProjectPath"

$BuildDir    = Join-Path $ProjectPath "build"
$StagingDir  = Join-Path $BuildDir "MSIX_Staging"
$AssetsDir   = Join-Path $StagingDir "Assets"

# 1. Compile via CMake (requires MSVC)
Write-Host "`n[1/4] Compiling Orka for Windows using CMake..."

if (!(Test-Path -Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Push-Location $BuildDir
try {
    # Use explicit MSVC generator for windows-latest runners
    Write-Host "Running cmake configure..."
    cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release "$ProjectPath"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configure failed with exit code $LASTEXITCODE"
        exit 1
    }

    Write-Host "Running cmake build..."
    cmake --build . --config Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake build failed with exit code $LASTEXITCODE"
        exit 1
    }
} finally {
    Pop-Location
}

$ExePath = Join-Path $BuildDir "Release" "orka.exe"
if (!(Test-Path -Path $ExePath)) {
    Write-Error "Compilation failed. orka.exe not found at $ExePath"
    exit 1
}
Write-Host "Build successful: $ExePath"

# 2. Prepare MSIX staging directory
Write-Host "`n[2/4] Preparing MSIX Staging Directory..."
if (Test-Path -Path $StagingDir) { Remove-Item -Recurse -Force $StagingDir }
New-Item -ItemType Directory -Path $StagingDir | Out-Null
New-Item -ItemType Directory -Path $AssetsDir  | Out-Null

Copy-Item $ExePath (Join-Path $StagingDir "orka.exe")

$ManifestSrc = Join-Path $ProjectPath "packaging" "windows" "AppxManifest.xml"
Copy-Item $ManifestSrc (Join-Path $StagingDir "AppxManifest.xml")

# Copy prepared logo assets from the packaging directory
$AssetsSrcDir = Join-Path $ProjectPath "packaging" "windows" "Assets"
if (Test-Path $AssetsSrcDir) {
    Copy-Item (Join-Path $AssetsSrcDir "*") $AssetsDir -Recurse -Force
    Write-Host "Prepared logo assets copied."
} else {
    Write-Warning "Assets directory not found at $AssetsSrcDir — Microsoft Store packaging requires properly sized images."
}

# 3. Create MSIX Package using MakeAppx.exe (from Windows SDK)
Write-Host "`n[3/4] Packaging MSIX container..."

# Dynamically locate MakeAppx.exe from Windows 10/11 SDK
$makeAppx = $null
$sdkRoot = "C:\Program Files (x86)\Windows Kits\10\bin"
if (Test-Path $sdkRoot) {
    # List SDK version directories, pick the newest
    $sdkVersions = Get-ChildItem -Path $sdkRoot -Directory |
        Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
        Sort-Object Name -Descending
    foreach ($ver in $sdkVersions) {
        $candidate = Join-Path $ver.FullName "x64" "makeappx.exe"
        if (Test-Path $candidate) {
            $makeAppx = $candidate
            break
        }
    }
}

if (!$makeAppx -or !(Test-Path $makeAppx)) {
    Write-Error "MakeAppx.exe not found. Ensure the Windows 10/11 SDK is installed."
    exit 1
}

$MsixOutput = Join-Path $BuildDir "Orka_1.0.0.0_x64.msix"
Write-Host "Found MakeAppx at: $makeAppx"
& $makeAppx pack /d $StagingDir /p $MsixOutput /o
if ($LASTEXITCODE -ne 0) {
    Write-Error "MakeAppx execution failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# 4. Success note
Write-Host "`n[4/4] Success! MSIX Package generated at $MsixOutput"
Write-Host "Reminder: To test locally, you must sign the MSIX using SignTool.exe and a test certificate."
Write-Host "To submit to Microsoft Store, use Partner Center."
