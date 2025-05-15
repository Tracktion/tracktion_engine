import React from "react";
import Track from "./Track";

export default function TrackList({ tracks }) {
  return (
    <div>
      {tracks.map((track) => (
        <Track key={track.id} track={track} />
      ))}
    </div>
  );
}
