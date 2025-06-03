@echo off
echo Building slang with MSVC...

REM Clean and create build directory
if exist build_msvc rmdir /s /q build_msvc
mkdir build_msvc
cd build_msvc

REM Try to find Visual Studio 2022
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :cmake
)

REM Try to find Visual Studio 2019
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    goto :cmake
)

echo ERROR: Visual Studio not found!
exit /b 1

:cmake
echo Configuring with CMake...
cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug ..
if %errorlevel% neq 0 (
    echo CMAKE CONFIGURATION FAILED!
    exit /b 1
)

echo Building...
nmake
if %errorlevel% neq 0 (
    echo BUILD FAILED!
    exit /b 1
)

echo BUILD SUCCESSFUL!
cd ..