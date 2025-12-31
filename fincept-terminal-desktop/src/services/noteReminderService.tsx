import React from 'react';
import { notesService, Note } from './notesService';
import { toast } from 'sonner';

class NoteReminderService {
  private checkInterval: NodeJS.Timeout | null = null;
  private notifiedReminders: Set<number> = new Set();
  private readonly CHECK_INTERVAL_MS = 60000; // Check every minute

  /**
   * Start monitoring for note reminders
   */
  start() {
    if (this.checkInterval) {
      console.log('[NoteReminderService] Already running');
      return;
    }

    console.log('[NoteReminderService] Starting reminder monitoring');
    this.checkReminders(); // Check immediately
    this.checkInterval = setInterval(() => this.checkReminders(), this.CHECK_INTERVAL_MS);
  }

  /**
   * Stop monitoring for note reminders
   */
  stop() {
    if (this.checkInterval) {
      clearInterval(this.checkInterval);
      this.checkInterval = null;
      console.log('[NoteReminderService] Stopped reminder monitoring');
    }
  }

  /**
   * Check for due reminders and show notifications
   */
  private async checkReminders() {
    try {
      const notesWithReminders = await notesService.getNotesWithReminders();
      const now = new Date();

      for (const note of notesWithReminders) {
        if (!note.reminder_date || !note.id) continue;

        const reminderDate = new Date(note.reminder_date);
        const timeDiff = reminderDate.getTime() - now.getTime();

        // Show reminder if it's due (within the next minute or overdue)
        if (timeDiff <= 60000 && timeDiff > -60000) {
          if (!this.notifiedReminders.has(note.id)) {
            this.showReminderNotification(note);
            this.notifiedReminders.add(note.id);
          }
        }
        // Clean up old notifications from the set
        else if (timeDiff < -60000) {
          this.notifiedReminders.delete(note.id);
        }
      }
    } catch (error) {
      console.error('[NoteReminderService] Failed to check reminders:', error);
    }
  }

  /**
   * Show a toast notification for a note reminder
   */
  private showReminderNotification(note: Note) {
    const priorityEmoji = this.getPriorityEmoji(note.priority);
    const sentimentEmoji = this.getSentimentEmoji(note.sentiment);

    toast(
      <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
        <div style={{ fontWeight: 700, fontSize: '13px', color: '#FF8800' }}>
          {priorityEmoji} NOTE REMINDER
        </div>
        <div style={{ fontSize: '12px', fontWeight: 600 }}>
          {note.title}
        </div>
        <div style={{ fontSize: '10px', color: '#999', display: 'flex', gap: '8px' }}>
          <span>{sentimentEmoji} {note.sentiment}</span>
          <span>•</span>
          <span>{note.category}</span>
          {note.tickers && (
            <>
              <span>•</span>
              <span style={{ color: '#00E5FF' }}>{note.tickers}</span>
            </>
          )}
        </div>
        {note.content && (
          <div style={{ fontSize: '11px', color: '#aaa', marginTop: '4px', lineHeight: '1.4' }}>
            {note.content.substring(0, 100)}{note.content.length > 100 ? '...' : ''}
          </div>
        )}
      </div>,
      {
        duration: 10000, // Show for 10 seconds
        position: 'top-right',
        className: 'note-reminder-toast',
        style: {
          backgroundColor: '#1A1A1A',
          border: '2px solid #FF8800',
          borderRadius: '4px',
          padding: '12px',
          boxShadow: '0 4px 16px rgba(255, 136, 0, 0.3)'
        },
        action: {
          label: 'View',
          onClick: () => {
            // Emit custom event to open the note
            window.dispatchEvent(new CustomEvent('openNote', { detail: { noteId: note.id } }));
          }
        }
      }
    );

    console.log(`[NoteReminderService] Showed reminder for note: ${note.title}`);
  }

  /**
   * Get emoji for priority level
   */
  private getPriorityEmoji(priority: string): string {
    switch (priority) {
      case 'HIGH':
        return '🔴';
      case 'MEDIUM':
        return '🟡';
      case 'LOW':
        return '🟢';
      default:
        return '';
    }
  }

  /**
   * Get emoji for sentiment
   */
  private getSentimentEmoji(sentiment: string): string {
    switch (sentiment) {
      case 'BULLISH':
        return '📈';
      case 'BEARISH':
        return '📉';
      case 'NEUTRAL':
        return '➡️';
      default:
        return '📊';
    }
  }

  /**
   * Manually trigger a reminder check (useful for testing or immediate updates)
   */
  async triggerCheck() {
    await this.checkReminders();
  }

  /**
   * Clear the notification history for a specific note
   */
  clearNotificationHistory(noteId: number) {
    this.notifiedReminders.delete(noteId);
  }

  /**
   * Get upcoming reminders (within next 24 hours)
   */
  async getUpcomingReminders(): Promise<Array<{ note: Note; hoursUntil: number }>> {
    try {
      const notesWithReminders = await notesService.getNotesWithReminders();
      const now = new Date();
      const upcoming: Array<{ note: Note; hoursUntil: number }> = [];

      for (const note of notesWithReminders) {
        if (!note.reminder_date) continue;

        const reminderDate = new Date(note.reminder_date);
        const timeDiff = reminderDate.getTime() - now.getTime();
        const hoursUntil = timeDiff / (1000 * 60 * 60);

        if (hoursUntil >= 0 && hoursUntil <= 24) {
          upcoming.push({ note, hoursUntil });
        }
      }

      // Sort by closest first
      upcoming.sort((a, b) => a.hoursUntil - b.hoursUntil);

      return upcoming;
    } catch (error) {
      console.error('[NoteReminderService] Failed to get upcoming reminders:', error);
      return [];
    }
  }
}

// Singleton instance
export const noteReminderService = new NoteReminderService();
