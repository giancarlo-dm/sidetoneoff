# Builds windows\build\SidetoneOff.exe with Visual Studio C++ toolset on this PC, with the app icon drawn by
# make-icon.ps1, and runs the unit tests.
# Usage (Windows PowerShell 5.1 or PowerShell 7): .\windows\build.ps1
# Fails fast: any tool error, a failing unit test, a toolset older than MSVC 14.52, or an exe over 300 KB
# stops the build.

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false  # PowerShell 7: exit codes are checked by Invoke-Tool

$MinToolset = [version]'14.52'  # first MSVC with stable /std:c++23
$MaxExeSize = 300KB

$Root     = $PSScriptRoot
$Src      = Join-Path $Root 'src'
$Res      = Join-Path $Root 'res'
$Build    = Join-Path $Root 'build'
$Obj      = Join-Path $Build 'obj'
$Exe      = Join-Path $Build 'SidetoneOff.exe'
$Pdb      = Join-Path $Build 'SidetoneOff.pdb'
$Manifest = Join-Path $Res 'app.manifest'
$Tests    = Join-Path $Root 'tests'
$TestObj  = Join-Path $Build 'obj-tests'
$TestExe  = Join-Path $Build 'SidetoneOffTests.exe'

# The portable sources the unit tests link against (no Windows code).
$TestedSources = @('hidpp', 'profile')

$CompileFlags = @(
    '/nologo', '/std:c++23', '/permissive-', '/utf-8', '/W4', '/WX', '/sdl'
    '/MT'                  # static CRT: no VC++ redistributable needed
    '/O1', '/GL', '/Zi', '/EHsc', '/guard:cf'
    '/DUNICODE', '/D_UNICODE', '/DWIN32_LEAN_AND_MEAN', '/DNOMINMAX', '/D_WIN32_WINNT=0x0A00'
    '/c'
)
$LinkFlags = @(
    '/nologo', '/WX', '/LTCG', '/MACHINE:X64', '/SUBSYSTEM:WINDOWS'
    '/DEBUG', '/INCREMENTAL:NO', '/OPT:REF', '/OPT:ICF', '/PDBALTPATH:%_PDB%'  # pdb without the local path
    '/GUARD:CF', '/CETCOMPAT'
    '/MANIFEST:EMBED', '/MANIFESTUAC:NO', "/MANIFESTINPUT:$Manifest"
    "/OUT:$Exe", "/PDB:$Pdb"
)
$Libs = @(
    'kernel32.lib', 'user32.lib', 'gdi32.lib', 'shell32.lib', 'comctl32.lib', 'advapi32.lib'
    'ole32.lib', 'uuid.lib', 'hid.lib', 'cfgmgr32.lib'
)

function Invoke-Tool {
    param([Parameter(Mandatory)][string]$Name, [Parameter(ValueFromRemainingArguments)][string[]]$Arguments)
    & $Name @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Name failed with exit code $LASTEXITCODE" }
}

# Enter-VsDevShell changes this process's environment; restore it afterwards so the caller's
# terminal is left as it was.
$savedEnv = @{}
Get-ChildItem env: | ForEach-Object { $savedEnv[$_.Name] = $_.Value }

try {
    # Find Visual Studio with the x64 C++ toolset. Build Tools are only listed with -products *.
    $installer = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer'
    $vswhere = Join-Path $installer 'vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found. Install the build tools: winget install --id Microsoft.VisualStudio.BuildTools -e"
    }
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'No Visual Studio installation with the x64 C++ toolset was found.' }

    # VsDevCmd calls vswhere by name, so it has to be on PATH.
    $env:PATH = "$installer;$env:PATH"
    Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64 -no_logo' | Out-Null

    $toolset = [version]$env:VCToolsVersion
    if ($toolset -lt $MinToolset) { throw "MSVC $toolset is too old: /std:c++23 needs $MinToolset or newer." }
    Write-Host "==> MSVC $toolset, Windows SDK $($env:WindowsSDKVersion.TrimEnd('\'))"

    # Always a clean build: the project is small and this keeps every build reproducible.
    if (Test-Path $Build) {
        try { Remove-Item $Build -Recurse -Force }
        catch { throw "Can't clean $Build. Is SidetoneOff.exe running from it? $($_.Exception.Message)" }
    }
    New-Item -ItemType Directory -Path $Obj, $TestObj | Out-Null

    Write-Host '==> Icon'
    & (Join-Path $Root 'make-icon.ps1') -Out (Join-Path $Build 'app.ico')

    Write-Host '==> Resources'
    Invoke-Tool rc /nologo /I $Src /I $Build /fo (Join-Path $Obj 'app.res') (Join-Path $Res 'app.rc')

    Write-Host '==> Compile'
    $sources = @(Get-ChildItem $Src -Filter *.cpp | ForEach-Object FullName)
    Push-Location $Obj  # cl writes .obj files to the current directory
    try { Invoke-Tool cl @CompileFlags @sources }
    finally { Pop-Location }

    # Tests run before the app is linked, so a failing test leaves no new SidetoneOff.exe behind.
    Write-Host '==> Tests'
    Push-Location $TestObj
    try { Invoke-Tool cl @CompileFlags /I $Src (Join-Path $Tests 'tests.cpp') }
    finally { Pop-Location }
    $testedObjects = @($TestedSources | ForEach-Object { Join-Path $Obj "$_.obj" })
    Invoke-Tool link /nologo /WX /LTCG /MACHINE:X64 /SUBSYSTEM:CONSOLE /INCREMENTAL:NO /MANIFEST:EMBED `
        "/OUT:$TestExe" (Join-Path $TestObj 'tests.obj') @testedObjects
    & $TestExe
    if ($LASTEXITCODE -ne 0) { throw 'Unit tests failed.' }

    Write-Host '==> Link'
    $objects = @(Get-ChildItem $Obj -Filter *.obj | ForEach-Object FullName)
    Invoke-Tool link @LinkFlags @objects (Join-Path $Obj 'app.res') @Libs

    $size = (Get-Item $Exe).Length
    if ($size -gt $MaxExeSize) { throw "SidetoneOff.exe is $([math]::Ceiling($size / 1KB)) KB, over the 300 KB limit." }
    Write-Host "==> Built $Exe ($([math]::Ceiling($size / 1KB)) KB)"
}
finally {
    Get-ChildItem env: | Where-Object { -not $savedEnv.ContainsKey($_.Name) } |
        ForEach-Object { Remove-Item -LiteralPath "env:$($_.Name)" }
    foreach ($name in $savedEnv.Keys) { Set-Item -LiteralPath "env:$name" -Value $savedEnv[$name] }
}
