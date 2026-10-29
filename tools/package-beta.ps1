# Builds the standalone Crackdown 2 beta ZIP from the current release build (same checks as Crackdown 1's
# tools/package-beta.ps1, without the launcher). Usage: powershell -File tools/package-beta.ps1 -Version 0.5.0
# Build first: build-sdk.bat, then build-cd2.bat. This script does not publish anything.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,39}$')][string]$Version,
    # Single-game launcher build folder (crackdown-recomp launcher, LAUNCHER_GAME=cd2). Also builds the app-launcher
    # ZIP Crackdown2-Windows-<version>.zip (Quiver): the launcher at the top, this package in cd2\. Its name must not
    # start with Crackdown2Recomp-: the combined launcher refuses a release with two ZIPs of its prefix.
    [string]$Launcher
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$sdk = Join-Path $repo 'sdk'
$output = Join-Path $repo 'out/packages'
$packageName = "Crackdown2Recomp-beta$Version"
$stage = Join-Path $output $packageName
$zip = Join-Path $output "$packageName.zip"

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)" }
}
function Git-Text([string]$Directory, [string[]]$Arguments) {
    $result = & git -c core.excludesFile= -C $Directory @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'Git inspection failed' }
    return ($result -join "`n")
}

# The SDK's tracked changes must equal the checked-in patch stack exactly (checked in a scratch index).
function Assert-ReleaseTree {
    $dirty = Git-Text $repo @('status','--porcelain','--ignore-submodules=dirty')
    if ($dirty) { throw "Refusing beta package from a dirty repository:`n$dirty" }
    $expected = Git-Text $repo @('rev-parse','HEAD:sdk')
    $actual = Git-Text $sdk @('rev-parse','HEAD')
    if ($expected -ne $actual) { throw 'SDK checkout differs from the pinned submodule commit' }
    Invoke-Checked git @('-c','core.excludesFile=','-C',$sdk,'diff','--cached','--exit-code')
    $scratch = Join-Path $output ("sdk-index-" + [guid]::NewGuid().ToString('N'))
    $oldIndex = $env:GIT_INDEX_FILE
    try {
        $env:GIT_INDEX_FILE = $scratch
        Invoke-Checked git @('-c','core.excludesFile=','-C',$sdk,'read-tree','HEAD')
        foreach ($patch in Get-ChildItem (Join-Path $repo 'patches/sdk') -Filter '*.patch' | Sort-Object Name) {
            Invoke-Checked git @('-c','core.excludesFile=','-C',$sdk,'apply','--unidiff-zero','--cached',$patch.FullName)
        }
        Invoke-Checked git @('-c','core.excludesFile=','-C',$sdk,'diff','--exit-code','--stat','--ignore-submodules=dirty')
    } finally {
        $env:GIT_INDEX_FILE = $oldIndex
        Remove-Item -LiteralPath $scratch -Force -ErrorAction SilentlyContinue
    }
}

New-Item -ItemType Directory -Path $output -Force | Out-Null
Assert-ReleaseTree
if ((Test-Path -LiteralPath $stage) -or (Test-Path -LiteralPath $zip)) { throw 'This package version already exists; choose a new version' }

$gameBuild = Join-Path $repo 'crackdown2/out/build/win-amd64-release'
$bin = Join-Path $sdk 'out/install/win-amd64/bin'
$cache = Get-Content (Join-Path $gameBuild 'CMakeCache.txt') -Raw
if ($cache -notmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$') { throw 'Game build is not Release' }
# The exe must be newer than every source file and SDK DLL, or the package would ship a stale build.
$exe = Get-Item (Join-Path $gameBuild 'crackdown2.exe')
foreach ($dll in 'rexruntime.dll','rexgpu-xenos.dll') {
    if ((Get-FileHash (Join-Path $bin $dll)).Hash -ne (Get-FileHash (Join-Path $gameBuild $dll)).Hash) { throw "$dll in the game build differs from the SDK install; rebuild" }
}
$newer = Get-ChildItem (Join-Path $repo 'crackdown2/src'), (Join-Path $repo 'crackdown2/config') -Recurse -File | Where-Object LastWriteTime -gt $exe.LastWriteTime
if ($newer) { throw "Sources are newer than crackdown2.exe; run build-cd2.bat:`n$($newer.FullName -join "`n")" }

New-Item -ItemType Directory -Path $stage | Out-Null
# Fixed allowlist: never copy a build directory, game dump, generated sources or config.
foreach ($file in @(
    (Join-Path $gameBuild 'crackdown2.exe'),
    (Join-Path $bin 'rexruntime.dll'),
    (Join-Path $bin 'rexgpu-xenos.dll'),
    (Join-Path $repo 'tools/game-manifest.txt'),
    (Join-Path $repo 'tools/setup_game.py'),
    (Join-Path $repo 'tools/xiso_extract.py'),
    (Join-Path $repo 'tools/stfs_extract.py'),
    (Join-Path $repo 'tools/install_dlc.py'),
    (Join-Path $repo 'tools/beta-README.txt')
)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing package input: $file" }
    $name = Split-Path $file -Leaf
    if ($name -eq 'beta-README.txt') { $name = 'README.txt' }
    Copy-Item -LiteralPath $file -Destination (Join-Path $stage $name)
}

$licenses = Join-Path $stage 'LICENSES'
New-Item -ItemType Directory -Path $licenses | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination (Join-Path $licenses 'Crackdown2Recomp.txt')
Copy-Item -LiteralPath (Join-Path $sdk 'LICENSE') -Destination (Join-Path $licenses 'ReXGlue-and-Xenia.txt')
foreach ($component in Get-ChildItem (Join-Path $sdk 'thirdparty') -Directory) {
    foreach ($notice in Get-ChildItem $component.FullName -File | Where-Object { $_.Name -match '^(LICENSE|COPYING|COPYRIGHT|UNLICENSE)([.-].*)?$' }) {
        Copy-Item -LiteralPath $notice.FullName -Destination (Join-Path $licenses "$($component.Name)-$($notice.Name).txt")
    }
}
foreach ($relative in @('libmspack/libmspack/COPYING.LIB','disruptorplus/LICENSE.txt','tiny-aes-c/unlicense.txt','stb/LICENSE')) {
    $notice = Join-Path $sdk "thirdparty/$relative"
    if (Test-Path -LiteralPath $notice) { Copy-Item -LiteralPath $notice -Destination (Join-Path $licenses (($relative -replace '/','-') + '.txt')) }
}
$sdkVersion = Git-Text $sdk @('describe','--tags','--always')
$repoVersion = Git-Text $repo @('describe','--tags','--always')
$patchList = Get-ChildItem (Join-Path $repo 'patches/sdk') -Filter '*.patch' | Sort-Object Name | ForEach-Object { "$($_.Name) SHA256=$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)" }
@("Crackdown 2 Recomp beta $Version", "Repo: $repoVersion", "SDK: $sdkVersion", 'Applied patches:', $patchList) | Set-Content (Join-Path $stage 'VERSION.txt') -Encoding UTF8

# Every non-system import of the exe and DLLs must be in the package (Windows CRT is a documented prerequisite).
$llvm = Join-Path $env:ProgramFiles 'LLVM/bin/llvm-readobj.exe'
if (-not (Test-Path -LiteralPath $llvm)) { throw 'llvm-readobj is required to inspect binary dependencies' }
$system = '^(?:(?:api-ms-win-|ext-ms-win-)[A-Za-z0-9_-]+|MSVCP140(?:_[A-Z0-9_]+)?|VCRUNTIME140(?:_[0-9]+)?|ucrtbase|KERNEL32|USER32|ole32|OLEAUT32|SHELL32|WS2_32|dxgi|d3d12|bcrypt|ADVAPI32|IMM32|WINMM|SETUPAPI|GDI32|VERSION|HID|SHLWAPI|COMDLG32|CFGMGR32|DWMAPI|NTDLL|SECUR32|CRYPT32|PROPSYS|WINHTTP|XINPUT1_4|DINPUT8|DSOUND|AVRT|MF|MFPlat|MFReadWrite|dcomp|PowrProf|DEVOBJ|MSIMG32|RPCRT4|NORMALIZ|USP10|WINTRUST|PSAPI|IPHLPAPI)\.dll$'
foreach ($binary in Get-ChildItem $stage -File | Where-Object Extension -In '.exe','.dll') {
    $imports = & $llvm --coff-imports $binary.FullName
    if ($LASTEXITCODE -ne 0) { throw 'Binary dependency inspection failed' }
    foreach ($line in $imports) {
        if ($line -notmatch '^\s+Name: (.+\.dll)\s*$') { continue }
        $dependency = $Matches[1]
        if (Test-Path -LiteralPath (Join-Path $stage $dependency)) { continue }
        if ($dependency -notmatch $system) { throw "Unpackaged dependency $dependency in $($binary.Name)" }
    }
}

Assert-ReleaseTree
foreach ($file in Get-ChildItem $stage -Recurse -File) {
    $relative = $file.FullName.Substring($stage.Length + 1)
    if ($relative -match '(^|[\\/])(game|tu|generated)([\\/]|$)|\.xex$|\.xexp$|\.iso$|_recomp\..*\.cpp$') { throw "Forbidden package input: $relative" }
}
# One package directory per ZIP (both launchers strip it). Entry names use '/' (Compress-Archive in PowerShell 5.1
# writes '\').
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
function New-PackageZip([string]$Folder, [string]$Name, [string]$Path) {
    $archive = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in Get-ChildItem $Folder -Recurse -File | Sort-Object FullName) {
            $entry = $Name + '/' + $file.FullName.Substring($Folder.Length + 1).Replace('\', '/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entry, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally { $archive.Dispose() }
    $hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $(Split-Path $Path -Leaf)" | Set-Content -LiteralPath "$Path.sha256" -Encoding ASCII
}
New-PackageZip $stage $packageName $zip
Write-Output "Created local beta package: $zip"

if ($Launcher) {
    $windowsName = "Crackdown2-Windows-$Version"
    $windowsStage = Join-Path $output $windowsName
    $windowsZip = Join-Path $output "$windowsName.zip"
    if ((Test-Path -LiteralPath $windowsStage) -or (Test-Path -LiteralPath $windowsZip)) { throw 'This Windows package version already exists' }
    $launcherExe = Join-Path $Launcher 'Crackdown2Recomp.exe'
    if (-not (Test-Path -LiteralPath $launcherExe -PathType Leaf)) { throw "No Crackdown2Recomp.exe in $Launcher (build the launcher with -DLAUNCHER_GAME=cd2)" }
    New-Item -ItemType Directory -Path $windowsStage | Out-Null
    # Exactly one exe at the top: app launchers start the only one there.
    Copy-Item -LiteralPath $launcherExe -Destination $windowsStage
    Copy-Item -LiteralPath (Join-Path $Launcher 'assets') -Destination $windowsStage -Recurse
    Copy-Item -LiteralPath (Join-Path $Launcher 'LICENSES') -Destination $windowsStage -Recurse
    Copy-Item -LiteralPath (Join-Path $repo 'tools/windows-README.txt') -Destination (Join-Path $windowsStage 'README.txt')
    # The game in cd2\, the layout the combined launcher installs, without the Python setup (the launcher installs).
    $cd2 = Join-Path $windowsStage 'cd2'
    New-Item -ItemType Directory -Path $cd2 | Out-Null
    foreach ($name in 'crackdown2.exe', 'rexruntime.dll', 'rexgpu-xenos.dll', 'game-manifest.txt', 'VERSION.txt') {
        Copy-Item -LiteralPath (Join-Path $stage $name) -Destination $cd2
    }
    Copy-Item -LiteralPath $licenses -Destination $cd2 -Recurse
    $tops = @(Get-ChildItem $windowsStage -File | Where-Object Extension -In '.exe', '.bat', '.cmd')
    if ($tops.Count -ne 1) { throw 'The Windows package must have exactly one program at the top' }
    New-PackageZip $windowsStage $windowsName $windowsZip
    Write-Output "Created local Windows (app launcher) package: $windowsZip"
}
