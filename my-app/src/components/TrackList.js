import React from "react";
import Track from "./Track";

export default function TrackList({
  tracks,
  updateTrackClip,
  selectedTrackId,
  onSelectTrack,
  onVolumeChange,
  onToggleMute,
}) {
  return (
    <div>
      {tracks.map((track) => (
        <Track
          key={track.id}
          track={track}
          updateTrackClip={updateTrackClip}
          isSelected={track.id === selectedTrackId}
          onSelect={() => onSelectTrack(track.id)}
        />
      ))}
    </div>
  );
}
