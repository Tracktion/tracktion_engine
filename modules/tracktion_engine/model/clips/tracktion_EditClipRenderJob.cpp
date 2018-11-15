/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


RenderManager::Job::Ptr EditRenderJob::getOrCreateRenderJob (Engine& e, Renderer::Parameters& params,
                                                             bool deleteEdit, bool silenceOnBackup, bool reverse)
{
    AudioFile targetFile (params.destFile);

    if (auto ptr = e.getRenderManager().getRenderJobWithoutCreating (targetFile))
    {
        // we'll need to delete the edit if we're already generating for it
        if (deleteEdit)
        {
            jassertfalse; // we shouldn't hit this any more, let me know if you do
            std::unique_ptr<Edit> scopedEdit (params.edit);
        }

        return ptr;
    }

    return new EditRenderJob (e, params, deleteEdit, silenceOnBackup, reverse);
}

RenderManager::Job::Ptr EditRenderJob::getOrCreateRenderJob (Engine& e, const AudioFile& destFile, const RenderOptions& ro,
                                                             ProjectItemID itemID, bool silenceOnBackup, bool reverse)
{
    if (auto ptr = e.getRenderManager().getRenderJobWithoutCreating (destFile))
        return ptr;

    return new EditRenderJob (e, destFile, ro, itemID, silenceOnBackup, reverse);
}

//==============================================================================
EditRenderJob::EditRenderJob (Engine& e, const Renderer::Parameters& p, bool deleteEdit,
                              bool silenceOnBackup_, bool reverse_)
    : Job (e, AudioFile (p.destFile)),
      renderOptions (e),
      params (p),
      editDeleter (p.edit, deleteEdit),
      silenceOnBackup (silenceOnBackup_),
      reverse (reverse_),
      thumbnailToUpdate (256, e.getAudioFileFormatManager().readFormatManager,
                         e.getAudioFileManager().getAudioThumbnailCache())
{
}

EditRenderJob::EditRenderJob (Engine& e, const AudioFile& destFile_, const RenderOptions& ro,
                              ProjectItemID edID, bool silenceOnBackup_, bool reverse_)
    : Job (e, AudioFile (destFile_)),
      renderOptions (ro, nullptr),
      itemID (edID),
      params (e),
      silenceOnBackup (silenceOnBackup_), reverse (reverse_),
      thumbnailToUpdate (256, e.getAudioFileFormatManager().readFormatManager,
                         e.getAudioFileManager().getAudioThumbnailCache())
{
}

EditRenderJob::~EditRenderJob()
{
    renderPasses.clear();

    if (! editDeleter.willDeleteObject())
        if (editDeleter != nullptr)
            editDeleter->getTransport().editHasChanged();
}

String EditRenderJob::getLastError() const
{
    const ScopedLock sl (errorLock);
    return lastError;
}

void EditRenderJob::setLastError (const String& e)
{
    const ScopedLock sl (errorLock);
    lastError = e;
}

//==============================================================================
bool EditRenderJob::setUpRender()
{
    if (params.edit == nullptr || itemID.isValid())
    {
        CRASH_TRACER

        Edit::LoadContext context;
        auto contextUpdater = std::async (std::launch::async, [this, &context]
                                          {
                                              while (! context.completed)
                                              {
                                                  context.shouldExit = shouldExit();

                                                  if (context.shouldExit)
                                                      break;

                                                  Thread::sleep (100);
                                              }
                                          });
        ignoreUnused (contextUpdater);
        auto edit = new Edit (*params.engine,
                              loadEditFromProjectManager (*ProjectManager::getInstance(), itemID),
                              Edit::forRendering, &context, 1); // always use saved version!
        editDeleter.setOwned (edit);

        // it's difficult to determine the marked region or selections at this point, so we'll ignore it,
        // assuming that this code will only be used for rendering entire EditClips, and not sections of edits.
        jassert (! renderOptions.markedRegion);
        jassert (! renderOptions.selectedClips);
        jassert (! renderOptions.selectedTracks);

        params = renderOptions.getRenderParameters (*edit);
        params.edit         = edit;
        params.destFile     = proxy.getFile();
        params.tracksToDo   = renderOptions.getTrackIndexes (*edit);
        params.category     = ProjectItem::Category::none;
    }

    CRASH_TRACER

    if (params.separateTracks)
        renderSeparateTracks();
    else
        renderPasses.add (new RenderPass (*this, params, "Edit Render"));

    return true;
}

bool EditRenderJob::renderNextBlock()
{
    CRASH_TRACER

    // do these in order so we don't jump in and out of the Edit
    if (auto pass = renderPasses.getFirst())
    {
        if (pass->task == nullptr)
            pass->initialise();

        if (pass->task == nullptr || pass->task->runJob() == ThreadPoolJob::jobHasFinished)
            renderPasses.removeObject (pass);
    }

    return renderPasses.isEmpty();
}

bool EditRenderJob::completeRender()
{
    CRASH_TRACER

    if (result.items.size() > 0
         || (params.category == ProjectItem::Category::none && proxy.getFile().existsAsFile()))
        result.result = Result::ok();

    return result.result.wasOk();
}

//==============================================================================
EditRenderJob::RenderPass::RenderPass (EditRenderJob& j, Renderer::Parameters& renderParams, const String& description)
    : owner (j), r (renderParams), desc (description), originalCategory (r.category),
      tempFile (r.destFile, TemporaryFile::useHiddenFile)
{
    r.category = ProjectItem::Category::none;
    r.destFile = tempFile.getFile();
}

EditRenderJob::RenderPass::~RenderPass()
{
    const String errorMessage (task != nullptr ? task->errorMessage : String());
    owner.setLastError (errorMessage);
    const bool completedOk = task != nullptr ? task->getCurrentTaskProgress() == 1.0f : false;
    task = nullptr;

    if (owner.editDeleter.willDeleteObject())
        callBlocking ([this] { Renderer::turnOffAllPlugins (*r.edit); });

    // overwite with temp file
    if (! errorMessage.isEmpty() && owner.silenceOnBackup)
        owner.generateSilence (tempFile.getFile());

    if (tempFile.getFile().existsAsFile() && (completedOk || owner.silenceOnBackup))
        tempFile.overwriteTargetFileWithTemporary();
    else
        tempFile.getTargetFile().deleteFile();

    // swap this back to the original
    r.destFile = tempFile.getTargetFile();
    r.category = originalCategory;

    // reverse if needed
    if (owner.reverse)
    {
        TemporaryFile tempReverseFile (r.destFile);

        if (r.destFile.existsAsFile())
            if (AudioFileUtils::reverse (r.destFile, tempReverseFile.getFile(), owner.progress, nullptr))
                if (tempReverseFile.getFile().existsAsFile())
                    tempReverseFile.overwriteTargetFileWithTemporary();
    }

    if (! r.destFile.existsAsFile())
        return;

    if (r.category != ProjectItem::Category::none && r.destFile.existsAsFile())
    {
        CRASH_TRACER

        auto proj = ProjectManager::getInstance()->getProject (*r.edit);

        if (proj == nullptr)
        {
            jassertfalse;
            return;
        }

        if (proj->isReadOnly())
        {
            r.edit->engine.getUIBehaviour().showWarningMessage (TRANS("Couldn't add the new file to the project (because this project is read-only)"));
        }
        else
        {
            bool ok = true;

            if (! r.createMidiFile && errorMessage.isNotEmpty())
            {
                ok = false;
                r.destFile.deleteFile();
            }

            if (ok)
            {
                String newItemDesc;
                newItemDesc << TRANS("Rendered from edit") << r.edit->getName().quoted() << " " << TRANS("On") << " "
                            << Time::getCurrentTime().toString (true, true);

                if (auto item = proj->createNewItem (r.destFile,
                                                     r.createMidiFile ? ProjectItem::midiItemType()
                                                                      : ProjectItem::waveItemType(),
                                                     r.destFile.getFileNameWithoutExtension().trim(),
                                                     newItemDesc,
                                                     r.category,
                                                     true))
                {
                    jassert (item->getID().isValid());
                    owner.result.items.add (item);
                }
                else
                {
                    jassertfalse;
                }
            }
        }
    }

    // validates the AudioFile by giving it a sample rate etc.
    owner.engine.getAudioFileManager().checkFileForChangesAsync (AudioFile (r.destFile));
}

bool EditRenderJob::RenderPass::initialise()
{
    jassert (task == nullptr);
    jassert (r.sampleRateForAudio > 7000);

    callBlocking ([this]
                  {
                      Renderer::turnOffAllPlugins (*r.edit);
                      r.edit->initialiseAllPlugins();
                      r.edit->getTransport().stop (false, true);
                  });

    if (r.tracksToDo.countNumberOfSetBits() > 0
        && r.destFile.hasWriteAccess()
        && ! r.destFile.isDirectory())
    {
        AudioNode* node = nullptr;

        CreateAudioNodeParams cnp;
        cnp.allowedClips = r.allowedClips.isEmpty() ? nullptr : &r.allowedClips;
        cnp.allowedTracks = &r.tracksToDo;
        cnp.forRendering = true;
        cnp.includePlugins = r.usePlugins;
        cnp.addAntiDenormalisationNoise = r.addAntiDenormalisationNoise;

        callBlocking ([this, &node, &cnp]
        {
            node = createRenderingNodeFromEdit (*r.edit, cnp, r.useMasterPlugins);
        });

        if (node != nullptr)
        {
            task = new Renderer::RenderTask (desc, r, node, owner.progress, &owner.thumbnailToUpdate);
            return task->errorMessage.isEmpty();
        }
    }

    return false;
}

//==============================================================================
void EditRenderJob::renderSeparateTracks()
{
    // The logic here is fairly complicated but esseintially we want the following resulting tracks:
    // 1. Any top-level audio tracks
    // 2. Any top-level sub-mix folder tracks
    // 3. Any audio or sub-mix folder tracks contained in Folder tracks (which aren't sub-mix tracks)
    // 4. Only tracks that are contained in the tracksToDo mask

    auto originalTracksToDo = params.tracksToDo;

    for (int i = 0; i <= originalTracksToDo.getHighestBit(); ++i)
    {
        if (originalTracksToDo[i])
        {
            if (auto track = dynamic_cast<Track*> (getAllTracks (*params.edit)[i]))
            {
                if (track->isPartOfSubmix())
                    continue;

                auto ft = dynamic_cast<FolderTrack*> (track);
                auto at = dynamic_cast<AudioTrack*> (track);

                if (ft == nullptr && at == nullptr)
                    continue;

                juce::BigInteger tracksToDo;
                tracksToDo.setBit (i);

                if (ft != nullptr)
                {
                    if (! ft->isSubmixFolder())
                        continue;

                    for (auto* subTrack : ft->getAllSubTracks (true))
                    {
                        const int subTrackIndex = subTrack->getIndexInEditTrackList();

                        if (originalTracksToDo[subTrackIndex])
                            tracksToDo.setBit (subTrackIndex);
                    }
                }

                auto getDescription = [at, ft]
                {
                    return ft != nullptr ? TRANS("Rendering Submix Track") + " " + String (ft->getFolderTrackNumber()) + "..."
                                         : TRANS("Rendering Track") + " " + String (at->getAudioTrackNumber()) + "...";
                };

                auto file = proxy.getFile();
                auto trackFile = file.getSiblingFile (file.getFileNameWithoutExtension()
                                                       + " " + track->getName()
                                                       + " " + TRANS("Render") + " 1"
                                                       + file.getFileExtension());

                params.destFile = File (File::createLegalPathName (getNonExistentSiblingWithIncrementedNumberSuffix (trackFile, false).getFullPathName()));
                params.tracksToDo = tracksToDo;

                if (Renderer::checkTargetFile (track->edit.engine, params.destFile))
                    renderPasses.add (new RenderPass (*this, params, getDescription()));
            }
        }
    }

    params.tracksToDo = originalTracksToDo;
}

bool EditRenderJob::generateSilence (const File& fileToWriteTo)
{
    CRASH_TRACER

    std::unique_ptr<FileOutputStream> os (fileToWriteTo.createOutputStream());

    if (os == nullptr || params.audioFormat == nullptr)
        return false;

    const int numChans = params.mustRenderInMono ? 1 : 2;
    std::unique_ptr<AudioFormatWriter> writer (params.audioFormat->createWriterFor (os.get(), params.sampleRateForAudio,
                                                                                    (unsigned int) numChans,
                                                                                    params.bitDepth, {}, 0));

    if (writer == nullptr)
        return false;

    os.release();
    auto numToDo = (int64) (params.time.getLength() * params.sampleRateForAudio);
    auto blockSize = jmin (32758, (int) numToDo);
    int64 numDone = 0;

    // should probably use an AudioScratchBuffer here
    juce::AudioBuffer<float> buffer (numChans, blockSize);
    buffer.clear();

    while (numDone < numToDo)
    {
        if (shouldExit())
            return false;

        auto numThisTime = jmin ((int) numToDo, blockSize);
        writer->writeFromAudioSampleBuffer (buffer, 0, numThisTime);

        progress = (float) (numDone / (float) numToDo);
        thumbnailToUpdate.addBlock (numDone, buffer, 0, numThisTime);

        numDone += numThisTime;
    }

    return true;
}
