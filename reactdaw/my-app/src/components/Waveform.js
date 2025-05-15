import React, { useEffect, useRef, useState } from "react";
import WaveSurfer from "wavesurfer.js";

export default function Waveform({ url }) {
  const containerRef = useRef(null);
  const wavesurferRef = useRef(null);
  const abortControllerRef = useRef(null);
  const [ready, setReady] = useState(false);

  useEffect(() => {
    if (!url || !containerRef.current) return;

    // Abort any previous loading process
    abortControllerRef.current?.abort();
    abortControllerRef.current = new AbortController();

    const wavesurfer = WaveSurfer.create({
      container: containerRef.current,
      waveColor: "#3fa9f5",
      progressColor: "#3fa9f5",
      height: 40,
      interact: false,
      responsive: true,
      cursorWidth: 0,
    });

    wavesurfer.load(url, null, abortControllerRef.current.signal);

    wavesurfer.on("ready", () => setReady(true));
    wavesurfer.on("error", (e) => {
      console.warn("WaveSurfer load error:", e);
    });

    wavesurferRef.current = wavesurfer;

    return () => {
      abortControllerRef.current?.abort(); // actively cancel fetch
      try {
        wavesurferRef.current?.destroy();
      } catch (e) {
        console.warn("WaveSurfer destroy error (expected during cleanup):", e);
      }
    };
  }, [url]);

  return <div ref={containerRef} style={{ opacity: ready ? 1 : 0.5 }} />;
}
