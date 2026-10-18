# Unattended test run: start the game with scripted input, save game-window screenshots at the given seconds,
# then close it. Usage: tools/run-shots.ps1 -AutoInput "START,A,..." -At 60,90 -Name test [-Extra "--flag=x"]
param([string]$AutoInput, [int[]]$At, [string]$Name = "run", [string[]]$Extra = @())
$r = Split-Path $PSScriptRoot -Parent
$b = "$r\crackdown2\out\build\win-amd64-release"
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class ShotWin{[DllImport("user32.dll")]public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint f);[DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr h,out R r);public struct R{public int l,t,r,b;}}
"@
if (Get-Process crackdown2 -EA SilentlyContinue) { "crackdown2 is already running (maybe someone is playing): not starting a test run"; exit 1 }
$argv = @('--game_data_root=..\..\..\..\game', '--update_data_root=..\..\..\..\tu', '--gpu_plugin=xenos', "--auto_input=$AutoInput") + $Extra
$p = Start-Process "$b\crackdown2.exe" -ArgumentList $argv -WorkingDirectory $b -PassThru
$last = 0
foreach ($t in $At) {
  Start-Sleep ($t - $last); $last = $t
  $p.Refresh(); if ($p.HasExited) { "exited before $t s (code $($p.ExitCode))"; break }
  $h = $p.MainWindowHandle; $q = New-Object ShotWin+R; [ShotWin]::GetWindowRect($h, [ref]$q) | Out-Null
  $bmp = New-Object Drawing.Bitmap ($q.r - $q.l), ($q.b - $q.t); $g = [Drawing.Graphics]::FromImage($bmp)
  $dc = $g.GetHdc(); [ShotWin]::PrintWindow($h, $dc, 2) | Out-Null; $g.ReleaseHdc($dc)
  $bmp.Save("$r\out\${Name}_$t.png"); "saved out\${Name}_$t.png"
}
$alive = -not $p.HasExited
if ($alive) { $p.Kill(); $p.WaitForExit(15000) | Out-Null }
$f = Get-ChildItem "$b\logs" | sort LastWriteTime | select -Last 1
"alive=$alive log=$($f.Name)"
Select-String -Path $f.FullName -Pattern "critical|FATAL|Unhandled guest" | select -First 3 | % { $_.Line.Substring(0, [Math]::Min(200, $_.Line.Length)) }
