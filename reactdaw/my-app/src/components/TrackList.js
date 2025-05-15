import React from "react";
import Track from "./Track";

export default function TrackList({ tracks, updateTrackClip }) {
  return (
    <div>
      {tracks.map((track) => (
        <Track key={track.id} track={track} updateTrackClip={updateTrackClip} />
      ))}
    </div>
  );
}