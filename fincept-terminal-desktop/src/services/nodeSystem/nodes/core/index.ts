/**
 * Core Nodes Index
 * Basic workflow utility nodes
 */

export * from './CodeNode';
export * from './SetNode';

import { CodeNode } from './CodeNode';
import { SetNode } from './SetNode';

export const CoreNodes = {
  CodeNode,
  SetNode,
};

export const CoreNodeTypes = [
  'code',
  'set',
] as const;

export type CoreNodeType = typeof CoreNodeTypes[number];
