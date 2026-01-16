const API_BASE = 'https://finceptbackend.share.zrok.io';

export interface NewsEvent {
  url?: string;
  domain?: string;
  event_category?: string;
  matched_keywords?: string;
  city?: string;
  country?: string;
  latitude?: number;
  longitude?: number;
  extracted_date?: string;
  created_at?: string;
}

export interface NewsEventsResponse {
  success: boolean;
  events: NewsEvent[];
  total: number;
  page: number;
  limit: number;
  message?: string;
  error_code?: string;
  pagination?: {
    current_page: number;
    total_pages: number;
    total_events: number;
    events_per_page: number;
    has_next: boolean;
    has_prev: boolean;
  };
  credits_used?: number;
  remaining_credits?: number;
}

export interface UniqueCity {
  city: string;
  country: string;
}

export interface UniqueCountry {
  country: string;
  event_count: number;
}

export interface UniqueCategory {
  event_category: string;
  event_count: number;
}

export class NewsEventsService {
  static async getUniqueCities(apiKey: string, country?: string): Promise<UniqueCity[]> {
    try {
      const params = new URLSearchParams();
      params.append('get_unique_cities', 'true');
      if (country) params.append('country', country);

      const url = `${API_BASE}/research/news-events?${params.toString()}`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'X-API-Key': apiKey,
          'accept': 'application/json'
        }
      });

      const data = await response.json();

      if (response.ok && data.success && data.data?.unique_cities) {
        return data.data.unique_cities;
      }

      return [];
    } catch (error) {
      console.error('Failed to fetch unique cities:', error);
      return [];
    }
  }

  static async getUniqueCountries(apiKey: string, eventCategory?: string): Promise<UniqueCountry[]> {
    try {
      const params = new URLSearchParams();
      params.append('get_unique_countries', 'true');
      if (eventCategory) params.append('event_category', eventCategory);

      const url = `${API_BASE}/research/news-events?${params.toString()}`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'X-API-Key': apiKey,
          'accept': 'application/json'
        }
      });

      const data = await response.json();

      if (response.ok && data.success && data.data?.unique_countries) {
        return data.data.unique_countries;
      }

      return [];
    } catch (error) {
      console.error('Failed to fetch unique countries:', error);
      return [];
    }
  }

  static async getUniqueCategories(apiKey: string, city?: string): Promise<UniqueCategory[]> {
    try {
      const params = new URLSearchParams();
      params.append('get_unique_categories', 'true');
      if (city) params.append('city', city);

      const url = `${API_BASE}/research/news-events?${params.toString()}`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'X-API-Key': apiKey,
          'accept': 'application/json'
        }
      });

      const data = await response.json();

      if (response.ok && data.success && data.data?.unique_categories) {
        return data.data.unique_categories;
      }

      return [];
    } catch (error) {
      console.error('Failed to fetch unique categories:', error);
      return [];
    }
  }

  static async getNewsEvents(
    apiKey: string,
    options: {
      country?: string;
      city?: string;
      cities?: string;
      event_category?: string;
      page?: number;
      limit?: number;
    } = {}
  ): Promise<NewsEventsResponse> {
    try {
      const params = new URLSearchParams();
      if (options.country) params.append('country', options.country);
      if (options.city) params.append('city', options.city);
      if (options.cities) params.append('cities', options.cities);
      if (options.event_category) params.append('event_category', options.event_category);
      if (options.page) params.append('page', options.page.toString());
      if (options.limit) params.append('limit', options.limit.toString());

      const url = `${API_BASE}/research/news-events${params.toString() ? '?' + params.toString() : ''}`;

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'X-API-Key': apiKey,
          'accept': 'application/json'
        }
      });

      const data = await response.json();

      if (!response.ok || !data.success) {
        // Handle NaN error from backend
        const errorMessage = data.message || 'Failed to fetch events';
        if (errorMessage.includes('NaN') || errorMessage.includes('not JSON compliant')) {
          return {
            success: false,
            events: [],
            total: 0,
            page: options.page || 1,
            limit: options.limit || 30,
            message: 'Some events have invalid coordinates. Try using specific country/city filters.',
            error_code: 'INVALID_COORDINATES'
          };
        }

        return {
          success: false,
          events: [],
          total: 0,
          page: options.page || 1,
          limit: options.limit || 30,
          message: errorMessage,
          error_code: data.error_code
        };
      }

      // Filter out events with invalid coordinates (NaN values)
      const events = (data.data?.events || []).filter((event: NewsEvent) => {
        return event.latitude !== undefined &&
               event.longitude !== undefined &&
               !isNaN(event.latitude) &&
               !isNaN(event.longitude) &&
               isFinite(event.latitude) &&
               isFinite(event.longitude);
      });

      return {
        success: true,
        events,
        total: data.data?.pagination?.total_events || 0,
        page: data.data?.pagination?.current_page || 1,
        limit: data.data?.pagination?.events_per_page || 30,
        pagination: data.data?.pagination,
        credits_used: data.data?.credits_used,
        remaining_credits: data.data?.remaining_credits
      };
    } catch (error) {
      console.error('News events API error:', error);
      const errorMsg = error instanceof Error ? error.message : 'Unknown error';
      return {
        success: false,
        events: [],
        total: 0,
        page: options.page || 1,
        limit: options.limit || 30,
        message: errorMsg.includes('JSON')
          ? 'Data format error. Try filtering by country (e.g., Ukraine, Syria, Israel)'
          : errorMsg
      };
    }
  }
}
