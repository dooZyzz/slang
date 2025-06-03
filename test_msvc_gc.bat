@echo off
echo Testing MSVC build of GC and related files...

if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
) else (
    echo ERROR: Visual Studio 2022 not found!
    exit /b 1
)

echo.
echo Compiling GC...
cl /nologo /c /I. /Iinclude /D_CRT_SECURE_NO_WARNINGS src/runtime/core/gc.c /Fo:gc.obj
if %errorlevel% neq 0 (
    echo GC compilation failed!
    exit /b 1
)

echo Compiling object_complete.c...
cl /nologo /c /I. /Iinclude /D_CRT_SECURE_NO_WARNINGS src/runtime/core/object_complete.c /Fo:object_complete.obj
if %errorlevel% neq 0 (
    echo object_complete compilation failed!
    exit /b 1
)

echo.
echo SUCCESS: All files compiled successfully with MSVC!
echo.

REM Clean up
del *.obj 2>nul