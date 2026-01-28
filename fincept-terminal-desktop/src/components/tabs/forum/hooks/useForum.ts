// Hook for managing forum data and API calls

import { useState, useEffect, useCallback, useRef } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { ForumApiService, ForumPost as APIForumPost, ForumCategory, ForumStats, ForumComment as APIForumComment } from '@/services/forum/forumApi';
import { sqliteService, getSetting, saveSetting } from '@/services/core/sqliteService';
import type { ForumPost, ForumComment, ForumCategoryUI, TrendingTopic, RecentActivity, ForumColors } from '../types';
import { CATEGORY_COLORS } from '../constants';

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

export function useForum({ colors }: UseForumProps): UseForumReturn {
  const { session } = useAuth();

  // Data states
  const [categories, setCategories] = useState<ForumCategoryUI[]>([]);
  const [forumPosts, setForumPosts] = useState<ForumPost[]>([]);
  const [forumStats, setForumStats] = useState<ForumStats | null>(null);
  const [trendingTopics, setTrendingTopics] = useState<TrendingTopic[]>([]);
  const [recentActivity, setRecentActivity] = useState<RecentActivity[]>([]);
  const [postComments, setPostComments] = useState<ForumComment[]>([]);
  const [searchResults, setSearchResults] = useState<ForumPost[]>([]);
  const [userProfile, setUserProfile] = useState<any>(null);

  // UI states
  const [activeCategory, setActiveCategory] = useState('ALL');
  const [sortBy, setSortBy] = useState('latest');
  const [onlineUsers, setOnlineUsers] = useState(0);
  const [isLoading, setIsLoading] = useState(false);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [lastRefreshTime, setLastRefreshTime] = useState<Date | null>(null);

  // Selected data
  const [selectedPost, setSelectedPost] = useState<ForumPost | null>(null);
  const [selectedUsername, setSelectedUsername] = useState('');

  // Auto-refresh interval ref
  const autoRefreshIntervalRef = useRef<NodeJS.Timeout | null>(null);

  // Get API credentials
  const getApiCredentials = useCallback(async () => {
    const apiKey = session?.api_key || await getSetting('fincept_api_key') || undefined;
    const deviceId = session?.device_id || await getSetting('fincept_device_id') || undefined;
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

  // Fetch forum categories
  const fetchCategories = useCallback(async (forceRefresh: boolean = false) => {
    try {
      if (!forceRefresh) {
        const cachedCategories = await sqliteService.getCachedForumCategories(5);
        if (cachedCategories) {
          setCategories(cachedCategories);
          return;
        }
      }

      const { apiKey, deviceId } = await getApiCredentials();
      const response = await ForumApiService.getCategories(apiKey, deviceId);

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

        setCategories(formattedCategories);
        await sqliteService.cacheForumCategories(formattedCategories);
      }
    } catch (error) {
      console.debug('Forum API not available (categories)');
    }
  }, [getApiCredentials, colors]);

  // Fetch forum posts
  const fetchPosts = useCallback(async (forceRefresh: boolean = false) => {
    setIsLoading(true);
    try {
      const categoryId = activeCategory === 'ALL' ? null : categories.find(c => c.name === activeCategory)?.id || null;
      const { apiKey, deviceId } = await getApiCredentials();
      let response;

      if (activeCategory === 'ALL') {
        const allPosts: any[] = [];
        for (const cat of categories.filter(c => c.id !== undefined)) {
          const catResponse = await ForumApiService.getPostsByCategory(cat.id, sortBy, 20, apiKey, deviceId);
          if (catResponse?.success && catResponse.data) {
            const posts = (catResponse.data as any).data?.posts || [];
            allPosts.push(...posts);
          }
        }
        response = { success: true, data: { data: { posts: allPosts } } };
      } else {
        if (categoryId) {
          response = await ForumApiService.getPostsByCategory(categoryId, sortBy, 20, apiKey, deviceId);
        }
      }

      if (response?.success && response.data) {
        const responseData = response.data as any;
        const posts = responseData.data?.posts || [];

        if (posts.length > 0) {
          const formattedPosts = posts.map(convertApiPostToUIFormat);
          setForumPosts(formattedPosts);
          await sqliteService.cacheForumPosts(categoryId, sortBy, formattedPosts);
        } else {
          setForumPosts([]);
        }
      }
    } catch (error) {
      console.debug('Forum API not available (posts)');
    } finally {
      setIsLoading(false);
    }
  }, [activeCategory, sortBy, categories, getApiCredentials, convertApiPostToUIFormat]);

  // Fetch forum statistics
  const fetchForumStats = useCallback(async (forceRefresh: boolean = false) => {
    try {
      if (!forceRefresh) {
        const cachedStats = await sqliteService.getCachedForumStats(5);
        if (cachedStats) {
          setForumStats(cachedStats);
          if (cachedStats.active_users) {
            setOnlineUsers(cachedStats.active_users);
          }
          return;
        }
      }

      const { apiKey, deviceId } = await getApiCredentials();
      const response = await ForumApiService.getForumStats(apiKey, deviceId);

      if (response.success && response.data?.data) {
        setForumStats(response.data.data);
        if (response.data.data.active_users) {
          setOnlineUsers(response.data.data.active_users);
        }
        await sqliteService.cacheForumStats(response.data.data);
      }
    } catch (error) {
      console.debug('Forum API not available (stats)');
    }
  }, [getApiCredentials]);

  // Create new post
  const handleCreatePost = useCallback(async (title: string, content: string): Promise<boolean> => {
    if (!title.trim() || title.trim().length < 5 || title.trim().length > 500) {
      alert('Post title must be between 5 and 500 characters');
      return false;
    }
    if (!content.trim() || content.trim().length < 10) {
      alert('Post content must be at least 10 characters long');
      return false;
    }

    const { apiKey, deviceId } = await getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to create posts');
      return false;
    }

    const categoryId = categories.find(c => c.name === activeCategory)?.id || categories[1]?.id || 1;

    try {
      const response = await ForumApiService.createPost(
        categoryId,
        { title: title.trim(), content: content.trim() },
        apiKey,
        deviceId
      );

      if (response.success) {
        alert('Post created successfully!');
        fetchPosts(true);
        return true;
      } else {
        alert(`Failed to create post: ${response.error || response.data?.message || 'Unknown error'}`);
        return false;
      }
    } catch (error) {
      alert('Error creating post');
      return false;
    }
  }, [getApiCredentials, categories, activeCategory, fetchPosts]);

  // View post details with comments
  const handleViewPost = useCallback(async (post: ForumPost) => {
    setSelectedPost(post);

    const { apiKey, deviceId } = await getApiCredentials();
    try {
      const response = await ForumApiService.getPostDetails(post.id, apiKey, deviceId);

      if (response.success && response.data?.data) {
        const comments = response.data.data.comments || [];
        const formattedComments = comments.map(convertApiCommentToUIFormat);
        setPostComments(formattedComments);
      }
    } catch (error) {
      console.debug('Forum API not available (post details)');
    }
  }, [getApiCredentials, convertApiCommentToUIFormat]);

  // Add comment to post
  const handleAddComment = useCallback(async (comment: string): Promise<boolean> => {
    if (!comment.trim() || !selectedPost) return false;

    const { apiKey, deviceId } = await getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to comment');
      return false;
    }

    try {
      const response = await ForumApiService.addComment(
        selectedPost.id,
        { content: comment },
        apiKey,
        deviceId
      );

      if (response.success) {
        handleViewPost(selectedPost);
        return true;
      } else {
        alert(`Failed to add comment: ${response.error || 'Unknown error'}`);
        return false;
      }
    } catch (error) {
      alert('Error adding comment');
      return false;
    }
  }, [getApiCredentials, selectedPost, handleViewPost]);

  // Vote on post
  const handleVotePost = useCallback(async (postId: string, voteType: 'up' | 'down') => {
    const { apiKey, deviceId } = await getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to vote');
      return;
    }

    try {
      const response = await ForumApiService.voteOnPost(postId, voteType, apiKey, deviceId);

      if (response.success) {
        fetchPosts(true);
        if (selectedPost && selectedPost.id === postId) {
          handleViewPost(selectedPost);
        }
      } else {
        alert(`Vote failed: ${response.error || 'Unknown error'}`);
      }
    } catch (error) {
      // Silently handle error
    }
  }, [getApiCredentials, fetchPosts, selectedPost, handleViewPost]);

  // Vote on comment
  const handleVoteComment = useCallback(async (commentId: string, voteType: 'up' | 'down') => {
    const { apiKey, deviceId } = await getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to vote');
      return;
    }

    try {
      const response = await ForumApiService.voteOnComment(commentId, voteType, apiKey, deviceId);

      if (response.success && selectedPost) {
        handleViewPost(selectedPost);
      }
    } catch (error) {
      // Silently handle error
    }
  }, [getApiCredentials, selectedPost, handleViewPost]);

  // Unified vote handler (delegates to post or comment)
  const handleVote = useCallback(async (itemId: string, voteType: 'up' | 'down', itemType: 'post' | 'comment') => {
    if (itemType === 'post') {
      await handleVotePost(itemId, voteType);
    } else {
      await handleVoteComment(itemId, voteType);
    }
  }, [handleVotePost, handleVoteComment]);

  // Search forum
  const handleSearch = useCallback(async (query: string) => {
    if (!query.trim()) return;

    const { apiKey, deviceId } = await getApiCredentials();
    setIsLoading(true);

    try {
      const response = await ForumApiService.searchPosts(query, 'all', 20, apiKey, deviceId);

      if (response.success && response.data?.data?.results?.posts) {
        const posts = response.data.data.results.posts;
        const formattedPosts = posts.map(convertApiPostToUIFormat);
        setSearchResults(formattedPosts);
      }
    } catch (error) {
      // Silently handle error
    } finally {
      setIsLoading(false);
    }
  }, [getApiCredentials, convertApiPostToUIFormat]);

  // View my profile
  const handleViewMyProfile = useCallback(async () => {
    const { apiKey, deviceId } = await getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to view profile');
      return;
    }

    try {
      const response = await ForumApiService.getMyProfile(apiKey, deviceId);

      if (response.success && response.data?.data) {
        setUserProfile(response.data.data);
        setSelectedUsername('');
      }
    } catch (error) {
      alert('Error loading profile');
    }
  }, [getApiCredentials]);

  // View user profile
  const handleViewUserProfile = useCallback(async (username: string) => {
    const { apiKey, deviceId } = await getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to view profiles');
      return;
    }

    setSelectedUsername(username);

    try {
      const response = await ForumApiService.getUserProfile(username, apiKey, deviceId);

      if (response.success && response.data?.data) {
        setUserProfile(response.data.data);
      }
    } catch (error) {
      alert('Error loading user profile');
    }
  }, [getApiCredentials]);

  // Update profile
  const handleUpdateProfile = useCallback(async (profileEdit: any): Promise<boolean> => {
    const { apiKey, deviceId } = await getApiCredentials();
    if (!apiKey) return false;

    try {
      const response = await ForumApiService.updateProfile(profileEdit, apiKey, deviceId);

      if (response.success) {
        alert('Profile updated successfully!');
        handleViewMyProfile();
        return true;
      } else {
        alert(`Failed to update profile: ${response.error || 'Unknown error'}`);
        return false;
      }
    } catch (error) {
      alert('Error updating profile');
      return false;
    }
  }, [getApiCredentials, handleViewMyProfile]);

  // Manual refresh
  const handleRefresh = useCallback(async () => {
    if (isRefreshing) return;

    setIsRefreshing(true);

    try {
      await Promise.all([
        fetchCategories(true),
        fetchForumStats(true),
        fetchPosts(true)
      ]);

      setLastRefreshTime(new Date());
    } catch (error) {
      // Silently handle error
    } finally {
      setIsRefreshing(false);
    }
  }, [isRefreshing, fetchCategories, fetchForumStats, fetchPosts]);

  // Generate trending topics from posts
  const generateTrendingTopics = useCallback(() => {
    const topicsMap: { [key: string]: { count: number; sentiment: string } } = {};

    forumPosts.forEach(post => {
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

    setTrendingTopics(trending);
  }, [forumPosts]);

  // Generate recent activity from posts
  const generateRecentActivity = useCallback(() => {
    const activities = forumPosts.slice(0, 5).map((post) => {
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

    setRecentActivity(activities);
  }, [forumPosts]);

  // Initial load
  useEffect(() => {
    const initForum = async () => {
      fetchCategories();
      fetchForumStats();

      const selectedPostId = await getSetting('forum_selected_post_id');
      if (selectedPostId) {
        await saveSetting('forum_selected_post_id', '', 'forum');
        setTimeout(async () => {
          const post = forumPosts.find(p => p.id === selectedPostId);
          if (post) {
            handleViewPost(post);
          } else {
            try {
              const { apiKey, deviceId } = await getApiCredentials();
              const response = await ForumApiService.getPostDetails(selectedPostId, apiKey, deviceId);
              if (response.success && response.data) {
                const apiPost = (response.data as any).data?.post || (response.data as any).post;
                const post = convertApiPostToUIFormat(apiPost);
                setSelectedPost(post);
                const comments = ((response.data as any).data?.comments || (response.data as any).comments || []).map(convertApiCommentToUIFormat);
                setPostComments(comments);
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
    if (categories.length > 0) {
      fetchPosts();
    }
  }, [activeCategory, sortBy, categories, fetchPosts]);

  // Generate derived data
  useEffect(() => {
    if (forumPosts.length > 0) {
      generateTrendingTopics();
      generateRecentActivity();
    }
  }, [forumPosts, generateTrendingTopics, generateRecentActivity]);

  // Online users are updated from API stats only (removed fake random updates)

  // Auto-refresh every 5 minutes
  useEffect(() => {
    const autoRefreshInterval = setInterval(() => {
      handleRefresh();
    }, 5 * 60 * 1000);

    autoRefreshIntervalRef.current = autoRefreshInterval;

    return () => {
      if (autoRefreshIntervalRef.current) {
        clearInterval(autoRefreshIntervalRef.current);
      }
    };
  }, [handleRefresh]);

  // Computed values
  const filteredPosts = activeCategory === 'ALL'
    ? forumPosts
    : forumPosts.filter(post => post.category === activeCategory);

  const totalPosts = forumStats?.total_posts || categories.find(c => c.name === 'ALL')?.count || 0;
  const postsToday = forumStats?.posts_today || 0;

  return {
    categories,
    forumPosts,
    filteredPosts,
    forumStats,
    trendingTopics,
    recentActivity,
    postComments,
    searchResults,
    userProfile,
    activeCategory,
    setActiveCategory,
    sortBy,
    setSortBy,
    onlineUsers,
    isLoading,
    isRefreshing,
    lastRefreshTime,
    selectedPost,
    setSelectedPost,
    setPostComments,
    selectedUsername,
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
