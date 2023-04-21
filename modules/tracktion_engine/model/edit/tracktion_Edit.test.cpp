/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_BENCHMARKS

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class EditItemIDBenchmarks  : public juce::UnitTest
{
public:
    EditItemIDBenchmarks()
        : juce::UnitTest ("EditItemID", "tracktion_benchmarks")
    {}

    void runTest() override
    {
        // Create an empty edit
        // Add a MIDI clip with some random data
        // Copy/paste that clip 49 times on the same track (50 clips)
        // Copy/paste that track 99 times (5000 clips)
        // Copy/paste all clips (10,000 clips)
        // Load edit again from tree and see how long it takes to load
        
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        Clipboard clipboard;
        auto edit = Edit::createSingleTrackEdit (engine);
        
        beginTest ("Benchmark: Copy/paste");

        {
            auto c = getAudioTracks (*edit)[0]->insertMIDIClip ({ 0.0s, TimePosition (1.0s) }, nullptr);
            auto t1 = c->getTrack();
            
            ScopedBenchmark sb (getDescription ("Copy/paste clip 49 times"));
            
            for (int i = 1; i < 50; ++i)
            {
                auto clipState = c->state.createCopy();
                EditItemID::remapIDs (clipState, nullptr, c->edit);
                
               #if JUCE_DEBUG
                auto newClipID = EditItemID::fromID (clipState);
                jassert (newClipID != EditItemID::fromID (c->state));
                jassert (findClipForID (c->edit, newClipID) == nullptr);
                jassert (edit->clipCache.findItem (newClipID) == nullptr);

                jassert (! t1->state.getChildWithProperty (IDs::id, newClipID).isValid());
               #endif

                t1->state.appendChild (clipState, c->getUndoManager());

               #if JUCE_DEBUG
                jassert (t1->findClipForID (newClipID) != nullptr);
               #endif
            }
        }

        {
            auto t1 = getAudioTracks (*edit)[0];
            jassert (t1->getNumTrackItems() == 50);
            auto preceeding = t1->state;
            
            ScopedBenchmark sb (getDescription ("Copy/paste track 99 times"));
            
            for (int i = 1; i < 100; ++i)
            {
                auto trackState = t1->state.createCopy();
                EditItemID::remapIDs (trackState, nullptr, *edit);

               #if JUCE_DEBUG
                auto newTrackID = EditItemID::fromID (trackState);
                jassert (newTrackID != EditItemID::fromID (t1->state));
                jassert (findTrackForID (*edit, newTrackID) == nullptr);
                jassert (edit->trackCache.findItem (newTrackID) == nullptr);

                jassert (! t1->state.getChildWithProperty (IDs::id, newTrackID).isValid());
               #endif

                edit->insertTrack (trackState, {}, preceeding, nullptr);
                preceeding = trackState;
                
               #if JUCE_DEBUG
                jassert (findTrackForID (*edit, newTrackID) != nullptr);
               #endif
            }
        }
        
        {
            auto allAudioTracks = getAudioTracks (*edit);
            jassert (allAudioTracks.size() == 100);
            
            ScopedBenchmark sb (getDescription ("Copy/paste all 5,000 clips using Clipboard"));
            Clipboard::Clips content;
            int trackOffset = 0;
            
            for (auto at : allAudioTracks)
            {
                for (auto c : at->getClips())
                    if (auto mc = dynamic_cast<MidiClip*> (c))
                        content.addClip (trackOffset, mc->state);
                
                ++trackOffset;
            }
            
            EditInsertPoint insertPoint (*edit);
            content.pasteIntoEdit (*edit, insertPoint, nullptr);
            
           #if JUCE_DEBUG
            const int numClips = std::accumulate (allAudioTracks.begin(), allAudioTracks.end(), 0,
                                                  [] (int total, auto t) { return total + t->getNumTrackItems(); });
            jassert (numClips == 10'000);
           #endif
        }
        
        {
            auto editStateCopy = edit->state.createCopy();
            
            ScopedBenchmark sb (getDescription ("Load Edit from state"));
            Edit editCopy ({ engine, editStateCopy, ProjectItemID::createNewID (0) });
            jassert (getAudioTracks (editCopy).size() == 100);
        }
    }

private:
    BenchmarkDescription getDescription (std::string bmName)
    {
        const auto bmCategory = (getName() + "/" + getCategory()).toStdString();
        const auto bmDescription = bmName;
        
        return { std::hash<std::string>{} (bmName + bmCategory + bmDescription),
                 bmCategory, bmName, bmDescription };
    }
};

static EditItemIDBenchmarks editItemIDBenchmarks;

}} // namespace tracktion { inline namespace engine

#endif //TRACKTION_BENCHMARKS
