// File: src/services/marketplace/excelMarketplaceService.ts
// Excel Marketplace API service for handling Excel file marketplace endpoints

import { fetch } from '@tauri-apps/plugin-http';

// API Configuration
const API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://finceptbackend.share.zrok.io',
  REQUEST_TIMEOUT: 30000
};

const getApiEndpoint = (path: string) => `${API_CONFIG.BASE_URL}${path}`;

// Response Types
interface ApiResponse<T = any> {
  success: boolean;
  data?: T;
  message?: string;
  error?: string;
  status_code?: number;
}

export interface ExcelCategory {
  category: string;
  count: number;
  description?: string;
}

export interface ExcelFile {
  id: number;
  filename: string;
  display_name: string;
  tier: string; // free, basic, standard, pro, enterprise
  file_size: number;
  file_size_mb: number;
  locked: boolean;
  can_download: boolean;
  title?: string;
  description?: string;
  category?: string;
  downloads_count?: number;
  created_at?: string;
  updated_at?: string;
  preview_url?: string;
  tags?: string[];
}

export interface ExcelFileDetails extends ExcelFile {
  detailed_description?: string;
  author?: string;
  version?: string;
  last_updated?: string;
  requirements?: string[];
  screenshots?: string[];
}

export interface ExcelDownloadStats {
  total_downloads: number;
  downloads_by_category: Record<string, number>;
  recent_downloads: Array<{
    file_id: number;
    file_title: string;
    downloaded_at: string;
  }>;
  unlocked_files_count: number;
  total_files_count: number;
}

export interface ExcelFilesListResponse {
  files: ExcelFile[];
  pagination: {
    current_page: number;
    per_page: number;
    total_files: number;
    total_pages: number;
  };
  user_access: {
    tier: string;
    accessible_files: number;
    total_files: number;
    locked_files: number;
  };
  tier_limits: {
    free: number;
    basic: number;
    standard: number;
    pro: number;
    enterprise: number;
  };
}

// Generic API request function
const makeApiRequest = async <T = any>(
  method: string,
  endpoint: string,
  data?: any,
  headers?: Record<string, string>
): Promise<ApiResponse<T>> => {
  try {
    const defaultHeaders: Record<string, string> = {
      'Content-Type': 'application/json',
      ...headers
    };

    const url = getApiEndpoint(endpoint);

    const config: RequestInit = {
      method,
      headers: defaultHeaders,
      mode: 'cors',
    };

    if (data && (method === 'POST' || method === 'PUT' || method === 'PATCH')) {
      config.body = JSON.stringify(data);
    }

    const response = await fetch(url, config);

    const responseText = await response.text();

    let responseData;
    try {
      responseData = JSON.parse(responseText);
    } catch (parseError) {
      console.error('[Excel Marketplace] JSON parse failed:', parseError);
      return {
        success: false,
        error: `Invalid JSON response: ${responseText.substring(0, 100)}...`,
        status_code: response.status
      };
    }

    // Check if API returned an error
    if (!response.ok) {
      console.error('[Excel Marketplace] HTTP error:', response.status, responseData.message || responseData.detail);
      return {
        success: false,
        data: responseData,
        status_code: response.status,
        error: responseData.message || responseData.detail || `HTTP ${response.status}`
      };
    }

    // Return the full response data structure
    return {
      success: true,
      data: responseData,
      status_code: response.status
    };

  } catch (error) {
    console.error('💥 Excel Marketplace request failed:', error);

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

// Get API key from AuthContext session (stored in SQLite)
const getApiKey = async (): Promise<string> => {
  try {
    const { getSetting } = await import('@/services/core/sqliteService');

    // Try to get from session (AuthContext stores it here)
    const sessionStr = await getSetting('fincept_session');

    if (sessionStr) {
      try {
        const session = JSON.parse(sessionStr);
        if (session?.api_key) {
          return session.api_key;
        }
      } catch (parseError) {
        console.warn('[Excel Marketplace] Failed to parse session:', parseError);
      }
    }

    // Fallback to legacy fincept_api_key setting
    const apiKey = await getSetting('fincept_api_key') || '';
    if (apiKey) {
      return apiKey;
    }

    throw new Error('API key not found. Please log in to access the marketplace.');
  } catch (error) {
    console.error('[Excel Marketplace] Failed to get API key:', error);
    throw new Error('API key not found. Please log in to access the marketplace.');
  }
};

/**
 * Excel Marketplace API Service
 */
export class ExcelMarketplaceService {

  /**
   * 1. GET /marketplace/excel-categories
   * Get Excel file categories with counts
   * Credits: Free
   */
  static async getCategories(): Promise<ApiResponse<{ categories: ExcelCategory[] }>> {
    try {
      const apiKey = await getApiKey();
      return makeApiRequest<{ categories: ExcelCategory[] }>(
        'GET',
        '/marketplace/excel-categories',
        undefined,
        { 'X-API-Key': apiKey }
      );
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to get categories'
      };
    }
  }

  /**
   * 2. GET /marketplace/excel-files
   * List Excel files with tier-based access control
   * Credits: Free
   */
  static async listFiles(params?: {
    tier_filter?: string;
    search?: string;
    page?: number;
    limit?: number;
  }): Promise<ApiResponse<ExcelFilesListResponse>> {
    try {
      const apiKey = await getApiKey();

      const queryParams = new URLSearchParams();
      if (params?.tier_filter) queryParams.append('tier_filter', params.tier_filter);
      if (params?.search) queryParams.append('search', params.search);
      if (params?.page) queryParams.append('page', params.page.toString());
      if (params?.limit) queryParams.append('limit', params.limit.toString());

      const endpoint = `/marketplace/excel-files${queryParams.toString() ? `?${queryParams.toString()}` : ''}`;

      return makeApiRequest<ExcelFilesListResponse>(
        'GET',
        endpoint,
        undefined,
        { 'X-API-Key': apiKey }
      );
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to list files'
      };
    }
  }

  /**
   * 3. GET /marketplace/excel-files/{file_id}
   * Get Excel file details
   * Credits: Free
   */
  static async getFileDetails(fileId: number): Promise<ApiResponse<ExcelFileDetails>> {
    try {
      const apiKey = await getApiKey();
      return makeApiRequest<ExcelFileDetails>(
        'GET',
        `/marketplace/excel-files/${fileId}`,
        undefined,
        { 'X-API-Key': apiKey }
      );
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to get file details'
      };
    }
  }

  /**
   * 4. GET /marketplace/excel-files/{file_id}/download
   * Download Excel file (tier-based access + credits)
   * Credits: 2 credits per download
   * Returns the file blob for download
   */
  static async downloadFile(fileId: number): Promise<{ success: boolean; blob?: Blob; error?: string; fileName?: string }> {
    try {
      const apiKey = await getApiKey();
      const url = getApiEndpoint(`/marketplace/excel-files/${fileId}/download`);

      console.log('📥 Downloading Excel file:', fileId);

      const response = await fetch(url, {
        method: 'GET',
        headers: {
          'X-API-Key': apiKey
        },
        mode: 'cors',
      });

      if (!response.ok) {
        const errorText = await response.text();
        let errorMessage = `HTTP ${response.status}`;
        try {
          const errorData = JSON.parse(errorText);
          errorMessage = errorData.message || errorData.detail || errorMessage;
        } catch {
          errorMessage = errorText || errorMessage;
        }

        console.error('Download failed:', errorMessage);
        return {
          success: false,
          error: errorMessage
        };
      }

      // Get filename from Content-Disposition header
      const contentDisposition = response.headers.get('Content-Disposition');
      let fileName = `excel_file_${fileId}.xlsx`;

      if (contentDisposition) {
        const match = contentDisposition.match(/filename[^;=\n]*=((['"]).*?\2|[^;\n]*)/);
        if (match && match[1]) {
          fileName = match[1].replace(/['"]/g, '');
        }
      }

      const blob = await response.blob();

      console.log('✅ File downloaded successfully:', fileName, blob.size, 'bytes');

      return {
        success: true,
        blob,
        fileName
      };

    } catch (error) {
      console.error('💥 Download error:', error);
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Download failed'
      };
    }
  }

  /**
   * 5. GET /marketplace/excel-files/stats
   * Get user's Excel file download statistics
   * Credits: Free
   */
  static async getDownloadStats(): Promise<ApiResponse<ExcelDownloadStats>> {
    try {
      const apiKey = await getApiKey();
      return makeApiRequest<ExcelDownloadStats>(
        'GET',
        '/marketplace/excel-files/stats',
        undefined,
        { 'X-API-Key': apiKey }
      );
    } catch (error) {
      return {
        success: false,
        error: error instanceof Error ? error.message : 'Failed to get download stats'
      };
    }
  }
}

export default ExcelMarketplaceService;
