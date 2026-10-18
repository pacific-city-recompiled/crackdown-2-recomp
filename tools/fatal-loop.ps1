# Run-driven gap filling: run the game with scripted input, read the "invalid or unregistered function" FATAL,
# register that address in config/gapfill.toml, codegen, rebuild, repeat. Usage: tools/fatal-loop.ps1 -Sdk <install dir> [-Max 8] [-Input START,A,A,A]
param([string]$Sdk, [int]$Max = 8, [string]$AutoInput = "START,A,A,A,A,A", [int]$Seconds = 120)
$r = Split-Path $PSScriptRoot -Parent
$b = "$r\crackdown2\out\build\win-amd64-release"
for ($i = 1; $i -le $Max; $i++) {
  Get-Process crackdown2 -EA SilentlyContinue | Stop-Process -Force
  $p = Start-Process "$b\crackdown2.exe" -ArgumentList '--game_data_root=..\..\..\..\game','--update_data_root=..\..\..\..\tu','--gpu_plugin=xenos',"--auto_input=$AutoInput" -WorkingDirectory $b -PassThru
  $p.WaitForExit($Seconds * 1000) | Out-Null
  $alive = -not $p.HasExited
  if ($alive) { Stop-Process $p -Force }
  $f = Get-ChildItem "$b\logs" | sort LastWriteTime | select -Last 1
  $m = Select-String -Path $f.FullName -Pattern "invalid or unregistered function at guest address (0x[0-9A-F]+)" | select -Last 1
  if (-not $m) { "iteration ${i}: no unregistered-function FATAL (alive=$alive)"; break }
  $addr = $m.Matches[0].Groups[1].Value
  "iteration ${i}: FATAL at $addr"
  if (Select-String -Path "$r\crackdown2\config\gapfill.toml" -Pattern "^$addr =" -Quiet) { "$addr already registered; the exe is stale or the entry does not help"; break }
  Add-Content "$r\crackdown2\config\gapfill.toml" "$addr = {}  # run-driven: runtime FATAL 'invalid or unregistered function' (tools/fatal-loop.ps1)"
  Push-Location "$r\crackdown2"
  & "$Sdk\bin\rexglue.exe" codegen crackdown2_manifest.toml > "$r\out\loop_codegen_$i.log" 2>&1
  if (Select-String -Path "$r\out\loop_codegen_$i.log" -Pattern "Unresolved" -Quiet) { "codegen reports unresolved for $addr; stop"; Pop-Location; break }
  Pop-Location
  & cmd /c "call `"$r\build-cd2.bat`" $Sdk > `"$r\out\loop_build_$i.log`" 2>&1"
}
