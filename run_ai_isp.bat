@echo off
REM AI ISP 测试程序运行脚本

echo ========================================
echo   AI ISP Test Runner
echo ========================================

set WORK_DIR=%~dp0
set EXE_PATH=%WORK_DIR%HDR-ISP-main\build\srcs\ai_isp_test_exe.exe

if not exist "%EXE_PATH%" (
    echo.
    echo [ERROR] AI ISP test executable not found!
    echo Please build first using: build_ai_isp.bat
    echo.
    pause
    exit /b 1
)

echo.
echo Running AI ISP test program...
echo Executable: %EXE_PATH%
echo.
echo ========================================
echo.

"%EXE_PATH%"

echo.
echo ========================================
echo.
echo Test completed. Check results above.
echo.
pause