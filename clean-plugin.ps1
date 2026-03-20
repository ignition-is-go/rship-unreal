param(
    [Parameter(Position=0)]
    [string]$PluginName
)

$projectRoot = $PSScriptRoot
if (-not $projectRoot) { $projectRoot = Get-Location }

$pluginsDir = Join-Path $projectRoot "Plugins"

if (-not $PluginName) {
    Write-Host "Usage: .\clean-plugin.ps1 <PluginName>" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Available plugins:"
    Get-ChildItem -Path $pluginsDir -Directory | ForEach-Object { Write-Host "  $($_.Name)" }
    exit 1
}

$pluginPath = Join-Path $pluginsDir $PluginName

if (-not (Test-Path $pluginPath)) {
    Write-Host "Plugin not found: $PluginName" -ForegroundColor Red
    exit 1
}

Write-Host "Cleaning plugin: $PluginName" -ForegroundColor Cyan

$cleaned = 0

# Plugin binaries and intermediate
foreach ($dir in @("Binaries", "Intermediate")) {
    $target = Join-Path $pluginPath $dir
    if (Test-Path $target) {
        Write-Host "  Removing $PluginName\$dir"
        Remove-Item -Recurse -Force $target
        $cleaned++
    }
}

# Project-level build artifacts (compiled modules + generated shaders)
foreach ($dir in @("Intermediate\Build", "Intermediate\ShaderAutogen")) {
    $target = Join-Path $projectRoot $dir
    if (Test-Path $target) {
        Write-Host "  Removing project $dir"
        Remove-Item -Recurse -Force $target
        $cleaned++
    }
}

# Saved/ShaderDebugInfo
$shaderDebugRoot = Join-Path $projectRoot "Saved\ShaderDebugInfo"
if (Test-Path $shaderDebugRoot) {
    Write-Host "  Clearing Saved\ShaderDebugInfo"
    Remove-Item -Recurse -Force $shaderDebugRoot
    $cleaned++
}

if ($cleaned -eq 0) {
    Write-Host "  Nothing to clean - already clean." -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "Done. Rebuild will happen on next editor launch." -ForegroundColor Green
}
