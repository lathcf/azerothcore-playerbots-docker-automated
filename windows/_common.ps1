# windows/_common.ps1
# Shared helpers for the AzerothCore Windows (WSL2 + Docker Desktop) scripts.
# Dot-sourced by Setup/Start/Stop — not meant to be run directly.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# Make `wsl.exe` list/status output clean UTF-8 instead of UTF-16LE (PowerShell mangles UTF-16).
$env:WSL_UTF8 = '1'

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    return ([Security.Principal.WindowsPrincipal]$id).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Assert-Admin {
    if (-not (Test-IsAdmin)) {
        throw "Run this from an ELEVATED PowerShell (right-click -> 'Run as administrator'). Firewall changes need admin rights."
    }
}

# Leading `wsl.exe` args, optionally targeting a specific distro (else the default distro).
function Get-WslPrefix {
    param([string]$Distro)
    if ($Distro) { return @('-d', $Distro) } else { return @() }
}

# Expand a (possibly ~/relative) WSL path to an absolute path. $null if it doesn't exist.
# `cd` is UNQUOTED so bash expands ~; the path therefore must not contain spaces or shell
# metacharacters — reject those up front with a clear error instead of silently cd'ing to a
# truncated path.
function Resolve-RepoPath {
    param([Parameter(Mandatory)][string]$RepoPath, [string]$Distro)
    if ($RepoPath -notmatch '^[~A-Za-z0-9._/\-]+$') {
        throw "WslPath '$RepoPath' contains unsupported characters (spaces or shell metacharacters). Use a simple path like ~/AzerothCore."
    }
    $abs = (& wsl.exe @(Get-WslPrefix $Distro) '--' 'bash' '-lc' "cd $RepoPath 2>/dev/null && pwd")
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($abs)) { return $null }
    return ([string]$abs).Trim()
}

# True if $AbsRepoPath/setup.sh exists in WSL.
function Test-RepoHasSetup {
    param([Parameter(Mandatory)][string]$AbsRepoPath, [string]$Distro)
    & wsl.exe @(Get-WslPrefix $Distro) '--' 'bash' '-lc' "test -f `"$AbsRepoPath/setup.sh`"" *> $null
    return ($LASTEXITCODE -eq 0)
}

# True if `docker compose` works from WSL (Docker Desktop running + WSL integration on).
function Test-DockerReady {
    param([string]$Distro)
    & wsl.exe @(Get-WslPrefix $Distro) '--' 'bash' '-lc' 'docker compose version' *> $null
    return ($LASTEXITCODE -eq 0)
}

# Ensure Docker Desktop is running and reachable from WSL; launch it and wait (up to 3 min) if not.
function Start-DockerDesktopIfNeeded {
    param([string]$Distro)
    if (Test-DockerReady -Distro $Distro) { return }
    Write-Host "Starting Docker Desktop..." -ForegroundColor Cyan
    $dd = Join-Path $env:ProgramFiles 'Docker\Docker\Docker Desktop.exe'
    if (Test-Path $dd) { Start-Process $dd }
    else { Write-Warning "Docker Desktop.exe not found at '$dd' — start Docker Desktop manually." }
    $deadline = (Get-Date).AddMinutes(3)
    while (-not (Test-DockerReady -Distro $Distro)) {
        if ((Get-Date) -gt $deadline) { throw "Docker Desktop did not become ready within 3 minutes." }
        Start-Sleep -Seconds 5
    }
}

# Run a bash command inside an ABSOLUTE $RepoPath in WSL, streaming output. Throws on non-zero.
function Invoke-Wsl {
    param(
        [Parameter(Mandatory)][string]$RepoPath,
        [Parameter(Mandatory)][string]$Command,
        [string]$Distro
    )
    $full = "cd `"$RepoPath`" && $Command"
    & wsl.exe @(Get-WslPrefix $Distro) '--' 'bash' '-lc' $full
    if ($LASTEXITCODE -ne 0) { throw "WSL command failed (exit $LASTEXITCODE)." }
}
