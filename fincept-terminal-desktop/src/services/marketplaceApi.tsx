// File: src/services/marketplaceApi.tsx
// Marketplace API service for handling all marketplace-related API calls

import { fetch } from '@tauri-apps/plugin-http';

// API Configuration
const API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://finceptbackend.share.zrok.io',
  API_VERSION: 'v1',
  CONNECTION_TIMEOUT: 10000,
  REQUEST_TIMEOUT: 30000
};

const getApiEndpoint = (path: string) => `${API_CONFIG.BASE_URL}${path}`;

// API Response Types
interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  message?: string;
  error?: string;
  status_code?: number;
}

// Request Types
export interface DatasetMetadata {
  title: string;
  description: string;
  category: string;
  price_tier: string;
  tags?: string[];
}

export interface DatasetPurchaseRequest {
  payment_method?: string;
}

export interface DatasetFilters {
  category?: string;
  search?: string;
  price_tier?: string;
  page?: number;
  limit?: number;
  sort_by?: string;
  sort_order?: string;
}

// Response Types
export interface Dataset {
  id: number;
  title: string;
  description: string;
  category: string;
  price_tier: string;
  price_credits: number;
  uploader_id: number;
  uploader_username: string;
  file_name: string;
  file_size: number;
  downloads_count: number;
  rating: number;
  tags: string[];
  status?: string; // pending, approved, rejected
  rejection_reason?: string;
  uploaded_at: string;
  updated_at: string;
}

export interface DatasetPurchase {
  id: number;
  dataset_id: number;
  dataset_title: string;
  price_paid: number;
  purchased_at: string;
  access_expires_at?: string;
}

export interface PricingTier {
  tier: string;
  price_credits: number;
  description: string;
  features: string[];
}

export interface DatasetAnalytics {
  total_datasets: number;
  total_downloads: number;
  total_revenue: number;
  datasets: Array<{
    id: number;
    title: string;
    downloads: number;
    revenue: number;
    rating: number;
  }>;
}

export interface RevenueAnalytics {
  total_revenue: number;
  total_transactions: number;
  revenue_by_tier: Record<string, number>;
  top_datasets: Array<{
    id: number;
    title: string;
    revenue: number;
    downloads: number;
  }>;
}

export interface MarketplaceStats {
  total_datasets: number;
  total_purchases: number;
  total_revenue: number;
  total_uploads: number;
  active_users: number;
  popular_categories: Record<string, number>;
}

// Generic API request function
const makeApiRequest = async <T = any>(
  method: string,
  endpoint: string,
  data?: any,
  headers?: Record<string, string>,
  isFormData: boolean = false
): Promise<ApiResponse<T>> => {
  try {
    const defaultHeaders: Record<string, string> = {
      ...(isFormData ? {} : { 'Content-Type': 'application/json' }),
      ...headers
    };

    const url = getApiEndpoint(endpoint);
    console.log('🚀 Making marketplace request:', {
      method,
      url,
      endpoint,
      isFormData,
      headers: { ...defaultHeaders, 'X-API-Key': (defaultHeaders as any)['X-API-Key'] ? '[REDACTED]' : undefined }
    });

    const config: RequestInit = {
      method,
      headers: defaultHeaders,
      mode: 'cors',
    };

    if (data) {
      if (isFormData) {
        config.body = data; // FormData
      } else if (method === 'POST' || method === 'PUT') {
        config.body = JSON.stringify(data);
      }
    }

    const response = await fetch(url, config);

    console.log('📡 Marketplace response received:', {
      status: response.status,
      statusText: response.statusText,
      url: response.url
    });

    const responseText = await response.text();
    console.log('📄 Raw marketplace response text:', responseText.substring(0, 200));

    let responseData;
    try {
      responseData = JSON.parse(responseText);
      console.log('✅ Parsed marketplace JSON:', responseData);
    } catch (parseError) {
      console.error('❌ Marketplace JSON parse failed:', parseError);
      return {
        success: false,
        error: `Invalid JSON response: ${responseText.substring(0, 100)}...`,
        status_code: response.status
      };
    }

    return {
      success: response.ok,
      data: responseData,
      status_code: response.status,
      error: response.ok ? undefined : responseData.message || `HTTP ${response.status}`
    };

  } catch (error) {
    console.error('💥 Marketplace request failed:', error);

    if (error instanceof TypeError && error.message.includes('Failed to fetch')) {
      return {
        success: false,
        error: 'Network error: Unable to connect to server. Please check your internet connection.'
      };
    }

    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error'
    };
  }
};

// Marketplace API Service Class
export class MarketplaceApiService {
  /**
   * 1. Upload dataset to marketplace
   * POST /marketplace/upload
   */
  static async uploadDataset(
    apiKey: string,
    file: File,
    metadata: DatasetMetadata
  ): Promise<ApiResponse<Dataset>> {
    try {
      const formData = new FormData();
      formData.append('file', file);

      // Create proper metadata object without tags if empty
      const metadataObj = {
        title: metadata.title,
        description: metadata.description,
        category: metadata.category,
        price_tier: metadata.price_tier,
        ...(metadata.tags && metadata.tags.length > 0 ? { tags: metadata.tags } : {})
      };

      const metadataString = JSON.stringify(metadataObj);
      console.log('Upload metadata:', metadataObj);

      // Append as query parameter in the URL
      const endpoint = `/marketplace/upload?metadata=${encodeURIComponent(metadataString)}`;

      return makeApiRequest<Dataset>(
        'POST',
        endpoint,
        formData,
        { 'X-API-Key': apiKey },
        true // isFormData
      );
    } catch (error) {
      console.error('Upload dataset error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Upload failed'
      };
    }
  }

  /**
   * 2. Browse datasets in marketplace
   * GET /marketplace/datasets
   */
  static async browseDatasets(
    apiKey: string,
    filters?: DatasetFilters
  ): Promise<ApiResponse<{ datasets: Dataset[]; total: number; page: number; total_pages: number }>> {
    const queryParams = new URLSearchParams();

    if (filters?.category) queryParams.append('category', filters.category);
    if (filters?.search) queryParams.append('search', filters.search);
    if (filters?.price_tier) queryParams.append('price_tier', filters.price_tier);
    if (filters?.page) queryParams.append('page', filters.page.toString());
    if (filters?.limit) queryParams.append('limit', filters.limit.toString());
    if (filters?.sort_by) queryParams.append('sort_by', filters.sort_by);
    if (filters?.sort_order) queryParams.append('sort_order', filters.sort_order);

    const endpoint = `/marketplace/datasets${queryParams.toString() ? `?${queryParams.toString()}` : ''}`;

    return makeApiRequest('GET', endpoint, undefined, { 'X-API-Key': apiKey });
  }

  /**
   * 3. Get dataset details
   * GET /marketplace/datasets/{dataset_id}
   */
  static async getDatasetDetails(
    apiKey: string,
    datasetId: number
  ): Promise<ApiResponse<Dataset>> {
    return makeApiRequest<Dataset>(
      'GET',
      `/marketplace/datasets/${datasetId}`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 4. Purchase dataset access
   * POST /marketplace/datasets/{dataset_id}/purchase
   */
  static async purchaseDataset(
    apiKey: string,
    datasetId: number,
    purchaseData?: DatasetPurchaseRequest
  ): Promise<ApiResponse<{ purchase_id: number; access_granted: boolean; credits_remaining: number }>> {
    return makeApiRequest(
      'POST',
      `/marketplace/datasets/${datasetId}/purchase`,
      purchaseData || { payment_method: 'subscription_credit' },
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 5. Download dataset
   * POST /marketplace/datasets/{dataset_id}/download
   */
  static async downloadDataset(
    apiKey: string,
    datasetId: number
  ): Promise<ApiResponse<{ download_url: string; expires_at: string }>> {
    return makeApiRequest(
      'POST',
      `/marketplace/datasets/${datasetId}/download`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 6. Get user's purchases
   * GET /marketplace/my-purchases
   */
  static async getUserPurchases(
    apiKey: string,
    page: number = 1,
    limit: number = 20
  ): Promise<ApiResponse<{ purchases: DatasetPurchase[]; total: number; page: number; total_pages: number }>> {
    return makeApiRequest(
      'GET',
      `/marketplace/my-purchases?page=${page}&limit=${limit}`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 7. Get pricing tiers
   * GET /marketplace/pricing
   */
  static async getPricingTiers(apiKey: string): Promise<ApiResponse<{ tiers: PricingTier[] }>> {
    return makeApiRequest('GET', '/marketplace/pricing', undefined, { 'X-API-Key': apiKey });
  }

  /**
   * 8. Get my uploaded datasets
   * GET /marketplace/my-datasets
   */
  static async getMyDatasets(
    apiKey: string,
    page: number = 1,
    limit: number = 20,
    status?: string
  ): Promise<ApiResponse<{ datasets: Dataset[]; total: number; page: number; total_pages: number }>> {
    const queryParams = new URLSearchParams();
    queryParams.append('page', page.toString());
    queryParams.append('limit', limit.toString());
    if (status) queryParams.append('status', status);

    return makeApiRequest(
      'GET',
      `/marketplace/my-datasets?${queryParams.toString()}`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 9. Update dataset metadata
   * PATCH /marketplace/datasets/{dataset_id}
   */
  static async updateDataset(
    apiKey: string,
    datasetId: number,
    updateData: Partial<DatasetMetadata>
  ): Promise<ApiResponse<Dataset>> {
    return makeApiRequest<Dataset>(
      'PATCH',
      `/marketplace/datasets/${datasetId}`,
      updateData,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 10. Delete dataset
   * DELETE /marketplace/datasets/{dataset_id}
   */
  static async deleteDataset(
    apiKey: string,
    datasetId: number
  ): Promise<ApiResponse<{ message: string }>> {
    return makeApiRequest(
      'DELETE',
      `/marketplace/datasets/${datasetId}`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 11. Get dataset analytics (for uploader)
   * GET /marketplace/my-datasets/analytics
   */
  static async getDatasetAnalytics(apiKey: string): Promise<ApiResponse<DatasetAnalytics>> {
    return makeApiRequest<DatasetAnalytics>(
      'GET',
      '/marketplace/my-datasets/analytics',
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 12. Get admin revenue analytics
   * GET /marketplace/admin/revenue-analytics
   */
  static async getAdminRevenueAnalytics(apiKey: string): Promise<ApiResponse<RevenueAnalytics>> {
    return makeApiRequest<RevenueAnalytics>(
      'GET',
      '/marketplace/admin/revenue-analytics',
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 13. Get marketplace statistics
   * GET /marketplace/admin/stats
   */
  static async getMarketplaceStats(apiKey: string): Promise<ApiResponse<MarketplaceStats>> {
    return makeApiRequest<MarketplaceStats>(
      'GET',
      '/marketplace/admin/stats',
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 14. Get pending datasets (admin)
   * GET /marketplace/admin/pending-datasets
   */
  static async getPendingDatasets(apiKey: string): Promise<ApiResponse<{ datasets: Dataset[] }>> {
    return makeApiRequest(
      'GET',
      '/marketplace/admin/pending-datasets',
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 15. Approve dataset (admin)
   * POST /marketplace/admin/datasets/{dataset_id}/approve
   */
  static async approveDataset(
    apiKey: string,
    datasetId: number
  ): Promise<ApiResponse<{ message: string; dataset: Dataset }>> {
    return makeApiRequest(
      'POST',
      `/marketplace/admin/datasets/${datasetId}/approve`,
      undefined,
      { 'X-API-Key': apiKey }
    );
  }

  /**
   * 16. Reject dataset (admin)
   * POST /marketplace/admin/datasets/{dataset_id}/reject
   */
  static async rejectDataset(
    apiKey: string,
    datasetId: number,
    reason: string
  ): Promise<ApiResponse<{ message: string; dataset: Dataset }>> {
    return makeApiRequest(
      'POST',
      `/marketplace/admin/datasets/${datasetId}/reject`,
      { reason },
      { 'X-API-Key': apiKey }
    );
  }
}

export default MarketplaceApiService;
