@echo off
echo Cleaning Unreal project build artifacts...

:: Project root folders
if exist "Binaries" (
    echo Deleting Binaries...
    rmdir /s /q "Binaries"
)

if exist "Intermediate" (
    echo Deleting Intermediate...
    rmdir /s /q "Intermediate"
)

if exist "DerivedDataCache" (
    echo Deleting DerivedDataCache...
    rmdir /s /q "DerivedDataCache"
)

:: Plugin folders
for /d %%p in (Plugins\*) do (
    if exist "%%p\Binaries" (
        echo Deleting %%p\Binaries...
        rmdir /s /q "%%p\Binaries"
    )
    if exist "%%p\Intermediate" (
        echo Deleting %%p\Intermediate...
        rmdir /s /q "%%p\Intermediate"
    )
)

echo.
echo Clean complete. Rebuild required on next editor launch.
