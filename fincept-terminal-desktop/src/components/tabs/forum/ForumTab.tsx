// Forum Tab - Main Component
// Refactored from 1764-line monolith into modular components

import React, { useState, useEffect, useMemo } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { APP_VERSION } from '@/constants/version';
import { TabFooter } from '@/components/common/TabFooter';

// Local imports
import { getForumColors } from './constants';
import { useForum } from './hooks/useForum';
import { useForumModals } from './hooks/useForumModals';
import {
  Header,
  FunctionBar,
  LeftPanel,
  CenterPanel,
  RightPanel,
  CreatePostModal,
  PostDetailModal,
  SearchModal,
  ProfileModal
} from './components';

const ForumTab: React.FC = () => {
  const { colors } = useTerminalTheme();
  const [currentTime, setCurrentTime] = useState(new Date());

  // Get forum-specific colors
  const forumColors = useMemo(() => getForumColors(colors), [colors]);

  // Forum data and actions
  const forum = useForum({ colors: forumColors });

  // Modal states
  const modals = useForumModals({ colors: forumColors });

  // Update time every second
  useEffect(() => {
    const interval = setInterval(() => setCurrentTime(new Date()), 1000);
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
    // Search results are stored in forum.searchResults
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

  const handleOpenEditProfile = () => {
    modals.setShowProfile(false);
    modals.setShowEditProfile(true);
  };

  const handleSaveProfile = async () => {
    const success = await forum.handleUpdateProfile(modals.profileEdit);
    if (success) {
      modals.setShowEditProfile(false);
      modals.setShowProfile(true);
    }
  };

  const handleCloseSearch = () => {
    modals.setShowSearch(false);
    modals.resetSearchForm();
  };

  const handleCloseProfile = () => {
    modals.setShowProfile(false);
    forum.setSelectedUsername('');
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: forumColors.DARK_BG,
      color: forumColors.WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '12px'
    }}>
      {/* Header */}
      <Header
        colors={forumColors}
        currentTime={currentTime}
        onlineUsers={forum.onlineUsers}
        postsToday={forum.postsToday}
        trendingTopics={forum.trendingTopics}
        isRefreshing={forum.isRefreshing}
        lastRefreshTime={forum.lastRefreshTime}
        onRefresh={forum.handleRefresh}
      />

      {/* Function Bar */}
      <FunctionBar
        colors={forumColors}
        activeCategory={forum.activeCategory}
        categories={forum.categories}
        onCategoryChange={forum.setActiveCategory}
        onCreatePost={() => modals.setShowCreatePost(true)}
        onSearch={() => modals.setShowSearch(true)}
        onViewProfile={handleViewProfile}
        onEditProfile={() => modals.setShowEditProfile(true)}
      />

      {/* Main Content Area */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden', padding: '4px', gap: '4px' }}>
        {/* Left Panel - Categories & Stats */}
        <LeftPanel
          colors={forumColors}
          categories={forum.categories}
          activeCategory={forum.activeCategory}
          onCategoryChange={forum.setActiveCategory}
          onViewUserProfile={handleViewUserProfile}
          forumStats={forum.forumStats}
          totalPosts={forum.totalPosts}
          postsToday={forum.postsToday}
          onlineUsers={forum.onlineUsers}
        />

        {/* Center Panel - Posts List */}
        <CenterPanel
          colors={forumColors}
          activeCategory={forum.activeCategory}
          sortBy={forum.sortBy}
          onSortChange={forum.setSortBy}
          filteredPosts={forum.filteredPosts}
          isLoading={forum.isLoading}
          onViewPost={handleViewPost}
        />

        {/* Right Panel - Trending & Activity */}
        <RightPanel
          colors={forumColors}
          trendingTopics={forum.trendingTopics}
          recentActivity={forum.recentActivity}
        />
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${forumColors.GRAY}`,
        backgroundColor: forumColors.PANEL_BG,
        padding: '2px 8px',
        fontSize: '10px',
        color: forumColors.GRAY,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Fincept Global Forum v{APP_VERSION} | Professional trading community</span>
            <span>Posts: {forum.totalPosts.toLocaleString()} | Users: {(forum.forumStats?.total_comments || 47832).toLocaleString()} | Active: {forum.onlineUsers}</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>Category: {forum.activeCategory}</span>
            <span>Sort: {forum.sortBy}</span>
            <span style={{ color: forumColors.GREEN }}>STATUS: LIVE</span>
          </div>
        </div>
      </div>

      {/* Modals */}
      <CreatePostModal
        colors={forumColors}
        show={modals.showCreatePost}
        newPostTitle={modals.newPostTitle}
        newPostContent={modals.newPostContent}
        onTitleChange={modals.setNewPostTitle}
        onContentChange={modals.setNewPostContent}
        onClose={() => modals.setShowCreatePost(false)}
        onSubmit={handleCreatePost}
      />

      <PostDetailModal
        colors={forumColors}
        show={modals.showPostDetail}
        selectedPost={forum.selectedPost}
        postComments={forum.postComments}
        newComment={modals.newComment}
        onCommentChange={modals.setNewComment}
        onClose={() => modals.setShowPostDetail(false)}
        onAddComment={handleAddComment}
        onVotePost={(postId, voteType) => forum.handleVotePost(postId, voteType)}
        onVoteComment={(commentId, voteType) => forum.handleVoteComment(commentId, voteType)}
      />

      <SearchModal
        colors={forumColors}
        show={modals.showSearch}
        searchQuery={modals.searchQuery}
        searchResults={forum.searchResults}
        isLoading={forum.isLoading}
        onQueryChange={modals.setSearchQuery}
        onSearch={handleSearch}
        onClose={handleCloseSearch}
        onViewPost={handleViewPost}
      />

      <ProfileModal
        colors={forumColors}
        showProfile={modals.showProfile}
        showEditProfile={modals.showEditProfile}
        userProfile={forum.userProfile}
        profileEdit={modals.profileEdit}
        isOwnProfile={!forum.selectedUsername}
        onProfileEditChange={modals.setProfileEdit}
        onCloseProfile={handleCloseProfile}
        onCloseEditProfile={() => modals.setShowEditProfile(false)}
        onOpenEditProfile={handleOpenEditProfile}
        onSaveProfile={handleSaveProfile}
      />

      <TabFooter
        tabName="COMMUNITY FORUM"
        leftInfo={[
          { label: `Category: ${forum.activeCategory || 'All'}`, color: colors.textMuted },
          { label: `Posts: ${forum.forumStats?.total_posts || 0}`, color: colors.textMuted },
        ]}
        statusInfo={`Users: ${forum.forumStats?.total_users || 0} | ${forum.isLoading ? 'Loading...' : 'Ready'}`}
        backgroundColor={colors.panel}
        borderColor={colors.textMuted}
      />
    </div>
  );
};

export default ForumTab;
