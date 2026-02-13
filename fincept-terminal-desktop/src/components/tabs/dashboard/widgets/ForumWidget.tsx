import React from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { ForumApiService } from '../../../../services/forum/forumApi';
import { saveSetting } from '@/services/core/sqliteService';
import { useAuth } from '@/contexts/AuthContext';
import { useCache } from '@/hooks/useCache';


interface ForumPost {
  id: string;
  title: string;
  author: string;
  category: string;
  likes: number;
  replies: number;
  time: string;
}

interface ForumWidgetProps {
  id: string;
  categoryId?: number;
  categoryName?: string;
  limit?: number;
  onRemove?: () => void;
  onNavigateToTab?: (tabName: string, postId?: string) => void;
}

export const ForumWidget: React.FC<ForumWidgetProps> = ({
  id,
  categoryId,
  categoryName = 'Recent Posts',
  limit = 5,
  onRemove,
  onNavigateToTab
}) => {
  const { t } = useTranslation('dashboard');
  const { session } = useAuth();

  const targetCategoryId = categoryId || 3;

  const {
    data: posts,
    isLoading: loading,
    error,
    refresh
  } = useCache<ForumPost[]>({
    key: `widget:forum:${targetCategoryId}:${limit}`,
    category: 'api-response',
    ttl: '10m',
    refetchInterval: 10 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      const apiKey = session?.api_key || undefined;
      const deviceId = session?.device_id || undefined;

      const response = await ForumApiService.getPostsByCategory(targetCategoryId, 'latest', limit, apiKey, deviceId);

      if (response.success) {
        const postsData = (response.data as any)?.data?.posts || (response.data as any)?.posts || [];
        return postsData.map((post: any) => ({
          id: post.post_uuid,
          title: post.title,
          author: post.author_display_name || 'Anonymous',
          category: post.category_name || 'General',
          likes: post.likes || 0,
          replies: post.reply_count || 0,
          time: new Date(post.created_at).toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit' })
        }));
      }
      throw new Error(response.error || `HTTP ${response.status_code || 'error'}`);
    }
  });

  const displayPosts = posts || [];

  return (
    <BaseWidget
      id={id}
      title={`${t('widgets.forum')} - ${categoryName}`}
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={loading && !posts}
      error={error?.message || null}
    >
      <div style={{ padding: '8px' }}>
        {displayPosts.map((post, index) => (
          <div
            key={post.id}
            onClick={async () => {
              await saveSetting('forum_selected_post_id', post.id, 'forum_widget');
              onNavigateToTab?.('Forum');
            }}
            style={{
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: index < displayPosts.length - 1 ? '1px solid var(--ft-border-color)' : 'none',
              cursor: 'pointer'
            }}
            onMouseEnter={(e) => { e.currentTarget.style.opacity = '0.7'; }}
            onMouseLeave={(e) => { e.currentTarget.style.opacity = '1'; }}
          >
            <div style={{ color: 'var(--ft-color-text)', fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', marginBottom: '2px', lineHeight: '1.2' }}>
              {post.title.substring(0, 60)}{post.title.length > 60 ? '...' : ''}
            </div>
            <div style={{ display: 'flex', gap: '8px', fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>
              <span style={{ color: 'var(--ft-color-accent)' }}>@{post.author}</span>
              <span style={{ color: 'var(--ft-color-info)' }}>[{post.category}]</span>
              <span>{post.time}</span>
            </div>
            <div style={{ display: 'flex', gap: '8px', fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)', marginTop: '2px' }}>
              <span style={{ color: 'var(--ft-color-success)' }}>👍 {post.likes}</span>
              <span>💬 {post.replies}</span>
            </div>
          </div>
        ))}
        {displayPosts.length === 0 && !loading && !error && (
          <div style={{ color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)', textAlign: 'center', padding: '12px' }}>
            {t('widgets.noForumPosts')}
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
