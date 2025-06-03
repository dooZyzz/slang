@echo off
echo Testing MSVC compilation...

REM Try to find Visual Studio 2022
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :compile
)

REM Try to find Visual Studio 2019
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :compile
)

echo ERROR: Visual Studio not found!
exit /b 1

:compile
echo Compiling a simple test...
cl /nologo /c /I. /Iinclude src/runtime/core/gc.c /Fo:test_gc.obj
if %errorlevel% neq 0 (
    echo COMPILATION FAILED!
    exit /b 1
)

echo SUCCESS: GC compiled with MSVC!
del test_gc.obj