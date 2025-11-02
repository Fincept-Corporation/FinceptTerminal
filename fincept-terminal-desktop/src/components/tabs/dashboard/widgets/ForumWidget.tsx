import React, { useState, useEffect } from 'react';
import { BaseWidget } from './BaseWidget';
import { ForumApiService } from '../../../../services/forumApi';

const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GRAY = '#787878';
const BLOOMBERG_BLUE = '#6496FA';
const BLOOMBERG_CYAN = '#00FFFF';
const BLOOMBERG_GREEN = '#00C800';

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
}

export const ForumWidget: React.FC<ForumWidgetProps> = ({
  id,
  categoryId,
  categoryName = 'Recent Posts',
  limit = 5,
  onRemove
}) => {
  const [posts, setPosts] = useState<ForumPost[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadPosts = async () => {
    setLoading(true);
    setError(null);
    try {
      // Get API credentials from session/localStorage
      const apiKey = localStorage.getItem('fincept_api_key') || undefined;
      const deviceId = localStorage.getItem('fincept_device_id') || undefined;

      console.log('[ForumWidget] Loading posts with:', { categoryId, limit, apiKey: !!apiKey, deviceId: !!deviceId });

      // If no categoryId is provided, default to category 1 (like ForumTab does)
      // The backend requires a category ID, there is no "get all posts" endpoint
      const targetCategoryId = categoryId || 1;

      const response = await ForumApiService.getPostsByCategory(targetCategoryId, 'latest', limit, apiKey, deviceId);

      console.log('[ForumWidget] Response:', response);

      if (response.success) {
        // Handle both response formats
        const postsData = (response.data as any)?.data?.posts || (response.data as any)?.posts || [];
        console.log('[ForumWidget] Posts data:', postsData);

        const formattedPosts = postsData.map((post: any) => ({
          id: post.post_uuid,
          title: post.title,
          author: post.author_display_name || 'Anonymous',
          category: post.category_name || 'General',
          likes: post.likes || 0,
          replies: post.reply_count || 0,
          time: new Date(post.created_at).toLocaleTimeString('en-US', { hour12: false, hour: '2-digit', minute: '2-digit' })
        }));

        console.log('[ForumWidget] Formatted posts:', formattedPosts);
        setPosts(formattedPosts);
      } else {
        console.error('[ForumWidget] Error response:', response);
        setError(response.error || `HTTP ${response.status_code || 'error'}`);
      }
    } catch (err) {
      console.error('[ForumWidget] Exception:', err);
      setError(err instanceof Error ? err.message : 'Failed to load forum posts');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadPosts();
    const interval = setInterval(loadPosts, 10 * 60 * 1000); // Refresh every 10 minutes
    return () => clearInterval(interval);
  }, [categoryId, limit]);

  return (
    <BaseWidget
      id={id}
      title={`FORUM - ${categoryName}`}
      onRemove={onRemove}
      onRefresh={loadPosts}
      isLoading={loading}
      error={error}
    >
      <div style={{ padding: '8px' }}>
        {posts.map((post, index) => (
          <div
            key={post.id}
            style={{
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: index < posts.length - 1 ? `1px solid ${BLOOMBERG_GRAY}` : 'none'
            }}
          >
            <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px', fontWeight: 'bold', marginBottom: '2px', lineHeight: '1.2' }}>
              {post.title.substring(0, 60)}{post.title.length > 60 ? '...' : ''}
            </div>
            <div style={{ display: 'flex', gap: '8px', fontSize: '8px', color: BLOOMBERG_GRAY }}>
              <span style={{ color: BLOOMBERG_CYAN }}>@{post.author}</span>
              <span style={{ color: BLOOMBERG_BLUE }}>[{post.category}]</span>
              <span>{post.time}</span>
            </div>
            <div style={{ display: 'flex', gap: '8px', fontSize: '8px', color: BLOOMBERG_GRAY, marginTop: '2px' }}>
              <span style={{ color: BLOOMBERG_GREEN }}>👍 {post.likes}</span>
              <span>💬 {post.replies}</span>
            </div>
          </div>
        ))}
        {posts.length === 0 && !loading && !error && (
          <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px', textAlign: 'center', padding: '12px' }}>
            No forum posts available
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
