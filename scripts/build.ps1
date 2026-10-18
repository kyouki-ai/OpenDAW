param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$CoreOnly
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $root
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) { $cmakePath = $cmake.Source }
else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install CMake 3.22+ and Visual Studio C++ Build Tools.' }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $cmakePath = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    if (!(Test-Path -LiteralPath $cmakePath)) { throw 'CMake not found in Visual Studio.' }
}
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
$env:_CL_ = '/MP1'
$buildDir = if ($CoreOnly) { 'build/core' } else { 'build/windows' }
$configureArgs = @('-S', '.', '-B', $buildDir, '-G', 'Visual Studio 17 2022', '-A', 'x64')
if ($CoreOnly) { $configureArgs += '-DOPENDAW_BUILD_APP=OFF' }
elseif (Test-Path -LiteralPath '.tools/JUCE-8.0.12/CMakeLists.txt') {
    $configureArgs += "-DOPENDAW_JUCE_PATH=$root/.tools/JUCE-8.0.12"
}
& $cmakePath @configureArgs
if ($LASTEXITCODE -ne 0) { throw "Configure failed: $LASTEXITCODE" }
& $cmakePath --build $buildDir --config $Configuration --parallel 1
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
$ctestPath = Join-Path (Split-Path $cmakePath) 'ctest.exe'
& $ctestPath --test-dir $buildDir -C $Configuration --output-on-failure --parallel 1
if ($LASTEXITCODE -ne 0) { throw "Tests failed: $LASTEXITCODE" }
