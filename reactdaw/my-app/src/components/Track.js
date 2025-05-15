import React, { useState, useEffect } from "react";
import * as Tone from "tone";

export default function Track({ track }) {
  const [isRecording, setIsRecording] = useState(false);
  const [media, setMedia] = useState(null);
  const [recorder, setRecorder] = useState(null);
  const [url, setUrl] = useState(null);

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
    setUrl(url);
  
    const audio = new Audio(url);
    audio.onloadedmetadata = () => {
      const duration = audio.duration;
  
      // Store clip data on the track
      track.clip = {
        url,
        start: 0, // default start time
        duration
      };
  
      const player = new Tone.Player(url).toDestination();
      track.player = player;
    };
  
    media.disconnect();
    setIsRecording(false);
  };
  

  const play = () => {
    if (track.player) {
      track.player.start();
    }
  };

  return (
    <div style={{ border: "1px solid white", margin: 10, padding: 10 }}>
      <p>Track #{track.id}</p>
      <button onClick={play}>Play Clip</button>
      {!isRecording ? (
        <button onClick={startRecording}>Record</button>
      ) : (
        <button onClick={stopRecording}>Stop</button>
      )}
      {url && <audio controls src={url} />}
    </div>
  );
}
