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
      clip: null, // holds { url, start, duration }
    };

    setTracks([...tracks, newTrack]);
  };

  return (
    <div className="App">
      <h1>🎛 Simple React DAW</h1>

      <TransportControls isPlaying={isPlaying} setIsPlaying={setIsPlaying} />

      <button onClick={addTrack}>Add Track</button>

      <Timeline tracks={tracks} numBeats={16} />

      <TrackList tracks={tracks} />
    </div>
  );
}

export default App;
