// Chat Database Service using IndexedDB (browser-compatible)
// Manages persistent storage of chat sessions and messages

export interface ChatSession {
  session_uuid: string;
  title: string;
  message_count: number;
  created_at: string;
  updated_at: string;
}

export interface ChatMessage {
  id: string;
  session_uuid: string;
  role: 'user' | 'assistant';
  content: string;
  timestamp: string;
  provider?: string;
  model?: string;
  tokens_used?: number;
}

class ChatDatabaseService {
  private db: IDBDatabase | null = null;
  private dbName = 'fincept-chat-db';
  private dbVersion = 1;
  private isInitialized: boolean = false;

  async initialize(): Promise<void> {
    if (this.isInitialized && this.db) return;

    return new Promise((resolve, reject) => {
      const request = indexedDB.open(this.dbName, this.dbVersion);

      request.onerror = () => {
        console.error('Failed to open database:', request.error);
        reject(request.error);
      };

      request.onsuccess = () => {
        this.db = request.result;
        this.isInitialized = true;
        console.log('Chat database initialized successfully');
        resolve();
      };

      request.onupgradeneeded = (event) => {
        const db = (event.target as IDBOpenDBRequest).result;

        // Create chat_sessions object store
        if (!db.objectStoreNames.contains('chat_sessions')) {
          const sessionsStore = db.createObjectStore('chat_sessions', { keyPath: 'session_uuid' });
          sessionsStore.createIndex('updated_at', 'updated_at', { unique: false });
          sessionsStore.createIndex('title', 'title', { unique: false });
        }

        // Create chat_messages object store
        if (!db.objectStoreNames.contains('chat_messages')) {
          const messagesStore = db.createObjectStore('chat_messages', { keyPath: 'id' });
          messagesStore.createIndex('session_uuid', 'session_uuid', { unique: false });
          messagesStore.createIndex('timestamp', 'timestamp', { unique: false });
        }
      };
    });
  }

  private ensureInitialized(): void {
    if (!this.db || !this.isInitialized) {
      throw new Error('Database not initialized. Call initialize() first.');
    }
  }

  // Session Management
  async createSession(title: string): Promise<ChatSession> {
    this.ensureInitialized();
    const session_uuid = crypto.randomUUID();
    const now = new Date().toISOString();

    const session: ChatSession = {
      session_uuid,
      title,
      message_count: 0,
      created_at: now,
      updated_at: now
    };

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction(['chat_sessions'], 'readwrite');
      const store = transaction.objectStore('chat_sessions');
      const request = store.add(session);

      request.onsuccess = () => resolve(session);
      request.onerror = () => reject(request.error);
    });
  }

  async getSessions(limit: number = 100): Promise<ChatSession[]> {
    this.ensureInitialized();

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction(['chat_sessions'], 'readonly');
      const store = transaction.objectStore('chat_sessions');
      const index = store.index('updated_at');
      const request = index.openCursor(null, 'prev'); // DESC order
      const sessions: ChatSession[] = [];

      request.onsuccess = (event) => {
        const cursor = (event.target as IDBRequest).result;
        if (cursor && sessions.length < limit) {
          sessions.push(cursor.value);
          cursor.continue();
        } else {
          resolve(sessions);
        }
      };

      request.onerror = () => reject(request.error);
    });
  }

  async getSession(session_uuid: string): Promise<ChatSession | null> {
    this.ensureInitialized();

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction(['chat_sessions'], 'readonly');
      const store = transaction.objectStore('chat_sessions');
      const request = store.get(session_uuid);

      request.onsuccess = () => resolve(request.result || null);
      request.onerror = () => reject(request.error);
    });
  }

  async updateSessionTitle(session_uuid: string, title: string): Promise<void> {
    this.ensureInitialized();
    const session = await this.getSession(session_uuid);
    if (!session) throw new Error('Session not found');

    session.title = title;
    session.updated_at = new Date().toISOString();

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction(['chat_sessions'], 'readwrite');
      const store = transaction.objectStore('chat_sessions');
      const request = store.put(session);

      request.onsuccess = () => resolve();
      request.onerror = () => reject(request.error);
    });
  }

  async deleteSession(session_uuid: string): Promise<void> {
    this.ensureInitialized();

    // Delete all messages for this session
    const messages = await this.getMessages(session_uuid);
    await Promise.all(messages.map(msg => this.deleteMessage(msg.id)));

    // Delete the session
    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction(['chat_sessions'], 'readwrite');
      const store = transaction.objectStore('chat_sessions');
      const request = store.delete(session_uuid);

      request.onsuccess = () => resolve();
      request.onerror = () => reject(request.error);
    });
  }

  async searchSessions(searchTerm: string): Promise<ChatSession[]> {
    this.ensureInitialized();
    const allSessions = await this.getSessions(1000);
    const lowerSearchTerm = searchTerm.toLowerCase();

    return allSessions.filter(session =>
      session.title.toLowerCase().includes(lowerSearchTerm)
    );
  }

  // Message Management
  async addMessage(message: Omit<ChatMessage, 'id' | 'timestamp'>): Promise<ChatMessage> {
    this.ensureInitialized();
    const id = crypto.randomUUID();
    const timestamp = new Date().toISOString();

    const fullMessage: ChatMessage = {
      id,
      ...message,
      timestamp
    };

    return new Promise(async (resolve, reject) => {
      // Add message
      const transaction = this.db!.transaction(['chat_messages', 'chat_sessions'], 'readwrite');
      const messagesStore = transaction.objectStore('chat_messages');
      const addRequest = messagesStore.add(fullMessage);

      addRequest.onsuccess = async () => {
        // Update session message count
        const session = await this.getSession(message.session_uuid);
        if (session) {
          session.message_count++;
          session.updated_at = timestamp;

          const sessionsStore = transaction.objectStore('chat_sessions');
          sessionsStore.put(session);
        }
        resolve(fullMessage);
      };

      addRequest.onerror = () => reject(addRequest.error);
    });
  }

  async getMessages(session_uuid: string): Promise<ChatMessage[]> {
    this.ensureInitialized();

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction(['chat_messages'], 'readonly');
      const store = transaction.objectStore('chat_messages');
      const index = store.index('session_uuid');
      const request = index.getAll(session_uuid);

      request.onsuccess = () => {
        const messages = request.result || [];
        // Sort by timestamp ASC
        messages.sort((a, b) => a.timestamp.localeCompare(b.timestamp));
        resolve(messages);
      };

      request.onerror = () => reject(request.error);
    });
  }

  async deleteMessage(id: string): Promise<void> {
    this.ensureInitialized();

    // Get message to find session_uuid
    const transaction1 = this.db!.transaction(['chat_messages'], 'readonly');
    const store1 = transaction1.objectStore('chat_messages');
    const getRequest = store1.get(id);

    return new Promise((resolve, reject) => {
      getRequest.onsuccess = async () => {
        const message = getRequest.result;
        if (!message) {
          resolve();
          return;
        }

        // Delete message
        const transaction2 = this.db!.transaction(['chat_messages', 'chat_sessions'], 'readwrite');
        const messagesStore = transaction2.objectStore('chat_messages');
        const deleteRequest = messagesStore.delete(id);

        deleteRequest.onsuccess = async () => {
          // Update session message count
          const session = await this.getSession(message.session_uuid);
          if (session && session.message_count > 0) {
            session.message_count--;
            const sessionsStore = transaction2.objectStore('chat_sessions');
            sessionsStore.put(session);
          }
          resolve();
        };

        deleteRequest.onerror = () => reject(deleteRequest.error);
      };

      getRequest.onerror = () => reject(getRequest.error);
    });
  }

  async clearSessionMessages(session_uuid: string): Promise<void> {
    this.ensureInitialized();
    const messages = await this.getMessages(session_uuid);

    await Promise.all(messages.map(msg => this.deleteMessage(msg.id)));

    // Update session
    const session = await this.getSession(session_uuid);
    if (session) {
      session.message_count = 0;
      session.updated_at = new Date().toISOString();

      return new Promise((resolve, reject) => {
        const transaction = this.db!.transaction(['chat_sessions'], 'readwrite');
        const store = transaction.objectStore('chat_sessions');
        const request = store.put(session);

        request.onsuccess = () => resolve();
        request.onerror = () => reject(request.error);
      });
    }
  }

  // Statistics
  async getStatistics(): Promise<{
    totalSessions: number;
    totalMessages: number;
    totalTokens: number;
  }> {
    this.ensureInitialized();

    return new Promise((resolve, reject) => {
      const transaction = this.db!.transaction(['chat_sessions', 'chat_messages'], 'readonly');

      const sessionsStore = transaction.objectStore('chat_sessions');
      const sessionsRequest = sessionsStore.count();

      const messagesStore = transaction.objectStore('chat_messages');
      const messagesRequest = messagesStore.getAll();

      let totalSessions = 0;
      let totalMessages = 0;
      let totalTokens = 0;

      sessionsRequest.onsuccess = () => {
        totalSessions = sessionsRequest.result;
      };

      messagesRequest.onsuccess = () => {
        const messages = messagesRequest.result;
        totalMessages = messages.length;
        totalTokens = messages.reduce((sum, msg) => sum + (msg.tokens_used || 0), 0);
      };

      transaction.oncomplete = () => {
        resolve({ totalSessions, totalMessages, totalTokens });
      };

      transaction.onerror = () => reject(transaction.error);
    });
  }

  // Cleanup old sessions
  async cleanupOldSessions(daysOld: number = 30): Promise<number> {
    this.ensureInitialized();
    const cutoffDate = new Date();
    cutoffDate.setDate(cutoffDate.getDate() - daysOld);
    const cutoffISO = cutoffDate.toISOString();

    const allSessions = await this.getSessions(10000);
    const oldSessions = allSessions.filter(session => session.updated_at < cutoffISO);

    await Promise.all(oldSessions.map(session => this.deleteSession(session.session_uuid)));

    return oldSessions.length;
  }

  // Close database connection
  async close(): Promise<void> {
    if (this.db) {
      this.db.close();
      this.db = null;
      this.isInitialized = false;
    }
  }
}

// Singleton instance
export const chatDatabase = new ChatDatabaseService();
