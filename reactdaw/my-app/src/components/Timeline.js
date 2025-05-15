import React from "react";
import "./Timeline.css";

export default function Timeline({ tracks = [], numBeats = 16 }) {
  return (
    <div className="timeline-container">
      <div className="timeline-grid">
        {[...Array(numBeats)].map((_, i) => (
          <div className="beat-marker" key={i}>
            {i + 1}
          </div>
        ))}
      </div>
      {tracks.map((track) => (
        <div className="track-lane" key={track.id}>
          {track.clip && (
            <div
              className="clip"
              style={{
                left: `${(track.clip.start / numBeats) * 100}%`,
                width: `${(track.clip.duration / numBeats) * 100}%`,
                position: "absolute",
                backgroundColor: "#3fa9f5",
              }}
              title={`Track ${track.id}`}
            />
          )}
        </div>
      ))}
    </div>
  );
}