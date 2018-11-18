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
call :BuildExample "PlaybackDemo"
call :BuildExample "PitchAndTimeDemo"
call :BuildExample "StepSequencerDemo"
exit /B %ERRORLEVEL%

:BuildExample
    echo "=========================================================="
    echo "Building example: %~1%"
    set EXAMPLE_NAME=%~1%
    set EXAMPLE_PIP_FILE=%EXAMPLES_DIR%/%EXAMPLE_NAME%.h
    set EXAMPLE_DEST_DIR=%EXAMPLES_DIR%/projects
    set EXAMPLE_ROOT_DIR=%EXAMPLE_DEST_DIR%/%EXAMPLE_NAME%

    call "%PROJUCER_EXE%" --create-project-from-pip "%EXAMPLE_PIP_FILE%" "%EXAMPLE_DEST_DIR%" "%JUCE_DIR%/modules" "%TRACKTION_ENGINE_DIR%"
    call "%PROJUCER_EXE%" --resave "%EXAMPLE_ROOT_DIR%/%EXAMPLE_NAME%.jucer"
    cd "%EXAMPLE_ROOT_DIR%/Builds/VisualStudio2017"
    "%MSBUILD_EXE%" %EXAMPLE_NAME%.sln /p:VisualStudioVersion=15.0 /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64 /p:TreatWarningsAsErrors=true
exit /B 0
