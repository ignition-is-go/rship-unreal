@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
pushd "%SCRIPT_DIR%\.." >nul

set "PLUGIN_DIR=Plugins\RshipExec"
set "TARGET_1=%PLUGIN_DIR%\Intermediate"
set "TARGET_2=%PLUGIN_DIR%\Binaries"

echo This will delete only:
echo   %TARGET_1%
echo   %TARGET_2%
echo.

if /I not "%~1"=="--force" (
    set /p "CONFIRM=Continue? [y/N]: "
    if /I not "%CONFIRM%"=="Y" (
        echo Aborted.
        popd >nul
        exit /b 1
    )
)

for %%D in ("%TARGET_1%" "%TARGET_2%") do (
    if exist "%%~D" (
        echo Removing %%~D
        rmdir /s /q "%%~D"
    ) else (
        echo Skipping %%~D (not found)
    )
)

echo Done.
popd >nul
exit /b 0
