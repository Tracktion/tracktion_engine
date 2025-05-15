import React, { useState, useEffect } from "react";
import * as Tone from "tone";

export default function Track({ track, updateTrackClip }) {
  const [isRecording, setIsRecording] = useState(false);
  const [media, setMedia] = useState(null);
  const [recorder, setRecorder] = useState(null);

  const startRecording = async () => {
    await Tone.start();
    const mic = new Tone.UserMedia();
    await mic.open();
    setMedia(mic);

    const rec = new Tone.Recorder();
    mic.connect(rec);
    rec.start();
    setRecorder(rec);
    setIsRecording(true);
  };

  const stopRecording = async () => {
    const recording = await recorder.stop();
    const blob = new Blob([recording], { type: "audio/wav" });
    const url = URL.createObjectURL(blob);
  
    const player = new Tone.Player({
      url,
      autostart: false,
      onload: () => {
        const duration = player.buffer.duration;
  
        // Sync and connect
        player.toDestination();
        player.sync().start(0); // start at beginning
        track.player = player;
  
        // Save clip info in parent state
        const clip = {
          url,
          start: 0,
          duration,
        };
        updateTrackClip(track.id, clip);
      },
      onerror: (err) => console.error("Player load error", err),
    });
  
    media.disconnect();
    setIsRecording(false);
  };

  return (
    <div style={{ border: "1px solid white", margin: 10, padding: 10 }}>
      <p>Track #{track.id}</p>
      {!isRecording ? (
        <button onClick={startRecording}>Record</button>
      ) : (
        <button onClick={stopRecording}>Stop</button>
      )}
      {track.clip && <p>Clip duration: {track.clip.duration.toFixed(2)}s</p>}
    </div>
  );
}