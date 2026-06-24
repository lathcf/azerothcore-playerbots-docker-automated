<#
.SYNOPSIS  Gracefully stop the AzerothCore server (waits for a clean save). No admin needed.
.PARAMETER WslPath  Repo path inside WSL. Default: ~/AzerothCore
.PARAMETER Distro   WSL distro name. Default: your default distro.
.PARAMETER Grace    Seconds to allow for a clean save. Default: 120
#>
[CmdletBinding()]
param([string]$WslPath = '~/AzerothCore', [string]$Distro, [int]$Grace = 120)

. "$PSScriptRoot\_common.ps1"

$abs = Resolve-RepoPath -RepoPath $WslPath -Distro $Distro
if (-not $abs -or -not (Test-RepoHasSetup -AbsRepoPath $abs -Distro $Distro)) {
    throw "Repo not found at '$WslPath' in WSL. Run Setup-AzerothCore.ps1 first."
}

Invoke-Wsl -RepoPath $abs -Command "./stop.sh $Grace" -Distro $Distro
