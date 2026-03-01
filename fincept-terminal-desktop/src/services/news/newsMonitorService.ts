// News Monitor Service
// SQLite-backed keyword alert monitors for the News Tab.
// All persistence is via Tauri invoke (not localStorage).

import { invoke } from '@tauri-apps/api/core';
import { NewsArticle } from './newsService';

// ─── Types ────────────────────────────────────────────────────────────────────

export interface NewsMonitor {
  id: string;
  label: string;
  keywords: string[];
  color: string;
  enabled: boolean;
  created_at: string;
  updated_at: string;
}

// ─── Color palette (10 colors, cycles when adding monitors) ──────────────────

export const MONITOR_COLORS: string[] = [
  '#00E5FF', // cyan
  '#00FF88', // green
  '#FF6B35', // orange
  '#FFD700', // gold
  '#9D4EDD', // purple
  '#FF4D6D', // red-pink
  '#4CC9F0', // sky blue
  '#F72585', // hot pink
  '#7FFF00', // chartreuse
  '#FF9500', // amber
];

// ─── Helpers ──────────────────────────────────────────────────────────────────

function escapeRegex(str: string): string {
  return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function buildMatchRegex(keywords: string[]): RegExp | null {
  if (keywords.length === 0) return null;
  const pattern = keywords
    .map(k => `\\b${escapeRegex(k.trim())}\\b`)
    .join('|');
  try {
    return new RegExp(pattern, 'gi');
  } catch {
    return null;
  }
}

function articleMatchesMonitor(article: NewsArticle, monitor: NewsMonitor): boolean {
  if (!monitor.enabled || monitor.keywords.length === 0) return false;
  const regex = buildMatchRegex(monitor.keywords);
  if (!regex) return false;
  const text = `${article.headline} ${article.summary}`;
  regex.lastIndex = 0;
  return regex.test(text);
}

// ─── CRUD — all backed by SQLite via Tauri ───────────────────────────────────

export async function getMonitors(): Promise<NewsMonitor[]> {
  try {
    return await invoke<NewsMonitor[]>('get_news_monitors');
  } catch (e) {
    console.error('[NewsMonitorService] get_news_monitors failed:', e);
    return [];
  }
}

export async function addMonitor(
  label: string,
  keywords: string[],
  color?: string
): Promise<NewsMonitor | null> {
  try {
    // Auto-assign next color if not provided (caller can pass a custom one)
    const assignedColor = color ?? MONITOR_COLORS[0];
    return await invoke<NewsMonitor>('add_news_monitor', {
      label: label.trim(),
      keywords: keywords.map(k => k.trim()).filter(Boolean),
      color: assignedColor,
    });
  } catch (e) {
    console.error('[NewsMonitorService] add_news_monitor failed:', e);
    return null;
  }
}

export async function updateMonitor(
  id: string,
  label: string,
  keywords: string[],
  color: string,
  enabled: boolean
): Promise<boolean> {
  try {
    await invoke('update_news_monitor', {
      id,
      label: label.trim(),
      keywords: keywords.map(k => k.trim()).filter(Boolean),
      color,
      enabled,
    });
    return true;
  } catch (e) {
    console.error('[NewsMonitorService] update_news_monitor failed:', e);
    return false;
  }
}

export async function deleteMonitor(id: string): Promise<boolean> {
  try {
    await invoke('delete_news_monitor', { id });
    return true;
  } catch (e) {
    console.error('[NewsMonitorService] delete_news_monitor failed:', e);
    return false;
  }
}

export async function toggleMonitor(id: string, enabled: boolean): Promise<boolean> {
  try {
    await invoke('toggle_news_monitor', { id, enabled });
    return true;
  } catch (e) {
    console.error('[NewsMonitorService] toggle_news_monitor failed:', e);
    return false;
  }
}

// ─── Scanning ────────────────────────────────────────────────────────────────

/**
 * Scan articles against all monitors.
 * Returns a Map of monitorId → matched articles (de-duplicated by article id).
 */
export function scanMonitors(
  monitors: NewsMonitor[],
  articles: NewsArticle[]
): Map<string, NewsArticle[]> {
  const results = new Map<string, NewsArticle[]>();

  for (const monitor of monitors) {
    if (!monitor.enabled) continue;
    const matches: NewsArticle[] = [];
    const seen = new Set<string>();
    for (const article of articles) {
      if (!seen.has(article.id) && articleMatchesMonitor(article, monitor)) {
        matches.push(article);
        seen.add(article.id);
      }
    }
    results.set(monitor.id, matches);
  }

  return results;
}

/**
 * Detect new monitor matches that weren't in the previous fetch.
 * Used to fire notifications only for newly-arrived articles.
 *
 * @param monitors    All active monitors
 * @param articles    Current article list
 * @param prevIds     Set of article IDs from the previous refresh cycle
 * @returns           Array of { monitor, article } pairs to notify about
 */
export function checkForNewBreakingMatches(
  monitors: NewsMonitor[],
  articles: NewsArticle[],
  prevIds: Set<string>
): { monitor: NewsMonitor; article: NewsArticle }[] {
  const results: { monitor: NewsMonitor; article: NewsArticle }[] = [];

  const newArticles = articles.filter(
    a => !prevIds.has(a.id) && (a.priority === 'FLASH' || a.priority === 'URGENT')
  );

  for (const monitor of monitors) {
    if (!monitor.enabled) continue;
    for (const article of newArticles) {
      if (articleMatchesMonitor(article, monitor)) {
        results.push({ monitor, article });
      }
    }
  }

  return results;
}

/**
 * Determine which color to assign the next monitor based on existing monitors.
 */
export function nextMonitorColor(existingMonitors: NewsMonitor[]): string {
  const usedColors = new Set(existingMonitors.map(m => m.color));
  return (
    MONITOR_COLORS.find(c => !usedColors.has(c)) ??
    MONITOR_COLORS[existingMonitors.length % MONITOR_COLORS.length]
  );
}
