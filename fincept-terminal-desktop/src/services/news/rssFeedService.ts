// RSS Feed Management Service
// Allows users to add, remove, and configure RSS news sources
// Includes automatic cache invalidation when feeds are modified

import { invoke } from '@tauri-apps/api/core';
import { invalidateNewsCache } from './newsService';

// ============================================================================
// Types
// ============================================================================

export interface RSSFeed {
  id: string | null;
  name: string;
  url: string;
  category: string;
  region: string;
  source: string;
  enabled: boolean;
  is_default: boolean;
}

export interface UserRSSFeed {
  id: string;
  name: string;
  url: string;
  category: string;
  region: string;
  source_name: string;
  enabled: boolean;
  is_default: boolean;
  created_at: string;
  updated_at: string;
}

export interface AddFeedParams {
  name: string;
  url: string;
  category: string;
  region: string;
  source_name: string;
}

// ============================================================================
// Available Categories and Regions
// ============================================================================

export const RSS_CATEGORIES = [
  'MARKETS',
  'CRYPTO',
  'TECH',
  'ENERGY',
  'REGULATORY',
  'ECONOMIC',
  'EARNINGS',
  'BANKING',
  'GEOPOLITICS',
  'GENERAL',
] as const;

export const RSS_REGIONS = [
  'GLOBAL',
  'US',
  'EU',
  'ASIA',
  'UK',
  'INDIA',
  'CHINA',
  'LATAM',
] as const;

export type RSSCategory = (typeof RSS_CATEGORIES)[number];
export type RSSRegion = (typeof RSS_REGIONS)[number];

// ============================================================================
// Service Functions
// ============================================================================

/**
 * Get all RSS feeds (default + user-configured)
 */
export async function getAllRSSFeeds(): Promise<RSSFeed[]> {
  try {
    return await invoke<RSSFeed[]>('get_all_rss_feeds');
  } catch (error) {
    console.error('[RSSFeedService] Failed to get all feeds:', error);
    return [];
  }
}

/**
 * Get default RSS feeds (built-in)
 */
export async function getDefaultFeeds(): Promise<RSSFeed[]> {
  try {
    return await invoke<RSSFeed[]>('get_default_feeds');
  } catch (error) {
    console.error('[RSSFeedService] Failed to get default feeds:', error);
    return [];
  }
}

/**
 * Get user-configured RSS feeds
 */
export async function getUserRSSFeeds(): Promise<UserRSSFeed[]> {
  try {
    return await invoke<UserRSSFeed[]>('get_user_rss_feeds');
  } catch (error) {
    console.error('[RSSFeedService] Failed to get user feeds:', error);
    return [];
  }
}

/**
 * Add a new user RSS feed
 * Invalidates news cache after adding
 */
export async function addUserRSSFeed(params: AddFeedParams): Promise<UserRSSFeed> {
  const result = await invoke<UserRSSFeed>('add_user_rss_feed', {
    name: params.name,
    url: params.url,
    category: params.category,
    region: params.region,
    sourceName: params.source_name,
  });
  // Invalidate cache since feed list changed
  await invalidateNewsCache();
  return result;
}

/**
 * Update an existing user RSS feed
 * Invalidates news cache after updating
 */
export async function updateUserRSSFeed(
  id: string,
  params: AddFeedParams & { enabled: boolean }
): Promise<void> {
  await invoke('update_user_rss_feed', {
    id,
    name: params.name,
    url: params.url,
    category: params.category,
    region: params.region,
    sourceName: params.source_name,
    enabled: params.enabled,
  });
  // Invalidate cache since feed configuration changed
  await invalidateNewsCache();
}

/**
 * Delete a user RSS feed
 * Invalidates news cache after deleting
 */
export async function deleteUserRSSFeed(id: string): Promise<void> {
  await invoke('delete_user_rss_feed', { id });
  // Invalidate cache since feed was removed
  await invalidateNewsCache();
}

/**
 * Toggle enabled status of a user RSS feed
 * Invalidates news cache after toggling
 */
export async function toggleUserRSSFeed(id: string, enabled: boolean): Promise<void> {
  await invoke('toggle_user_rss_feed', { id, enabled });
  // Invalidate cache since feed enabled status changed
  await invalidateNewsCache();
}

/**
 * Test if an RSS feed URL is valid and accessible
 */
export async function testRSSFeedUrl(url: string): Promise<boolean> {
  try {
    const result = await invoke<{ valid: boolean; error?: string }>('test_rss_feed_url', { url });
    if (!result.valid && result.error) {
      console.warn('[RSSFeedService] Feed test failed:', result.error);
    }
    return result.valid;
  } catch (error) {
    console.error('[RSSFeedService] Failed to test feed URL:', error);
    return false;
  }
}

/**
 * Toggle enabled status of a default RSS feed
 * Invalidates news cache after toggling
 */
export async function toggleDefaultRSSFeed(feedId: string, enabled: boolean): Promise<void> {
  await invoke('toggle_default_rss_feed', { feedId, enabled });
  // Invalidate cache since default feed enabled status changed
  await invalidateNewsCache();
}

/**
 * Get default feed preferences
 */
export async function getDefaultFeedPrefs(): Promise<Record<string, boolean>> {
  try {
    return await invoke<Record<string, boolean>>('get_default_feed_prefs');
  } catch (error) {
    console.error('[RSSFeedService] Failed to get default feed prefs:', error);
    return {};
  }
}

/**
 * Delete (hide) a default RSS feed
 * Invalidates news cache after deleting
 */
export async function deleteDefaultRSSFeed(feedId: string): Promise<void> {
  await invoke('delete_default_rss_feed', { feedId });
  // Invalidate cache since default feed was hidden
  await invalidateNewsCache();
}

/**
 * Restore a deleted default RSS feed
 * Invalidates news cache after restoring
 */
export async function restoreDefaultRSSFeed(feedId: string): Promise<void> {
  await invoke('restore_default_rss_feed', { feedId });
  // Invalidate cache since default feed was restored
  await invalidateNewsCache();
}

/**
 * Get list of deleted default feed IDs
 */
export async function getDeletedDefaultFeeds(): Promise<string[]> {
  try {
    return await invoke<string[]>('get_deleted_default_feeds');
  } catch (error) {
    console.error('[RSSFeedService] Failed to get deleted feeds:', error);
    return [];
  }
}

/**
 * Restore all deleted default feeds
 * Invalidates news cache after restoring
 */
export async function restoreAllDefaultFeeds(): Promise<void> {
  await invoke('restore_all_default_feeds');
  // Invalidate cache since all default feeds were restored
  await invalidateNewsCache();
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get category label for display
 */
export function getCategoryLabel(category: string): string {
  const labels: Record<string, string> = {
    MARKETS: 'Markets',
    CRYPTO: 'Crypto',
    TECH: 'Technology',
    ENERGY: 'Energy',
    REGULATORY: 'Regulatory',
    ECONOMIC: 'Economic',
    EARNINGS: 'Earnings',
    BANKING: 'Banking',
    GEOPOLITICS: 'Geopolitics',
    GENERAL: 'General',
  };
  return labels[category] || category;
}

/**
 * Get region label for display
 */
export function getRegionLabel(region: string): string {
  const labels: Record<string, string> = {
    GLOBAL: 'Global',
    US: 'United States',
    EU: 'Europe',
    ASIA: 'Asia',
    UK: 'United Kingdom',
    INDIA: 'India',
    CHINA: 'China',
    LATAM: 'Latin America',
  };
  return labels[region] || region;
}

/**
 * Validate RSS feed URL format
 */
export function isValidRSSUrl(url: string): boolean {
  try {
    const parsed = new URL(url);
    return parsed.protocol === 'http:' || parsed.protocol === 'https:';
  } catch {
    return false;
  }
}

export default {
  getAllRSSFeeds,
  getDefaultFeeds,
  getUserRSSFeeds,
  addUserRSSFeed,
  updateUserRSSFeed,
  deleteUserRSSFeed,
  toggleUserRSSFeed,
  testRSSFeedUrl,
  toggleDefaultRSSFeed,
  getDefaultFeedPrefs,
  deleteDefaultRSSFeed,
  restoreDefaultRSSFeed,
  getDeletedDefaultFeeds,
  restoreAllDefaultFeeds,
  getCategoryLabel,
  getRegionLabel,
  isValidRSSUrl,
  RSS_CATEGORIES,
  RSS_REGIONS,
};
