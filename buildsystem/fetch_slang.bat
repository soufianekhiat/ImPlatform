@echo off
REM Downloads the pinned Slang release for Windows into extern\slang and prints the path of slangc.exe.
REM Does nothing when that version is already there. Uses certutil and the curl.exe and tar.exe of System32
REM (Windows 10 1803 or later), called by full path so that Git's GNU tar or curl on PATH is not picked instead.
setlocal

set VERSION=2026.22
set HAVE=

pushd "%~dp0.."
set DEST=%CD%\extern\slang

if exist "%DEST%\bin\slangc.exe" if exist "%DEST%\VERSION" (
    set /p HAVE=<"%DEST%\VERSION"
)
if "%HAVE%"=="%VERSION%" if exist "%DEST%\bin\slangc.exe" goto done

REM SHA-256 from https://github.com/shader-slang/slang/releases/tag/v2026.22
if /i "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set ASSET=windows-aarch64.zip
    set SHA=3b74b69e1924d359d3626949b3cde0f6e65068a7b5eb2457e83a351697fe4e71
) else (
    set ASSET=windows-x86_64.zip
    set SHA=dd0c4a86bd44cdb7e9cf712af0c8ebf2fff1bba19fa5e3c128ff87384e8ff17b
)

set URL=https://github.com/shader-slang/slang/releases/download/v%VERSION%/slang-%VERSION%-%ASSET%
set ARCHIVE=%CD%\extern\slang-%VERSION%-%ASSET%
if not exist extern mkdir extern
echo Downloading %URL% 1>&2
"%SystemRoot%\System32\curl.exe" -fL --retry 3 -o "%ARCHIVE%" "%URL%" || goto fail

REM certutil prints the hash on its second line
set GOT=
for /f "skip=1 delims=" %%h in ('certutil -hashfile "%ARCHIVE%" SHA256') do if not defined GOT set GOT=%%h
set GOT=%GOT: =%
if /i not "%GOT%"=="%SHA%" (
    del "%ARCHIVE%"
    echo Error: SHA-256 mismatch for %URL% 1>&2
    goto fail
)

REM A previous version's files must not mix with the new ones
if exist "%DEST%" rmdir /s /q "%DEST%"
mkdir "%DEST%"
"%SystemRoot%\System32\tar.exe" -xf "%ARCHIVE%" -C "%DEST%" || goto fail
del "%ARCHIVE%"
>"%DEST%\VERSION" echo %VERSION%

:done
echo %DEST%\bin\slangc.exe
popd
exit /b 0

:fail
popd
exit /b 1
