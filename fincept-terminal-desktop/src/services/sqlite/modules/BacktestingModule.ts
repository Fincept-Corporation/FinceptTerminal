/**
 * Backtesting Module
 * Handles backtesting providers, strategies, runs, and optimization runs
 */

import { sqliteLogger } from '../../loggerService';
import type { BacktestingProvider, BacktestingStrategy, BacktestRun, OptimizationRun, OperationResult } from '../types';
import type { SQLiteBase } from '../SQLiteBase';

export class BacktestingModule {
    constructor(private base: SQLiteBase) { }

    // =========================
    // Providers
    // =========================

    async saveBacktestingProvider(provider: Omit<BacktestingProvider, 'created_at' | 'updated_at'>): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                `INSERT OR REPLACE INTO backtesting_providers (id, name, adapter_type, config, enabled, is_active, updated_at)
         VALUES ($1, $2, $3, $4, $5, $6, CURRENT_TIMESTAMP)`,
                [provider.id, provider.name, provider.adapter_type, provider.config, provider.enabled ? 1 : 0, provider.is_active ? 1 : 0]
            );

            return { success: true, message: 'Backtesting provider saved successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to save backtesting provider:', error);
            return { success: false, message: 'Failed to save backtesting provider' };
        }
    }

    async getBacktestingProviders(): Promise<BacktestingProvider[]> {
        await this.base.ensureInitialized();
        return await this.base.select<BacktestingProvider[]>('SELECT * FROM backtesting_providers ORDER BY name');
    }

    async getBacktestingProvider(name: string): Promise<BacktestingProvider | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<BacktestingProvider[]>(
            'SELECT * FROM backtesting_providers WHERE name = $1',
            [name]
        );
        return results[0] || null;
    }

    async getActiveBacktestingProvider(): Promise<BacktestingProvider | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<BacktestingProvider[]>(
            'SELECT * FROM backtesting_providers WHERE is_active = 1 LIMIT 1'
        );
        return results[0] || null;
    }

    async setActiveBacktestingProvider(name: string): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            // Deactivate all providers first
            await this.base.execute('UPDATE backtesting_providers SET is_active = 0');

            // Activate the specified provider
            await this.base.execute(
                'UPDATE backtesting_providers SET is_active = 1, updated_at = CURRENT_TIMESTAMP WHERE name = $1',
                [name]
            );

            return { success: true, message: `Activated backtesting provider: ${name}` };
        } catch (error) {
            sqliteLogger.error('Failed to set active provider:', error);
            return { success: false, message: 'Failed to set active provider' };
        }
    }

    async deleteBacktestingProvider(name: string): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                'DELETE FROM backtesting_providers WHERE name = $1',
                [name]
            );

            return { success: true, message: 'Provider deleted successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to delete provider:', error);
            return { success: false, message: 'Failed to delete provider' };
        }
    }

    // =========================
    // Strategies
    // =========================

    async saveBacktestingStrategy(strategy: Omit<BacktestingStrategy, 'created_at' | 'updated_at'>): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                `INSERT OR REPLACE INTO backtesting_strategies
         (id, name, description, version, author, provider_type, strategy_type, strategy_definition, tags, updated_at)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, CURRENT_TIMESTAMP)`,
                [
                    strategy.id,
                    strategy.name,
                    strategy.description || null,
                    strategy.version,
                    strategy.author || null,
                    strategy.provider_type,
                    strategy.strategy_type,
                    strategy.strategy_definition,
                    strategy.tags || null,
                ]
            );

            return { success: true, message: 'Strategy saved successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to save strategy:', error);
            return { success: false, message: 'Failed to save strategy' };
        }
    }

    async getBacktestingStrategies(): Promise<BacktestingStrategy[]> {
        await this.base.ensureInitialized();
        return await this.base.select<BacktestingStrategy[]>('SELECT * FROM backtesting_strategies ORDER BY updated_at DESC');
    }

    async getBacktestingStrategy(id: string): Promise<BacktestingStrategy | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<BacktestingStrategy[]>(
            'SELECT * FROM backtesting_strategies WHERE id = $1',
            [id]
        );
        return results[0] || null;
    }

    async getBacktestingStrategiesByProvider(providerType: string): Promise<BacktestingStrategy[]> {
        await this.base.ensureInitialized();
        return await this.base.select<BacktestingStrategy[]>(
            'SELECT * FROM backtesting_strategies WHERE provider_type = $1 ORDER BY updated_at DESC',
            [providerType]
        );
    }

    async deleteBacktestingStrategy(id: string): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                'DELETE FROM backtesting_strategies WHERE id = $1',
                [id]
            );

            return { success: true, message: 'Strategy deleted successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to delete strategy:', error);
            return { success: false, message: 'Failed to delete strategy' };
        }
    }

    // =========================
    // Backtest Runs
    // =========================

    async saveBacktestRun(run: Omit<BacktestRun, 'created_at' | 'completed_at' | 'duration_seconds'>): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                `INSERT INTO backtest_runs
         (id, strategy_id, provider_name, config, results, status, performance_metrics, error_message)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8)`,
                [
                    run.id,
                    run.strategy_id || null,
                    run.provider_name,
                    run.config,
                    run.results || null,
                    run.status,
                    run.performance_metrics || null,
                    run.error_message || null,
                ]
            );

            return { success: true, message: 'Backtest run saved successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to save backtest run:', error);
            return { success: false, message: 'Failed to save backtest run' };
        }
    }

    async updateBacktestRun(id: string, updates: Partial<BacktestRun>): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            const fields: string[] = [];
            const values: any[] = [];

            if (updates.results !== undefined) {
                fields.push('results = $' + (values.length + 1));
                values.push(updates.results);
            }
            if (updates.status !== undefined) {
                fields.push('status = $' + (values.length + 1));
                values.push(updates.status);
            }
            if (updates.performance_metrics !== undefined) {
                fields.push('performance_metrics = $' + (values.length + 1));
                values.push(updates.performance_metrics);
            }
            if (updates.error_message !== undefined) {
                fields.push('error_message = $' + (values.length + 1));
                values.push(updates.error_message);
            }
            if (updates.completed_at !== undefined) {
                fields.push('completed_at = $' + (values.length + 1));
                values.push(updates.completed_at);
            }
            if (updates.duration_seconds !== undefined) {
                fields.push('duration_seconds = $' + (values.length + 1));
                values.push(updates.duration_seconds);
            }

            if (fields.length === 0) {
                return { success: true, message: 'No updates to apply' };
            }

            values.push(id);
            await this.base.execute(
                `UPDATE backtest_runs SET ${fields.join(', ')} WHERE id = $${values.length}`,
                values
            );

            return { success: true, message: 'Backtest run updated successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to update backtest run:', error);
            return { success: false, message: 'Failed to update backtest run' };
        }
    }

    async getBacktestRuns(limit?: number): Promise<BacktestRun[]> {
        await this.base.ensureInitialized();
        const sql = limit
            ? `SELECT * FROM backtest_runs ORDER BY created_at DESC LIMIT ${limit}`
            : 'SELECT * FROM backtest_runs ORDER BY created_at DESC';
        return await this.base.select<BacktestRun[]>(sql);
    }

    async getBacktestRun(id: string): Promise<BacktestRun | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<BacktestRun[]>(
            'SELECT * FROM backtest_runs WHERE id = $1',
            [id]
        );
        return results[0] || null;
    }

    async getBacktestRunsByStrategy(strategyId: string): Promise<BacktestRun[]> {
        await this.base.ensureInitialized();
        return await this.base.select<BacktestRun[]>(
            'SELECT * FROM backtest_runs WHERE strategy_id = $1 ORDER BY created_at DESC',
            [strategyId]
        );
    }

    async getBacktestRunsByProvider(providerName: string): Promise<BacktestRun[]> {
        await this.base.ensureInitialized();
        return await this.base.select<BacktestRun[]>(
            'SELECT * FROM backtest_runs WHERE provider_name = $1 ORDER BY created_at DESC',
            [providerName]
        );
    }

    async deleteBacktestRun(id: string): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                'DELETE FROM backtest_runs WHERE id = $1',
                [id]
            );

            return { success: true, message: 'Backtest run deleted successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to delete backtest run:', error);
            return { success: false, message: 'Failed to delete backtest run' };
        }
    }

    // =========================
    // Optimization Runs
    // =========================

    async saveOptimizationRun(run: Omit<OptimizationRun, 'created_at' | 'completed_at' | 'duration_seconds'>): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                `INSERT INTO optimization_runs
         (id, strategy_id, provider_name, parameter_grid, objective, best_parameters, best_result, all_results, iterations, status, error_message)
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)`,
                [
                    run.id,
                    run.strategy_id || null,
                    run.provider_name,
                    run.parameter_grid,
                    run.objective,
                    run.best_parameters || null,
                    run.best_result || null,
                    run.all_results || null,
                    run.iterations || null,
                    run.status,
                    run.error_message || null,
                ]
            );

            return { success: true, message: 'Optimization run saved successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to save optimization run:', error);
            return { success: false, message: 'Failed to save optimization run' };
        }
    }

    async updateOptimizationRun(id: string, updates: Partial<OptimizationRun>): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            const fields: string[] = [];
            const values: any[] = [];

            if (updates.best_parameters !== undefined) {
                fields.push('best_parameters = $' + (values.length + 1));
                values.push(updates.best_parameters);
            }
            if (updates.best_result !== undefined) {
                fields.push('best_result = $' + (values.length + 1));
                values.push(updates.best_result);
            }
            if (updates.all_results !== undefined) {
                fields.push('all_results = $' + (values.length + 1));
                values.push(updates.all_results);
            }
            if (updates.iterations !== undefined) {
                fields.push('iterations = $' + (values.length + 1));
                values.push(updates.iterations);
            }
            if (updates.status !== undefined) {
                fields.push('status = $' + (values.length + 1));
                values.push(updates.status);
            }
            if (updates.error_message !== undefined) {
                fields.push('error_message = $' + (values.length + 1));
                values.push(updates.error_message);
            }
            if (updates.completed_at !== undefined) {
                fields.push('completed_at = $' + (values.length + 1));
                values.push(updates.completed_at);
            }
            if (updates.duration_seconds !== undefined) {
                fields.push('duration_seconds = $' + (values.length + 1));
                values.push(updates.duration_seconds);
            }

            if (fields.length === 0) {
                return { success: true, message: 'No updates to apply' };
            }

            values.push(id);
            await this.base.execute(
                `UPDATE optimization_runs SET ${fields.join(', ')} WHERE id = $${values.length}`,
                values
            );

            return { success: true, message: 'Optimization run updated successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to update optimization run:', error);
            return { success: false, message: 'Failed to update optimization run' };
        }
    }

    async getOptimizationRuns(limit?: number): Promise<OptimizationRun[]> {
        await this.base.ensureInitialized();
        const sql = limit
            ? `SELECT * FROM optimization_runs ORDER BY created_at DESC LIMIT ${limit}`
            : 'SELECT * FROM optimization_runs ORDER BY created_at DESC';
        return await this.base.select<OptimizationRun[]>(sql);
    }

    async getOptimizationRun(id: string): Promise<OptimizationRun | null> {
        await this.base.ensureInitialized();
        const results = await this.base.select<OptimizationRun[]>(
            'SELECT * FROM optimization_runs WHERE id = $1',
            [id]
        );
        return results[0] || null;
    }

    async deleteOptimizationRun(id: string): Promise<OperationResult> {
        try {
            await this.base.ensureInitialized();

            await this.base.execute(
                'DELETE FROM optimization_runs WHERE id = $1',
                [id]
            );

            return { success: true, message: 'Optimization run deleted successfully' };
        } catch (error) {
            sqliteLogger.error('Failed to delete optimization run:', error);
            return { success: false, message: 'Failed to delete optimization run' };
        }
    }
}
