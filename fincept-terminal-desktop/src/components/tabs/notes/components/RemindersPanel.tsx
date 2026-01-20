// Notes Tab Reminders Panel Component

import React from 'react';
import { Bell, RefreshCw } from 'lucide-react';
import { toast } from 'sonner';
import { FINCEPT, getPriorityColor, getSentimentColor } from '../constants';
import type { UpcomingReminder, Note } from '../types';

interface RemindersPanelProps {
  upcomingReminders: UpcomingReminder[];
  onRefresh: () => Promise<void>;
  onSelectNote: (note: Note) => void;
  onClose: () => void;
}

export const RemindersPanel: React.FC<RemindersPanelProps> = ({
  upcomingReminders,
  onRefresh,
  onSelectNote,
  onClose
}) => {
  const handleRefresh = async () => {
    await onRefresh();
    toast.success('Reminders refreshed');
  };

  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      padding: '12px',
      flexShrink: 0
    }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
        <div style={{ color: FINCEPT.YELLOW, fontSize: '11px', fontWeight: 700, display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Bell size={14} />
          UPCOMING REMINDERS (24H)
        </div>
        <button
          onClick={handleRefresh}
          style={{
            padding: '4px 8px',
            backgroundColor: FINCEPT.HEADER_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY,
            cursor: 'pointer',
            fontSize: '9px',
            fontWeight: 700,
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
        >
          <RefreshCw size={10} />
          REFRESH
        </button>
      </div>

      {upcomingReminders.length === 0 ? (
        <div style={{
          padding: '20px',
          textAlign: 'center',
          color: FINCEPT.GRAY,
          fontSize: '10px',
          backgroundColor: FINCEPT.HEADER_BG,
          border: `1px solid ${FINCEPT.BORDER}`
        }}>
          No upcoming reminders in the next 24 hours
        </div>
      ) : (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
          {upcomingReminders.map(({ note, hoursUntil }) => {
            const isUrgent = hoursUntil < 1;
            const minutesUntil = Math.round(hoursUntil * 60);
            const timeText = hoursUntil < 1
              ? `${minutesUntil} minute${minutesUntil !== 1 ? 's' : ''}`
              : `${Math.round(hoursUntil)} hour${Math.round(hoursUntil) !== 1 ? 's' : ''}`;

            return (
              <div
                key={note.id}
                onClick={() => {
                  onSelectNote(note);
                  onClose();
                }}
                style={{
                  padding: '10px',
                  backgroundColor: isUrgent ? `${FINCEPT.RED}10` : FINCEPT.HEADER_BG,
                  border: `1px solid ${isUrgent ? FINCEPT.RED : FINCEPT.BORDER}`,
                  borderLeft: `4px solid ${note.color_code}`,
                  cursor: 'pointer',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = isUrgent ? `${FINCEPT.RED}10` : FINCEPT.HEADER_BG;
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
                  <div style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700 }}>
                    {note.title}
                  </div>
                  <div style={{
                    padding: '2px 6px',
                    backgroundColor: isUrgent ? FINCEPT.RED : FINCEPT.YELLOW,
                    color: FINCEPT.DARK_BG,
                    fontSize: '8px',
                    fontWeight: 700
                  }}>
                    {isUrgent ? '🔴 URGENT' : ` ${timeText.toUpperCase()}`}
                  </div>
                </div>
                <div style={{ display: 'flex', gap: '8px', fontSize: '9px', color: FINCEPT.GRAY }}>
                  <span style={{ color: getPriorityColor(note.priority) }}>{note.priority}</span>
                  <span>•</span>
                  <span style={{ color: getSentimentColor(note.sentiment) }}>{note.sentiment}</span>
                  <span>•</span>
                  <span>{note.category}</span>
                  {note.tickers && (
                    <>
                      <span>•</span>
                      <span style={{ color: FINCEPT.CYAN }}>{note.tickers}</span>
                    </>
                  )}
                </div>
                <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '4px' }}>
                  Due: {new Date(note.reminder_date!).toLocaleString()}
                </div>
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
};
