/**
 * Chat Module
 * Handles chat sessions and messages
 */

import type { ChatSession, ChatMessage } from '../types';
import type { SQLiteBase } from '../SQLiteBase';

export class ChatModule {
    constructor(private base: SQLiteBase) { }

    async createChatSession(title: string): Promise<ChatSession> {
        await this.base.ensureInitialized();

        const sessionUuid = crypto.randomUUID();
        const timestamp = new Date().toISOString();

        await this.base.execute(
            `INSERT INTO chat_sessions (session_uuid, title, message_count, created_at, updated_at)
       VALUES ($1, $2, 0, $3, $4)`,
            [sessionUuid, title, timestamp, timestamp]
        );

        return {
            session_uuid: sessionUuid,
            title: title,
            message_count: 0,
            created_at: timestamp,
            updated_at: timestamp
        };
    }

    async getChatSessions(limit: number = 50): Promise<ChatSession[]> {
        await this.base.ensureInitialized();
        return await this.base.select<ChatSession[]>(
            `SELECT * FROM chat_sessions ORDER BY updated_at DESC LIMIT $1`,
            [limit]
        );
    }

    async getChatSession(sessionUuid: string): Promise<ChatSession | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<ChatSession[]>(
            'SELECT * FROM chat_sessions WHERE session_uuid = $1',
            [sessionUuid]
        );
        return results[0] || null;
    }

    async updateChatSessionTitle(sessionUuid: string, title: string): Promise<void> {
        await this.base.ensureInitialized();

        await this.base.execute(
            `UPDATE chat_sessions SET title = $1, updated_at = CURRENT_TIMESTAMP WHERE session_uuid = $2`,
            [title, sessionUuid]
        );
    }

    async deleteChatSession(sessionUuid: string): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute('DELETE FROM chat_sessions WHERE session_uuid = $1', [sessionUuid]);
    }

    async addChatMessage(message: Omit<ChatMessage, 'id' | 'timestamp'>): Promise<ChatMessage> {
        await this.base.ensureInitialized();

        const messageId = crypto.randomUUID();
        const timestamp = new Date().toISOString();

        await this.base.execute(
            `INSERT INTO chat_messages (id, session_uuid, role, content, timestamp, provider, model, tokens_used)
       VALUES ($1, $2, $3, $4, $5, $6, $7, $8)`,
            [
                messageId,
                message.session_uuid,
                message.role,
                message.content,
                timestamp,
                message.provider || null,
                message.model || null,
                message.tokens_used || null,
            ]
        );

        // Update message count
        await this.base.execute(
            `UPDATE chat_sessions
       SET message_count = message_count + 1, updated_at = CURRENT_TIMESTAMP
       WHERE session_uuid = $1`,
            [message.session_uuid]
        );

        return {
            id: messageId,
            session_uuid: message.session_uuid,
            role: message.role,
            content: message.content,
            timestamp: timestamp,
            provider: message.provider,
            model: message.model,
            tokens_used: message.tokens_used,
        };
    }

    async getChatMessages(sessionUuid: string): Promise<ChatMessage[]> {
        await this.base.ensureInitialized();
        return await this.base.select<ChatMessage[]>(
            'SELECT * FROM chat_messages WHERE session_uuid = $1 ORDER BY timestamp ASC',
            [sessionUuid]
        );
    }

    async deleteChatMessage(id: string): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute('DELETE FROM chat_messages WHERE id = $1', [id]);
    }

    async clearChatSessionMessages(sessionUuid: string): Promise<void> {
        await this.base.ensureInitialized();
        await this.base.execute('DELETE FROM chat_messages WHERE session_uuid = $1', [sessionUuid]);
        await this.base.execute(
            'UPDATE chat_sessions SET message_count = 0, updated_at = CURRENT_TIMESTAMP WHERE session_uuid = $1',
            [sessionUuid]
        );
    }
}
