import React from "react";
import "./Timeline.css";

export default function Timeline({
  tracks,
  numBeats,
  selectedTrackId,
  onSelectTrack,
  playheadPosition,
  onMoveClip,
  onDeleteClip,
  onSetClipVolume,
  onScrubPlayhead,
}) {
  return (
    <div className="timeline-container">
      {/* Scrub to move playhead */}
      <div
        className="timeline-grid"
        onClick={(e) => {
          const bounds = e.currentTarget.getBoundingClientRect();
          const clickX = e.clientX - bounds.left;
          const percent = clickX / bounds.width;
          const newPosition = percent * numBeats;
          onScrubPlayhead(newPosition);
        }}
      >
        {[...Array(numBeats)].map((_, i) => (
          <div className="beat-marker" key={i}>
            {i + 1}
          </div>
        ))}
      </div>

      {/* Playhead */}
      <div
        className="playhead"
        style={{
          left: `${(playheadPosition / numBeats) * 100}%`,
          height: `${tracks.length * 40}px`,
          position: "absolute",
          width: "2px",
          backgroundColor: "red",
          zIndex: 10,
        }}
      ></div>

      {/* Track lanes */}
      {tracks.map((track) => (
        <div
          className="track-lane"
          key={track.id}
          onClick={(e) => {
            e.stopPropagation();
            onSelectTrack(track.id);
          }}
          onDragOver={(e) => e.preventDefault()}
          onDrop={(e) => {
            const draggedTrackId = parseInt(e.dataTransfer.getData("trackId"));
            const draggedClipIndex = parseInt(e.dataTransfer.getData("clipIndex"));
            const bounds = e.currentTarget.getBoundingClientRect();
            const dropX = e.clientX - bounds.left;
            const percent = dropX / bounds.width;
            const newStart = percent * numBeats;

            onMoveClip(draggedTrackId, draggedClipIndex, newStart);
          }}
          style={{
            border: track.id === selectedTrackId ? "2px solid #3fa9f5" : "1px solid white",
            backgroundColor: track.id === selectedTrackId ? "#1a2a3a" : "#2a2a2a",
            position: "relative",
          }}
        >
          {track.clips.map((clip, clipIndex) => (
            <div
              key={clipIndex}
              className="clip"
              draggable
              onDragStart={(e) => {
                e.dataTransfer.setData("trackId", track.id);
                e.dataTransfer.setData("clipIndex", clipIndex);
              }}
              style={{
                left: `${(clip.start / numBeats) * 100}%`,
                width: `${(clip.duration / numBeats) * 100}%`,
                position: "absolute",
                backgroundColor: "#3fa9f5",
                cursor: "grab",
              }}
              title={`Start: ${clip.start.toFixed(2)}s`}
            />
          ))}
        </div>
      ))}
    </div>
  );
}
