<#
.SYNOPSIS  Start/resume the AzerothCore server (WSL2 + Docker Desktop). No admin needed.
.PARAMETER WslPath  Repo path inside WSL. Default: ~/AzerothCore
.PARAMETER Distro   WSL distro name. Default: your default distro.
#>
[CmdletBinding()]
param([string]$WslPath = '~/AzerothCore', [string]$Distro)

. "$PSScriptRoot\_common.ps1"

$abs = Resolve-RepoPath -RepoPath $WslPath -Distro $Distro
if (-not $abs -or -not (Test-RepoHasSetup -AbsRepoPath $abs -Distro $Distro)) {
    throw "Repo not found at '$WslPath' in WSL. Run Setup-AzerothCore.ps1 first."
}

Start-DockerDesktopIfNeeded -Distro $Distro

Invoke-Wsl -RepoPath $abs -Command './start.sh' -Distro $Distro
Write-Host "Started. Watch:  wsl docker compose -f $abs/azerothcore-wotlk/docker-compose.yml logs -f ac-worldserver" -ForegroundColor Cyan
