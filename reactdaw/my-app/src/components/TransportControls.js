import React from "react";
import * as Tone from "tone";

export default function TransportControls({ isPlaying, setIsPlaying }) {
    const handlePlayPause = async () => {
        await Tone.start();
        if (Tone.Transport.state === "started") {
          Tone.Transport.pause();
          setIsPlaying(false);
        } else {
          Tone.Transport.stop();  // 🔁 reset to beginning
          Tone.Transport.start(); // ▶️ play all synced clips
          setIsPlaying(true);
        }
      };
      

  return <button onClick={handlePlayPause}>{isPlaying ? "Pause" : "Play"}</button>;
}