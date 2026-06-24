<#
.SYNOPSIS  Update AzerothCore (pull latest fork + modules and rebuild) on Windows via WSL2 +
           Docker Desktop. No admin needed. Your config (env/dist/etc/*.conf) and database
           volume are preserved.
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

# update.sh rebuilds via `docker compose up -d --build`, so the engine must be up.
Start-DockerDesktopIfNeeded -Distro $Distro

Write-Host "Updating (pull latest + rebuild; this can take a while)..." -ForegroundColor Cyan
Invoke-Wsl -RepoPath $abs -Command './update.sh' -Distro $Distro
