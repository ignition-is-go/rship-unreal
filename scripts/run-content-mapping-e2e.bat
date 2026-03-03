@echo off
REM Run the content-mapping PIE end-to-end automation gate.
REM Usage:
REM   scripts\run-content-mapping-e2e.bat --uproject "C:\Path\Project.uproject" [--map "/Game/Maps/Main"] [--engine-root "C:\UE_5.7"] [--skip-build]

setlocal enabledelayedexpansion

set UPROJECT_PATH=
set MAP_PATH=/Game/VprodProject/Maps/Main
set ENGINE_ROOT=C:\Program Files\Epic Games\UE_5.7
set TEST_NAME=Rship.ContentMapping.E2E.PIE
set SKIP_BUILD=0

:parse_args
if "%~1"=="" goto args_done
if /I "%~1"=="--uproject" (
  set UPROJECT_PATH=%~2
  shift
  shift
  goto parse_args
)
if /I "%~1"=="--map" (
  set MAP_PATH=%~2
  shift
  shift
  goto parse_args
)
if /I "%~1"=="--engine-root" (
  set ENGINE_ROOT=%~2
  shift
  shift
  goto parse_args
)
if /I "%~1"=="--test" (
  set TEST_NAME=%~2
  shift
  shift
  goto parse_args
)
if /I "%~1"=="--skip-build" (
  set SKIP_BUILD=1
  shift
  goto parse_args
)

echo Unknown argument: %~1
exit /b 64

:args_done
if "%UPROJECT_PATH%"=="" (
  echo Missing --uproject argument.
  exit /b 64
)

if not exist "%UPROJECT_PATH%" (
  echo UProject not found: %UPROJECT_PATH%
  exit /b 66
)

set EDITOR_BIN=%ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe
set BUILD_BIN=%ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat

if not exist "%EDITOR_BIN%" (
  echo UnrealEditor not found: %EDITOR_BIN%
  exit /b 66
)

if not exist "%BUILD_BIN%" (
  echo Build script not found: %BUILD_BIN%
  exit /b 66
)

for %%I in ("%UPROJECT_PATH%") do set PROJECT_NAME=%%~nI
set EDITOR_TARGET=%PROJECT_NAME%Editor

for /f %%T in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set TS=%%T
set REPORT_DIR=%~dp0..\Saved\Automation\ContentMappingE2E-%TS%
if not exist "%REPORT_DIR%" mkdir "%REPORT_DIR%"

echo == Content Mapping E2E ==
echo UProject: %UPROJECT_PATH%
echo Map: %MAP_PATH%
echo Test: %TEST_NAME%
echo Engine: %ENGINE_ROOT%
echo Report: %REPORT_DIR%

if "%SKIP_BUILD%"=="0" (
  echo == Build ==
  call "%BUILD_BIN%" %EDITOR_TARGET% Win64 Development "%UPROJECT_PATH%" -waitmutex
  if errorlevel 1 exit /b %errorlevel%
)

echo == Automation ==
call "%EDITOR_BIN%" "%UPROJECT_PATH%" "%MAP_PATH%" ^
  -unattended -nop4 -nosplash -NoSound ^
  -ini:Engine:[SystemSettings]:CommonUI.Debug.CheckGameViewportClientValid=0 ^
  -CommonUI.Debug.CheckGameViewportClientValid=0 ^
  -ExecCmds="Automation RunTests %TEST_NAME%" ^
  -TestExit="Automation Test Queue Empty" ^
  -ReportExportPath="%REPORT_DIR%" ^
  -RshipContentMappingE2EMap="%MAP_PATH%"

if errorlevel 1 exit /b %errorlevel%

set RESULT_FILE=%REPORT_DIR%\index.json
if not exist "%RESULT_FILE%" (
  echo FAIL: automation report not found: %RESULT_FILE%
  exit /b 3
)

powershell -NoProfile -Command ^
  "$p=$env:RESULT_FILE; try { $j = Get-Content -Raw -Encoding UTF8 $p | ConvertFrom-Json; " ^
  "Write-Output ('Automation summary: succeeded={0} succeeded_with_warnings={1} failed={2} not_run={3}' -f $j.succeeded,$j.succeededWithWarnings,$j.failed,$j.notRun); " ^
  "if ([int]$j.failed -gt 0) { exit 2 } else { exit 0 } } catch { " ^
  "Write-Output ('Automation summary: error=report-read-failed detail=' + $_.Exception.Message); exit 3 }"
if errorlevel 1 (
  echo FAIL: automation reported one or more failed tests.
  echo Report output: %REPORT_DIR%
  exit /b %errorlevel%
)

echo PASS: automation completed.
echo Report output: %REPORT_DIR%
exit /b 0
