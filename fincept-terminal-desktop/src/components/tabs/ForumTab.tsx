import React, { useState, useEffect, useRef } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useAuth } from '../../contexts/AuthContext';
import { ForumApiService, ForumPost as APIForumPost, ForumCategory, ForumStats, ForumComment as APIForumComment } from '../../services/forumApi';
import { sqliteService } from '../../services/sqliteService';

interface ForumPost {
  id: string;
  time: string;
  author: string;
  title: string;
  content: string;
  category: string;
  categoryId: number;
  tags: string[];
  views: number;
  replies: number;
  likes: number;
  dislikes: number;
  sentiment: 'BULLISH' | 'BEARISH' | 'NEUTRAL';
  priority: 'HOT' | 'TRENDING' | 'NORMAL';
  verified: boolean;
}

interface ForumComment {
  id: string;
  author: string;
  content: string;
  time: string;
  likes: number;
  dislikes: number;
}

interface ForumUser {
  username: string;
  reputation: number;
  posts: number;
  joined: string;
  status: 'ONLINE' | 'OFFLINE';
}

const ForumTab: React.FC = () => {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const { session } = useAuth();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeCategory, setActiveCategory] = useState('ALL');
  const [sortBy, setSortBy] = useState('latest');
  const [onlineUsers, setOnlineUsers] = useState(1247);
  const [isLoading, setIsLoading] = useState(false);
  const [lastRefreshTime, setLastRefreshTime] = useState<Date | null>(null);
  const [isRefreshing, setIsRefreshing] = useState(false);

  // Auto-refresh interval ref
  const autoRefreshIntervalRef = useRef<NodeJS.Timeout | null>(null);

  // Modal states
  const [showCreatePost, setShowCreatePost] = useState(false);
  const [showPostDetail, setShowPostDetail] = useState(false);
  const [showSearch, setShowSearch] = useState(false);
  const [showProfile, setShowProfile] = useState(false);
  const [showEditProfile, setShowEditProfile] = useState(false);

  // Selected data
  const [selectedPost, setSelectedPost] = useState<ForumPost | null>(null);
  const [postComments, setPostComments] = useState<ForumComment[]>([]);
  const [selectedUsername, setSelectedUsername] = useState<string>('');

  // Form states
  const [newPostTitle, setNewPostTitle] = useState('');
  const [newPostContent, setNewPostContent] = useState('');
  const [newComment, setNewComment] = useState('');
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState<ForumPost[]>([]);

  // Profile states
  const [userProfile, setUserProfile] = useState<any>(null);
  const [profileEdit, setProfileEdit] = useState({
    display_name: '',
    bio: '',
    avatar_color: colors.primary,
    signature: ''
  });

  // API Data states
  const [categories, setCategories] = useState<Array<{ name: string; count: number; color: string; id: number }>>([]);
  const [forumPosts, setForumPosts] = useState<ForumPost[]>([]);
  const [forumStats, setForumStats] = useState<ForumStats | null>(null);
  const [trendingTopics, setTrendingTopics] = useState<Array<{ topic: string; mentions: number; sentiment: string; change: string }>>([]);
  const [recentActivity, setRecentActivity] = useState<Array<{ user: string; action: string; target: string; time: string }>>([]);

  // Bloomberg color scheme - use actual theme colors
  const BLOOMBERG_ORANGE = colors.primary;
  const BLOOMBERG_WHITE = colors.text;
  const BLOOMBERG_RED = colors.alert;
  const BLOOMBERG_GREEN = colors.secondary;
  const BLOOMBERG_YELLOW = colors.warning;
  const BLOOMBERG_GRAY = colors.textMuted;
  const BLOOMBERG_BLUE = colors.info;
  const BLOOMBERG_PURPLE = colors.purple;
  const BLOOMBERG_CYAN = colors.accent;
  const BLOOMBERG_DARK_BG = colors.background;
  const BLOOMBERG_PANEL_BG = colors.panel;

  // Top contributors - static for now
  const topContributors: ForumUser[] = [
    { username: 'QuantTrader_Pro', reputation: 8947, posts: 1234, joined: '2021-03-15', status: 'ONLINE' },
    { username: 'CryptoWhale_2024', reputation: 7823, posts: 892, joined: '2020-11-22', status: 'ONLINE' },
    { username: 'OptionsFlow_Alert', reputation: 7456, posts: 1567, joined: '2022-01-08', status: 'ONLINE' },
    { username: 'MacroHedgeFund', reputation: 6789, posts: 743, joined: '2021-07-19', status: 'OFFLINE' },
    { username: 'TechInvestor88', reputation: 6234, posts: 654, joined: '2022-05-12', status: 'ONLINE' },
    { username: 'EnergyAnalyst_TX', reputation: 5892, posts: 521, joined: '2021-09-30', status: 'ONLINE' }
  ];

  // Get API credentials
  const getApiCredentials = () => {
    const apiKey = session?.api_key || localStorage.getItem('fincept_api_key') || undefined;
    const deviceId = session?.device_id || localStorage.getItem('fincept_device_id') || undefined;
    return { apiKey, deviceId };
  };

  // Convert API post to UI format
  const convertApiPostToUIFormat = (apiPost: APIForumPost): ForumPost => {
    const voteScore = apiPost.likes - apiPost.dislikes;
    const sentiment: 'BULLISH' | 'BEARISH' | 'NEUTRAL' = voteScore > 10 ? 'BULLISH' : voteScore < -5 ? 'BEARISH' : 'NEUTRAL';
    const priority: 'HOT' | 'TRENDING' | 'NORMAL' = apiPost.likes > 300 ? 'HOT' : apiPost.likes > 150 ? 'TRENDING' : 'NORMAL';

    const tagMatches = apiPost.content.match(/#\w+/g) || [];
    const tags = tagMatches.slice(0, 5).map(tag => tag.substring(1));

    return {
      id: apiPost.post_uuid,
      time: new Date(apiPost.created_at).toLocaleTimeString('en-US', { hour12: false }),
      author: apiPost.author_display_name || 'Anonymous',
      title: apiPost.title,
      content: apiPost.content,
      category: apiPost.category_name?.toUpperCase() || 'GENERAL',
      categoryId: apiPost.category_id,
      tags: tags.length > 0 ? tags : ['DISCUSSION'],
      views: apiPost.views || 0,
      replies: apiPost.reply_count || 0,
      likes: apiPost.likes || 0,
      dislikes: apiPost.dislikes || 0,
      sentiment,
      priority,
      verified: apiPost.likes > 100
    };
  };

  // Convert API comment to UI format
  const convertApiCommentToUIFormat = (apiComment: APIForumComment): ForumComment => {
    return {
      id: apiComment.comment_uuid,
      author: apiComment.author_display_name || 'Anonymous',
      content: apiComment.content,
      time: new Date(apiComment.created_at).toLocaleTimeString('en-US', { hour12: false }),
      likes: apiComment.likes || 0,
      dislikes: apiComment.dislikes || 0
    };
  };

  // ENDPOINT 1: Fetch forum categories (with caching)
  const fetchCategories = async (forceRefresh: boolean = false) => {
    try {
      // Try to load from cache first (unless forcing refresh)
      if (!forceRefresh) {
        const cachedCategories = await sqliteService.getCachedForumCategories(5);
        if (cachedCategories) {
          console.log('[Forum] Loaded categories from cache');
          setCategories(cachedCategories);
          return;
        }
      }

      // Fetch from API
      const { apiKey, deviceId } = getApiCredentials();
      const response = await ForumApiService.getCategories(apiKey, deviceId);

      if (response.success && response.data?.data?.categories) {
        const apiCategories = response.data.data.categories;
        const categoryColors = [
          BLOOMBERG_BLUE, BLOOMBERG_PURPLE, BLOOMBERG_CYAN, BLOOMBERG_YELLOW,
          BLOOMBERG_GREEN, BLOOMBERG_ORANGE, BLOOMBERG_RED, BLOOMBERG_PURPLE
        ];

        const formattedCategories = [
          { name: 'ALL', count: apiCategories.reduce((sum: number, cat: ForumCategory) => sum + cat.post_count, 0), color: BLOOMBERG_WHITE, id: 0 },
          ...apiCategories.map((cat: ForumCategory, index: number) => ({
            name: cat.name.toUpperCase(),
            count: cat.post_count,
            color: categoryColors[index % categoryColors.length],
            id: cat.id
          }))
        ];

        setCategories(formattedCategories);

        // Cache the formatted categories
        await sqliteService.cacheForumCategories(formattedCategories);
        console.log('[Forum] Cached categories');
      }
    } catch (error) {
      // Silently fail - backend may not be available
      console.debug('Forum API not available (categories)');
    }
  };

  // ENDPOINT 2 & 6: Fetch forum posts (by category or trending) with caching
  const fetchPosts = async (forceRefresh: boolean = false) => {
    setIsLoading(true);
    try {
      const categoryId = activeCategory === 'ALL' ? null : categories.find(c => c.name === activeCategory)?.id || null;

      // Try to load from cache first (unless forcing refresh)
      if (!forceRefresh) {
        const cachedPosts = await sqliteService.getCachedForumPosts(categoryId, sortBy, 5);
        if (cachedPosts) {
          console.log('[Forum] Loaded posts from cache');
          setForumPosts(cachedPosts);
          setIsLoading(false);
          return;
        }
      }

      // Fetch from API
      const { apiKey, deviceId } = getApiCredentials();
      let response;

      if (activeCategory === 'ALL') {
        if (sortBy === 'popular') {
          response = await ForumApiService.getTrendingPosts(20, 'week', apiKey, deviceId);
        } else {
          const firstCategoryId = categories.find(c => c.name !== 'ALL')?.id || 1;
          response = await ForumApiService.getPostsByCategory(firstCategoryId, sortBy, 20, apiKey, deviceId);
        }
      } else {
        if (categoryId) {
          response = await ForumApiService.getPostsByCategory(categoryId, sortBy, 20, apiKey, deviceId);
        }
      }

      if (response?.success && response.data?.data) {
        const posts = response.data.data.posts || [];
        const formattedPosts = posts.map(convertApiPostToUIFormat);
        setForumPosts(formattedPosts);

        // Cache the formatted posts
        await sqliteService.cacheForumPosts(categoryId, sortBy, formattedPosts);
        console.log('[Forum] Cached posts');
      }
    } catch (error) {
      // Silently fail - backend may not be available
      console.debug('Forum API not available (posts)');
    } finally {
      setIsLoading(false);
    }
  };

  // ENDPOINT 5: Fetch forum statistics (with caching)
  const fetchForumStats = async (forceRefresh: boolean = false) => {
    try {
      // Try to load from cache first (unless forcing refresh)
      if (!forceRefresh) {
        const cachedStats = await sqliteService.getCachedForumStats(5);
        if (cachedStats) {
          console.log('[Forum] Loaded stats from cache');
          setForumStats(cachedStats);
          if (cachedStats.active_users) {
            setOnlineUsers(cachedStats.active_users);
          }
          return;
        }
      }

      // Fetch from API
      const { apiKey, deviceId } = getApiCredentials();
      const response = await ForumApiService.getForumStats(apiKey, deviceId);

      if (response.success && response.data?.data) {
        setForumStats(response.data.data);
        if (response.data.data.active_users) {
          setOnlineUsers(response.data.data.active_users);
        }

        // Cache the stats
        await sqliteService.cacheForumStats(response.data.data);
        console.log('[Forum] Cached stats');
      }
    } catch (error) {
      // Silently fail - backend may not be available
      console.debug('Forum API not available (stats)');
    }
  };

  // ENDPOINT 7: Create new post
  const handleCreatePost = async () => {
    // Validation
    if (!newPostTitle.trim()) {
      alert('Please enter a post title');
      return;
    }
    if (newPostTitle.trim().length < 5) {
      alert('Post title must be at least 5 characters long');
      return;
    }
    if (newPostTitle.trim().length > 500) {
      alert('Post title must be less than 500 characters');
      return;
    }
    if (!newPostContent.trim()) {
      alert('Please enter post content');
      return;
    }
    if (newPostContent.trim().length < 10) {
      alert('Post content must be at least 10 characters long');
      return;
    }

    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to create posts');
      return;
    }

    const categoryId = categories.find(c => c.name === activeCategory)?.id || categories[1]?.id || 1;

    try {
      const response = await ForumApiService.createPost(
        categoryId,
        { title: newPostTitle.trim(), content: newPostContent.trim() },
        apiKey,
        deviceId
      );

      if (response.success) {
        alert('Post created successfully!');
        setShowCreatePost(false);
        setNewPostTitle('');
        setNewPostContent('');
        fetchPosts(true); // Refresh posts (force refresh to bypass cache)
      } else {
        alert(`Failed to create post: ${response.error || response.data?.message || 'Unknown error'}`);
      }
    } catch (error) {
      console.error('Error creating post:', error);
      alert('Error creating post');
    }
  };

  // ENDPOINT 3 & 4: View post details with comments
  const handleViewPost = async (post: ForumPost) => {
    setSelectedPost(post);
    setShowPostDetail(true);

    const { apiKey, deviceId } = getApiCredentials();
    try {
      const response = await ForumApiService.getPostDetails(post.id, apiKey, deviceId);

      if (response.success && response.data?.data) {
        const comments = response.data.data.comments || [];
        const formattedComments = comments.map(convertApiCommentToUIFormat);
        setPostComments(formattedComments);
      }
    } catch (error) {
      // Silently fail - backend may not be available
      console.debug('Forum API not available (post details)');
    }
  };

  // ENDPOINT 8: Add comment to post
  const handleAddComment = async () => {
    if (!newComment.trim() || !selectedPost) return;

    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to comment');
      return;
    }

    try {
      const response = await ForumApiService.addComment(
        selectedPost.id,
        { content: newComment },
        apiKey,
        deviceId
      );

      if (response.success) {
        setNewComment('');
        // Refresh post details
        handleViewPost(selectedPost);
      } else {
        alert(`Failed to add comment: ${response.error || 'Unknown error'}`);
      }
    } catch (error) {
      console.error('Error adding comment:', error);
      alert('Error adding comment');
    }
  };

  // ENDPOINT 9: Vote on post
  const handleVotePost = async (postId: string, voteType: 'up' | 'down') => {
    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to vote');
      return;
    }

    try {
      const response = await ForumApiService.voteOnPost(postId, voteType, apiKey, deviceId);

      if (response.success) {
        fetchPosts(true); // Refresh posts to show updated vote counts (force refresh)
        if (selectedPost && selectedPost.id === postId) {
          handleViewPost(selectedPost); // Refresh post detail if viewing
        }
      } else {
        alert(`Vote failed: ${response.error || 'Unknown error'}`);
      }
    } catch (error) {
      console.error('Error voting on post:', error);
    }
  };

  // ENDPOINT 10: Vote on comment
  const handleVoteComment = async (commentId: string, voteType: 'up' | 'down') => {
    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to vote');
      return;
    }

    try {
      const response = await ForumApiService.voteOnComment(commentId, voteType, apiKey, deviceId);

      if (response.success && selectedPost) {
        handleViewPost(selectedPost); // Refresh comments
      }
    } catch (error) {
      console.error('Error voting on comment:', error);
    }
  };

  // ENDPOINT 4: Search forum
  const handleSearch = async () => {
    if (!searchQuery.trim()) return;

    const { apiKey, deviceId } = getApiCredentials();
    setIsLoading(true);

    try {
      const response = await ForumApiService.searchPosts(searchQuery, 'all', 20, apiKey, deviceId);

      if (response.success && response.data?.data?.results?.posts) {
        const posts = response.data.data.results.posts;
        const formattedPosts = posts.map(convertApiPostToUIFormat);
        setSearchResults(formattedPosts);
      }
    } catch (error) {
      console.error('Error searching:', error);
    } finally {
      setIsLoading(false);
    }
  };

  // ENDPOINT 12: Get my profile
  const handleViewMyProfile = async () => {
    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to view profile');
      return;
    }

    try {
      const response = await ForumApiService.getMyProfile(apiKey, deviceId);

      if (response.success && response.data?.data) {
        const profile = response.data.data;
        setUserProfile(profile);
        setProfileEdit({
          display_name: profile.display_name || '',
          bio: profile.bio || '',
          avatar_color: profile.avatar_color || colors.primary,
          signature: profile.signature || ''
        });
        setShowProfile(true);
      }
    } catch (error) {
      console.error('Error fetching profile:', error);
      alert('Error loading profile');
    }
  };

  // ENDPOINT 11: Get user profile
  const handleViewUserProfile = async (username: string) => {
    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) {
      alert('You must be logged in to view profiles');
      return;
    }

    setSelectedUsername(username);

    try {
      const response = await ForumApiService.getUserProfile(username, apiKey, deviceId);

      if (response.success && response.data?.data) {
        setUserProfile(response.data.data);
        setShowProfile(true);
      }
    } catch (error) {
      console.error('Error fetching user profile:', error);
      alert('Error loading user profile');
    }
  };

  // ENDPOINT 13: Update profile
  const handleUpdateProfile = async () => {
    const { apiKey, deviceId } = getApiCredentials();
    if (!apiKey) return;

    try {
      const response = await ForumApiService.updateProfile(profileEdit, apiKey, deviceId);

      if (response.success) {
        alert('Profile updated successfully!');
        setShowEditProfile(false);
        handleViewMyProfile(); // Refresh profile
      } else {
        alert(`Failed to update profile: ${response.error || 'Unknown error'}`);
      }
    } catch (error) {
      console.error('Error updating profile:', error);
      alert('Error updating profile');
    }
  };

  // Generate trending topics from posts
  const generateTrendingTopics = () => {
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
        mentions: data.count * 100,
        sentiment: data.sentiment,
        change: `+${Math.floor(Math.random() * 200 + 50)}%`
      }))
      .sort((a, b) => b.mentions - a.mentions)
      .slice(0, 6);

    setTrendingTopics(trending);
  };

  // Generate recent activity from posts
  const generateRecentActivity = () => {
    const activities = forumPosts.slice(0, 5).map((post, index) => ({
      user: post.author,
      action: index % 3 === 0 ? 'posted' : index % 3 === 1 ? 'replied to' : 'liked',
      target: post.title.substring(0, 30) + '...',
      time: `${(index + 1) * 3} min ago`
    }));

    setRecentActivity(activities);
  };

  // Manual refresh handler
  const handleRefresh = async () => {
    if (isRefreshing) return; // Prevent multiple simultaneous refreshes

    setIsRefreshing(true);
    console.log('[Forum] Manual refresh triggered');

    try {
      // Force refresh all data (bypass cache)
      await Promise.all([
        fetchCategories(true),
        fetchForumStats(true),
        fetchPosts(true)
      ]);

      setLastRefreshTime(new Date());
      console.log('[Forum] Manual refresh completed');
    } catch (error) {
      console.error('[Forum] Refresh failed:', error);
    } finally {
      setIsRefreshing(false);
    }
  };

  // Initial load
  useEffect(() => {
    fetchCategories();
    fetchForumStats();
  }, []);

  // Fetch posts when category or sort changes
  useEffect(() => {
    if (categories.length > 0) {
      fetchPosts();
    }
  }, [activeCategory, sortBy, categories]);

  // Generate derived data
  useEffect(() => {
    if (forumPosts.length > 0) {
      generateTrendingTopics();
      generateRecentActivity();
    }
  }, [forumPosts]);

  // Update clock
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
      setOnlineUsers(prev => Math.max(1000, prev + Math.floor(Math.random() * 10) - 5));
    }, 5000);
    return () => clearInterval(timer);
  }, []);

  // Auto-refresh every 5 minutes
  useEffect(() => {
    const autoRefreshInterval = setInterval(() => {
      console.log('[Forum] Auto-refresh triggered (5 min interval)');
      handleRefresh();
    }, 5 * 60 * 1000); // 5 minutes in milliseconds

    autoRefreshIntervalRef.current = autoRefreshInterval;

    // Cleanup on unmount
    return () => {
      if (autoRefreshIntervalRef.current) {
        clearInterval(autoRefreshIntervalRef.current);
      }
    };
  }, []); // Empty deps - set up once on mount

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'HOT': return BLOOMBERG_RED;
      case 'TRENDING': return BLOOMBERG_ORANGE;
      case 'NORMAL': return BLOOMBERG_GRAY;
      default: return BLOOMBERG_GRAY;
    }
  };

  const getSentimentColor = (sentiment: string) => {
    switch (sentiment) {
      case 'BULLISH': return BLOOMBERG_GREEN;
      case 'BEARISH': return BLOOMBERG_RED;
      case 'NEUTRAL': return BLOOMBERG_YELLOW;
      default: return BLOOMBERG_GRAY;
    }
  };

  const filteredPosts = activeCategory === 'ALL'
    ? forumPosts
    : forumPosts.filter(post => post.category === activeCategory);

  const totalPosts = forumStats?.total_posts || categories.find(c => c.name === 'ALL')?.count || 0;
  const postsToday = Math.floor(totalPosts * 0.003);

  // Modal components - useMemo to prevent re-creation and fix input focus
  const CreatePostModal = React.useMemo(() => {
    if (!showCreatePost) return null;
    return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `2px solid ${BLOOMBERG_ORANGE}`,
        padding: '16px',
        width: '600px',
        maxHeight: '80vh',
        overflow: 'auto'
      }}>
        <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px' }}>
          CREATE NEW POST
        </div>
        <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

        <div style={{ marginBottom: '12px' }}>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', marginBottom: '4px' }}>
            Title: <span style={{ color: newPostTitle.length < 5 ? BLOOMBERG_RED : BLOOMBERG_GREEN }}>
              ({newPostTitle.length}/500 chars - min 5)
            </span>
          </div>
          <input
            type="text"
            value={newPostTitle}
            onChange={(e) => setNewPostTitle(e.target.value)}
            maxLength={500}
            style={{
              width: '100%',
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${newPostTitle.length > 0 && newPostTitle.length < 5 ? BLOOMBERG_RED : BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '6px',
              fontSize: '12px',
              fontFamily: 'Consolas, monospace'
            }}
            placeholder="Enter post title (minimum 5 characters)..."
          />
        </div>

        <div style={{ marginBottom: '12px' }}>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', marginBottom: '4px' }}>
            Content: <span style={{ color: newPostContent.length < 10 ? BLOOMBERG_RED : BLOOMBERG_GREEN }}>
              ({newPostContent.length}/10000 chars - min 10)
            </span>
          </div>
          <textarea
            value={newPostContent}
            onChange={(e) => setNewPostContent(e.target.value)}
            maxLength={10000}
            rows={10}
            style={{
              width: '100%',
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${newPostContent.length > 0 && newPostContent.length < 10 ? BLOOMBERG_RED : BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '6px',
              fontSize: '11px',
              fontFamily: 'Consolas, monospace',
              resize: 'vertical'
            }}
            placeholder="Enter post content (minimum 10 characters)... Use #tags for topics"
          />
        </div>

        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
          <button
            onClick={() => setShowCreatePost(false)}
            style={{
              backgroundColor: BLOOMBERG_GRAY,
              color: BLOOMBERG_DARK_BG,
              border: 'none',
              padding: '6px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            CANCEL
          </button>
          <button
            onClick={handleCreatePost}
            style={{
              backgroundColor: BLOOMBERG_ORANGE,
              color: BLOOMBERG_DARK_BG,
              border: 'none',
              padding: '6px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            CREATE POST
          </button>
        </div>
      </div>
    </div>
    );
  }, [showCreatePost, newPostTitle, newPostContent, handleCreatePost, BLOOMBERG_ORANGE, BLOOMBERG_GRAY, BLOOMBERG_DARK_BG, BLOOMBERG_WHITE, BLOOMBERG_RED, BLOOMBERG_GREEN, BLOOMBERG_PANEL_BG]);

  const PostDetailModal = React.useMemo(() => {
    if (!showPostDetail || !selectedPost) return null;
    return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `2px solid ${BLOOMBERG_ORANGE}`,
        padding: '16px',
        width: '800px',
        maxHeight: '80vh',
        overflow: 'auto'
      }}>
        {selectedPost && (
          <>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
              <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold' }}>
                POST DETAILS
              </div>
              <button
                onClick={() => setShowPostDetail(false)}
                style={{
                  backgroundColor: 'transparent',
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '2px 8px',
                  fontSize: '10px',
                  cursor: 'pointer'
                }}
              >
                CLOSE [X]
              </button>
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

            {/* Post content */}
            <div style={{ marginBottom: '16px' }}>
              <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '8px', fontSize: '10px' }}>
                <span style={{ color: BLOOMBERG_CYAN }}>@{selectedPost.author}</span>
                <span style={{ color: BLOOMBERG_GRAY }}>•</span>
                <span style={{ color: BLOOMBERG_BLUE }}>[{selectedPost.category}]</span>
                <span style={{ color: getSentimentColor(selectedPost.sentiment) }}>{selectedPost.sentiment}</span>
              </div>

              <div style={{ color: BLOOMBERG_WHITE, fontSize: '13px', fontWeight: 'bold', marginBottom: '8px' }}>
                {selectedPost.title}
              </div>

              <div style={{ color: BLOOMBERG_GRAY, fontSize: '11px', lineHeight: '1.5', marginBottom: '8px', whiteSpace: 'pre-wrap' }}>
                {selectedPost.content}
              </div>

              <div style={{ display: 'flex', gap: '12px', fontSize: '10px', marginBottom: '8px' }}>
                <button
                  onClick={() => handleVotePost(selectedPost.id, 'up')}
                  style={{
                    backgroundColor: 'transparent',
                    border: `1px solid ${BLOOMBERG_GREEN}`,
                    color: BLOOMBERG_GREEN,
                    padding: '4px 12px',
                    fontSize: '10px',
                    cursor: 'pointer'
                  }}
                >
                  ▲ UPVOTE ({selectedPost.likes})
                </button>
                <button
                  onClick={() => handleVotePost(selectedPost.id, 'down')}
                  style={{
                    backgroundColor: 'transparent',
                    border: `1px solid ${BLOOMBERG_RED}`,
                    color: BLOOMBERG_RED,
                    padding: '4px 12px',
                    fontSize: '10px',
                    cursor: 'pointer'
                  }}
                >
                  ▼ DOWNVOTE ({selectedPost.dislikes})
                </button>
              </div>
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, paddingTop: '12px', marginBottom: '12px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
                COMMENTS ({postComments.length})
              </div>

              {/* Add comment form */}
              <div style={{ marginBottom: '12px' }}>
                <textarea
                  value={newComment}
                  onChange={(e) => setNewComment(e.target.value)}
                  maxLength={2000}
                  rows={3}
                  style={{
                    width: '100%',
                    backgroundColor: BLOOMBERG_DARK_BG,
                    border: `1px solid ${BLOOMBERG_GRAY}`,
                    color: BLOOMBERG_WHITE,
                    padding: '6px',
                    fontSize: '11px',
                    fontFamily: 'Consolas, monospace',
                    marginBottom: '4px'
                  }}
                  placeholder="Add a comment..."
                />
                <button
                  onClick={handleAddComment}
                  style={{
                    backgroundColor: BLOOMBERG_ORANGE,
                    color: BLOOMBERG_DARK_BG,
                    border: 'none',
                    padding: '4px 12px',
                    fontSize: '10px',
                    fontWeight: 'bold',
                    cursor: 'pointer'
                  }}
                >
                  POST COMMENT
                </button>
              </div>

              {/* Comments list */}
              {postComments.map((comment, index) => (
                <div key={comment.id} style={{
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                  padding: '8px',
                  marginBottom: '6px',
                  borderLeft: `2px solid ${BLOOMBERG_BLUE}`
                }}>
                  <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '4px', fontSize: '9px' }}>
                    <span style={{ color: BLOOMBERG_CYAN }}>@{comment.author}</span>
                    <span style={{ color: BLOOMBERG_GRAY }}>•</span>
                    <span style={{ color: BLOOMBERG_GRAY }}>{comment.time}</span>
                  </div>

                  <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px', marginBottom: '4px', whiteSpace: 'pre-wrap' }}>
                    {comment.content}
                  </div>

                  <div style={{ display: 'flex', gap: '8px', fontSize: '9px' }}>
                    <button
                      onClick={() => handleVoteComment(comment.id, 'up')}
                      style={{
                        backgroundColor: 'transparent',
                        border: 'none',
                        color: BLOOMBERG_GREEN,
                        cursor: 'pointer',
                        fontSize: '9px'
                      }}
                    >
                      ▲ {comment.likes}
                    </button>
                    <button
                      onClick={() => handleVoteComment(comment.id, 'down')}
                      style={{
                        backgroundColor: 'transparent',
                        border: 'none',
                        color: BLOOMBERG_RED,
                        cursor: 'pointer',
                        fontSize: '9px'
                      }}
                    >
                      ▼ {comment.dislikes}
                    </button>
                  </div>
                </div>
              ))}
            </div>
          </>
        )}
      </div>
    </div>
    );
  }, [showPostDetail, selectedPost, postComments, newComment, handleAddComment, handleVotePost, handleVoteComment, getSentimentColor, BLOOMBERG_ORANGE, BLOOMBERG_GRAY, BLOOMBERG_WHITE, BLOOMBERG_DARK_BG, BLOOMBERG_PANEL_BG, BLOOMBERG_CYAN, BLOOMBERG_BLUE, BLOOMBERG_GREEN, BLOOMBERG_RED, BLOOMBERG_YELLOW]);

  const SearchModal = React.useMemo(() => {
    if (!showSearch) return null;
    return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `2px solid ${BLOOMBERG_ORANGE}`,
        padding: '16px',
        width: '700px',
        maxHeight: '80vh',
        overflow: 'auto'
      }}>
        <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px' }}>
          SEARCH FORUM
        </div>
        <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

        <div style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
            style={{
              flex: 1,
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '8px',
              fontSize: '12px',
              fontFamily: 'Consolas, monospace'
            }}
            placeholder="Search posts and comments..."
          />
          <button
            onClick={handleSearch}
            style={{
              backgroundColor: BLOOMBERG_ORANGE,
              color: BLOOMBERG_DARK_BG,
              border: 'none',
              padding: '8px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            SEARCH
          </button>
          <button
            onClick={() => {
              setShowSearch(false);
              setSearchResults([]);
              setSearchQuery('');
            }}
            style={{
              backgroundColor: BLOOMBERG_GRAY,
              color: BLOOMBERG_DARK_BG,
              border: 'none',
              padding: '8px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            CLOSE
          </button>
        </div>

        {isLoading && (
          <div style={{ color: BLOOMBERG_YELLOW, textAlign: 'center', padding: '20px' }}>
            SEARCHING...
          </div>
        )}

        {searchResults.length > 0 && (
          <div>
            <div style={{ color: BLOOMBERG_YELLOW, fontSize: '11px', marginBottom: '8px' }}>
              FOUND {searchResults.length} RESULTS
            </div>
            {searchResults.map((post, index) => (
              <div
                key={post.id}
                onClick={() => {
                  setShowSearch(false);
                  handleViewPost(post);
                }}
                style={{
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                  padding: '8px',
                  marginBottom: '6px',
                  borderLeft: `3px solid ${getPriorityColor(post.priority)}`,
                  cursor: 'pointer'
                }}
              >
                <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
                  {post.title}
                </div>
                <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', marginBottom: '4px' }}>
                  {post.content.substring(0, 150)}...
                </div>
                <div style={{ display: 'flex', gap: '8px', fontSize: '9px' }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>@{post.author}</span>
                  <span style={{ color: BLOOMBERG_BLUE }}>[{post.category}]</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>👍 {post.likes}</span>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
    );
  }, [showSearch, searchQuery, searchResults, isLoading, handleSearch, handleViewPost, getPriorityColor, BLOOMBERG_ORANGE, BLOOMBERG_GRAY, BLOOMBERG_DARK_BG, BLOOMBERG_WHITE, BLOOMBERG_PANEL_BG, BLOOMBERG_YELLOW, BLOOMBERG_CYAN, BLOOMBERG_BLUE, BLOOMBERG_GREEN]);

  const ProfileModal = React.useMemo(() => {
    if (!showProfile || !userProfile) return null;
    return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `2px solid ${BLOOMBERG_ORANGE}`,
        padding: '16px',
        width: '600px',
        maxHeight: '80vh',
        overflow: 'auto'
      }}>
        {userProfile && (
          <>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
              <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold' }}>
                FORUM PROFILE
              </div>
              <button
                onClick={() => {
                  setShowProfile(false);
                  setUserProfile(null);
                }}
                style={{
                  backgroundColor: 'transparent',
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  color: BLOOMBERG_WHITE,
                  padding: '2px 8px',
                  fontSize: '10px',
                  cursor: 'pointer'
                }}
              >
                CLOSE [X]
              </button>
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

            <div style={{ marginBottom: '12px' }}>
              <div style={{ color: BLOOMBERG_CYAN, fontSize: '16px', fontWeight: 'bold', marginBottom: '8px' }}>
                @{userProfile.username}
              </div>
              <div style={{ color: BLOOMBERG_WHITE, fontSize: '12px', marginBottom: '4px' }}>
                {userProfile.display_name || 'No display name set'}
              </div>
              <div style={{ color: BLOOMBERG_GRAY, fontSize: '11px', marginBottom: '12px', fontStyle: 'italic' }}>
                {userProfile.bio || 'No bio available'}
              </div>

              <div style={{ display: 'flex', gap: '16px', fontSize: '11px', marginBottom: '12px' }}>
                <div>
                  <span style={{ color: BLOOMBERG_GRAY }}>Posts: </span>
                  <span style={{ color: BLOOMBERG_WHITE }}>{userProfile.post_count || 0}</span>
                </div>
                <div>
                  <span style={{ color: BLOOMBERG_GRAY }}>Comments: </span>
                  <span style={{ color: BLOOMBERG_WHITE }}>{userProfile.comment_count || 0}</span>
                </div>
                <div>
                  <span style={{ color: BLOOMBERG_GRAY }}>Reputation: </span>
                  <span style={{ color: BLOOMBERG_GREEN }}>{userProfile.reputation || 0}</span>
                </div>
              </div>

              {userProfile.signature && (
                <div style={{
                  backgroundColor: 'rgba(255,255,255,0.02)',
                  padding: '8px',
                  marginBottom: '12px',
                  borderLeft: `2px solid ${BLOOMBERG_YELLOW}`
                }}>
                  <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', marginBottom: '4px' }}>SIGNATURE:</div>
                  <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', fontStyle: 'italic' }}>
                    {userProfile.signature}
                  </div>
                </div>
              )}

              {!selectedUsername && (
                <button
                  onClick={() => {
                    setShowProfile(false);
                    setShowEditProfile(true);
                  }}
                  style={{
                    backgroundColor: BLOOMBERG_ORANGE,
                    color: BLOOMBERG_DARK_BG,
                    border: 'none',
                    padding: '6px 16px',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    cursor: 'pointer'
                  }}
                >
                  EDIT PROFILE
                </button>
              )}
            </div>
          </>
        )}
      </div>
    </div>
    );
  }, [showProfile, userProfile, selectedUsername, BLOOMBERG_ORANGE, BLOOMBERG_GRAY, BLOOMBERG_WHITE, BLOOMBERG_DARK_BG, BLOOMBERG_PANEL_BG, BLOOMBERG_CYAN, BLOOMBERG_GREEN, BLOOMBERG_YELLOW]);

  const EditProfileModal = React.useMemo(() => {
    if (!showEditProfile) return null;
    return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `2px solid ${BLOOMBERG_ORANGE}`,
        padding: '16px',
        width: '600px',
        maxHeight: '80vh',
        overflow: 'auto'
      }}>
        <div style={{ color: BLOOMBERG_ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px' }}>
          EDIT PROFILE
        </div>
        <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '12px' }}></div>

        <div style={{ marginBottom: '12px' }}>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', marginBottom: '4px' }}>Display Name:</div>
          <input
            type="text"
            value={profileEdit.display_name}
            onChange={(e) => setProfileEdit({ ...profileEdit, display_name: e.target.value })}
            style={{
              width: '100%',
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '6px',
              fontSize: '12px',
              fontFamily: 'Consolas, monospace'
            }}
          />
        </div>

        <div style={{ marginBottom: '12px' }}>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', marginBottom: '4px' }}>Bio:</div>
          <textarea
            value={profileEdit.bio}
            onChange={(e) => setProfileEdit({ ...profileEdit, bio: e.target.value })}
            rows={4}
            style={{
              width: '100%',
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '6px',
              fontSize: '11px',
              fontFamily: 'Consolas, monospace'
            }}
          />
        </div>

        <div style={{ marginBottom: '12px' }}>
          <div style={{ color: BLOOMBERG_WHITE, fontSize: '11px', marginBottom: '4px' }}>Signature:</div>
          <input
            type="text"
            value={profileEdit.signature}
            onChange={(e) => setProfileEdit({ ...profileEdit, signature: e.target.value })}
            style={{
              width: '100%',
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '6px',
              fontSize: '12px',
              fontFamily: 'Consolas, monospace'
            }}
          />
        </div>

        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
          <button
            onClick={() => setShowEditProfile(false)}
            style={{
              backgroundColor: BLOOMBERG_GRAY,
              color: BLOOMBERG_DARK_BG,
              border: 'none',
              padding: '6px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            CANCEL
          </button>
          <button
            onClick={handleUpdateProfile}
            style={{
              backgroundColor: BLOOMBERG_ORANGE,
              color: BLOOMBERG_DARK_BG,
              border: 'none',
              padding: '6px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            SAVE CHANGES
          </button>
        </div>
      </div>
    </div>
    );
  }, [showEditProfile, profileEdit, handleUpdateProfile, BLOOMBERG_ORANGE, BLOOMBERG_GRAY, BLOOMBERG_DARK_BG, BLOOMBERG_WHITE, BLOOMBERG_PANEL_BG]);

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      color: BLOOMBERG_WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '12px'
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '4px 8px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <span style={{ color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>FINCEPT GLOBAL FORUM</span>
            <span style={{ color: BLOOMBERG_WHITE }}>|</span>
            <span style={{ color: BLOOMBERG_GREEN }}>● LIVE</span>
            <span style={{ color: BLOOMBERG_WHITE }}>|</span>
            <span style={{ color: BLOOMBERG_YELLOW }}>USERS ONLINE: {onlineUsers.toLocaleString()}</span>
            <span style={{ color: BLOOMBERG_WHITE }}>|</span>
            <span style={{ color: BLOOMBERG_CYAN }}>POSTS TODAY: {postsToday.toLocaleString()}</span>
            <span style={{ color: BLOOMBERG_WHITE }}>|</span>
            <span style={{ color: BLOOMBERG_WHITE }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
          </div>
          <button
            onClick={handleRefresh}
            disabled={isRefreshing}
            style={{
              backgroundColor: isRefreshing ? BLOOMBERG_GRAY : BLOOMBERG_ORANGE,
              color: BLOOMBERG_DARK_BG,
              border: 'none',
              padding: '2px 8px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: isRefreshing ? 'not-allowed' : 'pointer',
              opacity: isRefreshing ? 0.6 : 1
            }}
            title={lastRefreshTime ? `Last refreshed: ${lastRefreshTime.toLocaleTimeString()}` : 'Refresh forum data'}
          >
            {isRefreshing ? '⟳ REFRESHING...' : '⟳ REFRESH'}
          </button>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
          <span style={{ color: BLOOMBERG_GRAY }}>TRENDING:</span>
          {trendingTopics.slice(0, 5).map((topic, i) => (
            <span key={i} style={{ color: i % 2 === 0 ? BLOOMBERG_RED : i % 3 === 0 ? BLOOMBERG_CYAN : BLOOMBERG_PURPLE }}>
              {topic.topic.replace('#', '').toUpperCase()}
            </span>
          ))}
          <span style={{ color: BLOOMBERG_WHITE }}>|</span>
          <span style={{ color: BLOOMBERG_GRAY }}>SENTIMENT:</span>
          <span style={{ color: BLOOMBERG_GREEN }}>BULLISH 68%</span>
          <span style={{ color: BLOOMBERG_RED }}>BEARISH 18%</span>
          <span style={{ color: BLOOMBERG_YELLOW }}>NEUTRAL 14%</span>
        </div>
      </div>

      {/* Function Keys */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '2px 4px',
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(12, 1fr)', gap: '2px' }}>
          {[
            { key: "F1", label: "ALL", action: () => setActiveCategory('ALL') },
            { key: "F2", label: categories[1]?.name || "MARKETS", action: () => setActiveCategory(categories[1]?.name || "MARKETS") },
            { key: "F3", label: categories[2]?.name || "TECH", action: () => setActiveCategory(categories[2]?.name || "TECH") },
            { key: "F4", label: categories[3]?.name || "CRYPTO", action: () => setActiveCategory(categories[3]?.name || "CRYPTO") },
            { key: "F5", label: categories[4]?.name || "OPTIONS", action: () => setActiveCategory(categories[4]?.name || "OPTIONS") },
            { key: "F6", label: categories[5]?.name || "BANKING", action: () => setActiveCategory(categories[5]?.name || "BANKING") },
            { key: "F7", label: categories[6]?.name || "ENERGY", action: () => setActiveCategory(categories[6]?.name || "ENERGY") },
            { key: "F8", label: categories[7]?.name || "MACRO", action: () => setActiveCategory(categories[7]?.name || "MACRO") },
            { key: "F9", label: "POST", action: () => setShowCreatePost(true) },
            { key: "F10", label: "SEARCH", action: () => setShowSearch(true) },
            { key: "F11", label: "PROFILE", action: () => handleViewMyProfile() },
            { key: "F12", label: "SETTINGS", action: () => setShowEditProfile(true) }
          ].map(item => (
            <button key={item.key}
              onClick={item.action}
              style={{
                backgroundColor: activeCategory === item.label ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: activeCategory === item.label ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                padding: '2px 4px',
                fontSize: '9px',
                height: '16px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontWeight: activeCategory === item.label ? 'bold' : 'normal',
                cursor: 'pointer'
              }}>
              <span style={{ color: BLOOMBERG_YELLOW }}>{item.key}:</span>
              <span style={{ marginLeft: '2px' }}>{item.label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Main Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px', backgroundColor: '#050505' }}>
        <div style={{ display: 'flex', gap: '4px', height: '100%' }}>

          {/* Left Panel - Categories & Stats */}
          <div style={{
            width: '280px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              FORUM CATEGORIES
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '4px' }}></div>

            {categories.map((cat, index) => (
              <button key={index}
                onClick={() => setActiveCategory(cat.name)}
                style={{
                  width: '100%',
                  display: 'flex',
                  justifyContent: 'space-between',
                  padding: '4px',
                  marginBottom: '2px',
                  backgroundColor: activeCategory === cat.name ? 'rgba(255,165,0,0.1)' : 'transparent',
                  border: `1px solid ${activeCategory === cat.name ? BLOOMBERG_ORANGE : 'transparent'}`,
                  color: cat.color,
                  fontSize: '10px',
                  cursor: 'pointer',
                  textAlign: 'left'
                }}>
                <span>{cat.name}</span>
                <span style={{ color: BLOOMBERG_CYAN }}>{cat.count}</span>
              </button>
            ))}

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '8px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                TOP CONTRIBUTORS
              </div>
              {topContributors.map((user, index) => (
                <div key={index}
                  onClick={() => handleViewUserProfile(user.username)}
                  style={{
                    padding: '3px',
                    marginBottom: '2px',
                    backgroundColor: 'rgba(255,255,255,0.02)',
                    fontSize: '9px',
                    cursor: 'pointer'
                  }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                    <span style={{ color: BLOOMBERG_WHITE }}>{user.username}</span>
                    <span style={{ color: user.status === 'ONLINE' ? BLOOMBERG_GREEN : BLOOMBERG_GRAY }}>
                      ●
                    </span>
                  </div>
                  <div style={{ display: 'flex', gap: '6px', color: BLOOMBERG_GRAY }}>
                    <span>REP: {user.reputation}</span>
                    <span>POSTS: {user.posts}</span>
                  </div>
                </div>
              ))}
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '8px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                FORUM STATISTICS
              </div>
              <div style={{ fontSize: '9px', lineHeight: '1.4', color: BLOOMBERG_GRAY }}>
                <div>Total Posts: {totalPosts.toLocaleString()}</div>
                <div>Total Users: {(forumStats?.total_comments || 47832).toLocaleString()}</div>
                <div>Posts Today: {postsToday.toLocaleString()}</div>
                <div>Active Now: {onlineUsers}</div>
                <div style={{ color: BLOOMBERG_GREEN }}>Avg Response: 12 min</div>
              </div>
            </div>
          </div>

          {/* Center Panel - Forum Posts */}
          <div style={{
            flex: 1,
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
              <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
                FORUM POSTS [{activeCategory}]
                {isLoading && <span style={{ color: BLOOMBERG_YELLOW, marginLeft: '8px' }}>LOADING...</span>}
              </div>
              <div style={{ display: 'flex', gap: '4px', fontSize: '9px' }}>
                <button onClick={() => setSortBy('latest')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'latest' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'latest' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>LATEST</button>
                <button onClick={() => setSortBy('popular')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'popular' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'popular' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>HOT</button>
                <button onClick={() => setSortBy('views')} style={{
                  padding: '2px 6px',
                  backgroundColor: sortBy === 'views' ? BLOOMBERG_ORANGE : BLOOMBERG_DARK_BG,
                  color: sortBy === 'views' ? BLOOMBERG_DARK_BG : BLOOMBERG_WHITE,
                  border: `1px solid ${BLOOMBERG_GRAY}`,
                  cursor: 'pointer'
                }}>TOP</button>
              </div>
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {filteredPosts.length === 0 && !isLoading && (
              <div style={{ color: BLOOMBERG_GRAY, textAlign: 'center', padding: '20px' }}>
                No posts available. Be the first to post!
              </div>
            )}

            {filteredPosts.map((post, index) => (
              <div key={post.id} style={{
                marginBottom: '6px',
                padding: '6px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                borderLeft: `3px solid ${getPriorityColor(post.priority)}`,
                paddingLeft: '6px',
                cursor: 'pointer'
              }}
                onClick={() => handleViewPost(post)}
              >
                <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '2px', fontSize: '8px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>{post.time}</span>
                  <span style={{ color: getPriorityColor(post.priority), fontWeight: 'bold' }}>[{post.priority}]</span>
                  <span style={{ color: BLOOMBERG_BLUE }}>[{post.category}]</span>
                  <span style={{ color: getSentimentColor(post.sentiment) }}>{post.sentiment}</span>
                  {post.verified && <span style={{ color: BLOOMBERG_CYAN }}>✓ VERIFIED</span>}
                </div>

                <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '2px', fontSize: '9px' }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>@{post.author}</span>
                  <span style={{ color: BLOOMBERG_GRAY }}>•</span>
                  <span style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>{post.title}</span>
                </div>

                <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', lineHeight: '1.3', marginBottom: '3px' }}>
                  {post.content.substring(0, 200)}{post.content.length > 200 ? '...' : ''}
                </div>

                <div style={{ display: 'flex', gap: '6px', fontSize: '8px', marginBottom: '2px' }}>
                  {post.tags.map((tag, i) => (
                    <span key={i} style={{
                      color: BLOOMBERG_YELLOW,
                      backgroundColor: 'rgba(255,255,0,0.1)',
                      padding: '1px 4px',
                      borderRadius: '2px'
                    }}>#{tag}</span>
                  ))}
                </div>

                <div style={{ display: 'flex', gap: '10px', fontSize: '8px' }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>👁️ {post.views.toLocaleString()}</span>
                  <span style={{ color: BLOOMBERG_PURPLE }}>💬 {post.replies}</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>👍 {post.likes}</span>
                  <span style={{ color: BLOOMBERG_RED }}>👎 {post.dislikes}</span>
                </div>
              </div>
            ))}
          </div>

          {/* Right Panel - Activity & Trending */}
          <div style={{
            width: '280px',
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              TRENDING TOPICS (1H)
            </div>
            <div style={{ borderBottom: `1px solid ${BLOOMBERG_GRAY}`, marginBottom: '8px' }}></div>

            {trendingTopics.map((item, index) => (
              <div key={index} style={{
                padding: '3px',
                marginBottom: '3px',
                backgroundColor: 'rgba(255,255,255,0.02)',
                fontSize: '9px'
              }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_YELLOW }}>{item.topic}</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>{item.change}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', color: BLOOMBERG_GRAY }}>
                  <span>{item.mentions} mentions</span>
                  <span style={{ color: getSentimentColor(item.sentiment) }}>{item.sentiment}</span>
                </div>
              </div>
            ))}

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                RECENT ACTIVITY
              </div>
              {recentActivity.map((activity, index) => (
                <div key={index} style={{
                  padding: '2px',
                  marginBottom: '2px',
                  fontSize: '8px',
                  color: BLOOMBERG_GRAY,
                  lineHeight: '1.3'
                }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>@{activity.user}</span>
                  {' '}<span>{activity.action}</span>{' '}
                  <span style={{ color: BLOOMBERG_WHITE }}>{activity.target}</span>
                  {' '}<span style={{ color: BLOOMBERG_YELLOW }}>• {activity.time}</span>
                </div>
              ))}
            </div>

            <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '12px', paddingTop: '8px' }}>
              <div style={{ color: BLOOMBERG_YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
                SENTIMENT ANALYSIS
              </div>
              <div style={{ fontSize: '9px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Bullish Posts:</span>
                  <span style={{ color: BLOOMBERG_GREEN }}>68%</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Bearish Posts:</span>
                  <span style={{ color: BLOOMBERG_RED }}>18%</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: BLOOMBERG_GRAY }}>Neutral Posts:</span>
                  <span style={{ color: BLOOMBERG_YELLOW }}>14%</span>
                </div>
                <div style={{ borderTop: `1px solid ${BLOOMBERG_GRAY}`, marginTop: '4px', paddingTop: '4px' }}>
                  <span style={{ color: BLOOMBERG_CYAN }}>Overall Sentiment:</span>
                  <span style={{ color: BLOOMBERG_GREEN, fontWeight: 'bold', marginLeft: '4px' }}>+0.62 BULLISH</span>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${BLOOMBERG_GRAY}`,
        backgroundColor: BLOOMBERG_PANEL_BG,
        padding: '2px 8px',
        fontSize: '10px',
        color: BLOOMBERG_GRAY,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Fincept Global Forum v2.4.1 | Professional trading community</span>
            <span>Posts: {totalPosts.toLocaleString()} | Users: {(forumStats?.total_comments || 47832).toLocaleString()} | Active: {onlineUsers}</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Category: {activeCategory}</span>
            <span>Sort: {sortBy}</span>
            <span style={{ color: BLOOMBERG_GREEN }}>STATUS: LIVE</span>
          </div>
        </div>
      </div>

      {/* Modals */}
      {CreatePostModal}
      {PostDetailModal}
      {SearchModal}
      {ProfileModal}
      {EditProfileModal}
    </div>
  );
};

export default ForumTab;
