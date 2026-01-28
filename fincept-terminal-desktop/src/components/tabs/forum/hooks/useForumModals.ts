// Hook for managing forum modal states

import { useState, useCallback } from 'react';
import type { ProfileEdit, ForumColors } from '../types';

interface UseForumModalsProps {
  colors: ForumColors;
}

export interface UseForumModalsReturn {
  // Modal visibility states
  showCreatePost: boolean;
  showPostDetail: boolean;
  showSearch: boolean;
  showProfile: boolean;
  showEditProfile: boolean;

  // Form states
  newPostTitle: string;
  newPostContent: string;
  newPostTags: string;
  newPostCategory: string;
  newComment: string;
  searchQuery: string;
  profileEdit: ProfileEdit;

  // Actions
  setShowCreatePost: (show: boolean) => void;
  setShowPostDetail: (show: boolean) => void;
  setShowSearch: (show: boolean) => void;
  setShowProfile: (show: boolean) => void;
  setShowEditProfile: (show: boolean) => void;
  setNewPostTitle: (title: string) => void;
  setNewPostContent: (content: string) => void;
  setNewPostTags: (tags: string) => void;
  setNewPostCategory: (category: string) => void;
  setNewComment: (comment: string) => void;
  setSearchQuery: (query: string) => void;
  setProfileEdit: (edit: ProfileEdit) => void;
  resetCreatePostForm: () => void;
  resetSearchForm: () => void;
  initProfileEdit: (profile: any) => void;
}

export function useForumModals({ colors }: UseForumModalsProps): UseForumModalsReturn {
  // Modal visibility states
  const [showCreatePost, setShowCreatePost] = useState(false);
  const [showPostDetail, setShowPostDetail] = useState(false);
  const [showSearch, setShowSearch] = useState(false);
  const [showProfile, setShowProfile] = useState(false);
  const [showEditProfile, setShowEditProfile] = useState(false);

  // Form states
  const [newPostTitle, setNewPostTitle] = useState('');
  const [newPostContent, setNewPostContent] = useState('');
  const [newPostTags, setNewPostTags] = useState('');
  const [newPostCategory, setNewPostCategory] = useState('');
  const [newComment, setNewComment] = useState('');
  const [searchQuery, setSearchQuery] = useState('');
  const [profileEdit, setProfileEdit] = useState<ProfileEdit>({
    display_name: '',
    bio: '',
    avatar_color: colors.ORANGE,
    signature: ''
  });

  // Reset create post form
  const resetCreatePostForm = useCallback(() => {
    setNewPostTitle('');
    setNewPostContent('');
    setNewPostTags('');
    setNewPostCategory('');
  }, []);

  // Reset search form
  const resetSearchForm = useCallback(() => {
    setSearchQuery('');
  }, []);

  // Initialize profile edit from profile data
  const initProfileEdit = useCallback((profile: any) => {
    setProfileEdit({
      display_name: profile?.display_name || '',
      bio: profile?.bio || '',
      avatar_color: profile?.avatar_color || colors.ORANGE,
      signature: profile?.signature || ''
    });
  }, [colors.ORANGE]);

  return {
    showCreatePost,
    showPostDetail,
    showSearch,
    showProfile,
    showEditProfile,
    newPostTitle,
    newPostContent,
    newPostTags,
    newPostCategory,
    newComment,
    searchQuery,
    profileEdit,
    setShowCreatePost,
    setShowPostDetail,
    setShowSearch,
    setShowProfile,
    setShowEditProfile,
    setNewPostTitle,
    setNewPostContent,
    setNewPostTags,
    setNewPostCategory,
    setNewComment,
    setSearchQuery,
    setProfileEdit,
    resetCreatePostForm,
    resetSearchForm,
    initProfileEdit
  };
}
