# build_windows_ms_store.ps1
# PowerShell script to compile and package ORKA for the Microsoft Store (MSIX format).
# Uses Rust/Cargo instead of CMake.

# ── Strict error handling ───────────────────────────────────────────
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

Write-Host "╔══════════════════════════════════════════╗"
Write-Host "║  Windows MSIX Package Build — ORKA       ║"
Write-Host "║  Rust Build                              ║"
Write-Host "╚══════════════════════════════════════════╝"

# ── Resolve project root ────────────────────────────────────────────
if ($PSScriptRoot -and (Test-Path $PSScriptRoot)) {
    $ProjectPath = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $ProjectPath = (Get-Location).Path
}
Write-Host "Project root: $ProjectPath"

$BuildDir    = Join-Path $ProjectPath "build"
$StagingDir  = Join-Path $BuildDir "MSIX_Staging"
$AssetsDir   = Join-Path $StagingDir "Assets"

# 1. Compile via Cargo
Write-Host "`n[1/4] Compiling Orka with Cargo (Rust)..."

cargo build --release --target x86_64-pc-windows-msvc
if ($LASTEXITCODE -ne 0) {
    Write-Error "Cargo build failed with exit code $LASTEXITCODE"
    exit 1
}

$ExePath = Join-Path $ProjectPath "target" "x86_64-pc-windows-msvc" "release" "orka.exe"
if (!(Test-Path -Path $ExePath)) {
    # Try default target directory
    $ExePath = Join-Path $ProjectPath "target" "release" "orka.exe"
}
if (!(Test-Path -Path $ExePath)) {
    Write-Error "Compilation failed. orka.exe not found."
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

# Copy logo assets
$LogoDir = Join-Path $ProjectPath "logo"
if (Test-Path $LogoDir) {
    Copy-Item (Join-Path $LogoDir "*") $AssetsDir -Recurse -Force
    # Rename for AppxManifest compatibility
    $storeLogo = Join-Path $AssetsDir "StoreLogo50х50.png"
    if (Test-Path $storeLogo) {
        Copy-Item $storeLogo (Join-Path $AssetsDir "StoreLogo.png") -Force
    }
    Write-Host "Logo assets copied from logo/ directory."
} else {
    Write-Warning "Logo directory not found — Microsoft Store packaging requires properly sized images."
}

# 3. Create MSIX Package
Write-Host "`n[3/4] Packaging MSIX container..."

$makeAppx = $null
$sdkRoot = "C:\Program Files (x86)\Windows Kits\10\bin"
if (Test-Path $sdkRoot) {
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
if (!(Test-Path -Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

Write-Host "Found MakeAppx at: $makeAppx"
& $makeAppx pack /d $StagingDir /p $MsixOutput /o
if ($LASTEXITCODE -ne 0) {
    Write-Error "MakeAppx execution failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

# 4. Success
Write-Host "`n[4/4] Success! MSIX Package generated at $MsixOutput"
Write-Host "Reminder: Sign with SignTool.exe and a test certificate for local testing."
