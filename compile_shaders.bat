@echo off
setlocal enabledelayedexpansion
echo ================================================
echo      Compiling Shaders with Include Guards
echo ================================================
echo.

REM Compile Shader2D
echo [1/2] Compiling Shader2D.glsl...
sokol-shdc --input Shader2D.glsl --output Shader2D.h --slang hlsl5
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile Shader2D.glsl
    pause
    exit /b 1
)
echo       SUCCESS: Shader2D.h generated
echo.

REM Compile Shader3D
echo [2/2] Compiling Shader3D.glsl...
sokol-shdc --input Shader3D.glsl --output Shader3D.h --slang hlsl5
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile Shader3D.glsl
    pause
    exit /b 1
)
echo       SUCCESS: Shader3D.h generated
echo.

REM Compile Shader3DLit
echo [2/2] Compiling Shader3DLit.glsl...
sokol-shdc --input Shader3DLit.glsl --output Shader3DLit.h --slang hlsl5
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile Shader3DLit.glsl
    pause
    exit /b 1
)
echo       SUCCESS: Shader3DLit.h generated
echo.


echo ================================================
echo      Adding Include Guards to vs_params_t
echo ================================================
echo.

REM Process Shader2D.h
echo [1/2] Adding guards to Shader2D.h...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$content = [System.IO.File]::ReadAllText('Shader2D.h'); $nl = [Environment]::NewLine; $content = $content -replace '(?s)(#pragma pack\(push,1\).*?\} vs_params_t;)[\r\n]+#pragma pack\(pop\)', (\"#ifndef VS_PARAMS_T_DEFINED$nl#define VS_PARAMS_T_DEFINED$nl`$1$nl#pragma pack(pop)$nl#endif\"); [System.IO.File]::WriteAllText('Shader2D.h', $content)"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to add guards to Shader2D.h
    pause
    exit /b 1
)
echo       SUCCESS: Include guards added to Shader2D.h
echo.

REM Process Shader3D.h
echo [2/2] Adding guards to Shader3D.h...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$content = [System.IO.File]::ReadAllText('Shader3D.h'); $nl = [Environment]::NewLine; $content = $content -replace '(?s)(#pragma pack\(push,1\).*?\} vs_params_t;)[\r\n]+#pragma pack\(pop\)', (\"#ifndef VS_PARAMS_T_DEFINED$nl#define VS_PARAMS_T_DEFINED$nl`$1$nl#pragma pack(pop)$nl#endif\"); [System.IO.File]::WriteAllText('Shader3D.h', $content)"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to add guards to Shader3D.h
    pause
    exit /b 1
)
echo       SUCCESS: Include guards added to Shader3D.h
echo.

REM Process Shader3DLit.h
echo [1/2] Adding guards to Shader3DLit.h...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$content = [System.IO.File]::ReadAllText('Shader3DLit.h'); $nl = [Environment]::NewLine; $content = $content -replace '(?s)(#pragma pack\(push,1\).*?\} vs_params_t;)[\r\n]+#pragma pack\(pop\)', (\"#ifndef VS_PARAMS_T_DEFINED$nl#define VS_PARAMS_T_DEFINED$nl`$1$nl#pragma pack(pop)$nl#endif\"); [System.IO.File]::WriteAllText('Shader3DLit.h', $content)"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to add guards to Shader3DLit.h
    pause
    exit /b 1
)
echo       SUCCESS: Include guards added to Shader3DLit.h
echo.

echo ================================================
echo      Shader Compilation Complete!
echo ================================================
echo.
echo All shaders compiled successfully with include guards.
echo You can now build your project.
echo.
pause