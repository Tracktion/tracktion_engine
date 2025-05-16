import React, { useState, useEffect } from "react";
import TransportControls from "./components/TransportControls";
import Timeline from "./components/Timeline";
import "./App.css";
import * as Tone from "tone";
import TrackList from "./components/TrackList";

function App() {
  const [isPlaying, setIsPlaying] = useState(false);
  const [tracks, setTracks] = useState([]);
  const [selectedTrackId, setSelectedTrackId] = useState(null);
  const [isRecording, setIsRecording] = useState(false);
  const [playheadPosition, setPlayheadPosition] = useState(0);

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

  // Use space bar on keyboard to control play/pause
  useEffect(() => {
    const handleKeyDown = (e) => {
      if (e.code === "Space") {
        e.preventDefault(); // prevent page scroll
        setIsPlaying((prev) => {
          const newPlay = !prev;
          if (newPlay) {
            Tone.Transport.start();
          } else {
            Tone.Transport.pause();
          }
          return newPlay;
        });
      }
    };

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, []);

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
                i === clipIndex
                  ? { ...clip, start: Math.max(0, newStart) }
                  : clip
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
      volume: 1,
      muted: false,
      instrument: "voice",
      gainNode: new Tone.Gain(1).toDestination(),
    };
    setTracks([...tracks, newTrack]);
  };

  const updateTrackVolume = (id, volume) => {
    setTracks((prev) =>
      prev.map((t) => {
        if (t.id !== id) return t;
        if (t.gainNode) t.gainNode.gain.value = t.muted ? 0 : volume;
        return { ...t, volume };
      })
    );
  };

  const toggleMuteTrack = (id) => {
    setTracks((prev) =>
      prev.map((t) => {
        if (t.id !== id) return t;
        const newMuted = !t.muted;
        if (t.gainNode) t.gainNode.gain.value = newMuted ? 0 : t.volume;
        return { ...t, muted: !t.muted };
      })
    );
  };

  const deleteSelectedTrack = () => {
    if (!selectedTrackId) return;
    setTracks((prev) => prev.filter((t) => t.id !== selectedTrackId));
    setSelectedTrackId(null);
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
        const track = tracks.find((t) => t.id === selectedTrackId);
        if (!track) return;
        player.connect(track.gainNode);
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
        <button
          onClick={deleteSelectedTrack}
          disabled={!selectedTrackId}
          style={{ marginLeft: "8px" }}
        >
          Delete Track
        </button>
        <button onClick={() => onScrubPlayhead(0)}>Reset Playhead</button>

        <br></br>
        <label>Playhead:</label>
        <input
          type="range"
          min={0}
          max={60}
          step={0.1}
          value={playheadPosition}
          onChange={(e) => onScrubPlayhead(parseFloat(e.target.value))}
          disabled={isPlaying}
        />
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
        onVolumeChange={updateTrackVolume} // ✅ added here
        onToggleMute={toggleMuteTrack} // ✅ added here
      />
    </div>
  );
}

export default App;
