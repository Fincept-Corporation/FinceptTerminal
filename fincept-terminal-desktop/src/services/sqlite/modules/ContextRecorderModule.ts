/**
 * Context Recorder Module
 * Handles recorded contexts and recording sessions for AI chat integration
 */

import type { RecordedContext, RecordingSession } from '../types';
import type { SQLiteBase } from '../SQLiteBase';

export interface ContextFilters {
    tabName?: string;
    dataType?: string;
    tags?: string[];
    limit?: number;
}

export interface ContextStorageStats {
    count: number;
    totalSize: number;
}

export class ContextRecorderModule {
    constructor(private base: SQLiteBase) { }

    // =========================
    // Recorded Contexts
    // =========================

    async saveRecordedContext(context: Omit<RecordedContext, 'created_at'>): Promise<void> {
        await this.base.ensureInitialized();

        const dataSize = new TextEncoder().encode(context.raw_data).length;

        await this.base.execute(
            `INSERT INTO recorded_contexts (id, tab_name, data_type, label, raw_data, metadata, data_size, tags)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8)`,
            [
                context.id,
                context.tab_name,
                context.data_type,
                context.label || null,
                context.raw_data,
                context.metadata || null,
                dataSize,
                context.tags || null
            ]
        );
    }

    async getRecordedContexts(filters?: ContextFilters): Promise<RecordedContext[]> {
        await this.base.ensureInitialized();

        let sql = 'SELECT * FROM recorded_contexts WHERE 1=1';
        const params: any[] = [];
        let paramIndex = 1;

        if (filters?.tabName) {
            sql += ` AND tab_name = $${paramIndex++}`;
            params.push(filters.tabName);
        }

        if (filters?.dataType) {
            sql += ` AND data_type = $${paramIndex++}`;
            params.push(filters.dataType);
        }

        if (filters?.tags && filters.tags.length > 0) {
            const tagConditions = filters.tags.map(() => `tags LIKE $${paramIndex++}`).join(' OR ');
            sql += ` AND (${tagConditions})`;
            params.push(...filters.tags.map(tag => `%${tag}%`));
        }

        sql += ' ORDER BY created_at DESC';

        if (filters?.limit) {
            sql += ` LIMIT $${paramIndex}`;
            params.push(filters.limit);
        }

        return await this.base.select<RecordedContext[]>(sql, params);
    }

    async getRecordedContext(id: string): Promise<RecordedContext | null> {
        await this.base.ensureInitialized();

        const results = await this.base.select<RecordedContext[]>(
            'SELECT * FROM recorded_contexts WHERE id = $1',
            [id]
        );

        return results[0] || null;
    }

    async deleteRecordedContext(id: string): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute('DELETE FROM recorded_contexts WHERE id = $1', [id]);
    }

    async clearOldContexts(maxAgeDays: number): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute(
            `DELETE FROM recorded_contexts
       WHERE datetime(created_at) < datetime('now', '-${maxAgeDays} days')`
        );
    }

    async getContextStorageStats(): Promise<ContextStorageStats> {
        await this.base.ensureInitialized();

        const result = await this.base.select<{ count: number; total_size: number }[]>(
            'SELECT COUNT(*) as count, SUM(data_size) as total_size FROM recorded_contexts'
        );

        return {
            count: result[0]?.count || 0,
            totalSize: result[0]?.total_size || 0
        };
    }

    // =========================
    // Recording Sessions
    // =========================

    async startRecordingSession(session: Omit<RecordingSession, 'started_at'>): Promise<void> {
        await this.base.ensureInitialized();

        // End any existing active session for this tab
        await this.base.execute(
            `UPDATE recording_sessions SET is_active = 0, ended_at = CURRENT_TIMESTAMP
       WHERE tab_name = $1 AND is_active = 1`,
            [session.tab_name]
        );

        // Start new session
        await this.base.execute(
            `INSERT INTO recording_sessions (id, tab_name, is_active, auto_record, filters)
       VALUES ($1, $2, $3, $4, $5)`,
            [
                session.id,
                session.tab_name,
                session.is_active ? 1 : 0,
                session.auto_record ? 1 : 0,
                session.filters || null
            ]
        );
    }

    async getActiveRecordingSession(tabName: string): Promise<RecordingSession | null> {
        await this.base.ensureInitialized();

        const results = await this.base.select<RecordingSession[]>(
            'SELECT * FROM recording_sessions WHERE tab_name = $1 AND is_active = 1 LIMIT 1',
            [tabName]
        );

        return results[0] || null;
    }

    async stopRecordingSession(tabName: string): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `UPDATE recording_sessions SET is_active = 0, ended_at = CURRENT_TIMESTAMP
       WHERE tab_name = $1 AND is_active = 1`,
            [tabName]
        );
    }

    // =========================
    // Chat Context Links
    // =========================

    async linkContextToChat(chatSessionUuid: string, contextId: string): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `INSERT OR REPLACE INTO chat_context_links (chat_session_uuid, context_id)
       VALUES ($1, $2)`,
            [chatSessionUuid, contextId]
        );
    }

    async unlinkContextFromChat(chatSessionUuid: string, contextId: string): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            'DELETE FROM chat_context_links WHERE chat_session_uuid = $1 AND context_id = $2',
            [chatSessionUuid, contextId]
        );
    }

    async getChatContexts(chatSessionUuid: string): Promise<RecordedContext[]> {
        await this.base.ensureInitialized();

        return await this.base.select<RecordedContext[]>(
            `SELECT rc.* FROM recorded_contexts rc
       INNER JOIN chat_context_links ccl ON rc.id = ccl.context_id
       WHERE ccl.chat_session_uuid = $1 AND ccl.is_active = 1
       ORDER BY ccl.added_at DESC`,
            [chatSessionUuid]
        );
    }

    async toggleChatContextActive(chatSessionUuid: string, contextId: string): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `UPDATE chat_context_links SET is_active = NOT is_active
       WHERE chat_session_uuid = $1 AND context_id = $2`,
            [chatSessionUuid, contextId]
        );
    }
}
