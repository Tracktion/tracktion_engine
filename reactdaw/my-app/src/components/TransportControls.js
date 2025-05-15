import React from "react";
import * as Tone from "tone";

export default function TransportControls({ isPlaying, setIsPlaying }) {
  const handlePlayPause = async () => {
    await Tone.start();
    if (Tone.Transport.state === "started") {
      Tone.Transport.pause();
      setIsPlaying(false);
    } else {
      Tone.Transport.start();
      setIsPlaying(true);
    }
  };

  return (
    <div>
      <button onClick={handlePlayPause}>
        {isPlaying ? "Pause" : "Play"}
      </button>
    </div>
  );
}
