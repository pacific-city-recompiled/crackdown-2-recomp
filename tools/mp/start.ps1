# Starts one of two local multiplayer test instances (out/mp/A or out/mp/B) on this PC, as Crackdown 1's
# tools/coop/start-live.ps1 did: separate user data + profile XUID, --net_mode=direct with fixed port offsets
# (A 20000, B 21000) and each other as peer. Prints the process id. Extra args go to the game.
# Usage: tools/mp/start.ps1 -Role A [-Extra "--auto_input=START,A --auto_input_delay_s=14"]
param([ValidateSet('A', 'B')][string]$Role = 'A', [string]$Extra = '',
      [string]$Build = 'win-amd64-release')
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$dir = "$repo\out\mp\$Role"
$exe = "$repo\crackdown2\out\build\$Build\crackdown2.exe"
$xuid = if ($Role -eq 'A') { 'B100000000000001' } else { 'B100000000000002' }
if (-not (Test-Path "$dir\user\$xuid")) {
  # First run: copy the user's data (DLC, saves) and give this instance its own profile.
  New-Item -ItemType Directory -Force "$dir\user" | Out-Null
  $src = "$env:USERPROFILE\Documents\crackdown2"
  Copy-Item "$src\0000000000000000", "$src\achievements" "$dir\user" -Recurse
  Copy-Item "$src\B13EBABEBABEBABE" "$dir\user\$xuid" -Recurse
}
$offset = if ($Role -eq 'A') { 20000 } else { 21000 }
$peer = if ($Role -eq 'A') { '127.0.0.1:22001' } else { '127.0.0.1:21001' }
$log = "$dir\$(Get-Date -Format 'yyyyMMdd-HHmmss').log"
$argv = @("--game_data_root=`"$repo\game`"", "--update_data_root=`"$repo\tu`"",
  "--user_data_root=`"$dir\user`"", "--cache_root=`"$dir\user\cache`"", "--log_file=`"$log`"",
  '--net_mode=direct', '--net_bind_ip=127.0.0.1', "--net_port_offset=$offset", "--net_peer_endpoints=$peer",
  "--net_instance_id=$Role", "--coop_profile_xuid=$([Convert]::ToUInt64($xuid, 16))", "--coop_profile_name=Agent$Role",
  '--fullscreen=false', '--window_width=960', '--window_height=540', '--log_flush_interval=1') +
  ($Extra -split ' ' | Where-Object { $_ })
$p = Start-Process $exe -ArgumentList $argv -WorkingDirectory (Split-Path $exe) -PassThru
$p.Id
