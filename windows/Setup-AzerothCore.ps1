<#
.SYNOPSIS
  Install/configure the AzerothCore LAN server on Windows via WSL2 + Docker Desktop.
.DESCRIPTION
  Run from an ELEVATED PowerShell. Configures the Windows-side bits (firewall, host LAN IP)
  and runs the repo's ./setup.sh inside WSL. The containers/compose are unchanged.
.PARAMETER WslPath
  Path to the cloned repo inside WSL. Default: ~/AzerothCore
.PARAMETER Distro
  WSL distro name. Default: your default distro.
.PARAMETER LanIp
  Override the auto-detected Windows host LAN IPv4.
.EXAMPLE
  .\Setup-AzerothCore.ps1
.EXAMPLE
  .\Setup-AzerothCore.ps1 -WslPath ~/AzerothCore -LanIp 192.168.1.50
#>
[CmdletBinding()]
param(
    [string]$WslPath = '~/AzerothCore',
    [string]$Distro,
    [string]$LanIp
)

. "$PSScriptRoot\_common.ps1"

Write-Host "== AzerothCore Windows (WSL2 + Docker Desktop) installer ==" -ForegroundColor Cyan
Assert-Admin

# --- Preflight: WSL present ---
if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw "WSL is not installed. Run 'wsl --install' (then reboot) and install a distro like Ubuntu."
}

# --- Preflight: repo present in WSL ---
$abs = Resolve-RepoPath -RepoPath $WslPath -Distro $Distro
if (-not $abs -or -not (Test-RepoHasSetup -AbsRepoPath $abs -Distro $Distro)) {
    throw @"
Repo not found at '$WslPath' inside WSL.
Open a WSL terminal and clone it into your WSL home first, e.g.:
  git clone <REPO_URL> ~/AzerothCore
Then re-run this script (or pass -WslPath <path>).
"@
}
Write-Host "Repo: $abs" -ForegroundColor Green
if ($abs -like '/mnt/*') {
    Write-Warning "Repo is on the Windows filesystem ($abs). For speed and correct Docker bind-mount permissions, clone it into the WSL native filesystem (e.g. ~/AzerothCore) instead."
}

# --- Preflight: Docker reachable from WSL ---
if (-not (Test-DockerReady -Distro $Distro)) {
    throw @"
Docker is not reachable from WSL. Make sure:
  1. Docker Desktop is running.
  2. Settings -> Resources -> WSL Integration is ON for your distro.
Then re-run this script.
"@
}
Write-Host "Docker Desktop: reachable from WSL." -ForegroundColor Green

# --- Detect the Windows host LAN IPv4 (default-route adapter; skip virtual/WSL/APIPA) ---
if (-not $LanIp) {
    # Pick the IPv4 of the real LAN adapter that owns the default route. Exclude WSL/Hyper-V/
    # virtual adapters — their NAT IP (172.x) is exactly what LAN clients CANNOT reach.
    $route = Get-NetRoute -DestinationPrefix '0.0.0.0/0' -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Where-Object {
            $a = Get-NetAdapter -InterfaceIndex $_.ifIndex -ErrorAction SilentlyContinue
            $a -and $a.Status -eq 'Up' -and $a.InterfaceDescription -notmatch 'Hyper-V|WSL|Virtual|Loopback'
        } |
        Sort-Object RouteMetric |
        Select-Object -First 1
    if ($route) {
        $ip = Get-NetIPAddress -AddressFamily IPv4 -InterfaceIndex $route.ifIndex -ErrorAction SilentlyContinue |
            Where-Object { $_.IPAddress -notlike '169.254.*' } |
            Select-Object -First 1
        if ($ip) { $LanIp = $ip.IPAddress }
    }
}
if (-not $LanIp) {
    throw "Could not auto-detect the Windows host LAN IP. Re-run with -LanIp <your.host.ip> (see 'ipconfig')."
}
Write-Host "Windows host LAN IP: $LanIp" -ForegroundColor Green

# --- Read auth/world ports from the WSL-side env (defaults 3724 / 8085) ---
$portScript = @'
f=.env; [ -f "$f" ] || f=.env.example
awk -F= '/^DOCKER_AUTH_EXTERNAL_PORT=/{a=$2} /^DOCKER_WORLD_EXTERNAL_PORT=/{w=$2} END{printf "%s %s",(a?a:"3724"),(w?w:"8085")}' "$f"
'@
$portsRaw = & wsl.exe @(Get-WslPrefix $Distro) '--' 'bash' '-lc' "cd `"$abs`" && $portScript"
$ports = ([string]$portsRaw).Trim() -split '\s+'
# Guard against StrictMode index-out-of-bounds if the read ever yields <2 tokens.
if ($ports.Count -ge 2) { $authPort = $ports[0]; $worldPort = $ports[1] }
else { $authPort = '3724'; $worldPort = '8085' }
Write-Host "Ports: auth=$authPort world=$worldPort" -ForegroundColor Green

# --- Idempotent Windows Firewall inbound rules ---
foreach ($r in @(
    @{ Name = 'AzerothCore Auth';  Port = $authPort },
    @{ Name = 'AzerothCore World'; Port = $worldPort }
)) {
    Get-NetFirewallRule -DisplayName $r.Name -ErrorAction SilentlyContinue |
        Remove-NetFirewallRule -ErrorAction SilentlyContinue
    New-NetFirewallRule -DisplayName $r.Name -Group 'AzerothCore' -Direction Inbound `
        -Action Allow -Protocol TCP -LocalPort $r.Port -Profile Any | Out-Null
    Write-Host "Firewall: allowed inbound TCP $($r.Port) ($($r.Name))." -ForegroundColor Green
}

# --- Persist LAN_IP into the WSL-side env so the realm advertises the Windows host IP ---
$ipScript = @'
# Repo-root .env is the source of truth; create it from the template if the operator has not
# yet, so LAN_IP lands where setup.sh will copy it to the live env.
[ -f .env ] || cp .env.example .env
f=.env
if grep -q '^LAN_IP=' "$f"; then
  sed -i "s|^LAN_IP=.*|LAN_IP=__IP__|" "$f"
else
  printf '\nLAN_IP=__IP__\n' >> "$f"
fi
echo "Set LAN_IP=__IP__ in $f"
'@ -replace '__IP__', $LanIp
Invoke-Wsl -RepoPath $abs -Command $ipScript -Distro $Distro

# --- Run the install inside WSL ---
Write-Host "Running ./setup.sh inside WSL (first run can take a long time)..." -ForegroundColor Cyan
Invoke-Wsl -RepoPath $abs -Command './setup.sh' -Distro $Distro

Write-Host ""
Write-Host "Done. On each player's PC, set realmlist to:" -ForegroundColor Cyan
Write-Host "    set realmlist $LanIp" -ForegroundColor Yellow
Write-Host "Daily ops:  .\Start-AzerothCore.ps1   /   .\Stop-AzerothCore.ps1" -ForegroundColor Cyan
