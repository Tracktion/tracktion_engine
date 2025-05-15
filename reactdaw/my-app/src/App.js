import React, { useState } from "react";
import TransportControls from "./components/TransportControls";
import TrackList from "./components/TrackList";
import Timeline from "./components/Timeline";
import "./App.css";

function App() {
  const [isPlaying, setIsPlaying] = useState(false);
  const [tracks, setTracks] = useState([]);

  const addTrack = () => {
    const newTrack = {
      id: Date.now(),
      player: null,
      clip: null, // { url, start, duration }
    };
    setTracks([...tracks, newTrack]);
  };

  const updateTrackClip = (id, clip) => {
    setTracks((prev) =>
      prev.map((t) => (t.id === id ? { ...t, clip } : t))
    );
  };

  return (
    <div className="App">
      <h1>🎛 Simple React DAW</h1>
      <TransportControls isPlaying={isPlaying} setIsPlaying={setIsPlaying} />
      <button onClick={addTrack}>Add Track</button>
      <Timeline tracks={tracks} numBeats={16} />
      <TrackList tracks={tracks} updateTrackClip={updateTrackClip} />
    </div>
  );
}

export default App;