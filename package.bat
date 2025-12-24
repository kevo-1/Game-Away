@echo off
setlocal

echo.
echo ========================================
echo    Creating Release Package
echo ========================================
echo.

:: Create release folder
if exist "release\" rmdir /s /q release
mkdir release

:: Copy executable
if not exist "GameAway.exe" (
    echo [ERROR] GameAway.exe not found. Run build.bat first.
    pause
    exit /b 1
)
echo Copying GameAway.exe...
copy GameAway.exe release\

:: Copy MinGW runtime DLLs (UCRT64)
set MINGW_BIN=C:\msys64\ucrt64\bin
if exist "%MINGW_BIN%\libstdc++-6.dll" (
    echo Copying MinGW runtime DLLs...
    copy "%MINGW_BIN%\libstdc++-6.dll" release\
    copy "%MINGW_BIN%\libgcc_s_seh-1.dll" release\
    copy "%MINGW_BIN%\libwinpthread-1.dll" release\
)

:: Copy ALL DLLs from vcpkg build output
echo Copying vcpkg dependency DLLs...
if exist "build\vcpkg_installed\x64-mingw-dynamic\bin\*.dll" (
    copy "build\vcpkg_installed\x64-mingw-dynamic\bin\*.dll" release\ 
)

:: Also copy any DLLs that ended up in the project root (from cmake output)
for %%f in (*.dll) do (
    echo Copying %%f...
    copy "%%f" release\
)

echo.
echo ========================================
echo    Release package created!
echo ========================================
echo.
echo Contents of release folder:
dir release\*.* /b
echo.
echo Zip the 'release' folder and upload to GitHub Releases!
echo.
pause
