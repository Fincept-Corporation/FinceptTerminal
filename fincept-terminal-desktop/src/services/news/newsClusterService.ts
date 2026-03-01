// News Cluster Service
// Groups related articles using Jaccard similarity on tokenized headlines.
// Inspired by World Monitor's analysis-core.ts clustering approach.

import { NewsArticle } from './newsService';

// ─── Types ────────────────────────────────────────────────────────────────────

export interface NewsCluster {
  id: string;
  leadArticle: NewsArticle;      // Highest-tier (lowest tier number) / most-recent
  articles: NewsArticle[];       // All grouped articles (always includes leadArticle)
  sourceCount: number;           // Distinct sources in this cluster
  velocity: 'rising' | 'stable' | 'falling';
  sentiment: 'BULLISH' | 'BEARISH' | 'NEUTRAL';
  category: string;
  tier: number;                  // Best (lowest number = highest quality) tier in cluster
  latestTime: string;
  latestSortTs: number;
  isBreaking: boolean;           // True if any article is FLASH or URGENT
}

export interface NewsMarker {
  lat: number;
  lng: number;
  headline: string;
  category: string;
  priority: string;
  tier: number;
  clusterId: string;
}

// ─── Region → coordinate map (50 major regions) ─────────────────────────────

const REGION_COORDS: Record<string, { lat: number; lng: number }> = {
  US: { lat: 38.9, lng: -77.0 },
  GLOBAL: { lat: 20.0, lng: 0.0 },
  EU: { lat: 50.9, lng: 10.5 },
  ASIA: { lat: 35.0, lng: 105.0 },
  MENA: { lat: 25.0, lng: 45.0 },
  INDIA: { lat: 20.6, lng: 78.9 },
  CHINA: { lat: 35.9, lng: 104.2 },
  UK: { lat: 51.5, lng: -0.1 },
  LATAM: { lat: -8.8, lng: -55.5 },
  AFRICA: { lat: 8.8, lng: 26.8 },
  RUSSIA: { lat: 61.5, lng: 90.0 },
  JAPAN: { lat: 36.2, lng: 138.3 },
  KOREA: { lat: 35.9, lng: 127.8 },
  AUSTRALIA: { lat: -25.3, lng: 133.8 },
  CANADA: { lat: 56.1, lng: -106.3 },
  BRAZIL: { lat: -14.2, lng: -51.9 },
  TURKEY: { lat: 38.9, lng: 35.2 },
  ISRAEL: { lat: 31.0, lng: 34.9 },
  UKRAINE: { lat: 48.4, lng: 31.2 },
  IRAN: { lat: 32.4, lng: 53.7 },
  SAUDI: { lat: 23.9, lng: 45.1 },
  UAE: { lat: 23.4, lng: 53.8 },
  PAKISTAN: { lat: 30.4, lng: 69.3 },
  GERMANY: { lat: 51.2, lng: 10.5 },
  FRANCE: { lat: 46.2, lng: 2.2 },
  ITALY: { lat: 41.9, lng: 12.6 },
  SPAIN: { lat: 40.5, lng: -3.7 },
  POLAND: { lat: 51.9, lng: 19.1 },
  TAIWAN: { lat: 23.7, lng: 120.9 },
  MEXICO: { lat: 23.6, lng: -102.5 },
  INDONESIA: { lat: -0.8, lng: 113.9 },
  NIGERIA: { lat: 9.1, lng: 8.7 },
  EGYPT: { lat: 26.8, lng: 30.8 },
  SINGAPORE: { lat: 1.4, lng: 103.8 },
  THAILAND: { lat: 15.9, lng: 100.9 },
  VIETNAM: { lat: 14.1, lng: 108.3 },
  MYANMAR: { lat: 19.2, lng: 96.7 },
  ETHIOPIA: { lat: 9.1, lng: 40.5 },
  DRC: { lat: -4.0, lng: 21.8 },
  VENEZUELA: { lat: 6.4, lng: -66.6 },
  COLOMBIA: { lat: 4.6, lng: -74.3 },
  ARGENTINA: { lat: -34.6, lng: -58.4 },
  CHILE: { lat: -33.5, lng: -70.7 },
  PERU: { lat: -9.2, lng: -75.0 },
  IRAQ: { lat: 33.2, lng: 43.7 },
  SYRIA: { lat: 34.8, lng: 38.9 },
  AFGHANISTAN: { lat: 33.9, lng: 67.7 },
  NORTH_KOREA: { lat: 40.3, lng: 127.5 },
  SOUTH_AFRICA: { lat: -30.6, lng: 22.9 },
};

// ─── Stopwords ────────────────────────────────────────────────────────────────

const STOPWORDS = new Set([
  'a', 'an', 'the', 'and', 'or', 'but', 'in', 'on', 'at', 'to', 'for',
  'of', 'with', 'by', 'from', 'is', 'are', 'was', 'were', 'be', 'been',
  'has', 'have', 'had', 'do', 'does', 'did', 'will', 'would', 'could',
  'should', 'may', 'might', 'as', 'it', 'its', 'this', 'that', 'not',
  'up', 'out', 'over', 'into', 'after', 'before', 'about', 'says',
  'said', 'amid', 'over', 'new', 'amid', 'amid', 'after', 'amid',
]);

// ─── Velocity tracking (module-level) ────────────────────────────────────────

const _prevClusterSizes = new Map<string, number>();

// ─── Core functions ───────────────────────────────────────────────────────────

function tokenize(headline: string): Set<string> {
  return new Set(
    headline
      .toLowerCase()
      .replace(/[^a-z0-9\s]/g, ' ')
      .split(/\s+/)
      .filter(t => t.length > 2 && !STOPWORDS.has(t))
  );
}

function jaccard(a: Set<string>, b: Set<string>): number {
  if (a.size === 0 && b.size === 0) return 0;
  let intersection = 0;
  for (const t of a) if (b.has(t)) intersection++;
  const union = a.size + b.size - intersection;
  return union === 0 ? 0 : intersection / union;
}

// Union-Find with path compression
class UnionFind {
  private parent: number[];
  constructor(n: number) {
    this.parent = Array.from({ length: n }, (_, i) => i);
  }
  find(x: number): number {
    if (this.parent[x] !== x) this.parent[x] = this.find(this.parent[x]);
    return this.parent[x];
  }
  union(x: number, y: number): void {
    this.parent[this.find(x)] = this.find(y);
  }
}

function dominantSentiment(articles: NewsArticle[]): 'BULLISH' | 'BEARISH' | 'NEUTRAL' {
  let bull = 0, bear = 0;
  for (const a of articles) {
    if (a.sentiment === 'BULLISH') bull++;
    else if (a.sentiment === 'BEARISH') bear++;
  }
  if (bull > bear) return 'BULLISH';
  if (bear > bull) return 'BEARISH';
  return 'NEUTRAL';
}

function pickLeadArticle(articles: NewsArticle[]): NewsArticle {
  // Prefer: lowest tier (T1 > T2 > T3), then most recent
  return articles.reduce((best, a) => {
    if (a.tier < best.tier) return a;
    if (a.tier === best.tier && a.sort_ts > best.sort_ts) return a;
    return best;
  });
}

function computeVelocity(clusterId: string, currentCount: number): 'rising' | 'stable' | 'falling' {
  const prev = _prevClusterSizes.get(clusterId) ?? currentCount;
  _prevClusterSizes.set(clusterId, currentCount);
  const delta = currentCount - prev;
  if (delta >= 2) return 'rising';
  if (delta <= -2) return 'falling';
  return 'stable';
}

// ─── Public API ───────────────────────────────────────────────────────────────

/**
 * Cluster articles by headline similarity using Jaccard on token sets.
 * Returns clusters sorted: breaking first, then by source count desc, then recency.
 */
export function clusterArticles(articles: NewsArticle[]): NewsCluster[] {
  if (articles.length === 0) return [];

  // Tokenize all headlines
  const tokens = articles.map(a => tokenize(a.headline));

  // Build inverted index: token → article indices
  const invertedIndex = new Map<string, number[]>();
  tokens.forEach((tokenSet, idx) => {
    for (const token of tokenSet) {
      const list = invertedIndex.get(token) ?? [];
      list.push(idx);
      invertedIndex.set(token, list);
    }
  });

  // Find candidate pairs (articles sharing at least one token)
  const checkedPairs = new Set<string>();
  const uf = new UnionFind(articles.length);

  for (const [, indices] of invertedIndex) {
    for (let i = 0; i < indices.length; i++) {
      for (let j = i + 1; j < indices.length; j++) {
        const a = indices[i], b = indices[j];
        const key = `${Math.min(a, b)}-${Math.max(a, b)}`;
        if (checkedPairs.has(key)) continue;
        checkedPairs.add(key);
        if (jaccard(tokens[a], tokens[b]) >= 0.25) {
          uf.union(a, b);
        }
      }
    }
  }

  // Group articles by their Union-Find root
  const groups = new Map<number, number[]>();
  articles.forEach((_, idx) => {
    const root = uf.find(idx);
    const group = groups.get(root) ?? [];
    group.push(idx);
    groups.set(root, group);
  });

  // Build NewsCluster objects
  const clusters: NewsCluster[] = [];
  for (const [, indices] of groups) {
    const groupArticles = indices.map(i => articles[i]);
    const leadArticle = pickLeadArticle(groupArticles);
    const latestSortTs = Math.max(...groupArticles.map(a => a.sort_ts));
    const sources = new Set(groupArticles.map(a => a.source));
    const bestTier = Math.min(...groupArticles.map(a => a.tier || 3));
    const isBreaking = groupArticles.some(
      a => a.priority === 'FLASH' || a.priority === 'URGENT'
    );
    const clusterId = `cluster-${leadArticle.id}`;
    const velocity = computeVelocity(clusterId, groupArticles.length);

    clusters.push({
      id: clusterId,
      leadArticle,
      articles: groupArticles.sort((a, b) => b.sort_ts - a.sort_ts),
      sourceCount: sources.size,
      velocity,
      sentiment: dominantSentiment(groupArticles),
      category: leadArticle.category,
      tier: bestTier,
      latestTime: leadArticle.time,
      latestSortTs,
      isBreaking,
    });
  }

  // Sort: breaking first → multi-source first → most recent
  clusters.sort((a, b) => {
    if (a.isBreaking && !b.isBreaking) return -1;
    if (!a.isBreaking && b.isBreaking) return 1;
    if (b.sourceCount !== a.sourceCount) return b.sourceCount - a.sourceCount;
    return b.latestSortTs - a.latestSortTs;
  });

  return clusters;
}

/**
 * Filter clusters to only breaking ones (FLASH or URGENT articles present).
 */
export function getBreakingClusters(clusters: NewsCluster[]): NewsCluster[] {
  return clusters.filter(c => c.isBreaking);
}

/**
 * Map articles to geo-coordinates using their region field.
 * Returns only articles that can be placed on the map.
 */
export function getArticleCoords(
  article: NewsArticle
): { lat: number; lng: number } | null {
  if (!article.region) return null;
  const key = article.region.toUpperCase().replace(/\s+/g, '_');
  return REGION_COORDS[key] ?? null;
}

/**
 * Build map marker data from a list of clusters.
 */
export function buildNewsMarkers(clusters: NewsCluster[]): NewsMarker[] {
  const markers: NewsMarker[] = [];
  for (const cluster of clusters) {
    const coords = getArticleCoords(cluster.leadArticle);
    if (!coords) continue;
    markers.push({
      lat: coords.lat,
      lng: coords.lng,
      headline: cluster.leadArticle.headline,
      category: cluster.category,
      priority: cluster.leadArticle.priority,
      tier: cluster.tier,
      clusterId: cluster.id,
    });
  }
  return markers;
}
