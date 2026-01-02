/**
 * Trigger Nodes Index
 * All workflow trigger entry points
 */

export * from './ManualTriggerNode';
export * from './ScheduleTriggerNode';
export * from './PriceAlertTriggerNode';
export * from './NewsEventTriggerNode';
export * from './MarketEventTriggerNode';
export * from './WebhookTriggerNode';

// Re-export for convenience
import { ManualTriggerNode } from './ManualTriggerNode';
import { ScheduleTriggerNode } from './ScheduleTriggerNode';
import { PriceAlertTriggerNode } from './PriceAlertTriggerNode';
import { NewsEventTriggerNode } from './NewsEventTriggerNode';
import { MarketEventTriggerNode } from './MarketEventTriggerNode';
import { WebhookTriggerNode } from './WebhookTriggerNode';

export const TriggerNodes = {
  ManualTriggerNode,
  ScheduleTriggerNode,
  PriceAlertTriggerNode,
  NewsEventTriggerNode,
  MarketEventTriggerNode,
  WebhookTriggerNode,
};

export const TriggerNodeTypes = [
  'manualTrigger',
  'scheduleTrigger',
  'priceAlertTrigger',
  'newsEventTrigger',
  'marketEventTrigger',
  'webhookTrigger',
] as const;

export type TriggerNodeType = typeof TriggerNodeTypes[number];
