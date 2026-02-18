/**
 * Trigger Nodes Index
 * All workflow trigger entry points
 */

export * from './ManualTriggerNode';
export * from './ScheduleTriggerNode';
export * from './PriceAlertTriggerNode';
export * from './NewsEventTriggerNode';
export * from './MarketEventTriggerNode';

import { ManualTriggerNode } from './ManualTriggerNode';
import { ScheduleTriggerNode } from './ScheduleTriggerNode';
import { PriceAlertTriggerNode } from './PriceAlertTriggerNode';
import { NewsEventTriggerNode } from './NewsEventTriggerNode';
import { MarketEventTriggerNode } from './MarketEventTriggerNode';

export const TriggerNodes = {
  ManualTriggerNode,
  ScheduleTriggerNode,
  PriceAlertTriggerNode,
  NewsEventTriggerNode,
  MarketEventTriggerNode,
};

export const TriggerNodeTypes = [
  'manualTrigger',
  'scheduleTrigger',
  'priceAlertTrigger',
  'newsEventTrigger',
  'marketEventTrigger',
] as const;

export type TriggerNodeType = typeof TriggerNodeTypes[number];
