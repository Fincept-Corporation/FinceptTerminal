import React from 'react';
import { Search, Trash2, Edit2, Check, X } from 'lucide-react';
import { ChatSession } from '@/services/core/sqliteService';

interface SessionsPanelProps {
  colors: any;
  fontSize: any;
  sessions: ChatSession[];
  filteredSessions: ChatSession[];
  currentSessionUuid: string | null;
  renamingSessionId: string | null;
  renameValue: string;
  searchQuery: string;
  statistics: { totalSessions: number; totalMessages: number; totalTokens: number };
  sessionsListRef: React.RefObject<HTMLDivElement | null>;
  t: (key: string) => string;
  onSelectSession: (uuid: string) => void;
  onDeleteSession: (uuid: string) => void;
  onDeleteAllSessions: () => void;
  onStartRenaming: (uuid: string, title: string) => void;
  onSaveRename: (uuid: string) => void;
  onCancelRename: () => void;
  onRenameValueChange: (v: string) => void;
  onSearchChange: (q: string) => void;
  formatSessionTime: (dateString: string | undefined) => string;
}

export function SessionsPanel({
  colors,
  fontSize,
  sessions,
  filteredSessions,
  currentSessionUuid,
  renamingSessionId,
  renameValue,
  searchQuery,
  statistics,
  sessionsListRef,
  t,
  onSelectSession,
  onDeleteSession,
  onDeleteAllSessions,
  onStartRenaming,
  onSaveRename,
  onCancelRename,
  onRenameValueChange,
  onSearchChange,
  formatSessionTime,
}: SessionsPanelProps) {
  return (
    <div style={{
      width: '280px',
      backgroundColor: colors.panel,
      borderRight: `1px solid ${'rgba(255,255,255,0.1)'}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Section Header */}
      <div style={{
        padding: '12px',
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${'rgba(255,255,255,0.1)'}`
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
          <span style={{
            fontSize: fontSize.small,
            fontWeight: 700,
            color: colors.textMuted,
            letterSpacing: '0.5px'
          }}>
            {t('history.sessions').toUpperCase()} ({statistics.totalSessions})
          </span>
          {sessions.length > 0 && (
            <button
              onClick={onDeleteAllSessions}
              style={{
                padding: '2px 6px',
                backgroundColor: `${colors.alert}20`,
                color: colors.alert,
                border: 'none',
                fontSize: fontSize.tiny,
                fontWeight: 700,
                borderRadius: '2px',
                cursor: 'pointer',
                letterSpacing: '0.5px',
                transition: 'all 0.2s'
              }}
              title={t('history.deleteAll')}
            >
              {t('history.clearAll').toUpperCase()}
            </button>
          )}
        </div>

        {/* Search */}
        <div style={{ position: 'relative', marginBottom: '8px' }}>
          <Search size={12} color={colors.textMuted} style={{ position: 'absolute', left: '8px', top: '9px' }} />
          <input
            id="session-search"
            type="text"
            placeholder={t('input.placeholder')}
            value={searchQuery}
            onChange={(e) => onSearchChange(e.target.value)}
            style={{
              width: '100%',
              padding: '8px 10px 8px 32px',
              backgroundColor: colors.background,
              color: colors.text,
              border: `1px solid ${'rgba(255,255,255,0.1)'}`,
              borderRadius: '2px',
              fontSize: fontSize.small,
              fontFamily: '"IBM Plex Mono", "Consolas", monospace'
            }}
          />
        </div>

        {/* Stats */}
        <div style={{
          fontSize: fontSize.small,
          color: colors.secondary,
          letterSpacing: '0.5px',
          fontFamily: '"IBM Plex Mono", "Consolas", monospace'
        }}>
          MESSAGES: {statistics.totalMessages} | TOKENS: {statistics.totalTokens.toLocaleString()}
        </div>
      </div>

      {/* Session List with scrollbar */}
      <div
        ref={sessionsListRef}
        style={{
          flex: 1,
          overflow: 'auto',
          padding: '8px'
        }}
      >
        {filteredSessions.map(session => (
          <div
            key={session.session_uuid}
            style={{
              padding: '10px 12px',
              backgroundColor: currentSessionUuid === session.session_uuid ? `${colors.primary}15` : 'transparent',
              borderLeft: currentSessionUuid === session.session_uuid ? `2px solid ${colors.primary}` : '2px solid transparent',
              cursor: renamingSessionId === session.session_uuid ? 'default' : 'pointer',
              transition: 'all 0.2s',
              marginBottom: '4px'
            }}
            onClick={() => renamingSessionId !== session.session_uuid && onSelectSession(session.session_uuid)}
            onMouseEnter={(e) => {
              if (currentSessionUuid !== session.session_uuid && renamingSessionId !== session.session_uuid) {
                e.currentTarget.style.backgroundColor = 'rgba(255,255,255,0.05)';
              }
            }}
            onMouseLeave={(e) => {
              if (currentSessionUuid !== session.session_uuid) {
                e.currentTarget.style.backgroundColor = 'transparent';
              }
            }}
          >
            {/* Title or Rename Input */}
            {renamingSessionId === session.session_uuid ? (
              <div style={{ marginBottom: '6px', display: 'flex', gap: '4px', alignItems: 'center' }}>
                <input
                  type="text"
                  value={renameValue}
                  onChange={(e) => onRenameValueChange(e.target.value)}
                  onKeyDown={(e) => {
                    if (e.key === 'Enter') onSaveRename(session.session_uuid);
                    if (e.key === 'Escape') onCancelRename();
                  }}
                  autoFocus
                  style={{
                    flex: 1,
                    padding: '4px 6px',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    color: colors.text,
                    fontSize: fontSize.small,
                    borderRadius: '2px',
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace'
                  }}
                />
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    onSaveRename(session.session_uuid);
                  }}
                  style={{
                    backgroundColor: 'transparent',
                    color: colors.success,
                    border: 'none',
                    cursor: 'pointer',
                    padding: '0',
                    display: 'flex',
                    transition: 'all 0.2s'
                  }}
                >
                  <Check size={12} />
                </button>
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    onCancelRename();
                  }}
                  style={{
                    backgroundColor: 'transparent',
                    color: colors.alert,
                    border: 'none',
                    cursor: 'pointer',
                    padding: '0',
                    display: 'flex',
                    transition: 'all 0.2s'
                  }}
                >
                  <X size={12} />
                </button>
              </div>
            ) : (
              <div style={{
                color: colors.text,
                fontSize: fontSize.small,
                marginBottom: '6px',
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap',
                fontWeight: 700
              }}>
                {session.title}
              </div>
            )}

            {/* Actions Row */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <span style={{
                color: colors.textMuted,
                fontSize: fontSize.small,
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", "Consolas", monospace'
              }}>
                {session.message_count} MSGS • {formatSessionTime(session.updated_at)}
              </span>
              <div style={{ display: 'flex', gap: '6px' }}>
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    onStartRenaming(session.session_uuid, session.title);
                  }}
                  style={{
                    backgroundColor: 'transparent',
                    color: colors.warning,
                    border: 'none',
                    fontSize: fontSize.small,
                    cursor: 'pointer',
                    padding: '0',
                    display: 'flex',
                    transition: 'all 0.2s'
                  }}
                  title="Rename session"
                >
                  <Edit2 size={10} />
                </button>
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    onDeleteSession(session.session_uuid);
                  }}
                  style={{
                    backgroundColor: 'transparent',
                    color: colors.alert,
                    border: 'none',
                    fontSize: fontSize.small,
                    cursor: 'pointer',
                    padding: '0',
                    display: 'flex',
                    transition: 'all 0.2s'
                  }}
                  title="Delete session"
                >
                  <Trash2 size={10} />
                </button>
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
