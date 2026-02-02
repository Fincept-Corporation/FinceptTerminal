// Forum Tab - Main Component
// Redesigned to follow Fincept Terminal UI Design System
// Uses: State machine (via hooks), cleanup, error boundaries

import React, { useState, useEffect, useRef } from 'react';
import { APP_VERSION } from '@/constants/version';
import { MessageSquare, Users, TrendingUp, RefreshCw, Plus, Search as SearchIcon, User, Shield } from 'lucide-react';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';

// Local imports
import { useForum } from './hooks/useForum';
import { useForumModals } from './hooks/useForumModals';
import {
  LeftPanel,
  CenterPanel,
  RightPanel,
  CreatePostModal,
  PostDetailModal,
  SearchModal,
  ProfileModal
} from './components';

// Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

const ForumTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const mountedRef = useRef(true);

  // Forum data and actions
  const forum = useForum({ colors: FINCEPT });

  // Modal states
  const modals = useForumModals({ colors: FINCEPT });

  // Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  // Update time every second with cleanup
  useEffect(() => {
    const interval = setInterval(() => {
      if (mountedRef.current) {
        setCurrentTime(new Date());
      }
    }, 1000);
    return () => clearInterval(interval);
  }, []);

  // Handlers that bridge modals and forum actions
  const handleCreatePost = async () => {
    if (!modals.newPostTitle.trim() || !modals.newPostContent.trim()) return;
    const success = await forum.handleCreatePost(modals.newPostTitle, modals.newPostContent);
    if (success) {
      modals.resetCreatePostForm();
      modals.setShowCreatePost(false);
    }
  };

  const handleViewPost = async (post: typeof forum.selectedPost) => {
    if (post) {
      await forum.handleViewPost(post);
      modals.setShowPostDetail(true);
    }
  };

  const handleAddComment = async () => {
    if (!modals.newComment.trim()) return;
    const success = await forum.handleAddComment(modals.newComment);
    if (success) {
      modals.setNewComment('');
    }
  };

  const handleSearch = async () => {
    if (!modals.searchQuery.trim()) return;
    await forum.handleSearch(modals.searchQuery);
  };

  const handleViewProfile = async () => {
    await forum.handleViewMyProfile();
    if (forum.userProfile) {
      modals.initProfileEdit(forum.userProfile);
    }
    modals.setShowProfile(true);
  };

  const handleViewUserProfile = async (username: string) => {
    await forum.handleViewUserProfile(username);
    modals.setShowProfile(true);
  };

  const handleSaveProfile = async () => {
    const success = await forum.handleUpdateProfile(modals.profileEdit);
    if (success) {
      modals.setShowEditProfile(false);
      modals.setShowProfile(true);
    }
  };

  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: FONT_FAMILY,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Custom Scrollbar */}
      <style>{`
        .forum-scroll::-webkit-scrollbar { width: 6px; height: 6px; }
        .forum-scroll::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        .forum-scroll::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        .forum-scroll::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
      `}</style>

      {/* Top Navigation Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <MessageSquare size={16} color={FINCEPT.ORANGE} />
            <span style={{
              fontSize: '12px',
              fontWeight: 700,
              color: FINCEPT.ORANGE,
              letterSpacing: '0.5px'
            }}>
              GLOBAL FORUM
            </span>
          </div>
          <span style={{ color: FINCEPT.BORDER }}>|</span>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <MessageSquare size={12} color={FINCEPT.CYAN} />
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
              {forum.totalPosts.toLocaleString()} POSTS
            </span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <Users size={12} color={FINCEPT.GREEN} />
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
              {forum.onlineUsers} ONLINE
            </span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            <TrendingUp size={12} color={FINCEPT.YELLOW} />
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
              {forum.postsToday} TODAY
            </span>
          </div>
        </div>

        {/* Action Buttons */}
        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
          <button
            onClick={() => modals.setShowCreatePost(true)}
            style={{
              padding: '6px 12px',
              backgroundColor: FINCEPT.ORANGE,
              color: FINCEPT.DARK_BG,
              border: 'none',
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontFamily: FONT_FAMILY
            }}
          >
            <Plus size={10} />
            NEW POST
          </button>
          <button
            onClick={() => modals.setShowSearch(true)}
            style={{
              padding: '6px 12px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontFamily: FONT_FAMILY
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              e.currentTarget.style.color = FINCEPT.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.BORDER;
              e.currentTarget.style.color = FINCEPT.GRAY;
            }}
          >
            <SearchIcon size={10} />
            SEARCH
          </button>
          <button
            onClick={handleViewProfile}
            style={{
              padding: '6px 12px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontFamily: FONT_FAMILY
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              e.currentTarget.style.color = FINCEPT.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.BORDER;
              e.currentTarget.style.color = FINCEPT.GRAY;
            }}
          >
            <User size={10} />
            PROFILE
          </button>
          <button
            onClick={forum.handleRefresh}
            disabled={forum.isRefreshing}
            style={{
              padding: '6px',
              backgroundColor: 'transparent',
              border: 'none',
              color: forum.isRefreshing ? FINCEPT.MUTED : FINCEPT.GRAY,
              cursor: forum.isRefreshing ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center'
            }}
            title="Refresh"
          >
            <RefreshCw size={12} style={{
              animation: forum.isRefreshing ? 'spin 1s linear infinite' : 'none'
            }} />
          </button>
        </div>
      </div>

      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
      `}</style>

      {/* Category Filter Bar */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        padding: '8px 16px',
        display: 'flex',
        gap: '6px',
        overflowX: 'auto',
        flexShrink: 0
      }}>
        {forum.categories.map((cat) => (
          <button
            key={cat.name}
            onClick={() => forum.setActiveCategory(cat.name)}
            style={{
              padding: '4px 12px',
              backgroundColor: forum.activeCategory === cat.name ? FINCEPT.ORANGE : 'transparent',
              color: forum.activeCategory === cat.name ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: forum.activeCategory === cat.name ? 'none' : `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              whiteSpace: 'nowrap',
              fontFamily: FONT_FAMILY,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (forum.activeCategory !== cat.name) {
                e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                e.currentTarget.style.color = FINCEPT.WHITE;
              }
            }}
            onMouseLeave={(e) => {
              if (forum.activeCategory !== cat.name) {
                e.currentTarget.style.backgroundColor = 'transparent';
                e.currentTarget.style.color = FINCEPT.GRAY;
              }
            }}
          >
            {cat.name} ({cat.count})
          </button>
        ))}
      </div>

      {/* Main Content - Three Panel Layout */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* Left Panel - Stats & Quick Info */}
        <ErrorBoundary name="ForumLeftPanel" variant="minimal">
          <LeftPanel
            colors={FINCEPT}
            categories={forum.categories}
            activeCategory={forum.activeCategory}
            onCategoryChange={forum.setActiveCategory}
            onViewUserProfile={handleViewUserProfile}
            forumStats={forum.forumStats}
            totalPosts={forum.totalPosts}
            postsToday={forum.postsToday}
            onlineUsers={forum.onlineUsers}
          />
        </ErrorBoundary>

        {/* Center Panel - Posts List */}
        <ErrorBoundary name="ForumCenterPanel" variant="minimal">
          <CenterPanel
            colors={FINCEPT}
            activeCategory={forum.activeCategory}
            sortBy={forum.sortBy}
            onSortChange={forum.setSortBy}
            filteredPosts={forum.filteredPosts}
            isLoading={forum.isLoading}
            onViewPost={handleViewPost}
          />
        </ErrorBoundary>

        {/* Right Panel - Trending & Activity */}
        <ErrorBoundary name="ForumRightPanel" variant="minimal">
          <RightPanel
            colors={FINCEPT}
            trendingTopics={forum.trendingTopics}
            recentActivity={forum.recentActivity}
            forumPosts={forum.forumPosts}
          />
        </ErrorBoundary>
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', gap: '16px' }}>
          <span>FORUM v{APP_VERSION}</span>
          <span>POSTS: <span style={{ color: FINCEPT.CYAN }}>{forum.totalPosts.toLocaleString()}</span></span>
          <span>USERS: <span style={{ color: FINCEPT.CYAN }}>{(forum.forumStats?.total_users || 0).toLocaleString()}</span></span>
        </div>
        <div style={{ display: 'flex', gap: '16px', alignItems: 'center' }}>
          <span>CATEGORY: <span style={{ color: FINCEPT.ORANGE }}>{forum.activeCategory}</span></span>
          <span>SORT: <span style={{ color: FINCEPT.ORANGE }}>{forum.sortBy.toUpperCase()}</span></span>
          <span>STATUS: <span style={{ color: FINCEPT.GREEN }}>LIVE</span></span>
          <Shield size={10} color={FINCEPT.GREEN} />
        </div>
      </div>

      {/* Modals */}
      <CreatePostModal
        show={modals.showCreatePost}
        colors={FINCEPT}
        title={modals.newPostTitle}
        content={modals.newPostContent}
        tags={modals.newPostTags}
        category={modals.newPostCategory}
        onTitleChange={modals.setNewPostTitle}
        onContentChange={modals.setNewPostContent}
        onTagsChange={modals.setNewPostTags}
        onCategoryChange={modals.setNewPostCategory}
        onSubmit={handleCreatePost}
        onClose={() => modals.setShowCreatePost(false)}
      />

      <PostDetailModal
        show={modals.showPostDetail}
        colors={FINCEPT}
        post={forum.selectedPost}
        comments={forum.postComments}
        newComment={modals.newComment}
        onCommentChange={modals.setNewComment}
        onAddComment={handleAddComment}
        onVote={forum.handleVote}
        onClose={() => modals.setShowPostDetail(false)}
      />

      <SearchModal
        show={modals.showSearch}
        colors={FINCEPT}
        query={modals.searchQuery}
        results={forum.searchResults}
        onQueryChange={modals.setSearchQuery}
        onSearch={handleSearch}
        onClose={() => {
          modals.setShowSearch(false);
          modals.resetSearchForm();
        }}
        onViewPost={handleViewPost}
      />

      <ProfileModal
        show={modals.showProfile}
        colors={FINCEPT}
        profile={forum.userProfile}
        isOwnProfile={!forum.selectedUsername}
        onEdit={() => {
          modals.setShowProfile(false);
          modals.setShowEditProfile(true);
        }}
        onClose={() => {
          modals.setShowProfile(false);
          forum.setSelectedUsername('');
        }}
      />
    </div>
  );
};

export default ForumTab;
