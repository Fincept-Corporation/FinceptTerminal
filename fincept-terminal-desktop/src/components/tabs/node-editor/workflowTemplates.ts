/**
 * Predefined Workflow Templates
 *
 * Ready-to-use workflow templates that users can import to quickly
 * understand how the node editor works and start building their own workflows.
 *
 * Templates are split by difficulty category:
 *  - templates/beginnerTemplates.ts      — hello world, stock quotes, data transform
 *  - templates/intermediateTemplates.ts  — technical analysis, backtesting, AI analysis
 *  - templates/advancedTemplates.ts      — portfolio risk, optimization, complete workflow
 */

import { Node, Edge } from 'reactflow';
import { COMPLETE_WORKFLOW_ALL_82_NODES } from './workflowTemplates-COMPLETE-ALL-82-NODES';
import { BEGINNER_TEMPLATES } from './templates/beginnerTemplates';
import { INTERMEDIATE_TEMPLATES } from './templates/intermediateTemplates';
import { ADVANCED_TEMPLATES } from './templates/advancedTemplates';

export interface WorkflowTemplate {
  id: string;
  name: string;
  description: string;
  category: 'beginner' | 'intermediate' | 'advanced';
  tags: string[];
  nodes: Node[];
  edges: Edge[];
}

// ─── Export All Templates ───────────────────────────────────────────────────
export const WORKFLOW_TEMPLATES: WorkflowTemplate[] = [
  ...BEGINNER_TEMPLATES,
  ...INTERMEDIATE_TEMPLATES,
  ...ADVANCED_TEMPLATES,
  // NEW: Comprehensive 82-node institutional trading system
  COMPLETE_WORKFLOW_ALL_82_NODES,
];
