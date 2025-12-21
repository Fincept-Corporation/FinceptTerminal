import Database from '@tauri-apps/plugin-sql';

export interface Note {
  id?: number;
  title: string;
  content: string;
  category: string; // RESEARCH, TRADE_IDEA, MARKET_ANALYSIS, EARNINGS, ECONOMIC, PORTFOLIO, GENERAL
  priority: string; // HIGH, MEDIUM, LOW
  tags: string; // Comma-separated tags
  tickers: string; // Comma-separated ticker symbols
  sentiment: string; // BULLISH, BEARISH, NEUTRAL
  is_favorite: number; // 0 or 1
  is_archived: number; // 0 or 1
  color_code: string; // Hex color for visual organization
  attachments: string; // JSON array of attachment paths
  created_at: string;
  updated_at: string;
  reminder_date: string | null; // ISO date string for reminders
  word_count: number;
}

export interface NoteTemplate {
  id?: number;
  name: string;
  description: string;
  content: string;
  category: string;
  created_at: string;
}

class NotesService {
  private db: Database | null = null;
  private isInitialized = false;

  async initialize() {
    if (this.isInitialized) return;

    try {
      this.db = await Database.load('sqlite:financial_notes.db');

      // Create notes table with financial-specific fields
      await this.db.execute(`
        CREATE TABLE IF NOT EXISTS financial_notes (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          title TEXT NOT NULL,
          content TEXT NOT NULL,
          category TEXT NOT NULL,
          priority TEXT DEFAULT 'MEDIUM',
          tags TEXT,
          tickers TEXT,
          sentiment TEXT DEFAULT 'NEUTRAL',
          is_favorite INTEGER DEFAULT 0,
          is_archived INTEGER DEFAULT 0,
          color_code TEXT DEFAULT '#FF8800',
          attachments TEXT,
          created_at TEXT NOT NULL,
          updated_at TEXT NOT NULL,
          reminder_date TEXT,
          word_count INTEGER DEFAULT 0
        )
      `);

      // Create note templates table
      await this.db.execute(`
        CREATE TABLE IF NOT EXISTS note_templates (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          name TEXT NOT NULL,
          description TEXT,
          content TEXT NOT NULL,
          category TEXT NOT NULL,
          created_at TEXT NOT NULL
        )
      `);

      // Create indexes for performance
      await this.db.execute(`
        CREATE INDEX IF NOT EXISTS idx_notes_category ON financial_notes(category)
      `);
      await this.db.execute(`
        CREATE INDEX IF NOT EXISTS idx_notes_updated ON financial_notes(updated_at DESC)
      `);
      await this.db.execute(`
        CREATE INDEX IF NOT EXISTS idx_notes_favorite ON financial_notes(is_favorite)
      `);
      await this.db.execute(`
        CREATE INDEX IF NOT EXISTS idx_notes_archived ON financial_notes(is_archived)
      `);
      await this.db.execute(`
        CREATE INDEX IF NOT EXISTS idx_notes_priority ON financial_notes(priority)
      `);

      // Insert default templates if they don't exist
      const existingTemplates = await this.db.select<NoteTemplate[]>(
        'SELECT COUNT(*) as count FROM note_templates'
      );

      if (!existingTemplates || (existingTemplates as any)[0]?.count === 0) {
        await this.insertDefaultTemplates();
      }

      this.isInitialized = true;
      console.log('[NotesService] Initialized successfully');
    } catch (error) {
      console.error('[NotesService] Failed to initialize:', error);
      throw error;
    }
  }

  private async insertDefaultTemplates() {
    const templates: Omit<NoteTemplate, 'id'>[] = [
      {
        name: 'Trade Idea',
        description: 'Document a new trading opportunity',
        category: 'TRADE_IDEA',
        content: `# Trade Idea

## Overview
**Ticker:** [SYMBOL]
**Entry Price:** $[PRICE]
**Target Price:** $[TARGET]
**Stop Loss:** $[STOP]
**Position Size:** [SIZE]

## Thesis
[Write your investment thesis here]

## Catalysts
- [Key catalyst 1]
- [Key catalyst 2]

## Risk Factors
- [Risk 1]
- [Risk 2]

## Timeline
**Expected Duration:** [TIMEFRAME]
**Key Dates:** [EVENTS]`,
        created_at: new Date().toISOString()
      },
      {
        name: 'Market Analysis',
        description: 'Analyze overall market conditions',
        category: 'MARKET_ANALYSIS',
        content: `# Market Analysis - [DATE]

## Market Overview
**Sentiment:** [BULLISH/BEARISH/NEUTRAL]
**Key Indices:**
- S&P 500: [LEVEL] ([CHANGE]%)
- NASDAQ: [LEVEL] ([CHANGE]%)
- VIX: [LEVEL]

## Sector Performance
- Best: [SECTOR] (+[%])
- Worst: [SECTOR] (-[%])

## Key Developments
1. [Development 1]
2. [Development 2]

## Technical Indicators
- **RSI:** [VALUE]
- **Moving Averages:** [ANALYSIS]

## Outlook
[Your market outlook for the next period]`,
        created_at: new Date().toISOString()
      },
      {
        name: 'Earnings Notes',
        description: 'Track company earnings and guidance',
        category: 'EARNINGS',
        content: `# Earnings Report - [TICKER]

## Report Date
**Quarter:** [Q1/Q2/Q3/Q4] [YEAR]
**Release Date:** [DATE]

## Key Metrics
- **EPS:** $[ACTUAL] vs $[ESTIMATE] (Expected)
- **Revenue:** $[ACTUAL] vs $[ESTIMATE]
- **Guidance:** [RAISED/LOWERED/MAINTAINED]

## Highlights
- [Highlight 1]
- [Highlight 2]

## Management Commentary
[Key quotes or insights from management]

## Market Reaction
**Stock Movement:** [+/-][%]
**After Hours:** [PRICE]

## Analysis
[Your analysis of the results]`,
        created_at: new Date().toISOString()
      },
      {
        name: 'Economic Data',
        description: 'Track important economic releases',
        category: 'ECONOMIC',
        content: `# Economic Data Release

## Indicator
**Name:** [INDICATOR NAME]
**Date:** [RELEASE DATE]

## Results
**Actual:** [VALUE]
**Expected:** [VALUE]
**Previous:** [VALUE]

## Market Impact
**Asset Class Impact:**
- Equities: [ANALYSIS]
- Bonds: [ANALYSIS]
- Currencies: [ANALYSIS]

## Implications
[Analysis of what this data means for markets]`,
        created_at: new Date().toISOString()
      },
      {
        name: 'Research Note',
        description: 'Deep dive research on a company or theme',
        category: 'RESEARCH',
        content: `# Research: [COMPANY/THEME]

## Executive Summary
[Brief overview]

## Investment Thesis
[Detailed thesis]

## Business Model
[How the company makes money]

## Competitive Advantages
1. [Moat 1]
2. [Moat 2]

## Financial Analysis
**Margins:** [DATA]
**Growth:** [DATA]
**Valuation:** [P/E], [P/S], [EV/EBITDA]

## Risks
- [Risk 1]
- [Risk 2]

## Conclusion
[Buy/Hold/Sell recommendation and price target]`,
        created_at: new Date().toISOString()
      },
      {
        name: 'Portfolio Review',
        description: 'Review and analyze your portfolio',
        category: 'PORTFOLIO',
        content: `# Portfolio Review - [DATE]

## Performance Summary
**Total Return:** [+/-][%]
**vs S&P 500:** [+/-][%]

## Top Performers
1. [TICKER]: [+%] - [REASON]
2. [TICKER]: [+%] - [REASON]

## Underperformers
1. [TICKER]: [-%%] - [REASON]
2. [TICKER]: [-%%] - [REASON]

## Allocation Analysis
- **Equities:** [%]
- **Fixed Income:** [%]
- **Cash:** [%]

## Adjustments Needed
- [ ] [Action item 1]
- [ ] [Action item 2]

## Next Review Date
[DATE]`,
        created_at: new Date().toISOString()
      }
    ];

    for (const template of templates) {
      await this.db!.execute(
        `INSERT INTO note_templates (name, description, content, category, created_at)
         VALUES (?, ?, ?, ?, ?)`,
        [template.name, template.description, template.content, template.category, template.created_at]
      );
    }
  }

  async createNote(note: Omit<Note, 'id'>): Promise<number> {
    await this.initialize();

    try {
      const result = await this.db!.execute(
        `INSERT INTO financial_notes
         (title, content, category, priority, tags, tickers, sentiment, is_favorite,
          is_archived, color_code, attachments, created_at, updated_at, reminder_date, word_count)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
        [
          note.title,
          note.content,
          note.category,
          note.priority,
          note.tags || '',
          note.tickers || '',
          note.sentiment,
          note.is_favorite || 0,
          note.is_archived || 0,
          note.color_code || '#FF8800',
          note.attachments || '[]',
          note.created_at,
          note.updated_at,
          note.reminder_date || null,
          note.word_count || 0
        ]
      );

      return result.lastInsertId || 0;
    } catch (error) {
      console.error('[NotesService] Failed to create note:', error);
      throw error;
    }
  }

  async updateNote(id: number, updates: Partial<Note>): Promise<void> {
    await this.initialize();

    try {
      const setParts: string[] = [];
      const values: any[] = [];

      if (updates.title !== undefined) {
        setParts.push('title = ?');
        values.push(updates.title);
      }
      if (updates.content !== undefined) {
        setParts.push('content = ?');
        values.push(updates.content);
      }
      if (updates.category !== undefined) {
        setParts.push('category = ?');
        values.push(updates.category);
      }
      if (updates.priority !== undefined) {
        setParts.push('priority = ?');
        values.push(updates.priority);
      }
      if (updates.tags !== undefined) {
        setParts.push('tags = ?');
        values.push(updates.tags);
      }
      if (updates.tickers !== undefined) {
        setParts.push('tickers = ?');
        values.push(updates.tickers);
      }
      if (updates.sentiment !== undefined) {
        setParts.push('sentiment = ?');
        values.push(updates.sentiment);
      }
      if (updates.is_favorite !== undefined) {
        setParts.push('is_favorite = ?');
        values.push(updates.is_favorite);
      }
      if (updates.is_archived !== undefined) {
        setParts.push('is_archived = ?');
        values.push(updates.is_archived);
      }
      if (updates.color_code !== undefined) {
        setParts.push('color_code = ?');
        values.push(updates.color_code);
      }
      if (updates.attachments !== undefined) {
        setParts.push('attachments = ?');
        values.push(updates.attachments);
      }
      if (updates.reminder_date !== undefined) {
        setParts.push('reminder_date = ?');
        values.push(updates.reminder_date);
      }
      if (updates.word_count !== undefined) {
        setParts.push('word_count = ?');
        values.push(updates.word_count);
      }

      setParts.push('updated_at = ?');
      values.push(new Date().toISOString());

      values.push(id);

      await this.db!.execute(
        `UPDATE financial_notes SET ${setParts.join(', ')} WHERE id = ?`,
        values
      );
    } catch (error) {
      console.error('[NotesService] Failed to update note:', error);
      throw error;
    }
  }

  async deleteNote(id: number): Promise<void> {
    await this.initialize();

    try {
      await this.db!.execute('DELETE FROM financial_notes WHERE id = ?', [id]);
    } catch (error) {
      console.error('[NotesService] Failed to delete note:', error);
      throw error;
    }
  }

  async getNoteById(id: number): Promise<Note | null> {
    await this.initialize();

    try {
      const result = await this.db!.select<Note[]>(
        'SELECT * FROM financial_notes WHERE id = ?',
        [id]
      );
      return result.length > 0 ? result[0] : null;
    } catch (error) {
      console.error('[NotesService] Failed to get note by ID:', error);
      return null;
    }
  }

  async getAllNotes(includeArchived = false): Promise<Note[]> {
    await this.initialize();

    try {
      const query = includeArchived
        ? 'SELECT * FROM financial_notes ORDER BY updated_at DESC'
        : 'SELECT * FROM financial_notes WHERE is_archived = 0 ORDER BY updated_at DESC';

      const result = await this.db!.select<Note[]>(query);
      return result;
    } catch (error) {
      console.error('[NotesService] Failed to get all notes:', error);
      return [];
    }
  }

  async getNotesByCategory(category: string, includeArchived = false): Promise<Note[]> {
    await this.initialize();

    try {
      const query = includeArchived
        ? 'SELECT * FROM financial_notes WHERE category = ? ORDER BY updated_at DESC'
        : 'SELECT * FROM financial_notes WHERE category = ? AND is_archived = 0 ORDER BY updated_at DESC';

      const result = await this.db!.select<Note[]>(query, [category]);
      return result;
    } catch (error) {
      console.error('[NotesService] Failed to get notes by category:', error);
      return [];
    }
  }

  async getFavoriteNotes(): Promise<Note[]> {
    await this.initialize();

    try {
      const result = await this.db!.select<Note[]>(
        'SELECT * FROM financial_notes WHERE is_favorite = 1 AND is_archived = 0 ORDER BY updated_at DESC'
      );
      return result;
    } catch (error) {
      console.error('[NotesService] Failed to get favorite notes:', error);
      return [];
    }
  }

  async searchNotes(query: string): Promise<Note[]> {
    await this.initialize();

    try {
      const result = await this.db!.select<Note[]>(
        `SELECT * FROM financial_notes
         WHERE (title LIKE ? OR content LIKE ? OR tags LIKE ? OR tickers LIKE ?)
         AND is_archived = 0
         ORDER BY updated_at DESC`,
        [`%${query}%`, `%${query}%`, `%${query}%`, `%${query}%`]
      );
      return result;
    } catch (error) {
      console.error('[NotesService] Failed to search notes:', error);
      return [];
    }
  }

  async getNotesWithReminders(): Promise<Note[]> {
    await this.initialize();

    try {
      const result = await this.db!.select<Note[]>(
        `SELECT * FROM financial_notes
         WHERE reminder_date IS NOT NULL AND is_archived = 0
         ORDER BY reminder_date ASC`
      );
      return result;
    } catch (error) {
      console.error('[NotesService] Failed to get notes with reminders:', error);
      return [];
    }
  }

  async getTemplates(): Promise<NoteTemplate[]> {
    await this.initialize();

    try {
      const result = await this.db!.select<NoteTemplate[]>(
        'SELECT * FROM note_templates ORDER BY name ASC'
      );
      return result;
    } catch (error) {
      console.error('[NotesService] Failed to get templates:', error);
      return [];
    }
  }

  async createTemplate(template: Omit<NoteTemplate, 'id'>): Promise<number> {
    await this.initialize();

    try {
      const result = await this.db!.execute(
        `INSERT INTO note_templates (name, description, content, category, created_at)
         VALUES (?, ?, ?, ?, ?)`,
        [template.name, template.description, template.content, template.category, template.created_at]
      );
      return result.lastInsertId || 0;
    } catch (error) {
      console.error('[NotesService] Failed to create template:', error);
      throw error;
    }
  }

  async deleteTemplate(id: number): Promise<void> {
    await this.initialize();

    try {
      await this.db!.execute('DELETE FROM note_templates WHERE id = ?', [id]);
    } catch (error) {
      console.error('[NotesService] Failed to delete template:', error);
      throw error;
    }
  }

  async getStatistics() {
    await this.initialize();

    try {
      const totalNotes = await this.db!.select<any[]>(
        'SELECT COUNT(*) as count FROM financial_notes WHERE is_archived = 0'
      );
      const archivedNotes = await this.db!.select<any[]>(
        'SELECT COUNT(*) as count FROM financial_notes WHERE is_archived = 1'
      );
      const favoriteNotes = await this.db!.select<any[]>(
        'SELECT COUNT(*) as count FROM financial_notes WHERE is_favorite = 1'
      );
      const categoryStats = await this.db!.select<any[]>(
        'SELECT category, COUNT(*) as count FROM financial_notes WHERE is_archived = 0 GROUP BY category'
      );
      const totalWords = await this.db!.select<any[]>(
        'SELECT SUM(word_count) as total FROM financial_notes WHERE is_archived = 0'
      );

      return {
        total: totalNotes[0]?.count || 0,
        archived: archivedNotes[0]?.count || 0,
        favorites: favoriteNotes[0]?.count || 0,
        byCategory: categoryStats,
        totalWords: totalWords[0]?.total || 0
      };
    } catch (error) {
      console.error('[NotesService] Failed to get statistics:', error);
      return {
        total: 0,
        archived: 0,
        favorites: 0,
        byCategory: [],
        totalWords: 0
      };
    }
  }
}

export const notesService = new NotesService();
