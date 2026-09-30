@echo off
REM Windows shim for `oops-apps`: finds Git Bash and runs `bin/oops-apps` with it.
setlocal
set "OOPS_APPS_SH=%~dp0oops-apps"

REM Git Bash first, PATH last: the bash on a default PATH is System32\bash.exe, the WSL
REM launcher, which cannot take a Windows path to the script.
set "BASH="
if exist "%ProgramFiles%\Git\bin\bash.exe" set "BASH=%ProgramFiles%\Git\bin\bash.exe"
if not defined BASH if exist "%ProgramFiles(x86)%\Git\bin\bash.exe" set "BASH=%ProgramFiles(x86)%\Git\bin\bash.exe"
if not defined BASH if exist "%LOCALAPPDATA%\Programs\Git\bin\bash.exe" set "BASH=%LOCALAPPDATA%\Programs\Git\bin\bash.exe"
if not defined BASH for %%B in (bash.exe) do if /i not "%%~dpB"=="%SystemRoot%\System32\" set "BASH=%%~$PATH:B"

if not defined BASH (
    echo error: oops-apps needs bash, and none was found. 1>&2
    echo Install Git for Windows, which provides it: https://git-scm.com/download/win 1>&2
    exit /b 1
)

"%BASH%" "%OOPS_APPS_SH%" %*
exit /b %ERRORLEVEL%
