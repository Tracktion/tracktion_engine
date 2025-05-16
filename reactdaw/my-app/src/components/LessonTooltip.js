import React from "react";
import "./LessonTooltip.css";


export default function LessonTooltip({ title, text, onContinue }) {
  return (
    <div className="lesson-tooltip">
      <h3 className="lesson-tooltip-title">{title}</h3>
      <p className="lesson-tooltip-text">{text}</p>
      <button className="lesson-tooltip-button" onClick={onContinue}>
        Continue
      </button>
    </div>
  );
}

