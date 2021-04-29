@echo OFF

cd "%~dp0%../.."
set ROOT=%cd%
set TESTS_DIR=%ROOT%/tests/windows
set EXAMPLES_DIR=%ROOT%/examples
set JUCE_DIR=%ROOT%/modules/juce
set TRACKTION_ENGINE_DIR=%ROOT%/modules
set PROJUCER_DIR=%JUCE_DIR%/extras/Projucer
set PROJUCER_EXE=%PROJUCER_DIR%/Builds/VisualStudio2017/x64/Release/App/Projucer.exe

if not defined MSBUILD_EXE set MSBUILD_EXE=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin\MSBuild.exe
echo MSBUILD_EXE: "%MSBUILD_EXE%"

::============================================================
::   Build Projucer
::============================================================
call "%TESTS_DIR%/build_projucer.bat" || exit 1

::============================================================
::   Build examples
::============================================================
call :BuildExample "tracktion_graph_PerformanceTests"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call :BuildExample "tracktion_graph_TestRunner"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call :BuildExample "TestRunner"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call :BuildExample "PlaybackDemo"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call :BuildExample "PitchAndTimeDemo"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call :BuildExample "StepSequencerDemo"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call :BuildExample "PatternGeneratorDemo"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call :BuildExample "RecordingDemo"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call :BuildExample "PluginDemo"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call :BuildExample "EngineInPluginDemo"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

call :BuildExample "MidiRecordingDemo"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

exit /b 0

:BuildExample
    echo "=========================================================="
    echo "Building example: %~1%"
    set EXAMPLE_NAME=%~1%
    set EXAMPLE_PIP_FILE=%EXAMPLES_DIR%/%EXAMPLE_NAME%.h
    set EXAMPLE_DEST_DIR=%EXAMPLES_DIR%/projects
    set EXAMPLE_ROOT_DIR=%EXAMPLE_DEST_DIR%/%EXAMPLE_NAME%
    set EXAMLE_PJ_FILE=%EXAMPLE_ROOT_DIR%/%EXAMPLE_NAME%/%EXAMPLE_NAME%.jucer

    if exist "%EXAMPLE_ROOT_DIR%/Builds/VisualStudio2017" rmdir /s /q "%EXAMPLE_ROOT_DIR%/Builds/VisualStudio2017"

    call "%PROJUCER_EXE%" --create-project-from-pip "%EXAMPLE_PIP_FILE%" "%EXAMPLE_DEST_DIR%" "%JUCE_DIR%/modules" "%TRACKTION_ENGINE_DIR%"
    if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
    powershell -Command "(Get-Content '%EXAMLE_PJ_FILE%') -replace '<JUCERPROJECT ', '<JUCERPROJECT cppLanguageStandard=\"17\" ' | Set-Content '%EXAMLE_PJ_FILE%'"
    if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
    call "%PROJUCER_EXE%" --resave "%EXAMLE_PJ_FILE%"
    if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

    if defined DISABLE_BUILD goto builtSection
        cd "%EXAMPLE_ROOT_DIR%/Builds/VisualStudio2017"
        set CL=/DJUCER_ENABLE_GPL_MODE /GL
        "%MSBUILD_EXE%" %EXAMPLE_NAME%.sln /p:VisualStudioVersion=15.0 /m /t:Clean /p:Configuration=Release /p:Platform=x64
        "%MSBUILD_EXE%" %EXAMPLE_NAME%.sln /p:VisualStudioVersion=15.0 /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64 /p:TreatWarningsAsErrors="true" /warnaserror
        if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
    :builtSection
exit /B %ERRORLEVEL%
