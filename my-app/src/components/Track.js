import React from "react";

export default function Track({
  track,
  updateTrackClip,
  isSelected,
  onSelect,
}) {
  return (
    <div
      onClick={onSelect}
      style={{
        border: isSelected ? "2px solid #3fa9f5" : "1px solid white",
        margin: 10,
        padding: 10,
        cursor: "pointer",
        backgroundColor: isSelected ? "#1a2a3a" : "#222",
        color: "#ccc",
        display: "flex",
        flexDirection: "column",
        gap: "4px",
      }}
    >
      {track.clip && <p>🎧 Clip duration: {track.clip.duration.toFixed(2)}s</p>}
    </div>
  );
}
