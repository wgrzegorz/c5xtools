@echo off
rem build.bat - compile the four c5xtools programs on Windows without make and
rem without tests. Uses MSVC (cl) if it is on PATH, otherwise MinGW-w64 (gcc).
rem
rem   - For MSVC, run from a "Developer Command Prompt for VS" (so cl is found),
rem     or after setting up the MSVC environment (vcvarsall / msvc-dev-cmd).
rem   - For MinGW, put the toolchain's bin directory on PATH so gcc is found.
rem
rem Produces c5xasm.exe, c5xlnk.exe, c5xhex.exe and c5xdis.exe in the repo root.

setlocal
cd /d "%~dp0.."

rem Stamp the version into the banner from the single source (VERSION), so the
rem binaries do not fall back to the hard-coded version in include\c5xbanner.h.
set VER=0.0.0
if exist VERSION set /p VER=<VERSION
set BUILD=
if defined SOURCE_DATE_EPOCH (
  for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "[DateTimeOffset]::FromUnixTimeSeconds($env:SOURCE_DATE_EPOCH).UtcDateTime.ToString('yyyy-MM-dd')"`) do set BUILD=%%i
) else (
  for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "(Get-Date).ToUniversalTime().ToString('yyyy-MM-dd')"`) do set BUILD=%%i
)
set GITDESC=
for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "git describe --tags --always --dirty --abbrev=8 2>$null"`) do set GITDESC=%%i
rem Append the git description only for non-release builds. At the exact release
rem tag (describe == v<VERSION>) the banner stays clean: "<tool> X.Y.Z build <date>".
set USEGIT=1
if not defined GITDESC set USEGIT=0
if "%GITDESC%"=="v%VER%" set USEGIT=0
echo building c5xtools %VER% %BUILD% %GITDESC%

set COFF=lib\c5xcoff\c5xcoff.c

where cl >nul 2>nul
if %errorlevel%==0 goto msvc
where gcc >nul 2>nul
if %errorlevel%==0 goto mingw

echo build.bat: no compiler found - put cl (MSVC) or gcc (MinGW) on PATH. 1>&2
exit /b 1

:msvc
echo Building with MSVC (cl)...
rem /std:c11 is required: the sources are C99 (declarations in for-init, mixed
rem declarations and statements), which the default MSVC C mode (C89) rejects.
rem CLFLAGS_EXTRA lets a caller add flags, e.g. /MT for a static CRT in release.
if not defined CLFLAGS_EXTRA set "CLFLAGS_EXTRA="
set INC=/I include /I lib\c5xcoff
set CLFLAGS=/nologo /std:c11 /O2 /D_CRT_SECURE_NO_WARNINGS %CLFLAGS_EXTRA% /DC5XTOOLS_VERSION=\"%VER%\" /DC5XTOOLS_BUILD=\"%BUILD%\"
if "%USEGIT%"=="1" set CLFLAGS=%CLFLAGS% "/DC5XTOOLS_GIT=\" +g%GITDESC%\""
cl %CLFLAGS% %INC% tools\c5xasm\src\c5xasm.c tools\c5xasm\src\operands.c %COFF% /Fe:c5xasm.exe || exit /b 1
cl %CLFLAGS% %INC% tools\c5xlnk\src\c5xlnk.c %COFF% /Fe:c5xlnk.exe || exit /b 1
cl %CLFLAGS% %INC% tools\c5xhex\src\c5xhex.c %COFF% /Fe:c5xhex.exe || exit /b 1
cl %CLFLAGS% %INC% tools\c5xdis\src\c5xdis.c %COFF% /Fe:c5xdis.exe || exit /b 1
del *.obj >nul 2>nul
goto done

:mingw
echo Building with MinGW (gcc)...
set INC=-Iinclude -Ilib\c5xcoff -DC5XTOOLS_VERSION=\"%VER%\" -DC5XTOOLS_BUILD=\"%BUILD%\"
if "%USEGIT%"=="1" set INC=%INC% "-DC5XTOOLS_GIT=\" +g%GITDESC%\""
gcc -std=c99 -O2 %INC% tools\c5xasm\src\c5xasm.c tools\c5xasm\src\operands.c %COFF% -o c5xasm.exe || exit /b 1
gcc -std=c99 -O2 %INC% tools\c5xlnk\src\c5xlnk.c %COFF% -o c5xlnk.exe || exit /b 1
gcc -std=c99 -O2 %INC% tools\c5xhex\src\c5xhex.c %COFF% -o c5xhex.exe || exit /b 1
gcc -std=c99 -O2 %INC% tools\c5xdis\src\c5xdis.c %COFF% -o c5xdis.exe || exit /b 1
goto done

:done
echo done
endlocal
