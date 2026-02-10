/**
 * ResultRaw - JSON display for raw result data
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY } from '../constants';

interface ResultRawProps {
  result: any;
}

export const ResultRaw: React.FC<ResultRawProps> = ({ result }) => (
  <pre style={{
    fontSize: '8px',
    color: FINCEPT.WHITE,
    fontFamily: FONT_FAMILY,
    whiteSpace: 'pre-wrap',
    wordBreak: 'break-word',
    lineHeight: 1.4,
  }}>
    {JSON.stringify(result, null, 2)}
  </pre>
);
