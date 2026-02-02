import React from 'react';
import { notesService, Note } from './notesService';
import { toast } from '@/components/ui/terminal-toast';

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
    const metadata = [
      { label: 'Category', value: note.category },
      { label: 'Priority', value: note.priority, color: this.getPriorityColor(note.priority) },
      { label: 'Sentiment', value: note.sentiment },
    ];

    if (note.tickers) {
      metadata.push({ label: 'Tickers', value: note.tickers, color: '#00E5FF' });
    }

    toast.custom({
      type: 'warning',
      header: { label: 'Note Reminder' },
      message: `${note.title}${note.content ? '\n\n' + note.content.substring(0, 100) + (note.content.length > 100 ? '...' : '') : ''}`,
      metadata,
      actions: [
        {
          label: 'View',
          onClick: () => {
            window.dispatchEvent(new CustomEvent('openNote', { detail: { noteId: note.id } }));
          },
        },
      ],
      duration: 10000,
      borderColor: '#FF8800',
    });

    console.log(`[NoteReminderService] Showed reminder for note: ${note.title}`);
  }

  /**
   * Get color for priority level
   */
  private getPriorityColor(priority: string): string {
    switch (priority) {
      case 'HIGH':
        return '#FF3B3B';
      case 'MEDIUM':
        return '#FFD700';
      case 'LOW':
        return '#00D66F';
      default:
        return '#787878';
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
