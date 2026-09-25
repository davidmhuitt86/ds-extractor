# AP-GUI-003: install/uninstall a Windows Explorer right-click context menu
# entry - "Extract with DS-Extractor" - on image files and PDFs, launching
# dx-extractor-gui.exe with the clicked file. The GUI's startup path already
# routes a command-line file argument through the same import-choice screen
# (AP-GUI-002: import as-is, or mask regions first) used when opening a file
# from inside the GUI - there is no separate, silent code path here.
#
# Registers under HKEY_CURRENT_USER only. No administrator rights are
# required, and nothing outside the current user's own registry hive is
# touched - this never writes to HKEY_LOCAL_MACHINE or affects other users
# on the machine.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\dx-install-context-menu.ps1
#   powershell -ExecutionPolicy Bypass -File tools\dx-install-context-menu.ps1 -Uninstall
#   powershell -ExecutionPolicy Bypass -File tools\dx-install-context-menu.ps1 -GuiPath "C:\custom\path\dx-extractor-gui.exe"

[CmdletBinding()]
param(
    [switch]$Uninstall,
    [string]$GuiPath
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot

function Step([string]$Message) {
    Write-Host ""
    Write-Host ("[DX-CONTEXT-MENU] " + $Message) -ForegroundColor Cyan
}

function Fail([string]$Message) {
    Write-Host ""
    Write-Host ("[DX-CONTEXT-MENU] FAILED: " + $Message) -ForegroundColor Red
    Write-Host ""
    Read-Host "Press Enter to close"
    exit 1
}

# One registry key name, reused as both the menu's key name and its
# MUIVerb text source, so install/uninstall always agree on exactly what
# they're touching.
$MenuKeyName = "DSExtractor"
$MenuLabel = "Extract with DS-Extractor"

# SystemFileAssociations\image applies to every file Windows classifies as
# a picture by its PerceivedType (png/jpg/jpeg/bmp/tif/tiff/gif/...) without
# enumerating each extension by hand. PDFs are not PerceivedType=image, so
# they get their own explicit .pdf entry alongside it.
$TargetKeys = @(
    "HKCU:\Software\Classes\SystemFileAssociations\image\shell\$MenuKeyName",
    "HKCU:\Software\Classes\SystemFileAssociations\.pdf\shell\$MenuKeyName"
)

function Remove-ContextMenu {
    Step "Removing context menu entries"
    foreach ($key in $TargetKeys) {
        if (Test-Path $key) {
            Remove-Item -Path $key -Recurse -Force
            Write-Host "  removed: $key"
        } else {
            Write-Host "  not present: $key"
        }
    }
    Step "Done. Restart Explorer (or sign out/in) if entries still appear cached."
}

function Install-ContextMenu {
    param([string]$ResolvedGuiPath)

    Step "Installing context menu entries"
    Write-Host "  GUI executable: $ResolvedGuiPath"

    $command = '"' + $ResolvedGuiPath + '" "%1"'

    foreach ($key in $TargetKeys) {
        New-Item -Path $key -Force | Out-Null
        Set-ItemProperty -Path $key -Name "(Default)" -Value $MenuLabel
        Set-ItemProperty -Path $key -Name "Icon" -Value $ResolvedGuiPath

        $commandKey = Join-Path $key "command"
        New-Item -Path $commandKey -Force | Out-Null
        Set-ItemProperty -Path $commandKey -Name "(Default)" -Value $command

        Write-Host "  installed: $key"
    }

    Step "Done. Right-click an image (png/jpg/bmp/tif) or a PDF in Explorer -> '$MenuLabel'."
}

try {
    Step "Repository: $repoRoot"

    if ($Uninstall) {
        Remove-ContextMenu
        exit 0
    }

    if ([string]::IsNullOrWhiteSpace($GuiPath)) {
        $GuiPath = Join-Path $repoRoot "build\Release\dx-extractor-gui.exe"
    }

    if (-not (Test-Path $GuiPath)) {
        Fail ("dx-extractor-gui.exe not found at: " + $GuiPath + "`n" +
              "Build it first (tools\dx-release.ps1 or cmake --build build --config Release), " +
              "or pass -GuiPath explicitly.")
    }

    $GuiPath = (Resolve-Path $GuiPath).Path
    Install-ContextMenu -ResolvedGuiPath $GuiPath
} catch {
    Fail $_.Exception.Message
}
