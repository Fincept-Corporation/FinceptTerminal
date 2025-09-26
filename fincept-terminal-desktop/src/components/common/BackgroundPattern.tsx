// File: src/components/common/BackgroundPattern.tsx
// Geometric crosshatch background pattern component

import React from 'react';

const BackgroundPattern: React.FC = () => {
  return (
    <div
      className="fixed inset-0 w-full h-full"
      style={{
        top: '-10vh',
        left: '-10vw',
        right: '-10vw',
        bottom: '-10vh',
        width: '120vw',
        height: '120vh'
      }}
    >
      <div className="w-full h-full bg-black">
        <svg
          className="w-full h-full"
          viewBox="0 0 1600 1200"
          preserveAspectRatio="none"
        >
          <defs>
            <pattern
              id="geometric-grid"
              x="0"
              y="0"
              width="60"
              height="60"
              patternUnits="userSpaceOnUse"
            >
              <line
                x1="0"
                y1="0"
                x2="60"
                y2="60"
                stroke="#333333"
                strokeWidth="0.8"
                opacity="0.8"
              />
              <line
                x1="60"
                y1="0"
                x2="0"
                y2="60"
                stroke="#333333"
                strokeWidth="0.8"
                opacity="0.8"
              />
            </pattern>
          </defs>
          <rect width="100%" height="100%" fill="url(#geometric-grid)" opacity="0.6" />
        </svg>
      </div>
    </div>
  );
};

export default BackgroundPattern;