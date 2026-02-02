// Hook for managing forum data and API calls
// Uses: State machine, cleanup, timeout, validation

import { useReducer, useEffect, useCallback, useRef } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { ForumApiService, ForumPost as APIForumPost, ForumCategory, ForumStats, ForumComment as APIForumComment } from '@/services/forum/forumApi';
import { sqliteService, getSetting, saveSetting } from '@/services/core/sqliteService';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import type { ForumPost, ForumComment, ForumCategoryUI, TrendingTopic, RecentActivity, ForumColors } from '../types';
import { CATEGORY_COLORS } from '../constants';
import { showWarning, showSuccess, showError } from '@/utils/notifications';

// ============================================================================
// Constants
// ============================================================================

const API_TIMEOUT_MS = 30000;
const CACHE_TIMEOUT_MS = 5000;

interface UseForumProps {
  colors: ForumColors;
}

export interface UseForumReturn {
  // Data states
  categories: ForumCategoryUI[];
  forumPosts: ForumPost[];
  filteredPosts: ForumPost[];
  forumStats: ForumStats | null;
  trendingTopics: TrendingTopic[];
  recentActivity: RecentActivity[];
  postComments: ForumComment[];
  searchResults: ForumPost[];
  userProfile: any;

  // UI states
  activeCategory: string;
  setActiveCategory: (category: string) => void;
  sortBy: string;
  setSortBy: (sort: string) => void;
  onlineUsers: number;
  isLoading: boolean;
  isRefreshing: boolean;
  lastRefreshTime: Date | null;

  // Selected data
  selectedPost: ForumPost | null;
  setSelectedPost: (post: ForumPost | null) => void;
  setPostComments: (comments: ForumComment[]) => void;
  selectedUsername: string;
  setSelectedUsername: (username: string) => void;

  // Actions
  fetchCategories: (forceRefresh?: boolean) => Promise<void>;
  fetchPosts: (forceRefresh?: boolean) => Promise<void>;
  fetchForumStats: (forceRefresh?: boolean) => Promise<void>;
  handleCreatePost: (title: string, content: string) => Promise<boolean>;
  handleViewPost: (post: ForumPost) => Promise<void>;
  handleAddComment: (comment: string) => Promise<boolean>;
  handleVote: (itemId: string, voteType: 'up' | 'down', itemType: 'post' | 'comment') => Promise<void>;
  handleVotePost: (postId: string, voteType: 'up' | 'down') => Promise<void>;
  handleVoteComment: (commentId: string, voteType: 'up' | 'down') => Promise<void>;
  handleSearch: (query: string) => Promise<void>;
  handleViewMyProfile: () => Promise<void>;
  handleViewUserProfile: (username: string) => Promise<void>;
  handleUpdateProfile: (profileEdit: any) => Promise<boolean>;
  handleRefresh: () => Promise<void>;

  // Computed values
  totalPosts: number;
  postsToday: number;
}

// ============================================================================
// State Machine
// ============================================================================

type ForumStatus = 'idle' | 'loading' | 'refreshing' | 'ready' | 'error';

interface ForumState {
  status: ForumStatus;
  categories: ForumCategoryUI[];
  forumPosts: ForumPost[];
  forumStats: ForumStats | null;
  trendingTopics: TrendingTopic[];
  recentActivity: RecentActivity[];
  postComments: ForumComment[];
  searchResults: ForumPost[];
  userProfile: any;
  activeCategory: string;
  sortBy: string;
  onlineUsers: number;
  lastRefreshTime: Date | null;
  selectedPost: ForumPost | null;
  selectedUsername: string;
  error: string | null;
}

type ForumAction =
  | { type: 'START_LOADING' }
  | { type: 'START_REFRESHING' }
  | { type: 'SET_READY' }
  | { type: 'SET_ERROR'; error: string }
  | { type: 'CLEAR_ERROR' }
  | { type: 'SET_CATEGORIES'; categories: ForumCategoryUI[] }
  | { type: 'SET_POSTS'; posts: ForumPost[] }
  | { type: 'SET_STATS'; stats: ForumStats }
  | { type: 'SET_ONLINE_USERS'; count: number }
  | { type: 'SET_TRENDING'; topics: TrendingTopic[] }
  | { type: 'SET_ACTIVITY'; activity: RecentActivity[] }
  | { type: 'SET_COMMENTS'; comments: ForumComment[] }
  | { type: 'SET_SEARCH_RESULTS'; results: ForumPost[] }
  | { type: 'SET_PROFILE'; profile: any }
  | { type: 'SET_ACTIVE_CATEGORY'; category: string }
  | { type: 'SET_SORT_BY'; sortBy: string }
  | { type: 'SET_SELECTED_POST'; post: ForumPost | null }
  | { type: 'SET_SELECTED_USERNAME'; username: string }
  | { type: 'REFRESH_COMPLETE'; time: Date };

const initialState: ForumState = {
  status: 'idle',
  categories: [],
  forumPosts: [],
  forumStats: null,
  trendingTopics: [],
  recentActivity: [],
  postComments: [],
  searchResults: [],
  userProfile: null,
  activeCategory: 'ALL',
  sortBy: 'latest',
  onlineUsers: 0,
  lastRefreshTime: null,
  selectedPost: null,
  selectedUsername: '',
  error: null,
};

function forumReducer(state: ForumState, action: ForumAction): ForumState {
  switch (action.type) {
    case 'START_LOADING':
      return { ...state, status: 'loading', error: null };
    case 'START_REFRESHING':
      return { ...state, status: 'refreshing', error: null };
    case 'SET_READY':
      return { ...state, status: 'ready' };
    case 'SET_ERROR':
      return { ...state, status: 'error', error: action.error };
    case 'CLEAR_ERROR':
      return { ...state, error: null };
    case 'SET_CATEGORIES':
      return { ...state, categories: action.categories };
    case 'SET_POSTS':
      return { ...state, forumPosts: action.posts, status: 'ready' };
    case 'SET_STATS':
      return { ...state, forumStats: action.stats };
    case 'SET_ONLINE_USERS':
      return { ...state, onlineUsers: action.count };
    case 'SET_TRENDING':
      return { ...state, trendingTopics: action.topics };
    case 'SET_ACTIVITY':
      return { ...state, recentActivity: action.activity };
    case 'SET_COMMENTS':
      return { ...state, postComments: action.comments };
    case 'SET_SEARCH_RESULTS':
      return { ...state, searchResults: action.results };
    case 'SET_PROFILE':
      return { ...state, userProfile: action.profile };
    case 'SET_ACTIVE_CATEGORY':
      return { ...state, activeCategory: action.category };
    case 'SET_SORT_BY':
      return { ...state, sortBy: action.sortBy };
    case 'SET_SELECTED_POST':
      return { ...state, selectedPost: action.post };
    case 'SET_SELECTED_USERNAME':
      return { ...state, selectedUsername: action.username };
    case 'REFRESH_COMPLETE':
      return { ...state, status: 'ready', lastRefreshTime: action.time };
    default:
      return state;
  }
}

export function useForum({ colors }: UseForumProps): UseForumReturn {
  const { session } = useAuth();

  // State machine
  const [state, dispatch] = useReducer(forumReducer, initialState);

  // Refs for cleanup
  const mountedRef = useRef(true);
  const autoRefreshIntervalRef = useRef<NodeJS.Timeout | null>(null);
  const fetchIdRef = useRef(0); // For race condition prevention

  // Get API credentials - centralized from AuthContext
  // No fallback to getSetting needed as AuthContext is the single source of truth
  const getApiCredentials = useCallback(() => {
    const apiKey = session?.api_key || undefined;
    const deviceId = session?.device_id || undefined;
    return { apiKey, deviceId };
  }, [session]);

  // Convert API post to UI format
  const convertApiPostToUIFormat = useCallback((apiPost: APIForumPost): ForumPost => {
    const likes = apiPost.likes || 0;
    const dislikes = apiPost.dislikes || 0;
    const voteScore = likes - dislikes;
    const sentiment: 'BULLISH' | 'BEARISH' | 'NEUTRAL' = voteScore > 10 ? 'BULLISH' : voteScore < -5 ? 'BEARISH' : 'NEUTRAL';
    const priority: 'HOT' | 'TRENDING' | 'NORMAL' = likes > 300 ? 'HOT' : likes > 150 ? 'TRENDING' : 'NORMAL';

    const tagMatches = apiPost.content.match(/#\w+/g) || [];
    const tags = tagMatches.slice(0, 5).map(tag => tag.substring(1));

    return {
      id: apiPost.post_uuid,
      time: new Date(apiPost.created_at).toLocaleTimeString('en-US', { hour12: false }),
      author: apiPost.author_display_name || 'Anonymous',
      title: apiPost.title,
      content: apiPost.content,
      category: apiPost.category_name?.toUpperCase() || 'GENERAL',
      categoryId: apiPost.category_id || 0,
      tags: tags.length > 0 ? tags : ['DISCUSSION'],
      views: apiPost.views || 0,
      replies: apiPost.reply_count || 0,
      likes,
      dislikes,
      sentiment,
      priority,
      verified: likes > 100
    };
  }, []);

  // Convert API comment to UI format
  const convertApiCommentToUIFormat = useCallback((apiComment: APIForumComment): ForumComment => {
    return {
      id: apiComment.comment_uuid,
      author: apiComment.author_display_name || 'Anonymous',
      content: apiComment.content,
      time: new Date(apiComment.created_at).toLocaleTimeString('en-US', { hour12: false }),
      likes: apiComment.likes || 0,
      dislikes: apiComment.dislikes || 0
    };
  }, []);

  // Fetch forum categories with timeout
  const fetchCategories = useCallback(async (forceRefresh: boolean = false) => {
    const currentFetchId = ++fetchIdRef.current;

    try {
      if (!forceRefresh) {
        const cachedCategories = await withTimeout(
          sqliteService.getCachedForumCategories(5),
          CACHE_TIMEOUT_MS,
          'Cache timeout'
        );
        if (cachedCategories && mountedRef.current && currentFetchId === fetchIdRef.current) {
          dispatch({ type: 'SET_CATEGORIES', categories: cachedCategories });
          return;
        }
      }

      const { apiKey, deviceId } = getApiCredentials();
      const response = await withTimeout(
        ForumApiService.getCategories(apiKey, deviceId),
        API_TIMEOUT_MS,
        'Categories fetch timeout'
      );

      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

      if (response.success && response.data?.data?.categories) {
        const apiCategories = response.data.data.categories;
        const categoryColors = CATEGORY_COLORS(colors);

        const formattedCategories = [
          { name: 'ALL', count: apiCategories.reduce((sum: number, cat: ForumCategory) => sum + cat.post_count, 0), color: colors.WHITE, id: 0 },
          ...apiCategories.map((cat: ForumCategory, index: number) => ({
            name: cat.name.toUpperCase(),
            count: cat.post_count,
            color: categoryColors[index % categoryColors.length],
            id: cat.id
          }))
        ];

        dispatch({ type: 'SET_CATEGORIES', categories: formattedCategories });
        await sqliteService.cacheForumCategories(formattedCategories);
      }
    } catch (error) {
      console.debug('Forum API not available (categories):', error);
    }
  }, [getApiCredentials, colors]);

  // Fetch forum posts with timeout
  const fetchPosts = useCallback(async (forceRefresh: boolean = false) => {
    const currentFetchId = ++fetchIdRef.current;

    dispatch({ type: 'START_LOADING' });
    try {
      const categoryId = state.activeCategory === 'ALL' ? null : state.categories.find(c => c.name === state.activeCategory)?.id || null;
      const { apiKey, deviceId } = getApiCredentials();
      let response;

      if (state.activeCategory === 'ALL') {
        const allPosts: any[] = [];
        for (const cat of state.categories.filter(c => c.id !== undefined)) {
          const catResponse = await withTimeout(
            ForumApiService.getPostsByCategory(cat.id, state.sortBy, 20, apiKey, deviceId),
            API_TIMEOUT_MS,
            'Posts fetch timeout'
          );
          if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;
          if (catResponse?.success && catResponse.data) {
            const posts = (catResponse.data as any).data?.posts || [];
            allPosts.push(...posts);
          }
        }
        response = { success: true, data: { data: { posts: allPosts } } };
      } else {
        if (categoryId) {
          response = await withTimeout(
            ForumApiService.getPostsByCategory(categoryId, state.sortBy, 20, apiKey, deviceId),
            API_TIMEOUT_MS,
            'Posts fetch timeout'
          );
        }
      }

      if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

      if (response?.success && response.data) {
        const responseData = response.data as any;
        const posts = responseData.data?.posts || [];

        if (posts.length > 0) {
          const formattedPosts = posts.map(convertApiPostToUIFormat);
          dispatch({ type: 'SET_POSTS', posts: formattedPosts });
          await sqliteService.cacheForumPosts(categoryId, state.sortBy, formattedPosts);
        } else {
          dispatch({ type: 'SET_POSTS', posts: [] });
        }
      } else {
        dispatch({ type: 'SET_READY' });
      }
    } catch (error) {
      if (mountedRef.current) {
        console.debug('Forum API not available (posts):', error);
        dispatch({ type: 'SET_READY' });
      }
    }
  }, [state.activeCategory, state.sortBy, state.categories, getApiCredentials, convertApiPostToUIFormat]);

  // Fetch forum statistics with timeout
  const fetchForumStats = useCallback(async (forceRefresh: boolean = false) => {
    try {
      if (!forceRefresh) {
        const cachedStats = await withTimeout(
          sqliteService.getCachedForumStats(5),
          CACHE_TIMEOUT_MS,
          'Cache timeout'
        );
        if (cachedStats && mountedRef.current) {
          dispatch({ type: 'SET_STATS', stats: cachedStats });
          if (cachedStats.active_users) {
            dispatch({ type: 'SET_ONLINE_USERS', count: cachedStats.active_users });
          }
          return;
        }
      }

      const { apiKey, deviceId } = getApiCredentials();
      const response = await withTimeout(
        ForumApiService.getForumStats(apiKey, deviceId),
        API_TIMEOUT_MS,
        'Stats fetch timeout'
      );

      if (!mountedRef.current) return;

      if (response.success && response.data?.data) {
        dispatch({ type: 'SET_STATS', stats: response.data.data });
        if (response.data.data.active_users) {
          dispatch({ type: 'SET_ONLINE_USERS', count: response.data.data.active_users });
        }
        await sqliteService.cacheForumStats(response.data.data);
      }
    } catch (error) {
      console.debug('Forum API not available (stats):', error);
    }
  }, [getApiCredentials]);

  // Create new post with validation and timeout
  const handleCreatePost = useCallback(async (title: string, content: string): Promise<boolean> => {
    // Sanitize inputs
    const sanitizedTitle = sanitizeInput(title);
    const sanitizedContent = sanitizeInput(content);

    // Validate
    if (!sanitizedTitle || sanitizedTitle.length < 5 || sanitizedTitle.length > 500) {
      showWarning('Post title must be between 5 and 500 characters');
      return false;
    }
    if (!sanitizedContent || sanitizedContent.length < 10) {
      showWarning('Post content must be at least 10 characters long');
      return false;
    }

    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      showWarning('You must be logged in to create posts');
      return false;
    }

    const categoryId = state.categories.find(c => c.name === state.activeCategory)?.id || state.categories[1]?.id || 1;

    try {
      const response = await withTimeout(
        ForumApiService.createPost(
          categoryId,
          { title: sanitizedTitle, content: sanitizedContent },
          apiKey,
          deviceId
        ),
        API_TIMEOUT_MS,
        'Create post timeout'
      );

      if (!mountedRef.current) return false;

      if (response.success) {
        showSuccess('Post created successfully');
        fetchPosts(true);
        return true;
      } else {
        showError('Failed to create post', [
          { label: 'ERROR', value: response.error || response.data?.message || 'Unknown error' }
        ]);
        return false;
      }
    } catch (error) {
      if (mountedRef.current) {
        showError('Error creating post');
      }
      return false;
    }
  }, [getApiCredentials, state.categories, state.activeCategory, fetchPosts]);

  // View post details with comments (with timeout)
  const handleViewPost = useCallback(async (post: ForumPost) => {
    dispatch({ type: 'SET_SELECTED_POST', post });

    const { apiKey, deviceId } = getApiCredentials();
    try {
      const response = await withTimeout(
        ForumApiService.getPostDetails(post.id, apiKey, deviceId),
        API_TIMEOUT_MS,
        'Post details timeout'
      );

      if (!mountedRef.current) return;

      if (response.success && response.data?.data) {
        const comments = response.data.data.comments || [];
        const formattedComments = comments.map(convertApiCommentToUIFormat);
        dispatch({ type: 'SET_COMMENTS', comments: formattedComments });
      }
    } catch (error) {
      console.debug('Forum API not available (post details):', error);
    }
  }, [getApiCredentials, convertApiCommentToUIFormat]);

  // Add comment to post (with validation and timeout)
  const handleAddComment = useCallback(async (comment: string): Promise<boolean> => {
    const sanitizedComment = sanitizeInput(comment);
    if (!sanitizedComment || !state.selectedPost) return false;

    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      showWarning('You must be logged in to comment');
      return false;
    }

    try {
      const response = await withTimeout(
        ForumApiService.addComment(
          state.selectedPost.id,
          { content: sanitizedComment },
          apiKey,
          deviceId
        ),
        API_TIMEOUT_MS,
        'Add comment timeout'
      );

      if (!mountedRef.current) return false;

      if (response.success) {
        handleViewPost(state.selectedPost);
        return true;
      } else {
        showError('Failed to add comment', [
          { label: 'ERROR', value: response.error || 'Unknown error' }
        ]);
        return false;
      }
    } catch (error) {
      if (mountedRef.current) {
        showError('Error adding comment');
      }
      return false;
    }
  }, [getApiCredentials, state.selectedPost, handleViewPost]);

  // Vote on post (with timeout)
  const handleVotePost = useCallback(async (postId: string, voteType: 'up' | 'down') => {
    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      showWarning('You must be logged in to vote');
      return;
    }

    try {
      const response = await withTimeout(
        ForumApiService.voteOnPost(postId, voteType, apiKey, deviceId),
        API_TIMEOUT_MS,
        'Vote timeout'
      );

      if (!mountedRef.current) return;

      if (response.success) {
        fetchPosts(true);
        if (state.selectedPost && state.selectedPost.id === postId) {
          handleViewPost(state.selectedPost);
        }
      } else {
        showError('Vote failed', [
          { label: 'ERROR', value: response.error || 'Unknown error' }
        ]);
      }
    } catch (error) {
      // Silently handle error
    }
  }, [getApiCredentials, fetchPosts, state.selectedPost, handleViewPost]);

  // Vote on comment (with timeout)
  const handleVoteComment = useCallback(async (commentId: string, voteType: 'up' | 'down') => {
    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      showWarning('You must be logged in to vote');
      return;
    }

    try {
      const response = await withTimeout(
        ForumApiService.voteOnComment(commentId, voteType, apiKey, deviceId),
        API_TIMEOUT_MS,
        'Vote timeout'
      );

      if (!mountedRef.current) return;

      if (response.success && state.selectedPost) {
        handleViewPost(state.selectedPost);
      }
    } catch (error) {
      // Silently handle error
    }
  }, [getApiCredentials, state.selectedPost, handleViewPost]);

  // Unified vote handler (delegates to post or comment)
  const handleVote = useCallback(async (itemId: string, voteType: 'up' | 'down', itemType: 'post' | 'comment') => {
    if (itemType === 'post') {
      await handleVotePost(itemId, voteType);
    } else {
      await handleVoteComment(itemId, voteType);
    }
  }, [handleVotePost, handleVoteComment]);

  // Search forum (with validation and timeout)
  const handleSearch = useCallback(async (query: string) => {
    const sanitizedQuery = sanitizeInput(query);
    if (!sanitizedQuery) return;

    const { apiKey, deviceId } = getApiCredentials();
    dispatch({ type: 'START_LOADING' });

    try {
      const response = await withTimeout(
        ForumApiService.searchPosts(sanitizedQuery, 'all', 20, apiKey, deviceId),
        API_TIMEOUT_MS,
        'Search timeout'
      );

      if (!mountedRef.current) return;

      if (response.success && response.data?.data?.results?.posts) {
        const posts = response.data.data.results.posts;
        const formattedPosts = posts.map(convertApiPostToUIFormat);
        dispatch({ type: 'SET_SEARCH_RESULTS', results: formattedPosts });
      }
      dispatch({ type: 'SET_READY' });
    } catch (error) {
      if (mountedRef.current) {
        dispatch({ type: 'SET_READY' });
      }
    }
  }, [getApiCredentials, convertApiPostToUIFormat]);

  // View my profile (with timeout)
  const handleViewMyProfile = useCallback(async () => {
    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      showWarning('You must be logged in to view profile');
      return;
    }

    try {
      const response = await withTimeout(
        ForumApiService.getMyProfile(apiKey, deviceId),
        API_TIMEOUT_MS,
        'Profile fetch timeout'
      );

      if (!mountedRef.current) return;

      if (response.success && response.data?.data) {
        dispatch({ type: 'SET_PROFILE', profile: response.data.data });
        dispatch({ type: 'SET_SELECTED_USERNAME', username: '' });
      }
    } catch (error) {
      if (mountedRef.current) {
        showError('Error loading profile');
      }
    }
  }, [getApiCredentials]);

  // View user profile (with timeout)
  const handleViewUserProfile = useCallback(async (username: string) => {
    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      showWarning('You must be logged in to view profiles');
      return;
    }

    dispatch({ type: 'SET_SELECTED_USERNAME', username });

    try {
      const response = await withTimeout(
        ForumApiService.getUserProfile(username, apiKey, deviceId),
        API_TIMEOUT_MS,
        'Profile fetch timeout'
      );

      if (!mountedRef.current) return;

      if (response.success && response.data?.data) {
        dispatch({ type: 'SET_PROFILE', profile: response.data.data });
      }
    } catch (error) {
      if (mountedRef.current) {
        showError('Error loading user profile');
      }
    }
  }, [getApiCredentials]);

  // Update profile (with validation and timeout)
  const handleUpdateProfile = useCallback(async (profileEdit: any): Promise<boolean> => {
    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) return false;

    // Sanitize profile fields
    const sanitizedProfile = {
      ...profileEdit,
      display_name: sanitizeInput(profileEdit.display_name || ''),
      bio: sanitizeInput(profileEdit.bio || ''),
      signature: sanitizeInput(profileEdit.signature || ''),
    };

    try {
      const response = await withTimeout(
        ForumApiService.updateProfile(sanitizedProfile, apiKey, deviceId),
        API_TIMEOUT_MS,
        'Profile update timeout'
      );

      if (!mountedRef.current) return false;

      if (response.success) {
        showSuccess('Profile updated successfully');
        handleViewMyProfile();
        return true;
      } else {
        showError('Failed to update profile', [
          { label: 'ERROR', value: response.error || 'Unknown error' }
        ]);
        return false;
      }
    } catch (error) {
      if (mountedRef.current) {
        showError('Error updating profile');
      }
      return false;
    }
  }, [getApiCredentials, handleViewMyProfile]);

  // Manual refresh (with deduplication)
  const handleRefresh = useCallback(async () => {
    if (state.status === 'refreshing') return; // Prevent duplicate refreshes

    dispatch({ type: 'START_REFRESHING' });

    try {
      await Promise.all([
        fetchCategories(true),
        fetchForumStats(true),
        fetchPosts(true)
      ]);

      if (mountedRef.current) {
        dispatch({ type: 'REFRESH_COMPLETE', time: new Date() });
      }
    } catch (error) {
      if (mountedRef.current) {
        dispatch({ type: 'SET_READY' });
      }
    }
  }, [state.status, fetchCategories, fetchForumStats, fetchPosts]);

  // Generate trending topics from posts
  const generateTrendingTopics = useCallback(() => {
    const topicsMap: { [key: string]: { count: number; sentiment: string } } = {};

    state.forumPosts.forEach(post => {
      post.tags.forEach(tag => {
        const topic = `#${tag}`;
        if (!topicsMap[topic]) {
          topicsMap[topic] = { count: 0, sentiment: 'NEUTRAL' };
        }
        topicsMap[topic].count += 1;
        if (post.sentiment === 'BULLISH') topicsMap[topic].sentiment = 'BULLISH';
      });
    });

    const trending = Object.entries(topicsMap)
      .map(([topic, data]) => ({
        topic,
        mentions: data.count,
        sentiment: data.sentiment,
        change: ''
      }))
      .sort((a, b) => b.mentions - a.mentions)
      .slice(0, 6);

    dispatch({ type: 'SET_TRENDING', topics: trending });
  }, [state.forumPosts]);

  // Generate recent activity from posts
  const generateRecentActivity = useCallback(() => {
    const activities = state.forumPosts.slice(0, 5).map((post) => {
      const postDate = new Date();
      const timeStr = post.time;

      // Calculate time ago from post time
      const now = new Date();
      const timeParts = timeStr.split(':');
      if (timeParts.length >= 2) {
        const postHour = parseInt(timeParts[0], 10);
        const postMinute = parseInt(timeParts[1], 10);
        postDate.setHours(postHour, postMinute, 0, 0);
      }

      const diffMs = now.getTime() - postDate.getTime();
      const diffMins = Math.floor(diffMs / 60000);
      const timeAgo = diffMins < 60
        ? `${diffMins} min ago`
        : diffMins < 1440
        ? `${Math.floor(diffMins / 60)} hr ago`
        : `${Math.floor(diffMins / 1440)} day ago`;

      return {
        user: post.author,
        action: 'posted',
        target: post.title.substring(0, 30) + (post.title.length > 30 ? '...' : ''),
        time: timeAgo
      };
    });

    dispatch({ type: 'SET_ACTIVITY', activity: activities });
  }, [state.forumPosts]);

  // Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  // Initial load
  useEffect(() => {
    const initForum = async () => {
      fetchCategories();
      fetchForumStats();

      const selectedPostId = await getSetting('forum_selected_post_id');
      if (selectedPostId && mountedRef.current) {
        await saveSetting('forum_selected_post_id', '', 'forum');
        setTimeout(async () => {
          if (!mountedRef.current) return;
          const post = state.forumPosts.find(p => p.id === selectedPostId);
          if (post) {
            handleViewPost(post);
          } else {
            try {
              const { apiKey, deviceId } = getApiCredentials();
              const response = await withTimeout(
                ForumApiService.getPostDetails(selectedPostId, apiKey, deviceId),
                API_TIMEOUT_MS,
                'Post details timeout'
              );
              if (!mountedRef.current) return;
              if (response.success && response.data) {
                const apiPost = (response.data as any).data?.post || (response.data as any).post;
                const post = convertApiPostToUIFormat(apiPost);
                dispatch({ type: 'SET_SELECTED_POST', post });
                const comments = ((response.data as any).data?.comments || (response.data as any).comments || []).map(convertApiCommentToUIFormat);
                dispatch({ type: 'SET_COMMENTS', comments });
              }
            } catch (err) {
              // Silently handle error
            }
          }
        }, 1000);
      }
    };
    initForum();
  }, []);

  // Fetch posts when category or sort changes
  useEffect(() => {
    if (state.categories.length > 0) {
      fetchPosts();
    }
  }, [state.activeCategory, state.sortBy, state.categories.length, fetchPosts]);

  // Generate derived data
  useEffect(() => {
    if (state.forumPosts.length > 0 && mountedRef.current) {
      generateTrendingTopics();
      generateRecentActivity();
    }
  }, [state.forumPosts, generateTrendingTopics, generateRecentActivity]);

  // Auto-refresh every 5 minutes with cleanup
  useEffect(() => {
    const autoRefreshInterval = setInterval(() => {
      if (mountedRef.current) {
        handleRefresh();
      }
    }, 5 * 60 * 1000);

    autoRefreshIntervalRef.current = autoRefreshInterval;

    return () => {
      if (autoRefreshIntervalRef.current) {
        clearInterval(autoRefreshIntervalRef.current);
        autoRefreshIntervalRef.current = null;
      }
    };
  }, [handleRefresh]);

  // Dispatch wrappers for setters
  const setActiveCategory = useCallback((category: string) => {
    dispatch({ type: 'SET_ACTIVE_CATEGORY', category });
  }, []);

  const setSortBy = useCallback((sortBy: string) => {
    dispatch({ type: 'SET_SORT_BY', sortBy });
  }, []);

  const setSelectedPost = useCallback((post: ForumPost | null) => {
    dispatch({ type: 'SET_SELECTED_POST', post });
  }, []);

  const setPostComments = useCallback((comments: ForumComment[]) => {
    dispatch({ type: 'SET_COMMENTS', comments });
  }, []);

  const setSelectedUsername = useCallback((username: string) => {
    dispatch({ type: 'SET_SELECTED_USERNAME', username });
  }, []);

  // Computed values
  const filteredPosts = state.activeCategory === 'ALL'
    ? state.forumPosts
    : state.forumPosts.filter(post => post.category === state.activeCategory);

  const totalPosts = state.forumStats?.total_posts || state.categories.find(c => c.name === 'ALL')?.count || 0;
  const postsToday = state.forumStats?.posts_today || 0;

  // Derived loading states for backward compatibility
  const isLoading = state.status === 'loading';
  const isRefreshing = state.status === 'refreshing';

  return {
    categories: state.categories,
    forumPosts: state.forumPosts,
    filteredPosts,
    forumStats: state.forumStats,
    trendingTopics: state.trendingTopics,
    recentActivity: state.recentActivity,
    postComments: state.postComments,
    searchResults: state.searchResults,
    userProfile: state.userProfile,
    activeCategory: state.activeCategory,
    setActiveCategory,
    sortBy: state.sortBy,
    setSortBy,
    onlineUsers: state.onlineUsers,
    isLoading,
    isRefreshing,
    lastRefreshTime: state.lastRefreshTime,
    selectedPost: state.selectedPost,
    setSelectedPost,
    setPostComments,
    selectedUsername: state.selectedUsername,
    setSelectedUsername,
    fetchCategories,
    fetchPosts,
    fetchForumStats,
    handleCreatePost,
    handleViewPost,
    handleAddComment,
    handleVote,
    handleVotePost,
    handleVoteComment,
    handleSearch,
    handleViewMyProfile,
    handleViewUserProfile,
    handleUpdateProfile,
    handleRefresh,
    totalPosts,
    postsToday
  };
}
