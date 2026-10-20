param([int]$Id,[string]$Out)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;using System.Runtime.InteropServices;
public class ShotWin{[DllImport("user32.dll")]public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint f);[DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr h,out R r);public struct R{public int l,t,r,b;}}
"@
$p=Get-Process -Id $Id -EA SilentlyContinue; if(!$p){"not running";exit 1}
$h=$p.MainWindowHandle; $q=New-Object ShotWin+R; [ShotWin]::GetWindowRect($h,[ref]$q)|Out-Null
$bmp=New-Object Drawing.Bitmap ($q.r-$q.l),($q.b-$q.t); $gr=[Drawing.Graphics]::FromImage($bmp)
$dc=$gr.GetHdc(); [ShotWin]::PrintWindow($h,$dc,2)|Out-Null; $gr.ReleaseHdc($dc)
$bmp.Save($Out); "saved $Out"
