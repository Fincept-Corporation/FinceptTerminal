import React from 'react';
import { StickyNote, Plus, Star, Archive, AlertCircle, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { notesService, Note } from '@/services/core/notesService';
import { useTranslation } from 'react-i18next';
import { useCache } from '@/hooks/useCache';

interface NotesWidgetProps {
  id: string;
  category?: string;
  limit?: number;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface NotesData {
  notes: Note[];
  total: number;
}

const PRIORITY_COLORS = {
  HIGH: 'var(--ft-color-alert)',
  MEDIUM: 'var(--ft-color-warning)',
  LOW: 'var(--ft-color-text-muted)'
};

const SENTIMENT_COLORS = {
  BULLISH: 'var(--ft-color-success)',
  BEARISH: 'var(--ft-color-alert)',
  NEUTRAL: 'var(--ft-color-text-muted)'
};

export const NotesWidget: React.FC<NotesWidgetProps> = ({
  id,
  category,
  limit = 10,
  onRemove,
  onNavigate
}) => {
  const { t } = useTranslation('dashboard');

  const {
    data: notesData,
    isLoading: loading,
    isFetching,
    error,
    refresh
  } = useCache<NotesData>({
    key: `widget:notes:${category || 'all'}`,
    category: 'notes',
    ttl: '1m',
    refetchInterval: 30 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        await notesService.initialize();
        const allNotes = await notesService.getAllNotes(false);

        let filteredNotes = allNotes.filter((note: Note) => !note.is_archived);

        if (category && category !== 'all') {
          filteredNotes = filteredNotes.filter((note: Note) => note.category === category);
        }

        // Sort by: favorites first, then by priority, then by updated date
        filteredNotes.sort((a: Note, b: Note) => {
          if (a.is_favorite !== b.is_favorite) {
            return a.is_favorite ? -1 : 1;
          }
          const priorityOrder = { HIGH: 0, MEDIUM: 1, LOW: 2 };
          const aPriority = priorityOrder[a.priority as keyof typeof priorityOrder] ?? 3;
          const bPriority = priorityOrder[b.priority as keyof typeof priorityOrder] ?? 3;
          if (aPriority !== bPriority) {
            return aPriority - bPriority;
          }
          return new Date(b.updated_at).getTime() - new Date(a.updated_at).getTime();
        });

        return {
          notes: filteredNotes.slice(0, limit),
          total: filteredNotes.length
        };
      } catch (err) {
        console.error('[NotesWidget] Error fetching notes:', err);
        return { notes: [], total: 0 };
      }
    }
  });

  const { notes, total } = notesData || { notes: [], total: 0 };

  const formatDate = (dateStr: string) => {
    const date = new Date(dateStr);
    const now = new Date();
    const diffMs = now.getTime() - date.getTime();
    const diffMins = Math.floor(diffMs / 60000);
    const diffHours = Math.floor(diffMs / 3600000);
    const diffDays = Math.floor(diffMs / 86400000);

    if (diffMins < 1) return 'Just now';
    if (diffMins < 60) return `${diffMins}m ago`;
    if (diffHours < 24) return `${diffHours}h ago`;
    if (diffDays < 7) return `${diffDays}d ago`;
    return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' });
  };

  const truncateContent = (content: string, maxLength: number = 80) => {
    if (content.length <= maxLength) return content;
    return content.substring(0, maxLength) + '...';
  };

  return (
    <BaseWidget
      id={id}
      title={category ? `NOTES: ${category}` : 'RECENT NOTES'}
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(loading && !notesData) || isFetching}
      error={error?.message || null}
      headerColor="var(--ft-color-warning)"
    >
      {notes.length === 0 ? (
        <div style={{
          padding: '24px',
          textAlign: 'center',
          color: 'var(--ft-color-text-muted)',
          fontSize: 'var(--ft-font-size-small)'
        }}>
          <StickyNote size={32} style={{ margin: '0 auto 12px', opacity: 0.3 }} />
          <div style={{ marginBottom: '8px' }}>No notes available</div>
          <div style={{ fontSize: 'var(--ft-font-size-tiny)' }}>
            Create notes to track research, ideas, and analysis
          </div>
          {onNavigate && (
            <button
              onClick={onNavigate}
              style={{
                marginTop: '12px',
                background: 'var(--ft-color-warning)',
                color: '#000',
                border: 'none',
                padding: '6px 16px',
                fontSize: 'var(--ft-font-size-tiny)',
                fontWeight: 'bold',
                cursor: 'pointer',
                borderRadius: '2px',
                display: 'inline-flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Plus size={12} /> CREATE NOTE
            </button>
          )}
        </div>
      ) : (
        <div style={{ padding: '4px' }}>
          {/* Header Stats */}
          <div style={{
            padding: '6px 8px',
            backgroundColor: 'var(--ft-color-panel)',
            margin: '4px',
            borderRadius: '2px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            fontSize: 'var(--ft-font-size-tiny)',
            color: 'var(--ft-color-text-muted)'
          }}>
            <div style={{ display: 'flex', gap: '12px' }}>
              <span>TOTAL: <span style={{ color: 'var(--ft-color-warning)', fontWeight: 'bold' }}>{total}</span></span>
              <span>SHOWN: <span style={{ color: 'var(--ft-color-primary)', fontWeight: 'bold' }}>{notes.length}</span></span>
            </div>
            <span style={{ color: 'var(--ft-color-text-muted)' }}>
              {notes.filter(n => n.is_favorite).length} ★
            </span>
          </div>

          {/* Notes List */}
          <div style={{ maxHeight: '100%', overflowY: 'auto' }}>
            {notes.map((note) => (
              <div
                key={note.id}
                onClick={() => onNavigate?.()}
                style={{
                  padding: '8px',
                  margin: '4px',
                  backgroundColor: 'var(--ft-color-panel)',
                  borderLeft: `3px solid ${note.color_code || 'var(--ft-color-warning)'}`,
                  borderRadius: '2px',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  position: 'relative'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = 'var(--ft-color-hover)';
                  e.currentTarget.style.transform = 'translateX(2px)';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = 'var(--ft-color-panel)';
                  e.currentTarget.style.transform = 'translateX(0)';
                }}
              >
                {/* Title Row */}
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'flex-start',
                  marginBottom: '4px'
                }}>
                  <div style={{
                    flex: 1,
                    fontSize: 'var(--ft-font-size-small)',
                    fontWeight: 'bold',
                    color: 'var(--ft-color-text)',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px'
                  }}>
                    {note.is_favorite && <Star size={10} fill="var(--ft-color-warning)" color="var(--ft-color-warning)" />}
                    {note.title || 'Untitled'}
                  </div>
                  <div style={{
                    fontSize: 'var(--ft-font-size-tiny)',
                    color: PRIORITY_COLORS[note.priority as keyof typeof PRIORITY_COLORS] || 'var(--ft-color-text-muted)',
                    fontWeight: 'bold'
                  }}>
                    {note.priority}
                  </div>
                </div>

                {/* Content Preview */}
                <div style={{
                  fontSize: 'var(--ft-font-size-tiny)',
                  color: 'var(--ft-color-text-muted)',
                  marginBottom: '6px',
                  lineHeight: '1.4'
                }}>
                  {truncateContent(note.content)}
                </div>

                {/* Meta Row */}
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  fontSize: 'var(--ft-font-size-tiny)',
                  color: 'var(--ft-color-text-muted)'
                }}>
                  <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                    {note.sentiment && note.sentiment !== 'NEUTRAL' && (
                      <span style={{
                        color: SENTIMENT_COLORS[note.sentiment as keyof typeof SENTIMENT_COLORS],
                        fontWeight: 'bold'
                      }}>
                        {note.sentiment === 'BULLISH' ? '↑' : '↓'}
                      </span>
                    )}
                    {note.tickers && (
                      <span style={{ color: 'var(--ft-color-primary)' }}>
                        {note.tickers.split(',').slice(0, 2).join(', ')}
                      </span>
                    )}
                    {note.category && (
                      <span style={{
                        backgroundColor: 'var(--ft-border-color)',
                        padding: '1px 4px',
                        borderRadius: '2px',
                        fontSize: '8px'
                      }}>
                        {note.category}
                      </span>
                    )}
                  </div>
                  <span>{formatDate(note.updated_at)}</span>
                </div>

                {/* Reminder Indicator */}
                {note.reminder_date && new Date(note.reminder_date) > new Date() && (
                  <div style={{
                    position: 'absolute',
                    top: '4px',
                    right: '4px',
                    fontSize: '10px'
                  }}>
                    <AlertCircle size={10} color="var(--ft-color-warning)" />
                  </div>
                )}
              </div>
            ))}
          </div>

          {/* Footer Action */}
          {onNavigate && total > limit && (
            <div
              onClick={onNavigate}
              style={{
                padding: '6px',
                textAlign: 'center',
                color: 'var(--ft-color-warning)',
                fontSize: 'var(--ft-font-size-tiny)',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '4px',
                borderTop: '1px solid var(--ft-border-color)',
                marginTop: '4px',
                fontWeight: 'bold'
              }}
            >
              <span>VIEW ALL {total} NOTES</span>
              <ExternalLink size={10} />
            </div>
          )}
        </div>
      )}
    </BaseWidget>
  );
};
