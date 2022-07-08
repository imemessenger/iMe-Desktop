@echo off
setlocal enabledelayedexpansion
set "FullScriptPath=%~dp0"
set "FullExecPath=%cd%"

set "Command=%1"
if "%Command%" == "header" (
  call :write_header %2
  exit /b %errorlevel%
) else if "%Command%" == "source" (
  call :write_source %2
  exit /b %errorlevel%
) else if "%Command%" == "" (
  echo This is an utility for fast blank module creation.
  echo Please provide module path.
  exit /b
)

call :write_module %Command%
exit /b %errorlevel%

:write_module
(
  set "CommandPath=%1"
  set "CommandPathUnix=!CommandPath:\=/!"
  if "!CommandPathUnix!" == "" (
    echo Provide module path.
    exit /b 1
  )
  echo Generating module !CommandPathUnix!..
  call %FullScriptPath%\create.bat header !CommandPathUnix!
  call %FullScriptPath%\create.bat source !CommandPathUnix!
  exit /b
)

:write_header
(
  set "CommandPath=%1"
  set "CommandPathUnix=!CommandPath:\=/!"
  set "CommandPathWin=!CommandPath:/=\!"

  if "!CommandPathUnix!" == "" (
    echo Provide header path.
    exit /b 1
  ) else if exist "!CommandPathWin!.h" (
    echo This header already exists.
    exit /b 1
  )
  echo Generating header !CommandPathUnix!.h..
  mkdir "!CommandPathWin!.h"
  rmdir "!CommandPathWin!.h"

  call :write_comment !CommandPathWin!.h
  set "header1=#pragma once"
  (
    echo !header1!
    echo.
  )>> "!CommandPathWin!.h"
  exit /b
)

:write_source
(
  set "CommandPath=%1"
  set "CommandPathUnix=!CommandPath:\=/!"
  set "CommandPathWin=!CommandPath:/=\!"
  if "!CommandPathUnix:~-4!" == "_mac" (
    set "CommandExt=mm"
  ) else (
    set "CommandExt=cpp"
  )
  if "!CommandPathUnix!" == "" (
    echo Provide source path.
    exit /b 1
  ) else if exist "!CommandPathWin!.!CommandExt!" (
    echo This source already exists.
    exit /b 1
  )
  echo Generating source !CommandPathUnix!.!CommandExt!..
  mkdir "!CommandPathWin!.!CommandExt!"
  rmdir "!CommandPathWin!.!CommandExt!"

  call :write_comment !CommandPathWin!.!CommandExt!
  set "quote="""
  set "quote=!quote:~0,1!"
  set "source1=#include !quote!!CommandPathUnix!.h!quote!"
  (
    echo !source1!
    echo.
  )>> "!CommandPathWin!.!CommandExt!"
  exit /b
)

:write_comment
(
  set "Path=%1"
  (
    echo // This file is part of Desktop App Toolkit,
    echo // a set of libraries for developing nice desktop applications.
    echo //
    echo // For license and copyright information please follow this link:
    echo // https://github.com/desktop-app/legal/blob/master/LEGAL
    echo //
  )> "!Path!"
  exit /b
)
