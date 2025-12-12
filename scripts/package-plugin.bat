@echo off
REM Package RshipExec plugin for distribution
REM Usage: scripts\package-plugin.bat [ue-version] [output-dir]
REM
REM Examples:
REM   scripts\package-plugin.bat 5.6 dist
REM   scripts\package-plugin.bat 5.5 C:\output

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set REPO_ROOT=%SCRIPT_DIR%..

set UE_VERSION=%1
if "%UE_VERSION%"=="" set UE_VERSION=5.6

set OUTPUT_DIR=%2
if "%OUTPUT_DIR%"=="" set OUTPUT_DIR=%REPO_ROOT%\dist

set PLUGIN_DIR=%REPO_ROOT%\Plugins\RshipExec
set UE_ROOT=C:\Program Files\Epic Games\UE_%UE_VERSION%
set UAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat

REM Verify UE installation
if not exist "%UAT%" (
    echo Error: Unreal Engine %UE_VERSION% not found at %UE_ROOT%
    echo Please install UE %UE_VERSION% or specify correct path
    exit /b 1
)

REM Verify plugin exists
if not exist "%PLUGIN_DIR%\RshipExec.uplugin" (
    echo Error: RshipExec.uplugin not found at %PLUGIN_DIR%
    exit /b 1
)

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
set OUTPUT_PATH=%OUTPUT_DIR%\RshipExec-%UE_VERSION%-Win64

echo ==========================================
echo Packaging RshipExec Plugin
echo ==========================================
echo UE Version:  %UE_VERSION%
echo Platform:    Win64
echo Plugin:      %PLUGIN_DIR%
echo Output:      %OUTPUT_PATH%
echo ==========================================

REM Run UAT BuildPlugin command
call "%UAT%" BuildPlugin ^
    -Plugin="%PLUGIN_DIR%\RshipExec.uplugin" ^
    -Package="%OUTPUT_PATH%" ^
    -TargetPlatforms=Win64 ^
    -Rocket ^
    -VS2022

if %ERRORLEVEL% neq 0 (
    echo.
    echo Error: Plugin packaging failed
    exit /b %ERRORLEVEL%
)

echo.
echo ==========================================
echo Plugin packaged successfully!
echo Output: %OUTPUT_PATH%
echo ==========================================

REM Create zip if 7zip is available
where 7z >nul 2>nul
if %ERRORLEVEL% equ 0 (
    cd "%OUTPUT_DIR%"
    set ZIP_NAME=RshipExec-%UE_VERSION%-Win64.zip
    if exist "!ZIP_NAME!" del "!ZIP_NAME!"
    7z a "!ZIP_NAME!" "RshipExec-%UE_VERSION%-Win64"
    echo Created: %OUTPUT_DIR%\!ZIP_NAME!
)

endlocal
