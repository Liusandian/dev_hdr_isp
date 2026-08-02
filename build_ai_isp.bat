@echo off
REM AI ISP 模块构建脚本

echo ========================================
echo   AI ISP Module Build Script
echo ========================================

set CMAKE_PATH=d:\01-Work\01-tools\w64devkit\bin\cmake.exe
set WORK_DIR=%~dp0
set PROJECT_DIR=%WORK_DIR%HDR-ISP-main
set BUILD_DIR=%PROJECT_DIR%\build

echo.
echo [1/3] Cleaning build directory...
if exist "%BUILD_DIR%" (
    rmdir /s /q "%BUILD_DIR%"
    echo Build directory cleaned.
) else (
    echo Build directory does not exist.
)

echo.
echo [2/3] Configuring CMake with AI ISP support...
"%CMAKE_PATH%" -S "%PROJECT_DIR%" -B "%BUILD_DIR%" ^
    -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_C_COMPILER=d:/01-Work/01-tools/w64devkit/bin/gcc.exe ^
    -DCMAKE_CXX_COMPILER=d:/01-Work/01-tools/w64devkit/bin/g++.exe ^
    -DBUILD_AI_ISP_MODULES=ON

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)

echo.
echo [3/3] Building AI ISP modules...
"%CMAKE_PATH%" --build "%BUILD_DIR%" --config Debug --target ai_isp_test_exe -- -j4

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo   Build Completed Successfully!
echo ========================================
echo.
echo Executable: %BUILD_DIR%\srcs\ai_isp_test_exe.exe
echo.
echo To run the test:
echo   %BUILD_DIR%\srcs\ai_isp_test_exe.exe
echo.

pause