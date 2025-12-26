/**
 * HDX (Humanitarian Data Exchange) Service
 * Access humanitarian datasets for geopolitical analysis
 */

import { invoke } from '@tauri-apps/api/core';

export interface HDXDataset {
  id: string;
  name: string;
  title: string;
  organization: string;
  dataset_date: string;
  num_resources?: number;
  tags: string[];
  notes?: string;
}

export interface HDXDatasetDetails {
  id: string;
  name: string;
  title: string;
  organization: string;
  maintainer: string;
  dataset_date: string;
  last_modified: string;
  update_frequency: string;
  methodology: string;
  caveats: string;
  license: string;
  tags: string[];
  groups: string[];
  notes: string;
  resources: HDXResource[];
}

export interface HDXResource {
  id: string;
  name: string;
  description: string;
  format: string;
  url: string;
  size: string;
  last_modified: string;
}

export interface HDXSearchResult {
  query: string;
  count: number;
  datasets: HDXDataset[];
}

export interface HDXConflictResult {
  country: string;
  count: number;
  datasets: HDXDataset[];
}

/**
 * Search HDX datasets by query
 */
export async function searchDatasets(
  query: string,
  limit: number = 10
): Promise<HDXSearchResult> {
  try {
    const result = await invoke<string>('hdx_search_datasets', {
      query,
      limit,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return data.data;
  } catch (error) {
    console.error('HDX search error:', error);
    throw error;
  }
}

/**
 * Get detailed information about a specific dataset
 */
export async function getDataset(datasetId: string): Promise<HDXDatasetDetails> {
  try {
    const result = await invoke<string>('hdx_get_dataset', {
      datasetId,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return data.data;
  } catch (error) {
    console.error('HDX get dataset error:', error);
    throw error;
  }
}

/**
 * Search for conflict-related datasets
 */
export async function searchConflict(
  country: string = '',
  limit: number = 10
): Promise<HDXConflictResult> {
  try {
    const result = await invoke<string>('hdx_search_conflict', {
      country,
      limit,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return data.data;
  } catch (error) {
    console.error('HDX conflict search error:', error);
    throw error;
  }
}

/**
 * Search for humanitarian crisis datasets
 */
export async function searchHumanitarian(
  country: string = '',
  limit: number = 10
): Promise<HDXConflictResult> {
  try {
    const result = await invoke<string>('hdx_search_humanitarian', {
      country,
      limit,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return data.data;
  } catch (error) {
    console.error('HDX humanitarian search error:', error);
    throw error;
  }
}

/**
 * Search datasets by country code
 */
export async function searchByCountry(
  countryCode: string,
  limit: number = 10
): Promise<HDXSearchResult> {
  try {
    const result = await invoke<string>('hdx_search_by_country', {
      countryCode,
      limit,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return {
      query: `groups:${countryCode}`,
      count: data.data.count,
      datasets: data.data.datasets,
    };
  } catch (error) {
    console.error('HDX search by country error:', error);
    throw error;
  }
}

/**
 * Search datasets by organization
 */
export async function searchByOrganization(
  orgSlug: string,
  limit: number = 10
): Promise<HDXSearchResult> {
  try {
    const result = await invoke<string>('hdx_search_by_organization', {
      orgSlug,
      limit,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return {
      query: `organization:${orgSlug}`,
      count: data.data.count,
      datasets: data.data.datasets,
    };
  } catch (error) {
    console.error('HDX search by organization error:', error);
    throw error;
  }
}

/**
 * Search datasets by topic tag
 */
export async function searchByTopic(
  topic: string,
  limit: number = 10
): Promise<HDXSearchResult> {
  try {
    const result = await invoke<string>('hdx_search_by_topic', {
      topic,
      limit,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return {
      query: `vocab_Topics:${topic}`,
      count: data.data.count,
      datasets: data.data.datasets,
    };
  } catch (error) {
    console.error('HDX search by topic error:', error);
    throw error;
  }
}

/**
 * Search datasets by data series
 */
export async function searchByDataSeries(
  seriesName: string,
  limit: number = 10
): Promise<HDXSearchResult> {
  try {
    const result = await invoke<string>('hdx_search_by_dataseries', {
      seriesName,
      limit,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return {
      query: `dataseries_name:${seriesName}`,
      count: data.data.count,
      datasets: data.data.datasets,
    };
  } catch (error) {
    console.error('HDX search by dataseries error:', error);
    throw error;
  }
}

/**
 * List all countries/groups
 */
export async function listCountries(
  limit: number = 300
): Promise<Array<{ id: string; name: string; title: string; description: string; package_count: number }>> {
  try {
    const result = await invoke<string>('hdx_list_countries', {
      limit,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return data.data.countries;
  } catch (error) {
    console.error('HDX list countries error:', error);
    throw error;
  }
}

/**
 * List top organizations providing data
 */
export async function listOrganizations(
  limit: number = 20
): Promise<Array<{ id: string; name: string; title: string; description: string; package_count: number }>> {
  try {
    const result = await invoke<string>('hdx_list_organizations', {
      limit,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return data.data.organizations;
  } catch (error) {
    console.error('HDX list organizations error:', error);
    throw error;
  }
}

/**
 * List all topic tags
 */
export async function listTopics(
  limit: number = 100
): Promise<Array<{ id: string; name: string }>> {
  try {
    const result = await invoke<string>('hdx_list_topics', {
      limit,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return data.data.topics;
  } catch (error) {
    console.error('HDX list topics error:', error);
    throw error;
  }
}

/**
 * Advanced search with multiple filters
 * @param filters - Object with filter keys (country, organization, topic, etc.)
 * @param limit - Maximum number of results
 * @example
 * advancedSearch({ country: 'ukr', organization: 'ocha' }, 20)
 */
export async function advancedSearch(
  filters: Record<string, string>,
  limit: number = 10
): Promise<HDXSearchResult> {
  try {
    // Build filter string
    const filterParts: string[] = [];
    for (const [key, value] of Object.entries(filters)) {
      if (value.includes(' ')) {
        filterParts.push(`${key}:"${value}"`);
      } else {
        filterParts.push(`${key}:${value}`);
      }
    }
    filterParts.push(`limit:${limit}`);

    const filterString = filterParts.join(' ');

    const result = await invoke<string>('hdx_advanced_search', {
      filters: filterString,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return {
      query: filterString,
      count: data.data.count,
      datasets: data.data.datasets,
    };
  } catch (error) {
    console.error('HDX advanced search error:', error);
    throw error;
  }
}

/**
 * Get download URL for a dataset resource
 */
export async function getResourceDownloadUrl(
  datasetId: string,
  resourceIndex: number = 0
): Promise<{ url: string; name: string; format: string; size: string }> {
  try {
    const result = await invoke<string>('hdx_get_resource_url', {
      datasetId,
      resourceIndex,
    });

    const data = JSON.parse(result);

    if (data.error) {
      throw new Error(data.error);
    }

    return data.data;
  } catch (error) {
    console.error('HDX get resource URL error:', error);
    throw error;
  }
}

/**
 * Test HDX connection
 */
export async function testConnection(): Promise<boolean> {
  try {
    const result = await invoke<string>('hdx_test_connection');
    const data = JSON.parse(result);

    return data.success === true;
  } catch (error) {
    console.error('HDX connection test error:', error);
    return false;
  }
}

/**
 * Get conflict data for specific countries
 */
export async function getConflictDataByCountries(
  countries: string[]
): Promise<Map<string, HDXConflictResult>> {
  const results = new Map<string, HDXConflictResult>();

  for (const country of countries) {
    try {
      const data = await searchConflict(country, 5);
      results.set(country, data);
    } catch (error) {
      console.error(`Error fetching conflict data for ${country}:`, error);
    }
  }

  return results;
}

/**
 * Common search queries and topic tags for geopolitical analysis
 * Based on HDX vocab_Topics taxonomy
 */
export const HDXQueries = {
  GLOBAL_CONFLICTS: 'vocab_Topics:"conflict, violence and peace"',
  HUMANITARIAN_CRISIS: 'vocab_Topics:"humanitarian access-aid-workers"',
  DISPLACEMENT: 'vocab_Topics:"refugees-persons of concern"',
  FOOD_SECURITY: 'vocab_Topics:"food security and nutrition"',
  HEALTH_CRISIS: 'vocab_Topics:"health facilities and services"',
  NATURAL_DISASTERS: 'vocab_Topics:"natural disasters"',
  GENDER_VIOLENCE: 'vocab_Topics:"gender-based violence-gbv"',
  POLITICAL_INSTABILITY: 'political instability',
  ECONOMIC_CRISIS: 'vocab_Topics:"socio-economic indicators"',
  CLIMATE_IMPACT: 'vocab_Topics:"climate"',
  WHO_WHAT_WHERE: 'vocab_Topics:"who is doing what and where-3w-4w-5w"',
  DISPLACED_POPULATIONS: 'vocab_Topics:"internally displaced persons-idp"',
};

/**
 * Get comprehensive geopolitical overview
 */
export async function getGeopoliticalOverview(): Promise<{
  conflicts: HDXSearchResult;
  humanitarian: HDXSearchResult;
  displacement: HDXSearchResult;
}> {
  try {
    const [conflicts, humanitarian, displacement] = await Promise.all([
      searchDatasets(HDXQueries.GLOBAL_CONFLICTS, 5),
      searchDatasets(HDXQueries.HUMANITARIAN_CRISIS, 5),
      searchDatasets(HDXQueries.DISPLACEMENT, 5),
    ]);

    return {
      conflicts,
      humanitarian,
      displacement,
    };
  } catch (error) {
    console.error('HDX geopolitical overview error:', error);
    throw error;
  }
}
