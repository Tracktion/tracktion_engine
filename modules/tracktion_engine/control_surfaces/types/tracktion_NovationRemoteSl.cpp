/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace NovationRemoteSL
{
    const uint8 PID = 0x03;

    static uint8 cmdOn[]         = { 0xF0, 0x00, 0x20, 0x29, 0x03, 0x03, 0x20, 0x02, 0x03, 0x00, 0x01, 0x01, 0xF7 };
    static uint8 cmdOff[]        = { 0xF0, 0x00, 0x20, 0x29, 0x03, 0x03, 0x20, 0x02, 0x03, 0x00, 0x01, 0x00, 0xF7 };

    static uint8 cmdClearLeft[]  = { 0xF0, 0x00, 0x20, 0x29, 0x03, 0x03, 0x11, 0x04, PID, 0x00, 0x02, 0x02, 0x04, 0xF7 };
    static uint8 cmdClearRight[] = { 0xF0, 0x00, 0x20, 0x29, 0x03, 0x03, 0x11, 0x04, PID, 0x00, 0x02, 0x02, 0x05, 0xF7 };

    static uint8 cmdWrite[]      = { 0xF0, 0x00, 0x20, 0x29, 0x03, 0x03, 0x11, 0x04, PID, 0x00, 0x02, 0x01 };

    static const char* panLetter (float p) noexcept
    {
        if (p == 0.0f) return "C";
        if (p < 0.0)   return "L";

        return "R";
    }
}

NovationRemoteSl::NovationRemoteSl (ExternalControllerManager& ecm)  : ControlSurface (ecm)
{
    deviceDescription               = "Novation ReMOTE SL";
    needsMidiChannel                = true;
    needsMidiBackChannel            = true;
    wantsClock                      = true;
    deletable                       = false;
    numberOfFaderChannels           = 8;
    numCharactersForTrackNames      = 8;
    numParameterControls            = 16;
    numCharactersForParameterLabels = 8;
    numMarkers                      = 0;
    numCharactersForMarkerLabels    = 0;
}

NovationRemoteSl::~NovationRemoteSl()
{
    notifyListenersOfDeletion();
}

void NovationRemoteSl::initialiseDevice (bool)
{
    CRASH_TRACER

    for (int i = 0; i < 8; ++i)
    {
        mute[i]         = false;
        solo[i]         = false;
        trackNames[i]   = {};
        pan[i]          = 0;
        level[i]        = 0;

        param[i].clear();
    }

    sendMidiArray (NovationRemoteSL::cmdOn);

    clearLeft();
    clearRight();
    clearAllLights();

    setRightMode (rmVol);
    setLeftMode (lmTracks);
}

void NovationRemoteSl::shutDownDevice()
{
    clearLeft();
    clearRight();

    clearAllLights();

    sendMidiArray (NovationRemoteSL::cmdOff);
}

void NovationRemoteSl::setRightMode(RightMode rm)
{
    MidiMessage m(MidiMessage::controllerEvent(1, 0x55, 0));

    switch (rightMode)
    {
        case rmVol:  m = MidiMessage::controllerEvent (1, 0x55, 0); break;
        case rmPan:  m = MidiMessage::controllerEvent (1, 0x55, 0); break;
        case rmMute: m = MidiMessage::controllerEvent (1, 0x56, 0); break;
        case rmSolo: m = MidiMessage::controllerEvent (1, 0x57, 0); break;
    }

    sendMidiCommandToController (m);

    rightMode = rm;

    switch (rightMode)
    {
        case rmVol:  m = MidiMessage::controllerEvent (1, 0x55, 1); break;
        case rmPan:  m = MidiMessage::controllerEvent (1, 0x55, 1); break;
        case rmMute: m = MidiMessage::controllerEvent (1, 0x56, 1); break;
        case rmSolo: m = MidiMessage::controllerEvent (1, 0x57, 1); break;
    }

    sendMidiCommandToController (m);
    refreshRight(false);
}

void NovationRemoteSl::setLeftMode(LeftMode lm)
{
    switch (leftMode)
    {
        case lmParam1: sendMidiCommandToController (MidiMessage::controllerEvent(1, 0x51, 0)); break;
        case lmParam2: sendMidiCommandToController (MidiMessage::controllerEvent(1, 0x53, 0)); break;
        default: break;
    }

    leftMode = lm;

    if (leftMode == lmTracks || leftMode == lmPlugins)
        setLock (false, isLocked || wasLocked);

    if ((leftMode == lmParam1 || leftMode == lmParam2) && wasLocked)
        setLock (true, false);

    if (leftMode == lmTracks)
        trackOffset = 0;

    if (leftMode == lmPlugins)
        pluginOffset = 0;

    switch (leftMode)
    {
        case lmParam1: sendMidiCommandToController (MidiMessage::controllerEvent(1, 0x51, 1)); break;
        case lmParam2: sendMidiCommandToController (MidiMessage::controllerEvent(1, 0x53, 1)); break;
        default: break;
    }

    refreshLeft (true);
}

void NovationRemoteSl::updateMiscFeatures()
{
}

void NovationRemoteSl::acceptMidiMessage (const MidiMessage& m)
{
    CRASH_TRACER

    if (m.isSysEx() && m.getRawDataSize() == 13)
    {
        auto d = m.getRawData();

        if (memcmp (d, "\xF0\x00\x20\x29", 4) == 0)
        {
            int host = d[8];
            int cmd  = d[10];
            int data = d[11];

            if (host == 3 && cmd == 1)
            {
                online = data == 1;

                if (online)
                {
                    for (int i = 0; i < 4; ++i)
                        screenContents[i] = String::repeatedString ("*", 8 * 9);

                    refreshLeft (true);
                    refreshRight (true);
                }
            }
        }
    }

    if (m.isController() && online)
    {
        int cn = m.getControllerNumber();

        // right - faders
        if (cn >= 0x10 && cn <= 0x17)
        {
            if (rightMode != rmVol && rightMode != rmPan)
                setRightMode(rmVol);

            if (rightMode == rmPan)
                userMovedPanPot(cn - 0x10, m.getControllerValue() / 127.0f * 2.0f - 1.0f);
            else
                userMovedFader(cn - 0x10, m.getControllerValue() / 127.0f);
        }
        // right - mute buttons
        if (cn >= 0x28 && cn <= 0x2f)
        {
            if (rightMode != rmMute)
                setRightMode(rmMute);

            if (m.getControllerValue() == 1)
                userPressedMute(cn - 0x28, false);
        }
        // right - solo buttons
        if (cn >= 0x30 && cn <= 0x37)
        {
            if (rightMode != rmSolo)
                setRightMode(rmSolo);

            if (m.getControllerValue() == 1)
                userPressedSolo(cn - 0x30);
        }
        // vol or pan mode
        if (cn == 0x55)
        {
            if (m.getControllerValue() == 1)
            {
                if (rightMode == rmVol)
                    setRightMode(rmPan);
                else
                    setRightMode(rmVol);
            }
        }
        // mute mode
        if (cn == 0x56)
        {
            if (m.getControllerValue() == 1 && rightMode != rmMute)
                setRightMode(rmMute);
        }
        // solo mode
        if (cn == 0x57)
        {
            if (m.getControllerValue() == 1 && rightMode != rmSolo)
                setRightMode(rmSolo);
        }
        // prev bank
        if (cn == 0x5A)
        {
            if (m.getControllerValue() == 1)
                userChangedFaderBanks(-8);
        }
        // next bank
        if (cn == 0x5B)
        {
            if (m.getControllerValue() == 1)
                userChangedFaderBanks(+8);
        }
        // rewind
        if (cn == 0x48)
        {
            userChangedRewindButton(m.getControllerValue() == 1);
        }
        // forward
        if (cn == 0x49)
        {
            userChangedFastForwardButton(m.getControllerValue() == 1);
        }
        // stop
        if (cn == 0x4A)
        {
            if (m.getControllerValue() == 1)
                userPressedStop();
        }
        // play
        if (cn == 0x4B)
        {
            if (m.getControllerValue() == 1)
                userPressedPlay();
        }
        // record
        if (cn == 0x4C)
        {
            if (m.getControllerValue() == 1)
                userPressedRecord();
        }
        // loop
        if (cn == 0x4D)
        {
            if (m.getControllerValue() == 1)
                userToggledLoopOnOff();
        }
        // left select
        if (cn >= 0x18 && cn <= 0x1F)
        {
            if (m.getControllerValue() == 1)
            {
                if (leftMode == lmTracks)
                {
                    activeTrack = cn - 0x18 + trackOffset;
                    setLeftMode (lmPlugins);
                }
                else if (leftMode == lmPlugins)
                {
                    if (auto t = externalControllerManager.getChannelTrack (activeTrack))
                    {
                        const bool w = wasLocked;

                        auto& pl = t->pluginList;

                        if (auto sm = externalControllerManager.getSelectionManager())
                            sm->selectOnly (pl [cn - 0x18 + pluginOffset]);

                        setLeftMode (lmParam1);

                        if (w)
                        {
                            isLocked  = false;
                            wasLocked = true;
                        }
                    }
                }
            }
        }
        // left encoders
        if (cn >= 0x00 && cn <= 0x07)
        {
            if (leftMode == lmParam1 || leftMode == lmParam2)
            {
                if (leftMode == lmParam2)
                    setLeftMode (lmParam1);

                if (std::strlen (param[cn - 0x00].label) > 0)
                {
                    param[cn - 0x00].value = jlimit (0.0f, 1.0f, param[cn - 0x00].value + (m.getControllerValue() - 64) / 150.0f);
                    userMovedParameterControl (cn - 0x00, param[cn - 0x00].value);
                }
            }
        }
        // left pots
        if (cn >= 0x08 && cn <= 0x0F)
        {
            if (leftMode == lmParam1 || leftMode == lmParam2)
            {
                if (leftMode == lmParam1)
                    setLeftMode (lmParam2);

                userMovedParameterControl (cn - 0x08 + 8, m.getControllerValue() / 127.0f);
            }
        }
        // lock
        if (cn == 0x50)
        {
            if ((leftMode == lmParam1 || leftMode == lmParam2) && m.getControllerValue() == 1)
                setLock (! (isLocked || wasLocked), false);
        }
        // param 1
        if (cn == 0x51)
        {
            if (leftMode == lmParam2 && m.getControllerValue() == 1)
                setLeftMode (lmParam1);
        }
        // param 2
        if (cn == 0x53)
        {
            if (leftMode == lmParam1 && m.getControllerValue() == 1)
                setLeftMode (lmParam2);
        }
        // back
        if (cn == 0x54)
        {
            if ((leftMode == lmParam1 || leftMode == lmParam2) && m.getControllerValue() == 1)
                setLeftMode (lmPlugins);
            else if (leftMode == lmPlugins && m.getControllerValue())
                setLeftMode (lmTracks);
        }
        // params up
        if (cn == 0x58)
        {
            if (m.getControllerValue() == 1)
            {
                if (leftMode == lmTracks)
                    trackOffset = jmax (0, trackOffset - 8);
                else if (leftMode == lmPlugins)
                    pluginOffset = jmax (0, pluginOffset - 8);
                else
                    userChangedParameterBank (-8);

                refreshLeft (true);
            }
        }
        // params down
        if (cn == 0x59)
        {
            if (m.getControllerValue() == 1)
            {
                if (leftMode == lmTracks)
                {
                    trackOffset = jmin (trackOffset + 8, externalControllerManager.getNumChannelTracks() - 8);
                }
                else if (leftMode == lmPlugins)
                {
                    if (auto t = externalControllerManager.getChannelTrack (activeTrack))
                    {
                        auto& pl = t->pluginList;
                        pluginOffset = jmin (pluginOffset + 8, pl.size() - 8);
                    }
                }
                else
                {
                    userChangedParameterBank (8);
                }

                refreshLeft (true);
            }
        }
        // tempo 1
        if (cn == 0x5E)
        {
            tempo &= 0xffffc07f;
            tempo |= (m.getControllerValue() << 7);
        }
        // tempo 2
        if (cn == 0x5F)
        {
            tempo &= 0xffffff80;
            tempo |= m.getControllerValue();

            if (auto ed = getEdit())
                ed->tempoSequence.getTempoAt (ed->getTransport().position).setBpm ((double) tempo);
        }
    }
}

void NovationRemoteSl::moveFader (int channelNum, float newSliderPos)
{
    level[channelNum] = volumeFaderPositionToDB (newSliderPos);

    if (rightMode == rmVol)
        refreshRight (false);
}

void NovationRemoteSl::moveMasterLevelFader (float, float) {}
void NovationRemoteSl::moveAux (int, const char*, float) {}

void NovationRemoteSl::movePanPot (int channelNum, float newPan)
{
    pan[channelNum] = newPan;

    if (rightMode == rmPan)
        refreshRight (false);
}

void NovationRemoteSl::updateSoloAndMute (int channelNum, Track::MuteAndSoloLightState state, bool isBright)
{
    solo[channelNum] = (state & Track::soloLit) != 0 || (isBright && (state & Track::soloFlashing) != 0);
    mute[channelNum] = (state & Track::muteLit) != 0 || (isBright && (state & Track::muteFlashing) != 0);

    if (rightMode == rmMute || rightMode == rmSolo)
        refreshRight (false);
}

void NovationRemoteSl::soloCountChanged (bool)
{
    if (rightMode == rmSolo)
        refreshRight (false);
}

void NovationRemoteSl::playStateChanged (bool) {}

void NovationRemoteSl::recordStateChanged (bool isRecording)
{
    if (online)
        sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x4C, isRecording ? 1 : 0));
}

void NovationRemoteSl::automationReadModeChanged (bool) {}
void NovationRemoteSl::automationWriteModeChanged (bool) {}

void NovationRemoteSl::faderBankChanged (int, const StringArray& newNames)
{
    for (int i = 0; i < 8; ++i)
        trackNames[i] = newNames[i];

    refreshRight (true);

    if (leftMode == lmTracks)
        refreshLeft (true);
}

void NovationRemoteSl::channelLevelChanged (int, float) {}
void NovationRemoteSl::trackSelectionChanged (int, bool) {}
void NovationRemoteSl::trackRecordEnabled (int, bool) {}
void NovationRemoteSl::masterLevelsChanged (float, float) {}
void NovationRemoteSl::timecodeChanged (int, int, int, int, bool, bool) {}
void NovationRemoteSl::clickOnOffChanged (bool) {}
void NovationRemoteSl::snapOnOffChanged (bool) {}
void NovationRemoteSl::loopOnOffChanged (bool) {}
void NovationRemoteSl::slaveOnOffChanged (bool) {}
void NovationRemoteSl::punchOnOffChanged (bool) {}

void NovationRemoteSl::parameterChanged (int parameterNumber, const ParameterSetting& newValue)
{
    param[parameterNumber] = newValue;

    if (leftMode == lmParam1 || leftMode == lmParam2)
        refreshLeft (false);
}

void NovationRemoteSl::clearParameter (int parameterNumber)
{
    param[parameterNumber].clear();

    if (rightMode == rmVol)
        refreshRight (false);
}

bool NovationRemoteSl::wantsMessage (const MidiMessage& m)
{
    if (m.isController())
    {
        const int cn = m.getControllerNumber();

        if ((cn >= 0x00 && cn <= 0x1F)
         || (cn >= 0x25 && cn <= 0x37)
         || (cn >= 0x48 && cn <= 0x4D)
         || (cn >= 0x50 && cn <= 0x51)
         || (cn >= 0x53 && cn <= 0x5B)
         || (cn >= 0x5E && cn <= 0x5F))
            return true;
    }
    else if (m.isSysEx())
    {
        return true;
    }

    return false;
}

bool NovationRemoteSl::eatsAllMessages()
{
    return false;
}

bool NovationRemoteSl::canChangeSelectedPlugin()
{
    return ! isLocked;
}

void NovationRemoteSl::currentSelectionChanged()
{
    if (online)
    {
        bool w = wasLocked;

        setLeftMode (lmParam1);
        refreshLeft (true);

        if (w)
        {
            isLocked = true;
            wasLocked = false;
        }
    }
}

bool NovationRemoteSl::showingPluginParams()
{
    return leftMode == lmParam1 || leftMode == lmParam2;
}

bool NovationRemoteSl::showingTracks()
{
    return true;
}

void NovationRemoteSl::setLock (bool l, bool w)
{
    if (isLocked != l || wasLocked != w)
    {
        if (online)
            sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x50, (l || w) ? 1 : 0));

        isLocked  = l;
        wasLocked = w;
    }
}

void NovationRemoteSl::clearLeft()
{
    if (online)
        sendMidiArray (NovationRemoteSL::cmdClearLeft);

    screenContents[0] = String::repeatedString (" ", 8 * 9);
    screenContents[2] = String::repeatedString (" ", 8 * 9);
}

void NovationRemoteSl::clearRight()
{
    if (online)
        sendMidiArray (NovationRemoteSL::cmdClearRight);

    screenContents[1] = String::repeatedString (" ", 8 * 9);
    screenContents[3] = String::repeatedString (" ", 8 * 9);
}

void NovationRemoteSl::clearAllLights()
{
    if (online)
    {
        sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x4C, 0));
        sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x50, 0));
        sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x51, 0));
        sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x52, 0));
        sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x53, 0));
        sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x54, 0));
        sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x55, 0));
        sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x56, 0));
        sendMidiCommandToController (MidiMessage::controllerEvent (1, 0x57, 0));
    }
}

void NovationRemoteSl::refreshLeft (bool includeTitle)
{
    leftBottomDirty = true;

    if (includeTitle)
        leftTopDirty = true;

    if (online)
        triggerAsyncUpdate();
}

void NovationRemoteSl::refreshRight (bool includeTitle)
{
    rightBottomDirty = true;

    if (includeTitle)
        rightTopDirty = true;

    if (online)
        triggerAsyncUpdate();
}

void NovationRemoteSl::handleAsyncUpdate()
{
    CRASH_TRACER

    if (getEdit() != nullptr)
    {
        if (leftTopDirty)
        {
            String s;

            if (leftMode == lmTracks)
            {
                for (int i = 0; i < 8; ++i)
                {
                    if (auto t = externalControllerManager.getChannelTrack (i + trackOffset))
                        s << padAndLimit (t->getName(), 9);
                    else
                        s << "         ";
                }
            }

            if (leftMode == lmPlugins)
            {
                if (auto t = externalControllerManager.getChannelTrack (activeTrack))
                {
                    auto& pl = t->pluginList;

                    for (int i = 0; i < 8; ++i)
                    {
                        if (auto af = pl[i])
                            s << padAndLimit (af->getName(), 9);
                        else
                            s << "         ";
                    }
                }
                else
                {
                    s = String::repeatedString (" ", 9 * 8);
                }
            }

            if (leftMode == lmParam1)
            {
                for (int i = 0; i < 8; ++i)
                    s += padAndLimit (String::fromUTF8 (param[i].label), 9);
            }

            if (leftMode == lmParam2)
            {
                for (int i = 0; i < 8; ++i)
                    s += padAndLimit (String::fromUTF8 (param[i + 8].label), 9);
            }

            drawString(s, 1);
        }

        if (leftBottomDirty)
        {
            String s;

            if (leftMode == lmParam1)
            {
                for (int i = 0; i < 8; ++i)
                    s += padAndLimit (String::fromUTF8 (param[i].valueDescription), 9);
            }

            if (leftMode == lmParam2)
            {
                for (int i = 0; i < 8; ++i)
                    s += padAndLimit (String::fromUTF8 (param[i + 8].valueDescription), 9);
            }

            if (leftMode == lmTracks || leftMode == lmPlugins)
                s = String::repeatedString (" ", 9 * 8);

            drawString(s, 3);
        }

        if (rightTopDirty)
        {
            String s;

            for (int i = 0; i < 8; ++i)
                s += padAndLimit (trackNames[i], 9);

            drawString(s, 2);
        }

        if (rightBottomDirty)
        {
            String s;

            for (int i = 0; i < 8; ++i)
            {
                if (trackNames[i].isNotEmpty())
                {
                    switch (rightMode)
                    {
                        case rmVol:
                            s += padAndLimit (juce::String::formatted ("%.2f db", level[i]), 9);
                            break;

                        case rmPan:
                            s += padAndLimit (juce::String (std::abs (pan[i]), 2)
                                                + " " + juce::String (NovationRemoteSL::panLetter (pan[i])), 9);
                            break;

                        case rmSolo:
                            s += solo[i] ? padAndLimit (TRANS("Solo"), 9) : "         ";
                            break;

                        case rmMute:
                            s += mute[i] ? padAndLimit (TRANS("Mute"), 9) : "         ";
                            break;
                    }
                }
                else
                {
                    s += "         ";
                }
            }

            drawString (s, 4);
        }
    }
    else
    {
        clearLeft();
        clearRight();
    }

    leftTopDirty      = false;
    leftBottomDirty   = false;
    rightTopDirty     = false;
    rightBottomDirty  = false;
}

void NovationRemoteSl::drawString (const String& s, int panel)
{
    jassert (s.length() == 9 * 8);

    if (s == screenContents[panel - 1])
        return;

    int startMatch = 0;
    int endMatch = 0;

    for (int i = 0; i < s.length(); ++i)
    {
        if (s[i] == screenContents[panel - 1][i])
            startMatch++;
        else
            break;
    }

    for (int i = s.length(); --i >= 0;)
    {
        if (s[i] == screenContents [panel - 1][i])
            endMatch++;
        else
            break;
    }

    auto prnt = s.substring (startMatch, s.length() - endMatch);

    auto len = sizeof (NovationRemoteSL::cmdWrite) + 3 + (size_t) prnt.length() + 1;
    HeapBlock<uint8> buffer (len);

    memcpy (buffer, NovationRemoteSL::cmdWrite, sizeof (NovationRemoteSL::cmdWrite));
    buffer[sizeof (NovationRemoteSL::cmdWrite) + 0] = (uint8) startMatch;
    buffer[sizeof (NovationRemoteSL::cmdWrite) + 1] = (uint8) panel;
    buffer[sizeof (NovationRemoteSL::cmdWrite) + 2] = (uint8) 0x04;

    memcpy (buffer + sizeof (NovationRemoteSL::cmdWrite) + 3, (const char*) prnt.toUTF8(),
            (size_t) prnt.length());

    buffer[len - 1] = 0xF7;

    sendMidiCommandToController (buffer, (int) len);

    screenContents[panel - 1] = s;
}

juce::String NovationRemoteSl::padAndLimit (const String& s, int max)
{
    if (s.length() == max)
        return s;

    if (s.length() > max)
        return s.substring(0, max);

    if (s.length() < max)
    {
        int start = (max - s.length()) / 2;
        int end   = (max - s.length()) / 2 + (max - s.length()) % 2;

        return String::repeatedString(" ", start) + s + String::repeatedString(" ", end);
    }

    return {};
}

void NovationRemoteSl::markerChanged (int, const MarkerSetting&) {}
void NovationRemoteSl::clearMarker (int) {}
