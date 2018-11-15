/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace AlphaTrack
{
    static uint8 cmdInitNativeMode[] = { 0xf0, 0x00, 0x01, 0x40, 0x20, 0x01, 0x00, 0xf7 };
    static uint8 cmdInquiry[]        = { 0xf0, 0x7e, 0x00, 0x06, 0x01, 0xf7 };
    static uint8 rspInitAck[]        = { 0xf0, 0x7e, 0x00, 0x06, 0x02, 0x00, 0x01, 0x40, 0x20, 0x00, 0x01, 0x00 };
    static uint8 cmdWrite[]          = { 0xf0, 0x00, 0x01, 0x40, 0x20, 0x00 };

    enum { TOUCH = 1, PARAM = 2 };
}

AlphaTrackControlSurface::AlphaTrackControlSurface (ExternalControllerManager& ecm)  : ControlSurface (ecm)
{
    needsMidiChannel                = true;
    needsMidiBackChannel            = true;
    numberOfFaderChannels           = 1;
    numCharactersForTrackNames      = 32;
    numParameterControls            = 0;
    numCharactersForParameterLabels = 0;
    deletable                       = false;
    shift                           = false;
    deviceDescription               = "Frontier Design Group AlphaTrack";
    wantsClock                      = false;
    numMarkers                      = 0;
    numCharactersForMarkerLabels    = 0;
    currentTrack                    = -1;
    init                            = false;
    fingers                         = 0;
    stripPos                        = -1;
    jogged                          = true;
    faderPos                        = 0;
    pan                             = 0;
    flip                            = 0;
    followsTrackSelection           = true;
    mode                            = Pan;
    isReading                       = false;
    isWriting                       = false;
    touch                           = 0;
    offset                          = 0;
    isPunching                      = false;
    isLoopOn                        = false;
    physicalFaderPos                = -1;

    zeromem (displayBuffer, sizeof (displayBuffer));
    memset (displayBuffer, '=', sizeof (displayBuffer) - 1);
}

AlphaTrackControlSurface::~AlphaTrackControlSurface()
{
    notifyListenersOfDeletion();
}

void AlphaTrackControlSurface::initialiseDevice (bool)
{
    CRASH_TRACER
    sendMidiArray (AlphaTrack::cmdInitNativeMode);
    sendMidiArray (AlphaTrack::cmdInquiry);

    displayPrint(0, 0, "                ");
    displayPrint(1, 0, "                ");

    clearAllLeds();
}

void AlphaTrackControlSurface::shutDownDevice()
{
    displayPrint(0, 0, "                ");
    displayPrint(1, 0, "                ");

    clearAllLeds();
}

void AlphaTrackControlSurface::updateMiscFeatures()
{
    if (shift)
    {
        setLed (0x56, isPunching);
        setLed (0x32, true);
    }
    else
    {
        setLed (0x56, isLoopOn);
        setLed (0x32, flip > 0);
    }
}

bool AlphaTrackControlSurface::isOnEditScreen() const
{
    return getEditIfOnEditScreen() != nullptr;
}

void AlphaTrackControlSurface::acceptMidiMessage (const MidiMessage& m)
{
    auto data = m.getRawData();
    auto datasize = m.getRawDataSize();

    // init ack
    if (datasize == sizeof (AlphaTrack::rspInitAck) + 5
         && memcmp (AlphaTrack::rspInitAck, data, sizeof (AlphaTrack::rspInitAck)) == 0
         && ! init)
    {
        init = true;
        setMode(Pan);
    }
    else if (datasize == 3 && data[0] == 0x90)
    {
        bool press = data[2] == 0x7f;

        if (press)
        {
            switch (data[1])
            {
                case 0x5b: // rewind
                    if (shift)
                        userPressedHome();
                    else
                        userChangedRewindButton(true);
                    break;
                case 0x5c: // ffwd
                    if (shift)
                        userPressedEnd();
                    else
                        userChangedFastForwardButton(true);
                    break;
                case 0x5d: // stop
                    userPressedStop();
                    break;
                case 0x5e: // play
                    userPressedPlay();
                    break;
                case 0x5f: // record
                    userPressedRecord();
                    break;
                case 0x46:
                    shift = ! shift;
                    setLed (0x46, shift);
                    updateMiscFeatures();
                    break;
                case 0x57: // track left
                    if (shift)
                        userPressedMarkIn();
                    else
                        userChangedFaderBanks(-1);
                    break;
                case 0x58: // track right
                    if (shift)
                        userPressedMarkOut();
                    else
                        userChangedFaderBanks(+1);
                    break;
                case 0x56: // loop
                    if (shift)
                        userToggledPunchOnOff();
                    else
                        userToggledLoopOnOff();
                    break;
                case 0x32: // flip
                    if (shift)
                    {
                        AppFunctions::showHideAllPanes();
                    }
                    else
                    {
                        flip = flip ? 0 : 1;

                        updateFlip();
                        updateDisplay();
                        updateMiscFeatures();
                    }
                    break;
                case 0x10: // mute
                    if (shift)
                        userPressedClearAllMute();
                    else
                        userPressedMute(0, false);
                    break;
                case 0x36: // f1
                    if (shift)
                    {
                        if (! doKeyPress (alphaTrackF5, ! isOnEditScreen()))
                            doKeyPress (KeyPress::F5Key);
                    }
                    else
                    {
                        if (! doKeyPress (alphaTrackF1, ! isOnEditScreen()))
                            AppFunctions::split();
                    }
                    break;
                case 0x37: // f2
                    if (shift)
                    {
                        if (! doKeyPress(alphaTrackF6, ! isOnEditScreen()))
                            doKeyPress(KeyPress::F6Key);
                    }
                    else
                    {
                        if (! doKeyPress(alphaTrackF2, ! isOnEditScreen()))
                            userPressedCut();
                    }
                    break;
                case 0x38: // f3
                    if (shift)
                    {
                        if (! doKeyPress(alphaTrackF7, ! isOnEditScreen()))
                            doKeyPress(KeyPress::F7Key);
                    }
                    else
                    {
                        if (! doKeyPress(alphaTrackF3, ! isOnEditScreen()))
                            userPressedCopy();
                    }
                    break;
                case 0x39: // f4
                    if (shift)
                    {
                        if (! doKeyPress(alphaTrackF8, ! isOnEditScreen()))
                            doKeyPress(KeyPress::F8Key);
                    }
                    else
                    {
                        if (! doKeyPress(alphaTrackF4, ! isOnEditScreen()))
                            userPressedPaste (false);
                    }
                    break;
                case 0x08: // solo
                    if (shift)
                        userPressedClearAllSolo();
                    else
                        userPressedSolo(0);
                    break;
                case 0x2a: //pan
                    setMode(Pan);
                    break;
                case 0x29: // send
                    setMode(Send);
                    break;
                case 0x2c: // eq
                    setMode(Eq);
                    break;
                case 0x2b: // plugin
                    if (shift)
                    {
                        if (mode == Plugin2)
                            setMode(Plugin1);
                    }
                    else
                    {
                        if (mode == Plugin2)
                            setMode(Plugin2);
                        else
                            setMode(Plugin1);
                    }
                    break;
                case 0x4a: // auto
                    setMode(Auto);
                    break;
                case 0x00: // rec arm
                    if (shift)
                        userPressedArmAll();
                    else
                        userPressedRecEnable(0, false);
                    break;
                case 0x67: // footswitch
                {
                    KeyPress key (alphaTrackFoot);

                    if (auto focused = Component::getCurrentlyFocusedComponent())
                        focused->keyPressed (key);
                    break;
                }
                case 0x78: // left encoder touch
                    touchParam(0);
                    break;

                case 0x20: // left encoder push
                    if (mode == Plugin1)
                    {
                        getPlugin(0);
                    }
                    else if (mode == Auto)
                    {
                        userPressedAutomationReading();
                    }
                    else if (flip)
                    {
                        flip = 1;
                        updateFlip();
                        updateDisplay();
                    }
                    else if (mode == Plugin2)
                    {
                        touchParam(0);
                        if (auto p = getParam(0))
                            p->midiControllerPressed();
                    }
                    break;
                case 0x79: // middle encoder touch
                    touchParam(1);
                    break;
                case 0x21: // middle encoder push
                    if (mode == Plugin1)
                    {
                        getPlugin(1);
                    }
                    else if (flip)
                    {
                        flip = 2;
                        updateFlip();
                        updateDisplay();
                    }
                    else if (mode == Plugin2)
                    {
                        touchParam(1);
                        if (auto p = getParam(1))
                            p->midiControllerPressed();
                    }
                    break;
                case 0x7a: // right encoder touch
                    touchParam(2);
                    break;
                case 0x22: // right encoder push
                    if (mode == Plugin1)
                    {
                        getPlugin(2);
                    }
                    else if (mode == Pan)
                    {
                        userPressedCreateMarker();
                    }
                    else if (mode == Auto)
                    {
                        userPressedAutomationWriting();
                    }
                    else if (flip)
                    {
                        flip = 3;
                        updateFlip();
                        updateDisplay();
                    }
                    else if (mode == Plugin2)
                    {
                        touchParam(2);
                        if (auto p = getParam(2))
                            p->midiControllerPressed();
                    }
                    break;
                case 0x68: // fader touch
                    if (flip == 0)
                    {
                        if (getEdit() != nullptr)
                        {
                            auto t = externalControllerManager.getChannelTrack (currentTrack);

                            if (auto at = dynamic_cast<AudioTrack*> (t))
                                if (auto vp = at->getVolumePlugin())
                                    setParam (vp->getAutomatableParameter(0));

                            if (auto ft = dynamic_cast<FolderTrack*> (t))
                                if (auto vca = ft->getVCAPlugin())
                                    setParam (vca->getAutomatableParameter(0));
                        }
                    }
                    else
                    {
                        touchParam(flip - 1);
                    }
                    break;
                case 0x74: // one finger touch
                case 0x7b: // two finder touch
                    stripPos = -1;
                    jogged = false;
                    break;
            }
        }
        else
        {
            switch (data[1])
            {
                case 0x5b: // rewind
                    userChangedRewindButton(false);
                    break;
                case 0x5c: // ffwd
                    userChangedFastForwardButton(false);
                    break;
                case 0x46:
                    break;
                case 0x67: // footswitch
                    break;
                case 0x78: // left encoder touch
                    break;
                case 0x20: // left encoder push
                    break;
                case 0x79: // middle encoder touch
                    break;
                case 0x21: // middle encoder push
                    break;
                case 0x7a: // right encoder touch
                    break;
                case 0x22: // right encoder push
                    break;
                case 0x68: // fader touch
                    break;
                case 0x74: // one finger touch
                case 0x7b: // two finder touch
                    if (! jogged)
                    {
                        if (stripPos <= 8)
                            userPressedPreviousMarker();
                        if (stripPos >= 24)
                            userPressedNextMarker();
                    }
                    stripPos = -1;
                    jogged = false;
                    break;
            }
        }
    }
    else if (datasize == 3 && data[0] == 0xe0)
    {
        int fpos = ((data[1] & 0x70) >> 4) | ((data[2] & 0x7f) << 3);

        if (flip == 0)
        {
            if (getEdit() != nullptr)
            {
                auto track = externalControllerManager.getChannelTrack (currentTrack);

                if (auto t = dynamic_cast<AudioTrack*> (track))
                    if (auto vp = t->getVolumePlugin())
                        setParam (vp->getAutomatableParameter(0));

                if (auto ft = dynamic_cast<FolderTrack*> (track))
                    if (auto vca = ft->getVCAPlugin())
                        setParam (vca->getAutomatableParameter(0));

                userMovedFader (0, float(fpos) / 0x3FF);
            }
        }
        else
        {
            touchParam (flip - 1);

            if (param)
            {
                pos = jlimit (0.0f, 1.0f, float(fpos) / 0x3FF);
                param->midiControllerMoved (pos);
                updateDisplay();
                updateFlip();
            }
        }
    }
    else if (datasize == 3 && data[0] == 0xe9)
    {
        int currentStripPos = (data[2] & 0x7c) >> 2;

        if (stripPos != -1)
        {
            int move = currentStripPos - stripPos;
            userMovedJogWheel(move / 1.5f);
            jogged = true;
        }

        stripPos = currentStripPos;
    }
    else if (datasize == 3 && data[0] == 0xb0)
    {
        int delta = data[2] < 0x41 ? data[2]
                                   : -(data[2] - 0x40);

        switch (data[1])
        {
            case 0x10:  turnParam (0, delta);  break;
            case 0x11:  turnParam (1, delta);  break;
            case 0x12:  turnParam (2, delta);  break;
        }
    }
}

void AlphaTrackControlSurface::moveFader(int channelNum, float newSliderPos)
{
    if (channelNum == 0)
        faderPos = newSliderPos;

    if (flip == 0)
        moveFaderInt (newSliderPos);

    updateDisplay();
}

void AlphaTrackControlSurface::moveFaderInt (float newSliderPos)
{
    int newPos = roundToInt (newSliderPos * 0x3FF);

    if (newPos != physicalFaderPos)
    {
        uint8 data[3];

        data[0] = 0xe0;
        data[1] = (uint8) (((unsigned int) newPos & 3) << 4);
        data[2] = (uint8) (((unsigned int) newPos & 0x3F8) >> 3);

        sendMidiArray (data);
        physicalFaderPos = newPos;
    }
}

void AlphaTrackControlSurface::moveMasterLevelFader (float, float)
{
}

void AlphaTrackControlSurface::movePanPot (int channelNum, float newPan)
{
    if (channelNum == 0)
        pan = newPan;

    if (mode == Pan)
        updateDisplay();
}

void AlphaTrackControlSurface::moveAux (int, const char*, float)
{
}

void AlphaTrackControlSurface::clearAux (int)
{
}

void AlphaTrackControlSurface::updateSoloAndMute (int, Track::MuteAndSoloLightState state, bool isBright)
{
    setLed (0x08, (state & Track::soloLit) != 0 || (isBright && (state & Track::soloFlashing) != 0));
    setLed (0x10, (state & Track::muteLit) != 0 || (isBright && (state & Track::muteFlashing) != 0));
}

void AlphaTrackControlSurface::soloCountChanged (bool anySoloTracks)
{
    setLed (0x73, anySoloTracks);
}

void AlphaTrackControlSurface::playStateChanged (bool)
{
}

void AlphaTrackControlSurface::recordStateChanged (bool isRecording)
{
    setLed (0x5f, isRecording);
}

void AlphaTrackControlSurface::automationReadModeChanged (bool isReading_)
{
    isReading = isReading_;
    setLed (0x4e, isReading);

    if (mode == Auto)
        updateDisplay();
}

void AlphaTrackControlSurface::automationWriteModeChanged (bool isWriting_)
{
    isWriting = isWriting_;
    setLed (0x4b, isWriting);

    if (mode == Auto)
        updateDisplay();
}

void AlphaTrackControlSurface::faderBankChanged (int newStartChannelNumber, const StringArray& trackNames)
{
    currentTrack = newStartChannelNumber;
    trackName = trackNames[0];

    if ((newStartChannelNumber + 1) == trackName.getIntValue() && trackName.containsOnly("0123456789"))
        trackName = TRANS("Track") + " " + String (newStartChannelNumber + 1);

    trackName = trackName.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890 ()! \"#$%&\'*+,-./:;<=>?@^_`{|}");

    updateDisplay();
}

void AlphaTrackControlSurface::channelLevelChanged (int, float)
{
}

void AlphaTrackControlSurface::trackSelectionChanged (int, bool)
{
}

void AlphaTrackControlSurface::trackRecordEnabled (int, bool isEnabled)
{
    setLed (0x00, isEnabled);
}

void AlphaTrackControlSurface::masterLevelsChanged (float, float)
{
}

void AlphaTrackControlSurface::timecodeChanged (int, int, int, int, bool, bool)
{
}

void AlphaTrackControlSurface::clickOnOffChanged (bool)
{
}

void AlphaTrackControlSurface::snapOnOffChanged (bool)
{
}

void AlphaTrackControlSurface::loopOnOffChanged (bool isLoopOn_)
{
    isLoopOn = isLoopOn_;

    if (! shift)
        setLed (0x56, isLoopOn);
}

void AlphaTrackControlSurface::slaveOnOffChanged (bool)
{
}

void AlphaTrackControlSurface::punchOnOffChanged (bool isPunching_)
{
    isPunching = isPunching_;

    if (shift)
        setLed (0x56, isPunching);
}

void AlphaTrackControlSurface::parameterChanged (int, const ParameterSetting&)
{
}

void AlphaTrackControlSurface::clearParameter (int)
{
}

void AlphaTrackControlSurface::markerChanged (int, const MarkerSetting&)
{
}

void AlphaTrackControlSurface::clearMarker (int)
{
}

void AlphaTrackControlSurface::currentSelectionChanged()
{
}

void AlphaTrackControlSurface::displayPrint (int line, int x, const char* const text)
{
    const size_t len = strlen (text);
    HeapBlock<uint8> buffer (len + 8);
    memcpy (buffer, AlphaTrack::cmdWrite, sizeof (AlphaTrack::cmdWrite));
    buffer [sizeof (AlphaTrack::cmdWrite)] = (uint8) (x + line * 16);
    memcpy (buffer + sizeof (AlphaTrack::cmdWrite) + 1, text, len);
    buffer [len + 8 - 1] = 0xf7;
    sendMidiCommandToController (buffer, (int) len + 8);
}

void AlphaTrackControlSurface::setLed (int led, bool state)
{
    uint8 data[3];
    data[0] = 0x90;
    data[1] = (uint8) led;
    data[2] = state ? 0x7f : 0x00;

    sendMidiArray (data);
}

static String dbToString (double db, int chars)
{
    if (db >= -96.0f)
        return String::formatted ("%+.1f", db).substring (0, chars);

    return String ("-INF").substring (0, chars);
}

void AlphaTrackControlSurface::handleAsyncUpdate()
{
    CRASH_TRACER

    char text[33] = { 0 };
    bool flipMark = false;

    if (flip)
        updateFlip();

    if (auto edit = getEdit())
    {
        if (auto track = externalControllerManager.getChannelTrack (currentTrack))
        {
            if (mode == Plugin2)
            {
                if (plugin == nullptr || plugin->getOwnerTrack() == nullptr || plugin->getOwnerTrack() != track)
                    setMode (Plugin1);
            }

            if (param)
            {
                auto name = param->getParameterShortName(16);
                auto val  = param->getLabelForValue (param->getCurrentValue());

                if (val.isEmpty())
                    val = param->getCurrentValueAsString();

                if (auto as = dynamic_cast<AuxSendPlugin*> (param->getPlugin()))
                {
                    int bus = as->getBusNumber();

                    name = edit->getAuxBusName (bus);

                    if (name.isEmpty())
                        name = "Send #" + String (bus + 1);
                }

                if (dynamic_cast<VolumeAndPanPlugin*> (param->getPlugin()) != nullptr
                      || dynamic_cast<VCAPlugin*> (param->getPlugin()) != nullptr)
                    if (auto t = param->getTrack())
                        name = t->getName();

                sprintf (text,      16, "%s", name.toRawUTF8());
                sprintf (text + 16, 16, "%s", val.toRawUTF8());
            }
            else if (mode == Pan)
            {
                sprintf (text, 16, "%-16s", trackName.toRawUTF8());

                if (track->isAudioTrack())
                {
                    const int panPercent = std::abs ((int) (pan * 100));

                    if (panPercent == 0)
                        text[16] = 'C';
                    else if (pan < 0)
                        sprintf (text + 16, 16, "%dL    ", panPercent);
                    else
                        sprintf (text + 16, 16, "%dR    ", panPercent);
                }

                sprintf (text + 28, 4, "mark");
                flipMark = true;
            }
            else if (mode == Send)
            {
                int sendCnt = 0;

                for (auto f : track->pluginList)
                {
                    if (auto send = dynamic_cast<AuxSendPlugin*> (f))
                    {
                        int bus = send->getBusNumber();
                        String name = edit->getAuxBusName(bus);
                        if (name.isEmpty())
                            name = "snd" + String (bus + 1);

                        name = ExternalController::shortenName (name, sendCnt == 2 ? 4 : 5);

                        String val = dbToString (send->getGainDb(), sendCnt == 2 ? 4 : 5);

                        if (sendCnt == 2)
                        {
                            sprintf (text + sendCnt * 6,      4, "%-4s", name.toRawUTF8());
                            sprintf (text + sendCnt * 6 + 16, 4, "%-4s", val.toRawUTF8());
                        }
                        else
                        {
                            sprintf (text + sendCnt * 6,      5, "%-5s", name.toRawUTF8());
                            sprintf (text + sendCnt * 6 + 16, 5, "%-5s", val.toRawUTF8());
                        }

                        if (++sendCnt == 3)
                            break;
                    }
                }

                flipMark = true;

                if (sendCnt == 0)
                {
                    sprintf (text,      16, "%s", track->getName().toRawUTF8());
                    sprintf (text + 16, 16, "No send");
                }
            }
            else if (mode == Eq)
            {
                auto at = dynamic_cast<AudioTrack*> (track);

                if (auto eq = at ? at->getEqualiserPlugin() : nullptr)
                {
                    sprintf (text,      16, "frq %d gain  q", offset / 3 + 1);
                    sprintf (text + 16, 16, "%-5s %-5s %-4s",
                             eq->getAutomatableParameter (offset + 0)->getCurrentValueAsString().substring (0, 5).toRawUTF8(),
                             eq->getAutomatableParameter (offset + 1)->getCurrentValueAsString().substring (0, 5).toRawUTF8(),
                             eq->getAutomatableParameter (offset + 2)->getCurrentValueAsString().substring (0, 4).toRawUTF8());
                }
                else
                {
                    sprintf (text,      16, "%s", track->getName().toRawUTF8());
                    sprintf (text + 16, 16, "No EQ");
                }

                flipMark = true;
            }
            else if (mode == Plugin1)
            {
                auto& list = track->pluginList;

                for (int i = 0; i < 3; ++i)
                    if (auto af = list[i + offset])
                        sprintf (text + 6 * i, i == 2 ? 4 : 5, "%s",
                                 ExternalController::shortenName (af->getName(), i == 2 ? 4 : 5).toRawUTF8());
            }
            else if (mode == Plugin2)
            {
                if (plugin)
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        auto params (plugin->getFlattenedParameterTree());

                        if (auto p = params[i + offset])
                        {
                            String name = ExternalController::shortenName (p->getParameterShortName (i == 2 ? 4 : 5), i == 2 ? 4 : 5);
                            String val  = p->getLabelForValue (p->getCurrentValue()).substring(0, i == 2 ? 4 : 5);

                            if (val.isEmpty())
                                val = p->getCurrentValueAsString().substring(0, i == 2 ? 4 : 5);

                            if (i == 2)
                            {
                                sprintf (text + i * 6,      4, "%-4s", name.toRawUTF8());
                                sprintf (text + 16 + i * 6, 4, "%-4s", val.toRawUTF8());
                            }
                            else
                            {
                                sprintf (text + i * 6,      5, "%-5s", name.toRawUTF8());
                                sprintf (text + 16 + i * 6, 5, "%-5s", val.toRawUTF8());
                            }
                        }
                    }

                    flipMark = true;
                }
            }
            else if (mode == Auto)
            {
                sprintf (text, 16, "read auto  write");
                sprintf (text + 17, 3, isReading ? "on" : "off");
                sprintf (text + 28, 3, isWriting ? "on" : "off");
            }

            if (flipMark && flip != 0 && getParam (flip - 1) != nullptr)
            {
                if (flip == 1)        text[21] = 0x7F;
                else if (flip == 2)   text[27] = 0x7F;
                else if (flip == 3)   text[27] = 0x7E;
            }
        }
    }

    for (int i = 0; i < 32; ++i)
        if (text[i] == 0)
            text[i] = ' ';

    text[32] = 0;

    if (memcmp (displayBuffer, text, 32) != 0)
    {
        displayPrint (0, 0, text);
        memcpy (displayBuffer, text, 32);
    }
}

void AlphaTrackControlSurface::updateDisplay()
{
    triggerAsyncUpdate();
}

void AlphaTrackControlSurface::clearAllLeds()
{
    setLed (0x5f, false);
    setLed (0x46, false);
    setLed (0x57, false);
    setLed (0x58, false);
    setLed (0x56, false);
    setLed (0x32, false);
    setLed (0x10, false);
    setLed (0x36, false);
    setLed (0x37, false);
    setLed (0x38, false);
    setLed (0x39, false);
    setLed (0x08, false);
    setLed (0x2a, false);
    setLed (0x29, false);
    setLed (0x2c, false);
    setLed (0x2b, false);
    setLed (0x4a, false);
    setLed (0x4e, false);
    setLed (0x4b, false);
    setLed (0x00, false);
    setLed (0x73, false);
}

void AlphaTrackControlSurface::setMode (Mode newMode)
{
    if (plugin != nullptr)
        externalControllerManager.repaintPlugin (*plugin);

    if (newMode != Plugin2)
        plugin = nullptr;

    if (param)
    {
        param = nullptr;
        updateDisplay();
    }

    switch (mode)
    {
        case Pan:     setLed (0x2a, false); break;
        case Send:    setLed (0x29, false); break;
        case Eq:      setLed (0x2c, false); break;
        case Plugin1: setLed (0x2b, false); break;
        case Plugin2: setLed (0x2b, false); break;
        case Auto:    setLed (0x4a, false); break;
    }

    if (newMode == Eq && mode == Eq)
    {
        offset += 3;
        if (offset > 9)
            offset = 0;
    }
    else if (newMode == Plugin1 && mode == Plugin1)
    {
        offset += 3;

        int max = 0;

        if (getEdit() != nullptr)
            if (auto t = externalControllerManager.getChannelTrack (currentTrack))
                max = t->pluginList.size();

        if (offset >= max)
            offset = 0;
    }
    else if (newMode == Plugin2 && mode == Plugin2)
    {
        offset += 3;

        if (offset >= plugin->getNumAutomatableParameters())
            offset = 0;
    }
    else
    {
        offset = 0;
    }

    if (auto t = dynamic_cast<AudioTrack*> (externalControllerManager.getChannelTrack (currentTrack)))
    {
        if (auto eq = t->getEqualiserPlugin())
        {
            externalControllerManager.repaintPlugin (*eq);

            if (newMode == Eq)
                if (auto sm = externalControllerManager.getSelectionManager())
                    sm->selectOnly (eq);
        }
    }

    mode = newMode;

    switch (mode)
    {
        case Pan:     setLed (0x2a, true); break;
        case Send:    setLed (0x29, true); break;
        case Eq:      setLed (0x2c, true); break;
        case Plugin1: setLed (0x2b, true); break;
        case Plugin2: setLed (0x2b, true); break;
        case Auto:    setLed (0x4a, true); break;
    }

    updateDisplay();
}

void AlphaTrackControlSurface::pluginBypass (bool)
{
    triggerAsyncUpdate();
}

void AlphaTrackControlSurface::setParam (AutomatableParameter::Ptr param_)
{
    if (param != param_)
    {
        param = param_;

        if (param != nullptr)
            pos = param->getCurrentNormalisedValue();

        updateDisplay();
    }

    startTimer (AlphaTrack::PARAM, 2000);
}

AutomatableParameter::Ptr AlphaTrackControlSurface::getParam (int paramIdx) const
{
    if (getEdit() != nullptr)
    {
        if (auto t = externalControllerManager.getChannelTrack (currentTrack))
        {
            if (mode == Pan)
            {
                if (paramIdx == 0)
                {
                    if (auto at = dynamic_cast<AudioTrack*> (t))
                        if (auto vp = at->getVolumePlugin())
                            return vp->getAutomatableParameter(1);
                }
            }
            else if (mode == Send)
            {
                AuxSendPlugin* as = nullptr;
                int sendCnt = 0;

                for (auto f : t->pluginList)
                {
                    if (auto send = dynamic_cast<AuxSendPlugin*> (f))
                    {
                        if (sendCnt == paramIdx)
                            as = send;

                        sendCnt++;
                    }
                }

                if (as != nullptr)
                    return as->getAutomatableParameter(0);
            }
            else if (mode == Eq)
            {
                if (auto at = dynamic_cast<AudioTrack*> (t))
                    if (auto eq = at->getEqualiserPlugin())
                        return eq->getAutomatableParameter (offset + paramIdx);
            }
            else if (mode == Plugin2)
            {
                if (plugin != nullptr)
                {
                    auto params (plugin->getFlattenedParameterTree());

                    return params[offset + paramIdx];
                }
            }
        }
    }

    return {};
}

void AlphaTrackControlSurface::touchParam (int paramIdx)
{
    if (auto p = getParam (paramIdx))
        setParam (p);
}

void AlphaTrackControlSurface::turnParam (int paramIdx, int delta)
{
    touchParam (paramIdx);

    if (param)
    {
        pos = jlimit (0.0f, 1.0f, pos + delta / 75.0f);
        param->midiControllerMoved (pos);
        updateDisplay();
    }
}

void AlphaTrackControlSurface::updateFlip()
{
    AutomatableParameter::Ptr p;

    if (flip > 0)
        p = getParam (flip - 1);

    if (p != nullptr)
        moveFaderInt (p->getCurrentNormalisedValue());
    else
        moveFaderInt (faderPos);
}

void AlphaTrackControlSurface::getPlugin (int idx)
{
    if (auto t = externalControllerManager.getChannelTrack (currentTrack))
    {
        if (auto p = t->pluginList[idx + offset])
        {
            plugin = p;
            setMode (Plugin2);

            if (auto sm = externalControllerManager.getSelectionManager())
                sm->selectOnly (*p);
        }
    }
}

bool AlphaTrackControlSurface::isPluginSelected (Plugin* p)
{
    if (getEdit() != nullptr)
    {
        if (mode == Eq)
        {
            if (auto at = dynamic_cast<AudioTrack*> (externalControllerManager.getChannelTrack (currentTrack)))
                return p == at->getEqualiserPlugin();
        }
        else if (mode == Plugin2)
        {
            return p == plugin.get();
        }
    }

    return false;
}

bool AlphaTrackControlSurface::doKeyPress (int key, bool force)
{
    KeyPress keyPress (key);

    if (auto acm = engine.getUIBehaviour().getApplicationCommandManager())
    {
        const CommandID id = acm->getKeyMappings()->findCommandForKeyPress (keyPress);

        if (id != 0)
        {
            acm->invokeDirectly (id, true);
            return true;
        }
    }

    if (force)
    {
        if (auto focused = Component::getCurrentlyFocusedComponent())
            focused->keyPressed (keyPress);

        return true;
    }

    return false;
}

void AlphaTrackControlSurface::timerCallback (int timerId)
{
    if (timerId == AlphaTrack::PARAM)
    {
        param = nullptr;
        updateDisplay();
    }
}
