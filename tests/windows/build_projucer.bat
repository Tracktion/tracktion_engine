@echo OFF

cd "%~dp0%../.."
set ROOT=%cd%

if not defined MSBUILD_EXE set MSBUILD_EXE=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe

::=========================================================
echo "Building Projucer"
::=========================================================
set PROJUCER_ROOT=%ROOT%/modules/juce/extras/Projucer/Builds/VisualStudio2017
set PROJUCER_EXE=%PROJUCER_ROOT%/x64/Release/App/Projucer.exe
cd "%PROJUCER_ROOT%"
set CL=/DJUCER_ENABLE_GPL_MODE /GL
"%MSBUILD_EXE%" Projucer.sln /p:VisualStudioVersion=15.0 /m /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64
if not exist "%PROJUCER_EXE%" exit 1
