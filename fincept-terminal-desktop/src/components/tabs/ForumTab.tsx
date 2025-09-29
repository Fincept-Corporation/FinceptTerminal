// File: src/components/tabs/ForumTab.tsx
// Forum UI component with Bloomberg terminal styling

import React, { useState, useEffect } from 'react';
import { Button } from '@/components/ui/button';
import { Input } from '@/components/ui/input';
import { Label } from '@/components/ui/label';
import { Badge } from '@/components/ui/badge';
import { Textarea } from '@/components/ui/textarea';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '@/components/ui/select';
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogTrigger } from '@/components/ui/dialog';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Separator } from '@/components/ui/separator';
import { ScrollArea } from '@/components/ui/scroll-area';
import { 
  Search, 
  Plus, 
  TrendingUp, 
  Clock, 
  MessageSquare, 
  Eye, 
  ThumbsUp, 
  ThumbsDown, 
  Pin,
  RefreshCw,
  HelpCircle,
  User
} from 'lucide-react';

import { useAuth } from '@/contexts/AuthContext';
import { 
  ForumApiService, 
  ForumCategory, 
  ForumPost, 
  ForumComment, 
  ForumStats,
  ForumUtils 
} from '@/services/forumApi';

interface ForumTabProps {}

const ForumTab: React.FC<ForumTabProps> = () => {
  const { session } = useAuth();
  
  // State management
  const [categories, setCategories] = useState<ForumCategory[]>([]);
  const [posts, setPosts] = useState<ForumPost[]>([]);
  const [stats, setStats] = useState<ForumStats | null>(null);
  const [selectedCategory, setSelectedCategory] = useState<number | null>(null);
  const [currentCategoryName, setCurrentCategoryName] = useState<string>('All Categories');
  const [searchQuery, setSearchQuery] = useState<string>('');
  const [sortBy, setSortBy] = useState<string>('latest');
  const [loading, setLoading] = useState<boolean>(false);
  const [error, setError] = useState<string>('');

  // Post creation state
  const [showCreatePost, setShowCreatePost] = useState<boolean>(false);
  const [newPostTitle, setNewPostTitle] = useState<string>('');
  const [newPostContent, setNewPostContent] = useState<string>('');
  const [newPostCategory, setNewPostCategory] = useState<string>('');
  const [createPostLoading, setCreatePostLoading] = useState<boolean>(false);

  // Post details state
  const [selectedPost, setSelectedPost] = useState<ForumPost | null>(null);
  const [postComments, setPostComments] = useState<ForumComment[]>([]);
  const [showPostDetails, setShowPostDetails] = useState<boolean>(false);
  const [newComment, setNewComment] = useState<string>('');
  const [commentLoading, setCommentLoading] = useState<boolean>(false);

  // Bloomberg terminal colors
  const colors = {
    orange: '#ea580c',
    white: '#ffffff',
    red: '#ef4444',
    green: '#22c55e',
    yellow: '#eab308',
    gray: '#64748b',
    blue: '#3b82f6',
    purple: '#8b5cf6'
  };

  // Authentication check
  const isAuthenticated = session?.authenticated || false;
  const apiKey = session?.api_key || undefined;
  const deviceId = session?.device_id || undefined;

  // Load initial data
  useEffect(() => {
    if (isAuthenticated || session?.user_type === 'guest') {
      loadInitialData();
    }
  }, [isAuthenticated, session]);

  const loadInitialData = async () => {
    setLoading(true);
    setError('');

    try {
      // Load categories and stats in parallel
      const [categoriesResult, statsResult] = await Promise.all([
        ForumApiService.getCategories(apiKey, deviceId),
        ForumApiService.getForumStats(apiKey, deviceId)
      ]);

      if (categoriesResult.success && categoriesResult.data?.success) {
        const categoriesData = categoriesResult.data.data.categories;
        setCategories(categoriesData);
        
        // Load posts from first category or all posts
        if (categoriesData.length > 0) {
          loadPosts(categoriesData[0].id, categoriesData[0].name);
        } else {
          loadAllPosts();
        }
      }

      if (statsResult.success && statsResult.data?.success) {
        setStats(statsResult.data.data);
      }
    } catch (err) {
      setError('Failed to load forum data. Please try again.');
      console.error('Forum data loading failed:', err);
    } finally {
      setLoading(false);
    }
  };

  const loadPosts = async (categoryId?: number, categoryName?: string) => {
    setLoading(true);
    setError('');

    try {
      let result;
      
      if (categoryId) {
        result = await ForumApiService.getPostsByCategory(
          categoryId, 
          sortBy, 
          20, 
          apiKey, 
          deviceId
        );
        setSelectedCategory(categoryId);
        setCurrentCategoryName(categoryName || 'Unknown Category');
      } else {
        result = await ForumApiService.getAllPosts(sortBy, 20, apiKey, deviceId);
        setSelectedCategory(null);
        setCurrentCategoryName('All Categories');
      }

      if (result.success && result.data?.success) {
        const postsData = result.data.data.posts || [];
        setPosts(postsData);
      } else {
        throw new Error(result.error || 'Failed to load posts');
      }
    } catch (err) {
      setError('Failed to load posts. Please try again.');
      console.error('Posts loading failed:', err);
    } finally {
      setLoading(false);
    }
  };

  const loadAllPosts = () => {
    loadPosts();
  };

  const handleSearch = async () => {
    if (!searchQuery.trim()) return;

    setLoading(true);
    setError('');

    try {
      const result = await ForumApiService.searchPosts(
        searchQuery, 
        'all', 
        20, 
        apiKey, 
        deviceId
      );

      if (result.success && result.data?.success) {
        const searchResults = result.data.data.results.posts || [];
        setPosts(searchResults);
        setCurrentCategoryName(`Search: "${searchQuery}"`);
        setSelectedCategory(null);
      } else {
        throw new Error(result.error || 'Search failed');
      }
    } catch (err) {
      setError('Search failed. Please try again.');
      console.error('Search failed:', err);
    } finally {
      setLoading(false);
    }
  };

  const handleVote = async (postUuid: string, voteType: 'up' | 'down') => {
    if (!isAuthenticated) {
      setError('You must be logged in to vote');
      return;
    }

    try {
      const result = await ForumApiService.voteOnPost(
        postUuid, 
        voteType, 
        apiKey!, 
        deviceId
      );

      if (result.success && result.data?.success) {
        // Refresh posts to show updated vote counts
        loadPosts(selectedCategory, currentCategoryName);
      } else {
        throw new Error(result.error || 'Vote failed');
      }
    } catch (err) {
      console.error('Vote failed:', err);
      setError('Vote failed. Please try again.');
    }
  };

  const handleCreatePost = async () => {
    if (!isAuthenticated) {
      setError('You must be logged in to create posts');
      return;
    }

    const validation = ForumUtils.validatePost(newPostTitle, newPostContent);
    if (!validation.valid) {
      setError(validation.error!);
      return;
    }

    if (!newPostCategory) {
      setError('Please select a category');
      return;
    }

    setCreatePostLoading(true);
    setError('');

    try {
      const result = await ForumApiService.createPost({
        title: newPostTitle,
        content: newPostContent,
        category_id: parseInt(newPostCategory)
      }, apiKey!, deviceId);

      if (result.success && result.data?.success) {
        setShowCreatePost(false);
        setNewPostTitle('');
        setNewPostContent('');
        setNewPostCategory('');
        // Refresh posts
        loadPosts(selectedCategory, currentCategoryName);
      } else {
        throw new Error(result.error || 'Failed to create post');
      }
    } catch (err) {
      setError('Failed to create post. Please try again.');
      console.error('Post creation failed:', err);
    } finally {
      setCreatePostLoading(false);
    }
  };

  const viewPostDetails = async (post: ForumPost) => {
    setSelectedPost(post);
    setShowPostDetails(true);
    
    try {
      const result = await ForumApiService.getPostDetails(
        post.post_uuid, 
        apiKey, 
        deviceId
      );

      if (result.success && result.data?.success) {
        setPostComments(result.data.data.comments || []);
      }
    } catch (err) {
      console.error('Failed to load post details:', err);
    }
  };

  const addComment = async () => {
    if (!isAuthenticated || !selectedPost) return;

    const validation = ForumUtils.validateComment(newComment);
    if (!validation.valid) {
      setError(validation.error!);
      return;
    }

    setCommentLoading(true);

    try {
      const result = await ForumApiService.addComment(
        selectedPost.post_uuid,
        { content: newComment },
        apiKey!,
        deviceId
      );

      if (result.success && result.data?.success) {
        setNewComment('');
        // Reload post details
        viewPostDetails(selectedPost);
      } else {
        throw new Error(result.error || 'Failed to add comment');
      }
    } catch (err) {
      setError('Failed to add comment. Please try again.');
      console.error('Comment failed:', err);
    } finally {
      setCommentLoading(false);
    }
  };

  // Render authentication error
  if (!session || (!isAuthenticated && session.user_type !== 'guest')) {
    return (
      <div className="h-full bg-black text-white p-8 flex items-center justify-center">
        <Card className="bg-zinc-900 border-zinc-700 max-w-md">
          <CardHeader>
            <CardTitle className="text-yellow-500 flex items-center gap-2">
              <HelpCircle className="h-5 w-5" />
              Authentication Required
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <p className="text-zinc-300">
              Please authenticate to access the Global Forum
            </p>
            <ul className="text-zinc-400 text-sm space-y-1">
              <li>• Access to all forum categories</li>
              <li>• Create posts and comments</li>
              <li>• Vote on posts and comments</li>
              <li>• Real-time forum activity</li>
              <li>• User profiles and reputation</li>
            </ul>
          </CardContent>
        </Card>
      </div>
    );
  }

  return (
    <div className="h-full bg-black text-white font-mono">
      {/* Header */}
      <div className="bg-zinc-900 border-b border-zinc-700 p-4">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-4">
            <span style={{ color: colors.orange }} className="text-lg font-bold">FINCEPT</span>
            <span className="text-white">GLOBAL FORUM</span>
            <span className="text-zinc-500">|</span>
            
            {/* Search */}
            <div className="flex items-center gap-2">
              <Input
                placeholder="Search posts, topics..."
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                onKeyPress={(e) => e.key === 'Enter' && handleSearch()}
                className="bg-zinc-800 border-zinc-600 text-white w-64"
              />
              <Button 
                onClick={handleSearch}
                className="bg-zinc-700 hover:bg-zinc-600"
              >
                <Search className="h-4 w-4" />
              </Button>
            </div>

            <span className="text-zinc-500">|</span>
            
            {/* User Status */}
            <div className="flex items-center gap-2">
              <User className="h-4 w-4" />
              {session.user_type === 'registered' ? (
                <span style={{ color: colors.green }}>
                  USER: {session.user_info?.username?.toUpperCase() || 'REGISTERED'}
                </span>
              ) : (
                <span style={{ color: colors.yellow }}>USER: GUEST</span>
              )}
            </div>
          </div>
        </div>

        {/* Function Keys */}
        <div className="flex items-center gap-4">
          <Button
            onClick={loadInitialData}
            className="bg-transparent hover:bg-zinc-700 text-yellow-500 text-xs h-6 px-2"
          >
            F1:HELP
          </Button>
          <Button
            onClick={loadInitialData}
            className="bg-transparent hover:bg-zinc-700 text-yellow-500 text-xs h-6 px-2"
          >
            <RefreshCw className="h-3 w-3 mr-1" />
            F2:REFRESH
          </Button>
          {isAuthenticated && (
            <Button
              onClick={() => setShowCreatePost(true)}
              className="bg-transparent hover:bg-zinc-700 text-yellow-500 text-xs h-6 px-2"
            >
              <Plus className="h-3 w-3 mr-1" />
              F3:NEW POST
            </Button>
          )}
          <Button
            onClick={() => document.querySelector('input')?.focus()}
            className="bg-transparent hover:bg-zinc-700 text-yellow-500 text-xs h-6 px-2"
          >
            F4:SEARCH
          </Button>
          <Button
            onClick={() => setSortBy('popular')}
            className="bg-transparent hover:bg-zinc-700 text-yellow-500 text-xs h-6 px-2"
          >
            <TrendingUp className="h-3 w-3 mr-1" />
            F5:TRENDING
          </Button>
        </div>
      </div>

      {/* Main Content */}
      <div className="flex h-[calc(100%-120px)]">
        {/* Left Panel - Categories */}
        <div className="w-80 bg-zinc-950 border-r border-zinc-700 p-4 overflow-y-auto">
          <div style={{ color: colors.orange }} className="font-bold mb-4">FORUM CATEGORIES</div>
          
          <div className="space-y-2 mb-4">
            <Button
              onClick={() => loadAllPosts()}
              variant={selectedCategory === null ? "default" : "ghost"}
              className="w-full justify-start text-xs h-8"
            >
              ALL
            </Button>
            <Button
              onClick={() => setSortBy('popular')}
              variant="ghost"
              className="w-full justify-start text-xs h-8"
            >
              TRENDING
            </Button>
            <Button
              onClick={() => setSortBy('latest')}
              variant="ghost"
              className="w-full justify-start text-xs h-8"
            >
              RECENT
            </Button>
          </div>

          <Separator className="bg-zinc-700 mb-4" />

          {/* Categories List */}
          <div className="space-y-2 mb-6">
            {categories.map((category) => (
              <Button
                key={category.id}
                onClick={() => loadPosts(category.id, category.name)}
                variant={selectedCategory === category.id ? "default" : "ghost"}
                className="w-full justify-start text-xs h-auto p-2 flex-col items-start"
              >
                <div className="flex justify-between w-full">
                  <span>{category.name}</span>
                  <Badge variant="secondary" className="text-xs">
                    {category.post_count}
                  </Badge>
                </div>
                {category.description && (
                  <span className="text-zinc-500 text-xs mt-1">
                    {ForumUtils.truncateText(category.description, 40)}
                  </span>
                )}
              </Button>
            ))}
          </div>

          <Separator className="bg-zinc-700 mb-4" />

          {/* Statistics */}
          {stats && (
            <div>
              <div style={{ color: colors.yellow }} className="font-bold mb-2">FORUM STATISTICS</div>
              <div className="text-xs space-y-1">
                <div>Categories: {stats.total_categories}</div>
                <div>Total Posts: {ForumUtils.formatNumber(stats.total_posts)}</div>
                <div>Total Comments: {ForumUtils.formatNumber(stats.total_comments)}</div>
                <div style={{ color: colors.green }}>Total Votes: {ForumUtils.formatNumber(stats.total_votes)}</div>
              </div>
            </div>
          )}
        </div>

        {/* Center Panel - Posts */}
        <div className="flex-1 bg-black overflow-y-auto">
          <div className="p-4">
            <div className="flex items-center justify-between mb-4">
              <div className="flex items-center gap-4">
                <span style={{ color: colors.orange }} className="font-bold">FORUM POSTS</span>
                <span className="text-zinc-500">|</span>
                <span className="text-white">{currentCategoryName}</span>
              </div>
              
              <div className="flex items-center gap-2">
                <Label className="text-zinc-400 text-xs">SORT:</Label>
                <Select value={sortBy} onValueChange={setSortBy}>
                  <SelectTrigger className="w-32 bg-zinc-800 border-zinc-600 text-white text-xs h-8">
                    <SelectValue />
                  </SelectTrigger>
                  <SelectContent className="bg-zinc-800 border-zinc-600">
                    <SelectItem value="latest">Latest</SelectItem>
                    <SelectItem value="popular">Popular</SelectItem>
                    <SelectItem value="views">Most Viewed</SelectItem>
                    <SelectItem value="replies">Most Replies</SelectItem>
                  </SelectContent>
                </Select>
              </div>
            </div>

            {loading && (
              <div className="text-yellow-500 mb-4">Loading posts...</div>
            )}

            {error && (
              <div className="text-red-400 bg-red-900/20 border border-red-900/50 rounded p-2 mb-4 text-xs">
                {error}
              </div>
            )}

            {/* Posts List */}
            <div className="space-y-4">
              {posts.map((post) => (
                <Card key={post.post_uuid} className="bg-zinc-900 border-zinc-700">
                  <CardContent className="p-4">
                    <div className="flex items-start justify-between mb-2">
                      <div className="flex items-center gap-2">
                        {post.is_pinned && <Pin className="h-4 w-4" style={{ color: colors.orange }} />}
                        <Button
                          onClick={() => viewPostDetails(post)}
                          variant="ghost"
                          className="p-0 h-auto text-left text-white hover:text-blue-400"
                        >
                          <span className="text-sm font-medium">
                            {ForumUtils.truncateText(post.title, 60)}
                          </span>
                        </Button>
                        <Badge 
                          variant="outline" 
                          className="text-xs"
                          style={{ borderColor: colors.blue, color: colors.blue }}
                        >
                          {post.category_name}
                        </Badge>
                      </div>
                    </div>

                    <div className="flex items-center gap-4 text-xs text-zinc-400 mb-2">
                      <div className="flex items-center gap-1">
                        <Eye className="h-3 w-3" />
                        {ForumUtils.formatNumber(post.views)}
                      </div>
                      <div 
                        className={`flex items-center gap-1 ${ForumUtils.getVoteColor(ForumUtils.calculateVoteScore(post.likes, post.dislikes))}`}
                      >
                        <ThumbsUp className="h-3 w-3" />
                        {ForumUtils.calculateVoteScore(post.likes, post.dislikes)}
                      </div>
                      <div className="flex items-center gap-1">
                        <MessageSquare className="h-3 w-3" />
                        {post.reply_count}
                      </div>
                      <div>By: {post.author_display_name}</div>
                      <div>{ForumUtils.formatTimestamp(post.created_at)}</div>
                    </div>

                    <p className="text-zinc-300 text-xs mb-3">
                      {ForumUtils.truncateText(post.content, 120)}
                    </p>

                    <div className="flex items-center gap-2">
                      <Button
                        onClick={() => viewPostDetails(post)}
                        size="sm"
                        className="bg-zinc-700 hover:bg-zinc-600 text-xs h-6"
                      >
                        VIEW
                      </Button>
                      
                      {isAuthenticated && (
                        <>
                          <Button
                            onClick={() => handleVote(post.post_uuid, 'up')}
                            size="sm"
                            className="bg-transparent hover:bg-green-900/20 text-green-400 border border-green-600 text-xs h-6"
                          >
                            👍 UP
                          </Button>
                          <Button
                            onClick={() => handleVote(post.post_uuid, 'down')}
                            size="sm"
                            className="bg-transparent hover:bg-red-900/20 text-red-400 border border-red-600 text-xs h-6"
                          >
                            👎 DOWN
                          </Button>
                        </>
                      )}
                    </div>
                  </CardContent>
                </Card>
              ))}
            </div>

            {posts.length === 0 && !loading && (
              <div className="text-center text-zinc-400 mt-8">
                <MessageSquare className="h-12 w-12 mx-auto mb-4 opacity-50" />
                <p>No posts found</p>
              </div>
            )}
          </div>
        </div>

        {/* Right Panel - User Dashboard */}
        <div className="w-80 bg-zinc-950 border-l border-zinc-700 p-4 overflow-y-auto">
          <div style={{ color: colors.orange }} className="font-bold mb-4">USER DASHBOARD</div>
          
          {/* User Info */}
          <div className="mb-6">
            <div style={{ color: colors.yellow }} className="font-bold mb-2">CURRENT USER</div>
            <div className="flex items-center gap-3 mb-2">
              <div 
                className="w-8 h-8 rounded flex items-center justify-center text-xs font-bold text-white"
                style={{ backgroundColor: session.user_type === 'registered' ? colors.green : colors.yellow }}
              >
                {session.user_type === 'registered' 
                  ? session.user_info?.username?.substring(0, 2).toUpperCase() || 'RU'
                  : 'GU'
                }
              </div>
              <div>
                <div className="text-white text-sm">
                  {session.user_type === 'registered' 
                    ? session.user_info?.username || 'Registered User'
                    : 'Guest User'
                  }
                </div>
                {session.user_type === 'registered' ? (
                  <div style={{ color: colors.green }} className="text-xs">
                    Credits: {session.user_info?.credit_balance || 0}
                  </div>
                ) : (
                  <div style={{ color: colors.yellow }} className="text-xs">
                    Limited Access
                  </div>
                )}
              </div>
            </div>
          </div>

          <Separator className="bg-zinc-700 mb-4" />

          {/* User Actions */}
          <div className="mb-6">
            <div style={{ color: colors.yellow }} className="font-bold mb-2">USER ACTIONS</div>
            <div className="space-y-2">
              {isAuthenticated ? (
                <>
                  <Button 
                    onClick={() => setShowCreatePost(true)}
                    className="w-full justify-start text-xs h-8"
                  >
                    CREATE NEW POST
                  </Button>
                  <Button className="w-full justify-start text-xs h-6">
                    MY POSTS
                  </Button>
                  <Button className="w-full justify-start text-xs h-6">
                    MY ACTIVITY
                  </Button>
                  <Button className="w-full justify-start text-xs h-6">
                    SETTINGS
                  </Button>
                </>
              ) : (
                <>
                  <Button 
                    onClick={() => loadAllPosts()}
                    className="w-full justify-start text-xs h-6"
                  >
                    VIEW POSTS
                  </Button>
                  <Button 
                    onClick={() => document.querySelector('input')?.focus()}
                    className="w-full justify-start text-xs h-6"
                  >
                    SEARCH FORUM
                  </Button>
                </>
              )}
            </div>
          </div>

          <Separator className="bg-zinc-700 mb-4" />

          {/* Recent Activity */}
          <div>
            <div style={{ color: colors.yellow }} className="font-bold mb-2">RECENT ACTIVITY</div>
            <div className="text-xs text-zinc-400 space-y-1">
              <div>Loading activity...</div>
            </div>
          </div>
        </div>
      </div>

      {/* Create Post Dialog */}
      <Dialog open={showCreatePost} onOpenChange={setShowCreatePost}>
        <DialogContent className="bg-zinc-900 border-zinc-700 text-white">
          <DialogHeader>
            <DialogTitle style={{ color: colors.orange }}>CREATE NEW POST</DialogTitle>
          </DialogHeader>
          <div className="space-y-4">
            <div>
              <Label className="text-white">Category</Label>
              <Select value={newPostCategory} onValueChange={setNewPostCategory}>
                <SelectTrigger className="bg-zinc-800 border-zinc-600 text-white">
                  <SelectValue placeholder="Select category" />
                </SelectTrigger>
                <SelectContent className="bg-zinc-800 border-zinc-600">
                  {categories.map((category) => (
                    <SelectItem key={category.id} value={category.id.toString()}>
                      {category.name}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
            </div>
            <div>
              <Label className="text-white">Title</Label>
              <Input
                value={newPostTitle}
                onChange={(e) => setNewPostTitle(e.target.value)}
                className="bg-zinc-800 border-zinc-600 text-white"
                placeholder="Enter post title..."
              />
            </div>
            <div>
              <Label className="text-white">Content</Label>
              <Textarea
                value={newPostContent}
                onChange={(e) => setNewPostContent(e.target.value)}
                className="bg-zinc-800 border-zinc-600 text-white min-h-32"
                placeholder="Enter post content..."
              />
            </div>
            {error && (
              <div className="text-red-400 text-xs">{error}</div>
            )}
            <div className="flex justify-end gap-2">
              <Button
                onClick={() => setShowCreatePost(false)}
                variant="ghost"
                className="text-zinc-400"
              >
                CANCEL
              </Button>
              <Button
                onClick={handleCreatePost}
                disabled={createPostLoading}
                style={{ backgroundColor: colors.orange }}
                className="hover:bg-orange-600"
              >
                {createPostLoading ? 'CREATING...' : 'CREATE POST'}
              </Button>
            </div>
          </div>
        </DialogContent>
      </Dialog>

      {/* Post Details Dialog */}
      <Dialog open={showPostDetails} onOpenChange={setShowPostDetails}>
        <DialogContent className="bg-zinc-900 border-zinc-700 text-white max-w-4xl max-h-[80vh] overflow-y-auto">
          {selectedPost && (
            <>
              <DialogHeader>
                <DialogTitle style={{ color: colors.orange }}>POST DETAILS</DialogTitle>
              </DialogHeader>
              <div className="space-y-4">
                <div>
                  <h3 className="text-lg font-bold text-white mb-2">{selectedPost.title}</h3>
                  <div className="flex items-center gap-4 text-xs text-zinc-400 mb-4">
                    <div>Author: {selectedPost.author_display_name}</div>
                    <div>Category: {selectedPost.category_name}</div>
                    <div>Views: {ForumUtils.formatNumber(selectedPost.views)}</div>
                    <div>Likes: {selectedPost.likes}</div>
                    <div>Comments: {postComments.length}</div>
                  </div>
                  <div className="text-zinc-300 whitespace-pre-wrap mb-4">
                    {selectedPost.content}
                  </div>
                </div>

                {/* Comments */}
                <div>
                  <h4 className="font-bold text-yellow-500 mb-2">COMMENTS ({postComments.length})</h4>
                  
                  {/* Add Comment */}
                  {isAuthenticated && (
                    <div className="flex gap-2 mb-4">
                      <Textarea
                        value={newComment}
                        onChange={(e) => setNewComment(e.target.value)}
                        placeholder="Add a comment..."
                        className="bg-zinc-800 border-zinc-600 text-white flex-1 min-h-20"
                      />
                      <Button
                        onClick={addComment}
                        disabled={commentLoading}
                        style={{ backgroundColor: colors.orange }}
                        className="hover:bg-orange-600"
                      >
                        {commentLoading ? 'POSTING...' : 'POST'}
                      </Button>
                    </div>
                  )}

                  {/* Comments List */}
                  <div className="space-y-3">
                    {postComments.map((comment) => (
                      <div key={comment.comment_uuid} className="bg-zinc-800 p-3 rounded border border-zinc-700">
                        <div className="flex items-center justify-between mb-2">
                          <div className="flex items-center gap-2">
                            <span className="text-white font-medium text-sm">
                              {comment.author_display_name}
                            </span>
                            <span className="text-zinc-500 text-xs">
                              {ForumUtils.formatTimestamp(comment.created_at)}
                            </span>
                          </div>
                          <div className="flex items-center gap-2">
                            <div 
                              className={`text-xs ${ForumUtils.getVoteColor(ForumUtils.calculateVoteScore(comment.likes, comment.dislikes))}`}
                            >
                              {ForumUtils.calculateVoteScore(comment.likes, comment.dislikes)}
                            </div>
                          </div>
                        </div>
                        <div className="text-zinc-300 text-sm whitespace-pre-wrap">
                          {comment.content}
                        </div>
                      </div>
                    ))}

                    {postComments.length === 0 && (
                      <div className="text-center text-zinc-500 py-8">
                        <MessageSquare className="h-8 w-8 mx-auto mb-2 opacity-50" />
                        <p className="text-sm">No comments yet</p>
                      </div>
                    )}
                  </div>
                </div>

                <div className="flex justify-end">
                  <Button
                    onClick={() => setShowPostDetails(false)}
                    className="bg-zinc-700 hover:bg-zinc-600"
                  >
                    CLOSE
                  </Button>
                </div>
              </div>
            </>
          )}
        </DialogContent>
      </Dialog>
    </div>
  );
};

export default ForumTab;