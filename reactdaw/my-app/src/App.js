import React, { useState, useEffect } from "react";
import TransportControls from "./components/TransportControls";
import Timeline from "./components/Timeline";
import "./App.css";
import * as Tone from "tone";
import LessonTooltip from "./components/LessonTooltip.js";
import lessons from "./Lessons";

function App() {
  const [isPlaying, setIsPlaying] = useState(false);
  const [tracks, setTracks] = useState([]);
  const [selectedTrackId, setSelectedTrackId] = useState(null);
  const [isRecording, setIsRecording] = useState(false);
  const [playheadPosition, setPlayheadPosition] = useState(0);


  // Lesson index tracking
  const [lessonIndex, setLessonIndex] = useState(0);
  const [stepIndex, setStepIndex] = useState(0);

  const currentLesson = lessons[lessonIndex]
  const currentStep = currentLesson?.steps[stepIndex];

  const handleContinue = () => {
  if (stepIndex < currentLesson.steps.length - 1) {
    setStepIndex(stepIndex + 1);
  } else {
    // End of current lesson
    setLessonIndex(lessonIndex + 1); 
    setStepIndex(0);
  }
  };


  useEffect(() => {
    let id;
    const update = () => {
      setPlayheadPosition(Tone.Transport.seconds);
      id = requestAnimationFrame(update);
    };

    if (isPlaying) {
      id = requestAnimationFrame(update);
    }

    return () => cancelAnimationFrame(id);
  }, [isPlaying]);

  const onScrubPlayhead = (positionInSeconds) => {
    Tone.Transport.seconds = positionInSeconds;
    setPlayheadPosition(positionInSeconds);
  };

  const onMoveClip = (trackId, clipIndex, newStart) => {
    setTracks((prev) =>
      prev.map((t) =>
        t.id === trackId
          ? {
              ...t,
              clips: t.clips.map((clip, i) =>
                i === clipIndex ? { ...clip, start: Math.max(0, newStart) } : clip
              ),
            }
          : t
      )
    );
  };

  const onDeleteClip = (trackId, clipIndex) => {
    setTracks((prev) =>
      prev.map((t) =>
        t.id === trackId
          ? {
              ...t,
              clips: t.clips.filter((_, i) => i !== clipIndex),
            }
          : t
      )
    );
  };

  const onSetClipVolume = (trackId, clipIndex, volume) => {
    setTracks((prev) =>
      prev.map((t) =>
        t.id === trackId
          ? {
              ...t,
              clips: t.clips.map((clip, i) =>
                i === clipIndex ? { ...clip, volume } : clip
              ),
            }
          : t
      )
    );
  };

  const addTrack = () => {
    const newTrack = {
      id: Date.now(),
      clips: [],
    };
    setTracks([...tracks, newTrack]);
  };

  const updateTrackClip = (id, clip) => {
    setTracks((prev) =>
      prev.map((t) =>
        t.id === id ? { ...t, clips: [...(t.clips || []), clip] } : t
      )
    );
  };

  const handleTrackSelect = (id) => {
    setSelectedTrackId(id);
  };

  const startRecording = async () => {
    if (!selectedTrackId) return;
    await Tone.start();
    const mic = new Tone.UserMedia();
    await mic.open();

    const rec = new Tone.Recorder();
    mic.connect(rec);
    rec.start();

    setIsRecording({ mic, rec, startTime: Tone.Transport.seconds });
  };

  const stopRecording = async () => {
    if (!isRecording || !selectedTrackId) return;
    const recording = await isRecording.rec.stop();
    const blob = new Blob([recording], { type: "audio/wav" });
    const url = URL.createObjectURL(blob);

    const clip = {
      url,
      start: isRecording.startTime,
      duration: 0,
      volume: 1,
    };

    const player = new Tone.Player({
      url,
      autostart: false,
      onload: () => {
        clip.duration = player.buffer.duration;
        const gain = new Tone.Gain(clip.volume).toDestination();
        player.connect(gain);
        player.sync().start(clip.start);
        updateTrackClip(selectedTrackId, clip);
        setIsRecording(false);
      },
    });

    isRecording.mic.disconnect();
  };

  return (
    <div className="App">
      <h1>🎛 Simple React DAW</h1>
      <TransportControls isPlaying={isPlaying} setIsPlaying={setIsPlaying} />
      <button onClick={addTrack}>Add Track</button>

      <div>
        <button
          onClick={isRecording ? stopRecording : startRecording}
          disabled={!selectedTrackId}
        >
          {isRecording ? "Stop Recording" : "Record"}
        </button>
      </div>

      <Timeline
        tracks={tracks}
        numBeats={16}
        selectedTrackId={selectedTrackId}
        onSelectTrack={handleTrackSelect}
        playheadPosition={playheadPosition}
        onMoveClip={onMoveClip}
        onDeleteClip={onDeleteClip}
        onSetClipVolume={onSetClipVolume}
        onScrubPlayhead={onScrubPlayhead}
      />

       {typeof stepIndex === "number" && (
        <LessonTooltip
          title={currentStep.title}
          text={currentStep.text}
          position={currentStep.position}
          visible={true}
          onContinue={handleContinue}
        />
      )}

    </div>
  );
}

export default App;
