import { FINCEPT } from '../finceptStyles';

/** Background tint based on value magnitude — green for positive, red for negative */
export const heatColor = (val: number, max: number = 15): string => {
  const norm = Math.min(Math.abs(val) / max, 1);
  if (val > 0) return `rgba(0,214,111,${0.08 + norm * 0.25})`;
  if (val < 0) return `rgba(255,59,59,${0.08 + norm * 0.25})`;
  return 'transparent';
};

/** Text color for positive/negative/zero values */
export const valColor = (v: number): string =>
  v > 0 ? FINCEPT.GREEN : v < 0 ? FINCEPT.RED : FINCEPT.GRAY;
