// File: src/services/forumApi.ts
// Forum API service for handling all forum-related API calls

import { fetch } from '@tauri-apps/plugin-http';

// API Configuration - matches your existing auth setup
const API_CONFIG = {
  BASE_URL: import.meta.env.DEV ? '/api' : 'https://finceptbackend.share.zrok.io',
  API_VERSION: 'v1',
  CONNECTION_TIMEOUT: 10000,
  REQUEST_TIMEOUT: 30000
};

const getApiEndpoint = (path: string) => `${API_CONFIG.BASE_URL}${path}`;

// Forum Types
export interface ForumCategory {
  id: number;
  name: string;
  description: string;
  post_count: number;
  created_at: string;
}

export interface ForumPost {
  post_uuid: string;
  title: string;
  content: string;
  author_display_name: string;
  category_id: number;
  category_name: string;
  likes: number;
  dislikes: number;
  reply_count: number;
  views: number;
  is_pinned: boolean;
  created_at: string;
  updated_at: string;
}

export interface ForumComment {
  comment_uuid: string;
  post_uuid: string;
  content: string;
  author_display_name: string;
  likes: number;
  dislikes: number;
  created_at: string;
  updated_at: string;
}

export interface ForumStats {
  total_categories: number;
  total_posts: number;
  total_comments: number;
  total_votes: number;
  active_users: number;
}

export interface PostDetails {
  post: ForumPost;
  comments: ForumComment[];
}

export interface SearchResults {
  posts: ForumPost[];
  total_results: number;
}

// API Response Types
interface ApiResponse<T = any> {
  success: boolean;
  data?: {
    success: boolean;
    data: T;
    message?: string;
  };
  message?: string;
  error?: string;
  status_code?: number;
}

// Request Types
interface CreatePostRequest {
  title: string;
  content: string;
  category_id: number;
}

interface CreateCommentRequest {
  content: string;
}

interface VoteRequest {
  vote_type: 'up' | 'down';
}

interface SearchRequest {
  q: string;
  post_type?: 'all' | 'posts' | 'comments';
  limit?: number;
}

// Generic API request function with auth headers
const makeForumRequest = async <T = any>(
  method: string,
  endpoint: string,
  apiKey?: string,
  deviceId?: string,
  data?: any
): Promise<ApiResponse<T>> => {
  try {
    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
      'skip_zrok_interstitial': '1'
    };

    // Add authentication headers if available
    if (apiKey) {
      headers['X-API-Key'] = apiKey;
    }
    if (deviceId) {
      headers['X-Device-ID'] = deviceId;
    }

    const url = getApiEndpoint(endpoint);
    console.log('🔥 Forum API Request:', {
      method,
      url,
      endpoint,
      hasApiKey: !!apiKey,
      hasDeviceId: !!deviceId,
      data
    });

    const config: RequestInit = {
      method,
      headers,
      mode: 'cors',
    };

    if (data && (method === 'POST' || method === 'PUT')) {
      config.body = JSON.stringify(data);
    }

    const response = await fetch(url, config);

    console.log('📡 Forum Response:', {
      status: response.status,
      statusText: response.statusText,
      url: response.url
    });

    const responseText = await response.text();
    console.log('📄 Forum Response Text:', responseText.substring(0, 200));

    let responseData;
    try {
      responseData = JSON.parse(responseText);
      console.log('✅ Forum Parsed JSON:', responseData);
    } catch (parseError) {
      console.error('❌ Forum JSON parse failed:', parseError);
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
    console.error('💥 Forum Request failed:', error);

    if (error instanceof TypeError && error.message.includes('Failed to fetch')) {
      return {
        success: false,
        error: 'Network error: Unable to connect to server. Please check your internet connection or try again later.'
      };
    }

    return {
      success: false,
      error: error instanceof Error ? error.message : 'Network error'
    };
  }
};

// Forum API Service Class
export class ForumApiService {
  
  // Get all forum categories
  static async getCategories(apiKey?: string, deviceId?: string): Promise<ApiResponse<{ categories: ForumCategory[] }>> {
    return makeForumRequest<{ categories: ForumCategory[] }>('GET', '/forum/categories', apiKey, deviceId);
  }

  // Get posts by category
  static async getPostsByCategory(
    categoryId: number, 
    sortBy: string = 'latest', 
    limit: number = 20,
    apiKey?: string, 
    deviceId?: string
  ): Promise<ApiResponse<{ posts: ForumPost[]; category: ForumCategory }>> {
    return makeForumRequest<{ posts: ForumPost[]; category: ForumCategory }>(
      'GET', 
      `/forum/categories/${categoryId}/posts?sort_by=${sortBy}&limit=${limit}`, 
      apiKey, 
      deviceId
    );
  }

  // Get all posts (recent/trending)
  static async getAllPosts(
    sortBy: string = 'latest', 
    limit: number = 20,
    apiKey?: string, 
    deviceId?: string
  ): Promise<ApiResponse<{ posts: ForumPost[] }>> {
    return makeForumRequest<{ posts: ForumPost[] }>(
      'GET', 
      `/forum/posts?sort_by=${sortBy}&limit=${limit}`, 
      apiKey, 
      deviceId
    );
  }

  // Get post details with comments
  static async getPostDetails(
    postUuid: string, 
    apiKey?: string, 
    deviceId?: string
  ): Promise<ApiResponse<PostDetails>> {
    return makeForumRequest<PostDetails>('GET', `/forum/posts/${postUuid}`, apiKey, deviceId);
  }

  // Create new post (note: endpoint requires category_id in path according to OpenAPI spec)
  static async createPost(
    categoryId: number,
    request: Omit<CreatePostRequest, 'category_id'>,
    apiKey: string,
    deviceId?: string
  ): Promise<ApiResponse<{ post: ForumPost }>> {
    return makeForumRequest<{ post: ForumPost }>(
      'POST',
      `/forum/categories/${categoryId}/posts`,
      apiKey,
      deviceId,
      { ...request, category_id: categoryId }
    );
  }

  // Add comment to post
  static async addComment(
    postUuid: string, 
    request: CreateCommentRequest, 
    apiKey: string, 
    deviceId?: string
  ): Promise<ApiResponse<{ comment: ForumComment }>> {
    return makeForumRequest<{ comment: ForumComment }>(
      'POST', 
      `/forum/posts/${postUuid}/comments`, 
      apiKey, 
      deviceId, 
      request
    );
  }

  // Vote on post
  static async voteOnPost(
    postUuid: string, 
    voteType: 'up' | 'down', 
    apiKey: string, 
    deviceId?: string
  ): Promise<ApiResponse<{ action: string; vote_score: number }>> {
    return makeForumRequest<{ action: string; vote_score: number }>(
      'POST', 
      `/forum/posts/${postUuid}/vote`, 
      apiKey, 
      deviceId, 
      { vote_type: voteType }
    );
  }

  // Vote on comment
  static async voteOnComment(
    commentUuid: string, 
    voteType: 'up' | 'down', 
    apiKey: string, 
    deviceId?: string
  ): Promise<ApiResponse<{ action: string; vote_score: number }>> {
    return makeForumRequest<{ action: string; vote_score: number }>(
      'POST', 
      `/forum/comments/${commentUuid}/vote`, 
      apiKey, 
      deviceId, 
      { vote_type: voteType }
    );
  }

  // Search forum posts
  static async searchPosts(
    query: string, 
    postType: string = 'all', 
    limit: number = 20,
    apiKey?: string, 
    deviceId?: string
  ): Promise<ApiResponse<{ results: { posts: ForumPost[] } }>> {
    return makeForumRequest<{ results: { posts: ForumPost[] } }>(
      'GET', 
      `/forum/search?q=${encodeURIComponent(query)}&post_type=${postType}&limit=${limit}`, 
      apiKey, 
      deviceId
    );
  }

  // Get forum statistics
  static async getForumStats(apiKey?: string, deviceId?: string): Promise<ApiResponse<ForumStats>> {
    return makeForumRequest<ForumStats>('GET', '/forum/stats', apiKey, deviceId);
  }

  // Get user's posts
  static async getUserPosts(apiKey: string, deviceId?: string): Promise<ApiResponse<{ posts: ForumPost[] }>> {
    return makeForumRequest<{ posts: ForumPost[] }>('GET', '/forum/user/posts', apiKey, deviceId);
  }

  // Get user's activity (posts and comments)
  static async getUserActivity(apiKey: string, deviceId?: string): Promise<ApiResponse<{
    posts: ForumPost[];
    comments: ForumComment[];
    total_votes_received: number;
  }>> {
    return makeForumRequest<{
      posts: ForumPost[];
      comments: ForumComment[];
      total_votes_received: number;
    }>('GET', '/forum/user/activity', apiKey, deviceId);
  }

  // Delete post (if user is author)
  static async deletePost(
    postUuid: string, 
    apiKey: string, 
    deviceId?: string
  ): Promise<ApiResponse<{ message: string }>> {
    return makeForumRequest<{ message: string }>('DELETE', `/forum/posts/${postUuid}`, apiKey, deviceId);
  }

  // Delete comment (if user is author)
  static async deleteComment(
    commentUuid: string, 
    apiKey: string, 
    deviceId?: string
  ): Promise<ApiResponse<{ message: string }>> {
    return makeForumRequest<{ message: string }>('DELETE', `/forum/comments/${commentUuid}`, apiKey, deviceId);
  }

  // Edit post (if user is author)
  static async editPost(
    postUuid: string, 
    updates: { title?: string; content?: string }, 
    apiKey: string, 
    deviceId?: string
  ): Promise<ApiResponse<{ post: ForumPost }>> {
    return makeForumRequest<{ post: ForumPost }>('PUT', `/forum/posts/${postUuid}`, apiKey, deviceId, updates);
  }

  // Edit comment (if user is author)
  static async editComment(
    commentUuid: string, 
    content: string, 
    apiKey: string, 
    deviceId?: string
  ): Promise<ApiResponse<{ comment: ForumComment }>> {
    return makeForumRequest<{ comment: ForumComment }>(
      'PUT', 
      `/forum/comments/${commentUuid}`, 
      apiKey, 
      deviceId, 
      { content }
    );
  }

  // Pin/Unpin post (admin only)
  static async togglePostPin(
    postUuid: string,
    apiKey: string,
    deviceId?: string
  ): Promise<ApiResponse<{ post: ForumPost }>> {
    return makeForumRequest<{ post: ForumPost }>('POST', `/forum/posts/${postUuid}/toggle-pin`, apiKey, deviceId);
  }

  // Get trending posts
  static async getTrendingPosts(
    limit: number = 10,
    timeframe: 'day' | 'week' | 'month' = 'week',
    apiKey?: string,
    deviceId?: string
  ): Promise<ApiResponse<{ posts: ForumPost[] }>> {
    return makeForumRequest<{ posts: ForumPost[] }>(
      'GET',
      `/forum/posts/trending?limit=${limit}&timeframe=${timeframe}`,
      apiKey,
      deviceId
    );
  }

  // Get forum user profile by username
  static async getUserProfile(
    username: string,
    apiKey: string,
    deviceId?: string
  ): Promise<ApiResponse<{
    username: string;
    display_name: string;
    bio: string;
    avatar_color: string;
    signature: string;
    post_count: number;
    comment_count: number;
    reputation: number;
    joined_at: string;
  }>> {
    return makeForumRequest(
      'GET',
      `/forum/profile/${username}`,
      apiKey,
      deviceId
    );
  }

  // Get current user's forum profile
  static async getMyProfile(
    apiKey: string,
    deviceId?: string
  ): Promise<ApiResponse<{
    username: string;
    display_name: string;
    bio: string;
    avatar_color: string;
    signature: string;
    show_email: boolean;
    email_notifications: boolean;
    post_count: number;
    comment_count: number;
    reputation: number;
    joined_at: string;
  }>> {
    return makeForumRequest(
      'GET',
      '/forum/profile',
      apiKey,
      deviceId
    );
  }

  // Update forum profile
  static async updateProfile(
    updates: {
      display_name?: string;
      bio?: string;
      avatar_color?: string;
      signature?: string;
      show_email?: boolean;
      email_notifications?: boolean;
    },
    apiKey: string,
    deviceId?: string
  ): Promise<ApiResponse<{ message: string; profile: any }>> {
    return makeForumRequest(
      'PUT',
      '/forum/profile',
      apiKey,
      deviceId,
      updates
    );
  }

  // Create forum category (admin only)
  static async createCategory(
    categoryData: {
      name: string;
      description: string;
      icon?: string;
      color?: string;
    },
    apiKey: string,
    deviceId?: string
  ): Promise<ApiResponse<{ category: ForumCategory }>> {
    return makeForumRequest<{ category: ForumCategory }>(
      'POST',
      '/forum/admin/categories',
      apiKey,
      deviceId,
      categoryData
    );
  }

  // Get admin forum statistics
  static async getAdminStatistics(
    apiKey: string,
    deviceId?: string
  ): Promise<ApiResponse<{
    total_posts: number;
    total_comments: number;
    total_users: number;
    posts_today: number;
    comments_today: number;
    active_users_today: number;
    most_active_categories: Array<{ category_name: string; post_count: number }>;
    recent_activity: Array<any>;
  }>> {
    return makeForumRequest(
      'GET',
      '/forum/admin/statistics',
      apiKey,
      deviceId
    );
  }
}

// Utility functions for forum operations
export const ForumUtils = {
  
  // Format timestamp for display
  formatTimestamp: (timestamp: string): string => {
    try {
      if (!timestamp) return 'Recent';
      
      const date = timestamp.endsWith('Z') 
        ? new Date(timestamp.replace('Z', '+00:00')) 
        : new Date(timestamp);
        
      return date.toLocaleString('en-US', {
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit',
        hour12: false
      });
    } catch (error) {
      return 'Recent';
    }
  },

  // Calculate vote score
  calculateVoteScore: (likes: number, dislikes: number): number => {
    return likes - dislikes;
  },

  // Get vote color based on score
  getVoteColor: (score: number): string => {
    if (score > 0) return 'text-green-400';
    if (score < 0) return 'text-red-400';
    return 'text-zinc-400';
  },

  // Truncate text for preview
  truncateText: (text: string, maxLength: number): string => {
    if (text.length <= maxLength) return text;
    return text.substring(0, maxLength) + '...';
  },

  // Format large numbers
  formatNumber: (num: number): string => {
    if (num >= 1000000) {
      return (num / 1000000).toFixed(1) + 'M';
    } else if (num >= 1000) {
      return (num / 1000).toFixed(1) + 'K';
    }
    return num.toString();
  },

  // Validate post content
  validatePost: (title: string, content: string): { valid: boolean; error?: string } => {
    if (!title.trim()) {
      return { valid: false, error: 'Post title is required' };
    }
    if (title.length > 200) {
      return { valid: false, error: 'Post title must be less than 200 characters' };
    }
    if (!content.trim()) {
      return { valid: false, error: 'Post content is required' };
    }
    if (content.length > 10000) {
      return { valid: false, error: 'Post content must be less than 10,000 characters' };
    }
    return { valid: true };
  },

  // Validate comment content
  validateComment: (content: string): { valid: boolean; error?: string } => {
    if (!content.trim()) {
      return { valid: false, error: 'Comment content is required' };
    }
    if (content.length > 2000) {
      return { valid: false, error: 'Comment must be less than 2,000 characters' };
    }
    return { valid: true };
  }
};

export default ForumApiService;