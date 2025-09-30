// Apache Kafka Streaming Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class KafkaAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const brokers = this.getConfig<string>('brokers', 'localhost:9092');
      const clientId = this.getConfig<string>('clientId', 'fincept-terminal');
      const saslMechanism = this.getConfig<string>('saslMechanism');
      const saslUsername = this.getConfig<string>('saslUsername');
      const saslPassword = this.getConfig<string>('saslPassword');
      const ssl = this.getConfig<boolean>('ssl', false);

      if (!brokers) {
        return this.createErrorResult('Broker addresses are required');
      }

      // Parse brokers (can be comma-separated)
      const brokerList = brokers.split(',').map((b) => b.trim());

      console.log('Validating Kafka configuration (client-side only)');

      return this.createSuccessResult(
        `Configuration validated for Kafka cluster`,
        {
          validatedAt: new Date().toISOString(),
          brokers: brokerList,
          clientId,
          ssl,
          hasSasl: !!(saslMechanism && saslUsername && saslPassword),
          saslMechanism,
          note: 'Client-side validation only. Kafka connections require backend integration with kafkajs or similar library.',
        }
      );
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/kafka', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'kafka',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Kafka operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      brokers: this.getConfig('brokers'),
      clientId: this.getConfig('clientId', 'fincept-terminal'),
      ssl: this.getConfig('ssl', false),
      hasSasl: !!(this.getConfig('saslUsername') && this.getConfig('saslPassword')),
      saslMechanism: this.getConfig('saslMechanism'),
    };
  }

  /**
   * List all topics
   */
  async listTopics(): Promise<any> {
    return await this.query('listTopics');
  }

  /**
   * Get topic metadata
   */
  async getTopicMetadata(topics?: string[]): Promise<any> {
    return await this.query('getTopicMetadata', { topics });
  }

  /**
   * Create a new topic
   */
  async createTopic(topic: string, numPartitions: number = 1, replicationFactor: number = 1): Promise<any> {
    return await this.query('createTopic', {
      topic,
      numPartitions,
      replicationFactor,
    });
  }

  /**
   * Delete a topic
   */
  async deleteTopic(topic: string): Promise<any> {
    return await this.query('deleteTopic', { topic });
  }

  /**
   * Produce a message to a topic
   */
  async produce(topic: string, messages: Array<{ key?: string; value: string; partition?: number }>): Promise<any> {
    return await this.query('produce', { topic, messages });
  }

  /**
   * Consume messages from a topic
   */
  async consume(
    topic: string,
    groupId: string,
    options?: {
      fromBeginning?: boolean;
      maxMessages?: number;
      timeout?: number;
    }
  ): Promise<any> {
    return await this.query('consume', {
      topic,
      groupId,
      ...options,
    });
  }

  /**
   * Get consumer group information
   */
  async getConsumerGroups(): Promise<any> {
    return await this.query('getConsumerGroups');
  }

  /**
   * Get consumer group offsets
   */
  async getConsumerGroupOffsets(groupId: string, topics?: string[]): Promise<any> {
    return await this.query('getConsumerGroupOffsets', { groupId, topics });
  }

  /**
   * Get topic offsets
   */
  async getTopicOffsets(topic: string): Promise<any> {
    return await this.query('getTopicOffsets', { topic });
  }

  /**
   * Seek to specific offset
   */
  async seekToOffset(topic: string, partition: number, offset: number): Promise<any> {
    return await this.query('seekToOffset', { topic, partition, offset });
  }

  /**
   * Get cluster metadata
   */
  async getClusterMetadata(): Promise<any> {
    return await this.query('getClusterMetadata');
  }

  /**
   * List consumer group members
   */
  async getConsumerGroupMembers(groupId: string): Promise<any> {
    return await this.query('getConsumerGroupMembers', { groupId });
  }

  /**
   * Reset consumer group offsets
   */
  async resetConsumerGroupOffsets(groupId: string, topic: string, toEarliest: boolean = false): Promise<any> {
    return await this.query('resetConsumerGroupOffsets', {
      groupId,
      topic,
      toEarliest,
    });
  }

  /**
   * Get partition count for a topic
   */
  async getPartitionCount(topic: string): Promise<any> {
    return await this.query('getPartitionCount', { topic });
  }

  /**
   * Check if topic exists
   */
  async topicExists(topic: string): Promise<boolean> {
    const result = await this.query('topicExists', { topic });
    return result.exists;
  }
}
