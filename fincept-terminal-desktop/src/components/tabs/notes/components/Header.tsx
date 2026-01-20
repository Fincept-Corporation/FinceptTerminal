// Notes Tab Header Component

import React from 'react';
import { FileText, Clock, RefreshCw, Bell } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { FINCEPT } from '../constants';
import type { NoteStatistics } from '../types';

interface HeaderProps {
  statistics: NoteStatistics | null;
  currentTime: Date;
  upcomingReminders: Array<{ note: any; hoursUntil: number }>;
  showReminders: boolean;
  onRefresh: () => void;
  onToggleReminders: () => void;
}

export const Header: React.FC<HeaderProps> = ({
  statistics,
  currentTime,
  upcomingReminders,
  showReminders,
  onRefresh,
  onToggleReminders
}) => {
  const upcomingRemindersCount = upcomingReminders.length;
  const { t } = useTranslation('notes');

  return (
    <div style={{
      backgroundColor: FINCEPT.HEADER_BG,
      borderBottom: `2px solid ${FINCEPT.ORANGE}`,
      padding: '6px 12px',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      flexShrink: 0,
      boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <FileText size={18} color={FINCEPT.ORANGE} style={{ filter: `drop-shadow(0 0 4px ${FINCEPT.ORANGE})` }} />
          <span style={{
            color: FINCEPT.ORANGE,
            fontWeight: 700,
            fontSize: '14px',
            letterSpacing: '0.5px',
            textShadow: `0 0 10px ${FINCEPT.ORANGE}40`
          }}>
            {t('title')}
          </span>
        </div>

        <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

        {statistics && (
          <>
            <div style={{ fontSize: '10px', color: FINCEPT.CYAN }}>
              {t('header.total')}: <span style={{ fontWeight: 700 }}>{statistics.total}</span>
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.YELLOW }}>
              {t('header.favorites')}: <span style={{ fontWeight: 700 }}>{statistics.favorites}</span>
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.PURPLE }}>
              {t('header.words')}: <span style={{ fontWeight: 700 }}>{statistics.totalWords.toLocaleString()}</span>
            </div>
          </>
        )}
      </div>

      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '10px', color: FINCEPT.CYAN }}>
          <Clock size={12} />
          {currentTime.toLocaleTimeString('en-US', { hour12: false })}
        </div>

        <button
          onClick={onRefresh}
          style={{
            padding: '4px 8px',
            backgroundColor: 'transparent',
            border: `1px solid ${FINCEPT.BORDER}`,
            color: FINCEPT.GRAY,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            fontSize: '10px',
            transition: 'all 0.2s'
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.borderColor = FINCEPT.ORANGE;
            e.currentTarget.style.color = FINCEPT.ORANGE;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.borderColor = FINCEPT.BORDER;
            e.currentTarget.style.color = FINCEPT.GRAY;
          }}
        >
          <RefreshCw size={12} />
          {t('header.refresh')}
        </button>

        <button
          onClick={onToggleReminders}
          style={{
            padding: '4px 8px',
            backgroundColor: showReminders ? FINCEPT.YELLOW : 'transparent',
            border: `1px solid ${upcomingRemindersCount > 0 ? FINCEPT.YELLOW : FINCEPT.BORDER}`,
            color: showReminders ? FINCEPT.DARK_BG : upcomingRemindersCount > 0 ? FINCEPT.YELLOW : FINCEPT.GRAY,
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            fontSize: '10px',
            fontWeight: 700,
            transition: 'all 0.2s'
          }}
          title="View upcoming reminders"
        >
          <Bell size={12} />
          {upcomingRemindersCount > 0 && `(${upcomingRemindersCount})`}
        </button>
      </div>
    </div>
  );
};
