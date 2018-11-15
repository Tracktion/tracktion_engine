/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


static const uint8 cmdInitNativeMode[] = { 0xf0, 0x00, 0x01, 0x40, 0x10, 0x01, 0x00, 0xf7 };
static const uint8 cmdInquiry[]        = { 0xf0, 0x7e, 0x00, 0x06, 0x01, 0xf7 };

static const uint8 cmdLEDRecordOn[]    = { 0x90, 0x5f, 0x7f };
static const uint8 cmdLEDRecordOff[]   = { 0x90, 0x5f, 0x00 };
static const uint8 cmdLEDArmRecOn[]    = { 0x90, 0x00, 0x7f };
static const uint8 cmdLEDArmRecOff[]   = { 0x90, 0x00, 0x00 };
static const uint8 cmdLEDMuteOn[]      = { 0x90, 0x10, 0x7f };
static const uint8 cmdLEDMuteOff[]     = { 0x90, 0x10, 0x00 };
static const uint8 cmdLEDSoloOn[]      = { 0x90, 0x08, 0x7f };
static const uint8 cmdLEDSoloOff[]     = { 0x90, 0x08, 0x00 };
static const uint8 cmdLEDAnySoloOn[]   = { 0x90, 0x73, 0x7f };
static const uint8 cmdLEDAnySoloOff[]  = { 0x90, 0x73, 0x00 };
static const uint8 cmdLEDPunchOn[]     = { 0x90, 0x78, 0x7f };
static const uint8 cmdLEDPunchOff[]    = { 0x90, 0x78, 0x00 };
static const uint8 cmdLEDLoopOn[]      = { 0x90, 0x56, 0x7f };
static const uint8 cmdLEDLoopOff[]     = { 0x90, 0x56, 0x00 };
static const uint8 cmdWrite[]          = { 0xf0, 0x00, 0x01, 0x40, 0x10, 0x00 };

static const uint8 rspInitAck[]           = { 0xf0, 0x7e, 0x00, 0x06, 0x02, 0x00, 0x01, 0x40, 0x00, 0x00, 0x01, 0x00 };

static const uint8 rspButtonRewDown[]     = { 0x90, 0x5b, 0x7f };
static const uint8 rspButtonRewUp[]       = { 0x90, 0x5b, 0x00 };
static const uint8 rspButtonFfwdDown[]    = { 0x90, 0x5c, 0x7f };
static const uint8 rspButtonFfwdUp[]      = { 0x90, 0x5c, 0x00 };
static const uint8 rspButtonStopDown[]    = { 0x90, 0x5d, 0x7f };
//static const uint8 rspButtonStopUp[]      = { 0x90, 0x5d, 0x00 };
static const uint8 rspButtonPlayDown[]    = { 0x90, 0x5e, 0x7f };
//static const uint8 rspButtonPlayUp[]      = { 0x90, 0x5e, 0x00 };
static const uint8 rspButtonRecordDown[]  = { 0x90, 0x5f, 0x7f };
//static const uint8 rspButtonRecordUp[]    = { 0x90, 0x5f, 0x00 };

static const uint8 rspButtonPTrackDown[]  = { 0x90, 0x30, 0x7f };
//static const uint8 rspButtonPTrackUp[]    = { 0x90, 0x30, 0x00 };
static const uint8 rspButtonNTrackDown[]  = { 0x90, 0x31, 0x7f };
//static const uint8 rspButtonNTrackUp[]    = { 0x90, 0x31, 0x00 };
static const uint8 rspButtonRecDown[]     = { 0x90, 0x00, 0x7f };
//static const uint8 rspButtonRecUp[]       = { 0x90, 0x00, 0x00 };
static const uint8 rspButtonMuteDown[]    = { 0x90, 0x10, 0x7f };
//static const uint8 rspButtonMuteUp[]      = { 0x90, 0x10, 0x00 };
static const uint8 rspButtonSoloDown[]    = { 0x90, 0x08, 0x7f };
//static const uint8 rspButtonSoloUp[]      = { 0x90, 0x08, 0x00 };

static const uint8 rspButtonInDown[]      = { 0x90, 0x57, 0x7f };
//static const uint8 rspButtonInUp[]        = { 0x90, 0x57, 0x00 };
static const uint8 rspButtonOutDown[]     = { 0x90, 0x58, 0x7f };
//static const uint8 rspButtonOutUp[]       = { 0x90, 0x58, 0x00 };
static const uint8 rspButtonPunchDown[]   = { 0x90, 0x78, 0x7f };
//static const uint8 rspButtonPunchUp[]     = { 0x90, 0x78, 0x00 };
static const uint8 rspButtonLoopDown[]    = { 0x90, 0x56, 0x7f };
//static const uint8 rspButtonLoopUp[]      = { 0x90, 0x56, 0x00 };

static const uint8 rspButtonPrevDown[]    = { 0x90, 0x54, 0x7f };
//static const uint8 rspButtonPrevUp[]      = { 0x90, 0x54, 0x00 };
static const uint8 rspButtonAddDown[]     = { 0x90, 0x52, 0x7f };
//static const uint8 rspButtonAddUp[]       = { 0x90, 0x52, 0x00 };
static const uint8 rspButtonNextDown[]    = { 0x90, 0x55, 0x7f };
//static const uint8 rspButtonNextUp[]      = { 0x90, 0x55, 0x00 };

static const uint8 rspButtonUndoDown[]    = { 0x90, 0x4c, 0x7f };
//static const uint8 rspButtonUndoUp[]      = { 0x90, 0x4c, 0x00 };
static const uint8 rspButtonShiftDown[]   = { 0x90, 0x79, 0x7f };
static const uint8 rspButtonShiftUp[]     = { 0x90, 0x79, 0x00 };
static const uint8 rspButtonFootDown[]    = { 0x90, 0x67, 0x7f };
//static const uint8 rspButtonFootUp[]      = { 0x90, 0x67, 0x00 };

template <int size>
static inline bool matchesMessage (const MidiMessage& m, const juce::uint8 (&data)[size])
{
    return size == m.getRawDataSize() && memcmp (m.getRawData(), data, (size_t) size) == 0;
}


TranzportControlSurface::TranzportControlSurface (ExternalControllerManager& ecm)  : ControlSurface (ecm)
{
    needsMidiChannel                = true;
    needsMidiBackChannel            = true;
    numberOfFaderChannels           = 1;
    numCharactersForTrackNames      = 11;
    numParameterControls            = 0;
    numCharactersForParameterLabels = 0;
    deletable                       = false;
    deviceDescription               = "Frontier Design Group Tranzport";
    wantsClock                      = false;
    numMarkers                      = 0;
    numCharactersForMarkerLabels    = 0;
    followsTrackSelection           = true;
}

TranzportControlSurface::~TranzportControlSurface()
{
    notifyListenersOfDeletion();
}

void TranzportControlSurface::initialiseDevice (bool)
{
    CRASH_TRACER
    sendMidiArray (cmdInitNativeMode);
    sendMidiArray (cmdInquiry);

    displayPrint (0, "                                        ");

    sendMidiArray (cmdLEDRecordOff);
    sendMidiArray (cmdLEDArmRecOff);
    sendMidiArray (cmdLEDMuteOff);
    sendMidiArray (cmdLEDSoloOff);
    sendMidiArray (cmdLEDAnySoloOff);
    sendMidiArray (cmdLEDPunchOff);
    sendMidiArray (cmdLEDLoopOff);
}

void TranzportControlSurface::shutDownDevice()
{
    displayPrint (0, "                                        ");

    sendMidiArray (cmdLEDRecordOff);
    sendMidiArray (cmdLEDArmRecOff);
    sendMidiArray (cmdLEDMuteOff);
    sendMidiArray (cmdLEDSoloOff);
    sendMidiArray (cmdLEDAnySoloOff);
    sendMidiArray (cmdLEDPunchOff);
    sendMidiArray (cmdLEDLoopOff);
}

void TranzportControlSurface::updateMiscFeatures()
{
}

void TranzportControlSurface::acceptMidiMessage (const MidiMessage& m)
{
    CRASH_TRACER

    auto data = m.getRawData();
    auto datasize = (size_t) m.getRawDataSize();

    // init ack
    if (datasize == sizeof (rspInitAck) + 5
         && memcmp (rspInitAck, data, sizeof(rspInitAck)) == 0
         && ! init)
    {
        init = true;
    }
    // rewind
    else if (matchesMessage (m, rspButtonRewDown))
    {
        if (shift)
            userPressedHome();
        else
            userChangedRewindButton (true);
    }
    // fast forward
    else if (matchesMessage (m, rspButtonFfwdDown))
    {
        if (shift)
            userPressedEnd();
        else
            userChangedFastForwardButton (true);
    }
    // rewind up
    else if (matchesMessage (m, rspButtonRewUp))
    {
        userChangedRewindButton (false);
    }
    // fast forward up
    else if (matchesMessage (m, rspButtonFfwdUp))
    {
        userChangedFastForwardButton (false);
    }
    // stop
    else if (matchesMessage (m, rspButtonStopDown))
    {
        if (shift)
            userToggledEtoE();
        else
            userPressedStop();
    }
    // play
    else if (matchesMessage (m, rspButtonPlayDown))
    {
        if (shift)
            userPressedAutomationReading();
        else
            userPressedPlay();
    }
    // record
    else if (matchesMessage (m, rspButtonRecordDown))
    {
        if (shift)
            userPressedAutomationWriting();
        else
            userPressedRecord();
    }
    // previous track
    else if (matchesMessage (m, rspButtonPTrackDown))
    {
        if (shift)
            userPressedSave();
        else
            userChangedFaderBanks(-1);
    }
    // next track
    else if (matchesMessage (m, rspButtonNTrackDown))
    {
        if (shift)
            userToggledSnapOnOff();
        else
            userChangedFaderBanks(1);
    }
    // arm track
    else if (matchesMessage (m, rspButtonRecDown))
    {
        if (shift)
            userPressedArmAll();
        else
            userPressedRecEnable(0, false);
    }
    // mute track
    else if (matchesMessage (m, rspButtonMuteDown))
    {
        if (shift)
            userPressedClearAllMute();
        else
            userPressedMute(0, false);
    }
    // solo track
    else if (matchesMessage (m, rspButtonSoloDown))
    {
        if (shift)
            userPressedClearAllSolo();
        else
            userPressedSolo(0);
    }
    // in
    else if (matchesMessage (m, rspButtonInDown))
    {
        if (shift)
            userPressedMarkIn();
        else
            userPressedJumpToMarkIn();
    }
    // out
    else if (matchesMessage (m, rspButtonOutDown))
    {
        if (shift)
            userPressedMarkOut();
        else
            userPressedJumpToMarkOut();
    }
    // punch
    else if (matchesMessage (m, rspButtonPunchDown))
    {
        if (shift)
            userToggledClickOnOff();
        else
            userToggledPunchOnOff();
    }
    // loop
    else if (matchesMessage (m, rspButtonLoopDown))
    {
        if (shift)
        {
            panMode = !panMode;
            updateDisplay();
        }
        else
        {
            userToggledLoopOnOff();
        }
    }
    // previous marker
    else if (matchesMessage (m, rspButtonPrevDown))
    {
        if (shift)
            userPressedZoomIn();
        else
            userPressedPreviousMarker();
    }
    // add marker
    else if (matchesMessage (m, rspButtonAddDown))
    {
        if (shift)
            userPressedZoomToFit();
        else
            userPressedCreateMarker();
    }
    // next marker
    else if (matchesMessage (m, rspButtonNextDown))
    {
        if (shift)
            userPressedZoomOut();
        else
            userPressedNextMarker();
    }
    // undo
    else if (matchesMessage (m, rspButtonUndoDown))
    {
        if (shift)
            userPressedRedo();
        else
            userPressedUndo();
    }
    // shift
    else if (matchesMessage (m, rspButtonShiftDown))
    {
        shift = true;
    }
    // shift up
    else if (matchesMessage (m, rspButtonShiftUp))
    {
        shift = false;
    }
    // foot switch
    else if (matchesMessage (m, rspButtonFootDown))
    {
        userPressedRecord();
    }
    // jog wheel
    else if (datasize == 3 && data[0] == 0xb0 && data[1] == 0x3c)
    {
        int delta = data[2] < 0x41 ? data[2] : -(data[2] - 0x40);

        if (shift)
        {
            if (panMode)
                userMovedPanPot (0, pan + delta / 75.0f);
            else
                userMovedFader (0, faderPos + delta / 75.0f);
        }
        else
        {
            userMovedJogWheel (delta / (snap ? 20.0f : 1.0f));
        }
    }
}

void TranzportControlSurface::moveFader (int channelNum, float newSliderPos)
{
    if (channelNum == 0)
        faderPos = newSliderPos;

    updateDisplay();
}

void TranzportControlSurface::moveMasterLevelFader (float, float)
{
}

void TranzportControlSurface::movePanPot (int channelNum, float newPan)
{
    if (channelNum == 0)
        pan = newPan;

    if (init)
        updateDisplay();
}

void TranzportControlSurface::moveAux (int, const char*, float)
{
}

void TranzportControlSurface::updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState state, bool isBright)
{
    if (init && channelNum == 0)
    {
        if ((state & Track::soloLit) != 0 || (isBright && (state & Track::soloFlashing) != 0))
            sendMidiArray (cmdLEDSoloOn);
        else
            sendMidiArray (cmdLEDSoloOff);

        if ((state & Track::muteLit) != 0 || (isBright && (state & Track::muteFlashing) != 0))
            sendMidiArray (cmdLEDMuteOn);
        else
            sendMidiArray (cmdLEDMuteOff);
    }
}

void TranzportControlSurface::soloCountChanged (bool anySoloTracks)
{
    if (init)
    {
        if (anySoloTracks)
            sendMidiArray (cmdLEDAnySoloOn);
        else
            sendMidiArray (cmdLEDAnySoloOff);
    }
}

void TranzportControlSurface::playStateChanged (bool)
{
}

void TranzportControlSurface::recordStateChanged (bool isRecording)
{
    if (init)
    {
        if (isRecording)
            sendMidiArray (cmdLEDRecordOn);
        else
            sendMidiArray (cmdLEDRecordOff);
    }
}

void TranzportControlSurface::automationReadModeChanged (bool)
{
}

void TranzportControlSurface::automationWriteModeChanged (bool)
{
}

void TranzportControlSurface::faderBankChanged (int newStartChannelNumber, const StringArray& trackNames)
{
    if (init && currentTrack != newStartChannelNumber)
    {
        currentTrack = newStartChannelNumber;
        trackName = trackNames[0];

        if ((newStartChannelNumber + 1) == trackName.getIntValue() && trackName.containsOnly ("0123456789"))
            trackName = TRANS("Track") + " " + String (newStartChannelNumber + 1);

        trackName = trackName.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890 ()! \"#$%&\'*+,-./:;<=>?@^_`{|}");

        updateDisplay();
    }
}

void TranzportControlSurface::channelLevelChanged (int, float)
{
}

void TranzportControlSurface::trackSelectionChanged (int, bool)
{
}

void TranzportControlSurface::trackRecordEnabled (int, bool isEnabled)
{
    if (init)
    {
        if (isEnabled)
            sendMidiArray (cmdLEDArmRecOn);
        else
            sendMidiArray (cmdLEDArmRecOff);
    }
}

void TranzportControlSurface::masterLevelsChanged (float, float)
{
}

void TranzportControlSurface::timecodeChanged (int barsOrHours, int beatsOrMinutes, int ticksOrSeconds, int millisecs, bool isBarsBeats, bool isFrames)
{
    if (init)
    {
        char d[16];

        if (isBarsBeats)
            sprintf (d, sizeof (d), "%d|%d|%03d", barsOrHours, beatsOrMinutes, ticksOrSeconds);
        else if (isFrames)
            sprintf (d, sizeof (d), "%02d:%02d:%02d.%02d", barsOrHours, beatsOrMinutes, ticksOrSeconds, millisecs);
        else
            sprintf (d, sizeof (d), "%02d:%02d:%02d.%03d", barsOrHours, beatsOrMinutes, ticksOrSeconds, millisecs);

        updateTCDisplay (d);
    }
}

void TranzportControlSurface::clickOnOffChanged (bool)
{
}

void TranzportControlSurface::snapOnOffChanged (bool isSnapOn)
{
    snap = isSnapOn;
}

void TranzportControlSurface::loopOnOffChanged (bool isLoopOn)
{
    if (init)
    {
        if (isLoopOn)
            sendMidiArray (cmdLEDLoopOn);
        else
            sendMidiArray (cmdLEDLoopOff);
    }
}

void TranzportControlSurface::slaveOnOffChanged (bool)
{
}

void TranzportControlSurface::punchOnOffChanged (bool isPunching)
{
    if (init)
    {
        if (isPunching)
            sendMidiArray (cmdLEDPunchOn);
        else
            sendMidiArray (cmdLEDPunchOff);
    }
}

void TranzportControlSurface::parameterChanged (int, const ParameterSetting&)
{
}

void TranzportControlSurface::clearParameter (int)
{
}

bool TranzportControlSurface::canChangeSelectedPlugin()
{
    return false;
}

void TranzportControlSurface::currentSelectionChanged()
{
}

void TranzportControlSurface::updateDisplay()
{
    // display top row of text
    char text[128];
    memset (text, ' ', sizeof (text));

    sprintf (text, sizeof (text), "%-10s", trackName.toRawUTF8());

    text[10] = panMode ? ' ' : 0x7E;

    const float dB = volumeFaderPositionToDB (faderPos);

    if (dB < -99.0f)
    {
        memcpy (text + 11, "-INF", 4);
    }
    else
    {
        sprintf (text + 11, sizeof (text) - 11, "%+4.1f", dB);

        if (text[14] == '.')
            text[14] = ' ';

        text[15] = ' ';
        text[16] = ' ';
    }

    if (std::abs (int (pan * 100)) == 0)
    {
        text[19] = 'C';
    }
    else
    {
        sprintf (text + 16, sizeof (text) - 16, "%3d", abs (int (pan * 100)));
        text[19] = pan < 0 ? 'L' : 'R';
    }

    for (int i = 0; i < 20; ++i)
        if (text[i] == 0)
            text[i] = ' ';

    text[20] = 0;

    if (panMode)
        *strrchr (text, ' ') = 0x7E;

    displayPrint (0, text);

    memset (text, ' ', sizeof (text));

    // display bottom row of text
    if (auto t = externalControllerManager.getChannelTrack (currentTrack))
    {
        if (auto at = dynamic_cast<AudioTrack*> (t))
            sprintf (text, sizeof (text), "%s %d   ", "trk", at->getAudioTrackNumber());
        else if (auto ft = dynamic_cast<FolderTrack*> (t))
            sprintf (text, sizeof (text), "%s %d   ", "fld", ft->getFolderTrackNumber());

        text[6] = 0;
    }

    for (int i = 0; i < 20; ++i)
        if (text[i] == 0)
            text[i] = ' ';

    text[20] = 0;

    displayPrint (20, text);
}

void TranzportControlSurface::updateTCDisplay (const char* text)
{
    int len = (int) strlen (text);

    if (len > 14)
        text += len - 14;

    char buffer[16];
    memset (buffer, ' ', 15);
    sprintf (buffer, sizeof (buffer), "%14s", text);

    displayPrint (20 + 6, buffer);
}

void TranzportControlSurface::displayPrint (int pos, const char* text)
{
    const size_t len = strlen (text);
    HeapBlock<uint8> buffer (len + 8);
    memcpy (buffer, cmdWrite, sizeof (cmdWrite));
    buffer[sizeof(cmdWrite)] = (uint8) pos;
    memcpy (buffer + sizeof (cmdWrite) + 1, text, len);
    buffer [len + 8 - 1] = 0xf7;
    sendMidiCommandToController (buffer, (int) len + 8);
}

void TranzportControlSurface::markerChanged (int, const MarkerSetting&)
{
}

void TranzportControlSurface::clearMarker (int)
{
}
