// Forum Tab Types

import type { ForumPost as APIForumPost, ForumCategory, ForumStats, ForumComment as APIForumComment } from '@/services/forum/forumApi';

// Re-export API types
export type { APIForumPost, ForumCategory, ForumStats, APIForumComment };

export interface ForumPost {
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

export interface ForumComment {
  id: string;
  author: string;
  content: string;
  time: string;
  likes: number;
  dislikes: number;
}

export interface ForumUser {
  username: string;
  reputation: number;
  posts: number;
  joined: string;
  status: 'ONLINE' | 'OFFLINE';
  // Profile fields (from API)
  display_name?: string;
  bio?: string;
  signature?: string;
  post_count?: number;
  comment_count?: number;
}

export interface ForumCategoryUI {
  name: string;
  count: number;
  color: string;
  id: number;
}

export interface TrendingTopic {
  topic: string;
  mentions: number;
  sentiment: string;
  change?: string;
}

export interface RecentActivity {
  user: string;
  action: string;
  target: string;
  time: string;
}

export interface ProfileEdit {
  display_name: string;
  bio: string;
  avatar_color: string;
  signature: string;
}

export interface ForumColors {
  ORANGE: string;
  WHITE: string;
  RED: string;
  GREEN: string;
  YELLOW: string;
  GRAY: string;
  BLUE: string;
  PURPLE: string;
  CYAN: string;
  DARK_BG: string;
  PANEL_BG: string;
  HEADER_BG: string;
  BORDER: string;
  HOVER: string;
  MUTED: string;
}
