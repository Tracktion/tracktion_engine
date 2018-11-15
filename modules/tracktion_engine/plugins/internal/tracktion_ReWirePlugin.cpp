/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if TRACKTION_ENABLE_REWIRE

#include "ReWire/ReWireSDK/ReWire.h"
#include "ReWire/ReWireSDK/ReWireAPI.h"
#include "ReWire/ReWireSDK/ReWireMixerAPI.h"

#if JUCE_WINDOWS
  #pragma comment(lib, "version.lib")
#endif

using namespace ReWire;

//==============================================================================
static String getReWireErrorMessage (ReWireError err)
{
    const char* e = "Unknown error";

    switch (err)
    {
        case kReWireError_AccessDenied:                 e = "Access Denied"; break;
        case kReWireError_ReWireOpenByOtherApplication: e = "ReWire in use by another application"; break;
        case kReWireError_DLLNotFound:                  e = "ReWire DLL not found"; break;
        case kReWireError_DLLTooOld:                    e = "ReWire DLL too old"; break;
        case kReWireError_UnableToLoadDLL:              e = "Unable to load ReWire DLL"; break;
        case kReWireError_NotEnoughMemoryForDLL:        e = "Not enough memory for ReWire DLL"; break;
        case kReWireError_OutOfMemory:                  e = "Out of memory"; break;
        case kReWireError_UnableToOpenDevice:           e = "Unable to open device"; break;
        case kReWireError_AlreadyExists:                e = "Already exists"; break;
        case kReWireError_NotFound:                     e = "Not found"; break;
        case kReWireError_Busy:                         e = "Busy"; break;
        case kReWireError_BufferFull:                   e = "Buffer full"; break;
        case kReWireError_PortNotConnected:             e = "Port not connected"; break;
        case kReWireError_PortConnected:                e = "Port connected"; break;
        case kReWireError_PortStale:                    e = "Port stale"; break;
        case kReWireError_ReadError:                    e = "Read error"; break;
        case kReWireError_NoMoreMessages:               e = "No more messages"; break;
        case kReWireImplError_ReWireNotOpen:            e = "ReWire not open"; break;
        case kReWireImplError_ReWireAlreadyOpen:        e = "ReWire already open"; break;
        case kReWireImplError_DeviceNotOpen:            e = "Device not open"; break;
        case kReWireImplError_DeviceAlreadyOpen:        e = "Device already open"; break;
        case kReWireImplError_AudioInfoInvalid:         e = "Audio info invalid"; break;
        case kReWireImplError_InvalidParameter:         e = "Invalid Parameter"; break;
        case kReWireImplError_InvalidSignature:         e = "Invalid Signature"; break;
        case kReWireError_Undefined:                    e = "Undefined error"; break;
        case kReWireError_NoError:                      e = "No Error"; break;
        default: break;
    }

    return e;
}

static void logRewireError (ReWireError res)
{
    if (res != kReWireError_NoError)
        TRACKTION_LOG_ERROR (getReWireErrorMessage (res));
}

const uint32 inputEventBufferSize = 200;

//==============================================================================
/**
    Represents a ReWire device.

    This will be shared by multiple ReWirePlugin objects. To do this is horrible,
    and involves temp buffers, etc. depending on the order in which it all gets
    processed.
*/
class ReWireSystem::Device  : private Timer
{
public:
    Device (Engine& e, TRWMDeviceHandle h, const String& name, int index)
      : engine (e),
        handle (h),
        deviceName (name),
        deviceIndex (index),
        buffer (2, 128)
    {
        CRASH_TRACER
        jassert (isReWireEnabled (engine));

        zeromem (reWireToLocalChanMap, sizeof (reWireToLocalChanMap));

        ReWireDeviceInfo devInfo;
        ReWirePrepareDeviceInfo (&devInfo);
        auto res = RWMGetDeviceInfo (index, &devInfo);

        if (res == kReWireError_NoError)
        {
            for (int i = 0; i < devInfo.fChannelCount; ++i)
                channelNames.add (CharPointer_UTF8 (devInfo.fChannelNames[i]));
        }
        else
        {
            logRewireError (res);
        }

        // prepare all this stuff in case the timer has to call DriveAudio before the
        // real callback
        ReWireDriveAudioInputParams& in = inputToDeviceParams;
        ReWireDriveAudioOutputParams& out = outputFromDeviceParams;

        ReWirePrepareDriveAudioInputParams (&in, inputEventBufferSize, inputToDeviceBuffer);

        ReWireClearBitField (in.fRequestedChannelsBitField, kReWireAudioChannelCount);

        outputEventBufferSize = (uint32) jmax (32, (int) devInfo.fMaxEventOutputBufferSize);
        outputFromDeviceBuffer.calloc (outputEventBufferSize);

        ReWirePrepareDriveAudioOutputParams (&out, (ReWire_uint32_t) outputEventBufferSize, outputFromDeviceBuffer);

        in.fFramesToRender = 128;
        in.fTempo = 1000 * 120;
        in.fSignatureNumerator = 4;
        in.fSignatureDenominator = 4;
        in.fLoopStartPPQ15360Pos = rewireLoopStart;
        in.fLoopEndPPQ15360Pos = rewireLoopEnd;
        in.fLoopOn = rewireLooping;

        ReWirePrepareEventTarget (&eventTarget, 0, 0);

        startTimer (50);
    }

    bool closeIfPossible()
    {
        CRASH_TRACER

        if (handle == nullptr)
            return true;

        char okFlag = 0;

        if (RWMIsCloseDeviceOK (handle, &okFlag) == kReWireError_NoError && okFlag != 0)
        {
            if (RWMCloseDevice (handle) == kReWireError_NoError)
            {
                handle = nullptr;
                return true;
            }
        }

        return false;
    }

    void addReference()
    {
        jassert (isReWireEnabled (engine));
        ++references;
    }

    void removeReference()
    {
        --references;
        jassert (references >= 0);
    }

    void prepareToPlay (double sr, int blockSize, int leftChanIndex, int rightChanIndex, Edit* edit)
    {
        CRASH_TRACER
        jassert (isReWireEnabled (engine));
        jassert (edit != nullptr);

        const ScopedLock sl (lock);

        buffer.clear();
        storedMessages.clear();

        sampleRate = sr;
        containerEdit = edit;

        bufferSourceChannels.setBit (leftChanIndex);
        bufferSourceChannels.setBit (rightChanIndex);
        buffer.setSize (bufferSourceChannels.countNumberOfSetBits(), blockSize);

        zeromem (reWireToLocalChanMap, sizeof (reWireToLocalChanMap));
        int localChan = 0;

        for (int i = 0; i < kReWireAudioChannelCount; ++i)
            if (bufferSourceChannels[i])
                reWireToLocalChanMap[i] = (short) localChan++;

        ReWireDeviceInfo devInfo;
        ReWirePrepareDeviceInfo (&devInfo);
        auto res = RWMGetDeviceInfo (deviceIndex, &devInfo);
        logRewireError (res);

        ReWireAudioInfo info;
        ReWirePrepareAudioInfo (&info, (ReWire_int32_t) sr, blockSize + 512);
        res = RWMSetAudioInfo (&info);

        if (res != kReWireError_NoError)
        {
            logRewireError (res);
            engine.getUIBehaviour().showWarningMessage (TRANS("Couldn't start ReWire plugin")
                                                          + ": " + getReWireErrorMessage (res));
        }
        else
        {
            auto& in = inputToDeviceParams;
            auto& out = outputFromDeviceParams;

            // setup the input fields
            ReWirePrepareDriveAudioInputParams (&in,
                                                inputEventBufferSize,
                                                inputToDeviceBuffer);

            ReWireClearBitField (in.fRequestedChannelsBitField, kReWireAudioChannelCount);

            for (int i = kReWireAudioChannelCount; --i >= 0;)
            {
                if (bufferSourceChannels[i])
                {
                    ReWireSetBitInBitField (in.fRequestedChannelsBitField, (unsigned short)i);
                    in.fAudioBuffers[i] = buffer.getWritePointer (reWireToLocalChanMap[i], 0);
                }
            }

            // setup the output fields
            outputEventBufferSize = (uint32) jmax (32, (int) devInfo.fMaxEventOutputBufferSize);
            outputFromDeviceBuffer.calloc (outputEventBufferSize);

            ReWirePrepareDriveAudioOutputParams (&out, (ReWire_uint32_t) outputEventBufferSize, outputFromDeviceBuffer);

            rewireLoopStart = 0;
            rewireLoopEnd = 0;
            rewireLooping = 0;

            if (edit != nullptr)
            {
                if (auto* transport = getTransport())
                {
                    TempoSequencePosition markPos (edit->tempoSequence);
                    const auto loopRange = transport->getLoopRange();

                    markPos.setTime (loopRange.getStart());
                    rewireLoopStart = roundToInt (markPos.getPPQTime() * kReWirePPQ);

                    markPos.setTime (loopRange.getEnd());
                    rewireLoopEnd = roundToInt (markPos.getPPQTime() * kReWirePPQ);

                    rewireLooping = transport->looping;
                }
            }

            // time limit for guessing if we need to chase the time
            timePerBlock = 0.060 + blockSize / sampleRate;

            ReWirePrepareEventTarget (&eventTarget, 0, 0);

            timeSigRequest = false;
            requestTempo = false;
            requestLoop = false;
            requestedReposition = false;
            requestedPlay = false;
            requestedStop = false;
            wasPlaying = false;
        }
    }

    void deinitialise()
    {
        bufferSourceChannels.clear();
        storedMessages.clear();
    }

    void updateTempoInfo (const TempoSequencePosition& position)
    {
        auto& t = position.getCurrentTempo();

        inputToDeviceParams.fTempo = (t.bpm < 10) ? 120000 : (ReWire_uint32_t) (1000 * t.bpm);
        inputToDeviceParams.fSignatureNumerator   = (t.numerator <= 0)   ? 4 : (ReWire_uint32_t) t.numerator;
        inputToDeviceParams.fSignatureDenominator = (t.denominator == 0) ? 4 : (ReWire_uint32_t) t.denominator;

        inputToDeviceParams.fPPQ15360TickOfBatchStart = roundToInt (position.getPPQTime() * kReWirePPQ);

        pluginsServedThisFrame = 0;
    }

    struct ReWireMidiMessageSorter
    {
        static int compareElements (ReWireMIDIEvent* first, ReWireMIDIEvent* second) noexcept
        {
            return first->fRelativeSamplePos - second->fRelativeSamplePos;
        }
    };

    void getAudioOutput (const AudioRenderContext& fc,
                         int leftChannelIndex, int rightChannelIndex,
                         int bus, int channel)
    {
        const ScopedLock sl (lock);

        auto& in = inputToDeviceParams;
        auto& out = outputFromDeviceParams;

        if (fc.bufferForMidiMessages != nullptr && references <= 1)
        {
            fc.bufferForMidiMessages->sortByTimestamp();

            auto num = jmin ((int) fc.bufferForMidiMessages->size(), (int) inputEventBufferSize);
            auto* event = &in.fEventInBuffer.fEventBuffer [in.fEventInBuffer.fCount];

            for (int i = 0; i < num; ++i)
            {
                auto& m = (*fc.bufferForMidiMessages)[i];
                const int type = *(m.getRawData());

                if (type < 0xf0 && type >= 0x80)
                {
                    setupMidiEvent (*ReWireConvertToMIDIEvent (event, &eventTarget),
                                    type, m, fc.bufferNumSamples, bus, channel);

                    ++event;
                    in.fEventInBuffer.fCount++;
                }
            }

            fc.bufferForMidiMessages->clear();
        }

        if (pluginsServedThisFrame == 0)
        {
            in.fFramesToRender = (ReWire_uint32_t) fc.bufferNumSamples;
            bool isPlaying = this->isPlaying (fc, in);

            in.fLoopStartPPQ15360Pos = rewireLoopStart;
            in.fLoopEndPPQ15360Pos = rewireLoopEnd;
            in.fLoopOn = rewireLooping && ! fc.isRendering;

            out.fEventOutBuffer.fCount = 0;
            ReWireClearBitField (out.fServedChannelsBitField, kReWireAudioChannelCount);

            if (storedMessages.size() > 0)
            {
                ReWireMidiMessageSorter sorter;
                storedMessages.sort (sorter);

                auto* event = &in.fEventInBuffer.fEventBuffer [in.fEventInBuffer.fCount];
                const int num = jmin (storedMessages.size(), (int) inputEventBufferSize);

                for (int i = 1; i < num; ++i)
                {
                    auto* e1 = storedMessages.getUnchecked (i - 1);
                    auto* e2 = storedMessages.getUnchecked (i);

                    if (e1->fData1 == e2->fData1
                         && e1->fRelativeSamplePos == e2->fRelativeSamplePos
                         && e1->fMIDIEventType == 0x90
                         && e2->fMIDIEventType == 0x80)
                    {
                        e1->fMIDIEventType = 0x80;
                        e2->fMIDIEventType = 0x90;
                        std::swap (e1->fData2, e2->fData2);
                    }
                }

                for (int i = 0; i < num; ++i)
                {
                    auto* midiEvent = ReWireConvertToMIDIEvent (event, &eventTarget);
                    *midiEvent = *storedMessages.getUnchecked(i);
                    ++event;
                    in.fEventInBuffer.fCount++;
                }

                storedMessages.clear();
            }

            if (wasPlaying && ! isPlaying)
                in.fEventInBuffer.fCount = 0;

            wasPlaying = isPlaying;

            if (handle != nullptr)
                RWMDriveAudio (handle, &in, &out);

            lastDriveAudioTime = Time::getApproximateMillisecondCounter();

            in.fEventInBuffer.fCount = 0;
        }

        if (references > 1 && fc.bufferForMidiMessages != nullptr)
        {
            for (auto& m : *fc.bufferForMidiMessages)
            {
                auto type = *(m.getRawData());

                if (type < 0xf0 && type >= 0x80)
                {
                    auto midiEvent = new ReWireMIDIEvent();

                    setupMidiEvent (*ReWireConvertToMIDIEvent ((ReWireEvent*) midiEvent, &eventTarget),
                                    type, m, fc.bufferNumSamples, bus, channel);

                    storedMessages.add (midiEvent);
                }
            }

            fc.bufferForMidiMessages->clear();
        }

        if (pluginsServedThisFrame++ == 0)
            handleEvents (out, fc.bufferForMidiMessages);

        if (fc.destBuffer != nullptr)
        {
            if (ReWireIsBitInBitFieldSet (out.fServedChannelsBitField, (unsigned short) leftChannelIndex))
                FloatVectorOperations::copy (fc.destBuffer->getWritePointer (0, fc.bufferStartSample),
                                             in.fAudioBuffers[leftChannelIndex],
                                             fc.bufferNumSamples);

            if (ReWireIsBitInBitFieldSet (out.fServedChannelsBitField, (unsigned short) rightChannelIndex))
                FloatVectorOperations::copy (fc.destBuffer->getWritePointer (1, fc.bufferStartSample),
                                             in.fAudioBuffers[rightChannelIndex],
                                             fc.bufferNumSamples);
        }
    }

    void setupMidiEvent (ReWireMIDIEvent& e, int type, const MidiMessage& m, int numSamples, int bus, int channel) const
    {
        e.fMIDIEventType = (unsigned short) (0xf0 & type);
        e.fData1 = m.getRawData()[1];
        e.fData2 = m.getRawData()[2];

        // need to make sure note-ons with vel=0 are converted to note-offs
        if (e.fData2 == 0 && (e.fMIDIEventType & 0xf0) == 0x90)
            e.fMIDIEventType = 0x80;

        e.fRelativeSamplePos = jlimit (0, numSamples - 1, roundToInt (m.getTimeStamp() * sampleRate));
        e.fEventTarget.fMIDIBusIndex = (unsigned short) bus;
        e.fEventTarget.fChannel = (unsigned short) channel;
    }

    bool isPlaying (const AudioRenderContext& fc, ReWireDriveAudioInputParams& in)
    {
        auto playheadOutputTime = fc.getEditTime().editRange1.getStart();

        if ((fc.playhead.isPlaying() && playheadOutputTime >= 0) || fc.isRendering)
        {
            if (lastTime > playheadOutputTime || lastTime < playheadOutputTime - timePerBlock)
                in.fPlayMode = kReWirePlayModeChaseAndPlay;
            else
                in.fPlayMode = kReWirePlayModeKeepPlaying;

            lastTime = playheadOutputTime;
            return true;
        }

        in.fPlayMode = kReWirePlayModeStop;
        return false;
    }

    void handleEvents (ReWireDriveAudioOutputParams& out,
                       MidiMessageArray* bufferForMidiMessages)
    {
        auto numEventsOut = (int) out.fEventOutBuffer.fCount;

        for (int i = 0; i < numEventsOut; ++i)
        {
            auto event = &out.fEventOutBuffer.fEventBuffer[i];

            switch (event->fEventType)
            {
            case kReWireRequestSignatureEvent:
                {
                    auto theEvent = ReWireCastToRequestSignatureEvent (event);
                    requestedTimeSigNum   = jmax (1, (int) theEvent->fSignatureNumerator);
                    requestedTimeSigDenom = jmax (1, (int) theEvent->fSignatureDenominator);
                    timeSigRequest = true;
                }
                break;

            case kReWireRequestTempoEvent:
                {
                    auto theEvent = ReWireCastToRequestTempoEvent (event);
                    requestedTempo = (int) theEvent->fTempo;
                    requestTempo = true;
                }
                break;

            case kReWireRequestLoopEvent:
                {
                    auto theEvent = ReWireCastToRequestLoopEvent (event);
                    rewireLoopStart = theEvent->fLoopStartPPQ15360Pos;
                    rewireLoopEnd = theEvent->fLoopEndPPQ15360Pos;
                    rewireLooping = theEvent->fLoopOn != 0;
                    requestLoop = true;
                }
                break;

            case kReWireRequestRepositionEvent:
                {
                    auto theEvent = ReWireCastToRequestRepositionEvent (event);
                    requestedPosition = theEvent->fPPQ15360Pos / (double) kReWirePPQ;
                    requestedReposition = true;
                }
                break;

            case kReWireRequestPlayEvent:
                requestedPlay = true;
                break;

            case kReWireRequestStopEvent:
                requestedStop = true;
                break;

            case kReWireMIDIEvent:
                if (bufferForMidiMessages != nullptr)
                {
                    auto m = ReWireCastToMIDIEvent (event);

                    bufferForMidiMessages->addMidiMessage (MidiMessage (m->fMIDIEventType | (0xf & m->fEventTarget.fChannel),
                                                                        m->fData1, m->fData2),
                                                           0, midiSourceID);
                }
                break;

            default:
                break;
            }
        }
    }

    void timerCallback() override
    {
        if (! Selectable::isSelectableValid (containerEdit))
            containerEdit = nullptr;

        if (timeSigRequest)
        {
            CRASH_TRACER

            if (containerEdit != nullptr && containerEdit->tempoSequence.getNumTempos() == 1)
            {
                containerEdit->tempoSequence.getTimeSig(0)->setStringTimeSig (String (requestedTimeSigNum)
                                                                               + "/"
                                                                               + String (requestedTimeSigDenom));
            }

            timeSigRequest = false;
        }

        if (requestTempo)
        {
            CRASH_TRACER

            if (containerEdit != nullptr && containerEdit->tempoSequence.getNumTempos() == 1)
                containerEdit->tempoSequence.getTempo(0)->setBpm (requestedTempo / 1000.0);

            requestTempo = false;
        }

        if (requestLoop)
        {
            CRASH_TRACER

            if (containerEdit != nullptr)
            {
                if (auto* transport = getTransport())
                {
                    transport->looping = rewireLooping;

                    TempoSequencePosition markPos (containerEdit->tempoSequence);

                    markPos.setPPQTime (rewireLoopStart / (double)kReWirePPQ);
                    transport->setLoopIn (markPos.getTime());

                    markPos.setPPQTime (rewireLoopEnd / (double)kReWirePPQ);
                    transport->setLoopOut (markPos.getTime());
                }
            }

            requestLoop = false;
        }
        else if (containerEdit != nullptr)
        {
            CRASH_TRACER

            if (auto transport = getTransport())
            {
                TempoSequencePosition markPos (containerEdit->tempoSequence);
                const auto loopRange = transport->getLoopRange();
                markPos.setTime (loopRange.getStart());
                rewireLoopStart = roundToInt (markPos.getPPQTime() * kReWirePPQ);

                markPos.setTime (loopRange.getEnd());
                rewireLoopEnd = roundToInt (markPos.getPPQTime() * kReWirePPQ);

                rewireLooping = transport->looping;
            }
        }

        if (requestedReposition)
        {
            CRASH_TRACER
            if (auto* transport = getTransport())
            {
                TempoSequencePosition pos (containerEdit->tempoSequence);
                pos.setPPQTime (requestedPosition);
                transport->setCurrentPosition (pos.getTime());
            }

            requestedReposition = false;
        }

        if (requestedPlay)
        {
            CRASH_TRACER
            if (auto* transport = getTransport())
                transport->play (true);

            requestedPlay = false;
        }

        if (requestedStop)
        {
            CRASH_TRACER
            if (auto* transport = getTransport())
                transport->stop (false, false);

            requestedStop = false;
        }

        if (Time::getApproximateMillisecondCounter() - lastDriveAudioTime > 400)
        {
            wasPlaying = false;

            ReWireDriveAudioInputParams& in = inputToDeviceParams;
            ReWireDriveAudioOutputParams& out = outputFromDeviceParams;

            const ScopedLock sl (lock);

            // might have been waiting for real callback, so check again..
            if (Time::getApproximateMillisecondCounter() - lastDriveAudioTime > 400)
            {
                CRASH_TRACER

                in.fPlayMode = kReWirePlayModeStop;
                in.fEventInBuffer.fCount = 0;

                in.fLoopStartPPQ15360Pos = rewireLoopStart;
                in.fLoopEndPPQ15360Pos = rewireLoopEnd;
                in.fLoopOn = rewireLooping;

                out.fEventOutBuffer.fCount = 0;
                ReWireClearBitField (out.fServedChannelsBitField, kReWireAudioChannelCount);

                if (handle != 0)
                    RWMDriveAudio (handle, &in, &out);

                const int numEventsOut = (int) out.fEventOutBuffer.fCount;

                for (int i = 0; i < numEventsOut; ++i)
                {
                    auto event = &out.fEventOutBuffer.fEventBuffer[i];

                    switch (event->fEventType)
                    {
                    case kReWireRequestSignatureEvent:
                        {
                            auto theEvent = ReWireCastToRequestSignatureEvent (event);
                            requestedTimeSigNum = jmax (1, (int) theEvent->fSignatureNumerator);
                            requestedTimeSigDenom = jmax (1, (int) theEvent->fSignatureDenominator);
                            timeSigRequest = true;
                        }
                        break;

                    case kReWireRequestTempoEvent:
                        {
                            auto theEvent = ReWireCastToRequestTempoEvent (event);
                            requestedTempo = (int) theEvent->fTempo;
                            requestTempo = true;
                        }
                        break;

                    case kReWireRequestLoopEvent:
                        {
                            auto theEvent = ReWireCastToRequestLoopEvent (event);
                            rewireLoopStart = theEvent->fLoopStartPPQ15360Pos;
                            rewireLoopEnd = theEvent->fLoopEndPPQ15360Pos;
                            rewireLooping = theEvent->fLoopOn != 0;
                            requestLoop = true;
                        }
                        break;

                    case kReWireRequestRepositionEvent:
                        {
                            auto theEvent = ReWireCastToRequestRepositionEvent (event);
                            requestedPosition = theEvent->fPPQ15360Pos / (double)kReWirePPQ;
                            requestedReposition = true;
                        }
                        break;

                    case kReWireRequestPlayEvent:
                        requestedPlay = true;
                        break;

                    case kReWireRequestStopEvent:
                        requestedStop = true;
                        break;

                    default:
                        break;
                    }
                }
            }
        }
    }

    TransportControl* getTransport() const
    {
        if (containerEdit != nullptr)
            return &containerEdit->getTransport();

        return {};
    }

    Engine& engine;
    TRWMDeviceHandle handle;
    String deviceName;
    StringArray channelNames;

private:
    const int deviceIndex;
    ReWireDriveAudioInputParams inputToDeviceParams;
    ReWireDriveAudioOutputParams outputFromDeviceParams;
    ReWireEvent inputToDeviceBuffer [inputEventBufferSize];
    HeapBlock<ReWireEvent> outputFromDeviceBuffer;
    uint32 outputEventBufferSize;
    ReWireEventTarget eventTarget;

    uint32 lastDriveAudioTime = 0;
    juce::AudioBuffer<float> buffer;
    short reWireToLocalChanMap[kReWireAudioChannelCount];
    BigInteger bufferSourceChannels;

    OwnedArray<ReWireMIDIEvent> storedMessages;
    MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::createUniqueMPESourceID();

    int references = 0, pluginsServedThisFrame = 0;
    double sampleRate = 0, lastTime = 0, timePerBlock = 0;
    bool wasPlaying = false;
    Edit* containerEdit = nullptr;

    double requestedPosition = 0;
    int requestedTempo = 0, requestedTimeSigNum = 0, requestedTimeSigDenom = 0;
    int rewireLoopStart = 0, rewireLoopEnd = 0;
    bool timeSigRequest = false;
    bool requestTempo = false;
    bool rewireLooping = false;
    bool requestLoop = false;
    bool requestedReposition = false;
    bool requestedPlay = false;
    bool requestedStop = false;

    CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Device)
};

//==============================================================================
static ReWireSystem* rewireSystemInstance = nullptr;

ReWireSystem::ReWireSystem (Engine& e)  : engine (e)
{
    CRASH_TRACER
    jassert (rewireSystemInstance == nullptr);
    jassert (isReWireEnabled (engine));

    TRACKTION_LOG ("Initialising ReWire...");

    ReWireOpenInfo openInfo;
    ReWirePrepareOpenInfo (&openInfo, 44100, 6400);

    auto res = RWMOpen (&openInfo);

    if (res != kReWireError_NoError)
    {
        openError = getReWireErrorMessage (res);
        logRewireError (res);
    }
    else
    {
        isOpen = true;

        ReWire_int32_t numDevs = 0;
        res = RWMGetDeviceCount (&numDevs);

        if (res == kReWireError_NoError)
        {
            for (int i = 0; i < numDevs; ++i)
            {
                ReWireDeviceInfo devInfo;
                ReWirePrepareDeviceInfo (&devInfo);

                res = RWMGetDeviceInfo (i, &devInfo);

                if (res == kReWireError_NoError)
                {
                    deviceNames.add (CharPointer_UTF8 (devInfo.fName));
                    devices.add (nullptr);
                }
                else
                {
                    logRewireError (res);
                }
            }

            if (numDevs > 0)
                startTimer (100); // for idle calls
        }
        else
        {
            logRewireError (res);
        }

        RWMIdle();
    }
}

ReWireSystem::~ReWireSystem()
{
    jassert (rewireSystemInstance == this);
    closeSystem();
    rewireSystemInstance = nullptr;
}

bool ReWireSystem::isReWireEnabled (Engine& e, bool returnCurrentState)
{
    if (returnCurrentState)
    {
        static bool systemEnabled = isReWireEnabled (e, false);
        return systemEnabled;
    }

    return e.getPropertyStorage().getProperty (SettingID::reWireEnabled, true);
}

void ReWireSystem::setReWireEnabled (Engine& e, bool b)
{
    e.getPropertyStorage().setProperty (SettingID::reWireEnabled, b);
}

ReWireSystem* ReWireSystem::getInstanceIfActive()           { return rewireSystemInstance; }

const char* ReWireSystem::getReWireLibraryName()            { return ReWire::getReWireLibraryName(); }
const char* ReWireSystem::getReWireFolderName()             { return ReWire::getReWireFolderName(); }
const char* ReWireSystem::getPropellerheadFolderName()      { return ReWire::getPropellerheadFolderName(); }
int ReWireSystem::getRequiredVersionNumMajor()              { return ReWire::getRequiredVersionNumMajor(); }
int ReWireSystem::getRequiredVersionNumMinor()              { return ReWire::getRequiredVersionNumMinor(); }

void ReWireSystem::initialise (Engine& e)
{
    CRASH_TRACER

    if (rewireSystemInstance == nullptr && isReWireEnabled (e))
    {
        setReWireEnabled (e, false);

        {
            DeadMansPedalMessage dmp (e.getPropertyStorage(),
                                      TRANS("The ReWire system failed to start up correctly last time "
                                            "Tracktion ran - it has now been disabled (see the settings panel to re-enable it)")
                                        .replace ("Tracktion", e.getPropertyStorage().getApplicationName()));

            rewireSystemInstance = new ReWireSystem (e);
        }

        setReWireEnabled (e, true);
    }
}

bool ReWireSystem::shutdown()
{
    CRASH_TRACER

    if (rewireSystemInstance != nullptr)
    {
        if (rewireSystemInstance->tryToCloseAllOpenDevices())
        {
            delete rewireSystemInstance;
            return true;
        }

        return false;
    }

    return true;
}

bool ReWireSystem::closeSystem()
{
    CRASH_TRACER
    jassert (isReWireEnabled (engine));

    if (isOpen)
    {
        isOpen = false;
        char okFlag = 0;

        if (RWMIsCloseOK (&okFlag) == kReWireError_NoError && okFlag)
        {
            auto res = RWMClose();

            if (res == kReWireError_NoError)
                return true;

            jassertfalse;
            logRewireError (res);
            openError = getReWireErrorMessage (res);
        }
    }

    return false;
}

ReWireSystem::Device* ReWireSystem::openDevice (const String& devName, String& error)
{
    CRASH_TRACER
    jassert (isOpen);
    jassert (isReWireEnabled (engine));
    auto index = deviceNames.indexOf (devName);

    if (index >= 0)
    {
        if (auto dev = devices[index])
        {
            dev->addReference();
            return dev;
        }

        DeadMansPedalMessage dmp (engine.getPropertyStorage(),
                                  "The ReWire device \"" + devName + "\" crashed while being initialised.\n\n"
                                  "You may want to remove this device or disable ReWire (in Tracktion's settings panel).");

        if (auto dev = createDevice (index, devName, error))
        {
            devices.set (index, dev);
            dev->addReference();
            return dev;
        }
    }

    if (error.isEmpty())
        error = TRANS("Unknown device");

    return {};
}

ReWireSystem::Device* ReWireSystem::createDevice (int index, const String& devName, String& error)
{
    CRASH_TRACER
    TRWMDeviceHandle handle = nullptr;
    auto res = RWMOpenDevice (index, &handle);

    if (res != kReWireError_NoError)
    {
        logRewireError (res);
        error = getReWireErrorMessage (res);
        return {};
    }

    return new Device (engine, handle, devName, index);
}

bool ReWireSystem::tryToCloseAllOpenDevices()
{
    if (! isOpen)
        return true;

    CRASH_TRACER

    bool ok = true;
    bool waitForDevices = false;

    for (auto& dev : devices)
    {
        if (dev != nullptr)
        {
            char isRunningFlag = 0;

            if (RWMIsPanelAppLaunched (dev->handle, &isRunningFlag) == kReWireError_NoError
                 && isRunningFlag != 0)
            {
                auto res = RWMQuitPanelApp (dev->handle);
                jassert (res == kReWireError_NoError); (void) res;

                RWMIdle();
            }

            ok = ok && dev->closeIfPossible();
            waitForDevices = true;
        }
    }

    TRACKTION_LOG ("ReWire - closing system");

    if (ok && closeSystem())
        return true;

    juce::ignoreUnused (waitForDevices);

   #if JUCE_WINDOWS
    if (waitForDevices)
    {
        for (int j = 20; --j >= 0;)
        {
            RWMIdle();
            Thread::sleep (10);
        }
    }
   #endif

    TRACKTION_LOG ("ReWire - done");
    return false;
}

void ReWireSystem::timerCallback()
{
    CRASH_TRACER
    auto err = RWMIdle();
    jassert (err == kReWireError_NoError); (void) err;
}


//==============================================================================
const char* ReWirePlugin::xmlTypeName = "ReWire";

ReWirePlugin::ReWirePlugin (PluginCreationInfo info)  : Plugin (info)
{
    auto* um = getUndoManager();

    currentDeviceName.referTo (state, IDs::device, um);
    currentChannelNameL.referTo (state, IDs::channelL, um);
    currentChannelNameR.referTo (state, IDs::channelR, um);
    currentBus.referTo (state, IDs::bus, um);
    currentChannel.referTo (state, IDs::channel, um);

    if (ReWireSystem::isReWireEnabled (info.edit.engine))
        ReWireSystem::initialise (info.edit.engine);
}

ReWirePlugin::~ReWirePlugin()
{
    if (device != nullptr)
        device->removeReference();

    notifyListenersOfDeletion();
}

void ReWirePlugin::initialiseFully()
{
    openDevice (currentDeviceName);
}

void ReWirePlugin::valueTreeChanged()
{
    Plugin::valueTreeChanged();
    triggerAsyncUpdate();
}

void ReWirePlugin::handleAsyncUpdate()
{
    initialiseFully();
}

juce::String ReWirePlugin::getName()
{
    if (device != nullptr)
        return currentDeviceName;

    return TRANS("ReWire Device");
}

void ReWirePlugin::getChannelNames (StringArray* ins, StringArray* outs)
{
    getLeftRightChannelNames (ins);

    if (outs != nullptr)
    {
        outs->add (currentChannelNameL);
        outs->add (currentChannelNameR);
    }
}

void ReWirePlugin::initialise (const PlaybackInitialisationInfo& info)
{
    if (device != nullptr)
    {
        channelIndexL = jmax (0, device->channelNames.indexOf (currentChannelNameL.get()));
        channelIndexR = jmax (0, device->channelNames.indexOf (currentChannelNameR.get()));

        device->prepareToPlay (info.sampleRate, info.blockSizeSamples,
                               channelIndexL, channelIndexR, &edit);

        currentTempoPosition = new TempoSequencePosition (edit.tempoSequence);
    }
}

void ReWirePlugin::deinitialise()
{
    if (device != nullptr)
        device->deinitialise();
}

void ReWirePlugin::prepareForNextBlock (const AudioRenderContext& rc)
{
    if (currentTempoPosition != nullptr && device != nullptr)
    {
        currentTempoPosition->setTime (rc.playhead.streamTimeToSourceTime (rc.streamTime.getStart()));

        device->updateTempoInfo (TempoSequencePosition (*currentTempoPosition));
    }
}

void ReWirePlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (fc.destBuffer != nullptr && device != nullptr)
    {
        SCOPED_REALTIME_CHECK

        fc.destBuffer->setSize (2, fc.destBuffer->getNumSamples(), true);

        device->getAudioOutput (fc, channelIndexL, channelIndexR,
                                currentBus, currentChannel);
    }
}

//==============================================================================
juce::String ReWirePlugin::openDevice (const String& newDev)
{
    CRASH_TRACER
    auto error = TRANS("ReWire is disabled");

    if (ReWireSystem::isReWireEnabled (engine))
    {
        if (rewireSystemInstance != nullptr)
        {
            error = rewireSystemInstance->openError;

            if (device == nullptr || newDev != device->deviceName)
            {
                edit.getTransport().stop (false, true);

                if (auto newDevice = rewireSystemInstance->openDevice (newDev, error))
                {
                    if (device != nullptr)
                        device->removeReference();

                    device = newDevice;
                    currentDeviceName = newDev;

                    if (! device->channelNames.contains (currentChannelNameL.get()))
                        currentChannelNameL = device->channelNames[0];

                    if (! device->channelNames.contains (currentChannelNameR.get()))
                        currentChannelNameR = device->channelNames[jmin (1, device->channelNames.size() - 1)];

                    startTimer (2000);
                }

                if (newDev.isNotEmpty() && error.isNotEmpty())
                    engine.getUIBehaviour().showWarningMessage (TRANS("ReWire error - Couldn't open device")
                                                                   + ": " + error);

                CRASH_TRACER
                updateBusesAndChannels();

                changed();
                propertiesChanged();
            }
        }
    }

    return error;
}

bool ReWirePlugin::updateBusesAndChannels()
{
    StringArray newBuses, newChannels;
    bool hasChanged = false;

    if (device != nullptr)
    {
        ReWireEventInfo eventInfo;
        ReWirePrepareEventInfo (&eventInfo);

        auto err = RWMGetEventInfo (device->handle, &eventInfo);

        if (err != kReWireError_NoError)
        {
            logRewireError (err);
        }
        else
        {
            for (int i = 0; i < kReWireReservedEventBusIndex; ++i)
            {
                if (ReWireIsBitInBitFieldSet (eventInfo.fUsedBusBitField, (unsigned short)i))
                {
                    ReWireEventBusInfo eventBusInfo;
                    ReWirePrepareEventBusInfo (&eventBusInfo);

                    err = RWMGetEventBusInfo (device->handle, (unsigned short)i, &eventBusInfo);
                    jassert (err == kReWireError_NoError);

                    if (err == kReWireError_NoError)
                    {
                        String busName (CharPointer_UTF8 (eventBusInfo.fBusName));

                        if (busName.trim().isEmpty())
                            busName = "(" + TRANS("Unnamed") + ")";

                        newBuses.add (String (i + 1) + ". " + busName);
                    }
                }
            }
        }

        hasChanged = newBuses != buses;
        buses = newBuses;

        ReWireEventBusInfo eventBusInfo;
        ReWirePrepareEventBusInfo (&eventBusInfo);

        err = RWMGetEventBusInfo (device->handle, (unsigned short)currentBus, &eventBusInfo);
        jassert (err == kReWireError_NoError);

        if (err == kReWireError_NoError)
        {
            for (int j = 0; j < 16; ++j)
            {
                if (ReWireIsBitInBitFieldSet (eventBusInfo.fUsedChannelBitField, (unsigned short)j))
                {
                    ReWireEventChannelInfo eventChannelInfo;
                    ReWirePrepareEventChannelInfo (&eventChannelInfo);

                    ReWireEventTarget eventTarget;
                    ReWirePrepareEventTarget (&eventTarget, (unsigned short)currentBus, (unsigned short)j);

                    err = RWMGetEventChannelInfo (device->handle, &eventTarget, &eventChannelInfo);

                    jassert (err == kReWireError_NoError);

                    if (err == kReWireError_NoError)
                    {
                        const String chanName (CharPointer_UTF8 (eventChannelInfo.fChannelName));
                        newChannels.add (String (j + 1) + ". " + chanName);
                    }
                }
            }
        }

        hasChanged = hasChanged || (channels != newChannels);
        channels = newChannels;

        char isRunningFlag = 0;
        err = RWMIsPanelAppLaunched (device->handle, &isRunningFlag);
        bool nowRunning = (err == kReWireError_NoError && isRunningFlag != 0);
        hasChanged = hasChanged || (uiIsRunning != nowRunning);
        uiIsRunning = nowRunning;
    }
    else
    {
        uiIsRunning = false;
    }

    return hasChanged;
}

void ReWirePlugin::openExternalUI()
{
    if (device != nullptr)
    {
        auto err = RWMLaunchPanelApp (device->handle);

        if (err != kReWireError_NoError)
        {
            logRewireError (err);
            engine.getUIBehaviour().showWarningMessage (TRANS("ReWire error opening interface")
                                                          + ": " + getReWireErrorMessage (err));
        }

        updateBusesAndChannels();
    }
}

void ReWirePlugin::setLeftChannel (const String& channelName)
{
    if (currentChannelNameL == channelName)
        return;

    currentChannelNameL = channelName;
    changed();
    updateBusesAndChannels();
    TransportControl::restartAllTransports (engine, true);
}

void ReWirePlugin::setRightChannel (const String& channelName)
{
    if (currentChannelNameR == channelName)
        return;

    currentChannelNameR = channelName;
    changed();
    updateBusesAndChannels();
    TransportControl::restartAllTransports (engine, true);
}

void ReWirePlugin::setMidiBus (int busNum)
{
    unsigned short v = (unsigned short) (jmax (0, busNum));

    if (currentBus != v)
    {
        currentBus = v;

        if (updateBusesAndChannels())
            SelectionManager::refreshAllPropertyPanels();
    }
}

void ReWirePlugin::setMidiChannel (int channel)
{
    unsigned short v = (unsigned short) (jmax (0, channel));

    if (currentChannel != v)
    {
        currentChannel = v;
        changed();
    }
}

bool ReWirePlugin::hasNameForMidiNoteNumber (int note, int midiChannel, String& name)
{
    if (device != nullptr)
    {
        --midiChannel;

        ReWireEventTarget eventTarget;
        ReWirePrepareEventTarget (&eventTarget, (unsigned short)currentBus, (unsigned short)currentChannel);

        ReWireEventNoteInfo noteInfo;
        ReWirePrepareEventNoteInfo (&noteInfo);

        if (RWMGetNoteInfo (device->handle, &eventTarget, (unsigned short)note, &noteInfo) == kReWireError_NoError
             && noteInfo.fType != kReWireEventNoteTypeUnused)
        {
            name = noteInfo.fKeyName;
            return name.isNotEmpty();
        }
    }

    return false;
}

void ReWirePlugin::timerCallback()
{
    if (updateBusesAndChannels())
        propertiesChanged();
}

StringArray ReWirePlugin::getDeviceChannelNames() const
{
    return device->channelNames;
}

#endif
