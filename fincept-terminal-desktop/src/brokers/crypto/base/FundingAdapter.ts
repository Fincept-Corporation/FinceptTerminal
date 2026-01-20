/**
 * Funding Adapter
 *
 * Deposits, withdrawals, and transfer methods
 * Extends DerivativesAdapter with funding functionality
 */

import { DerivativesAdapter } from './DerivativesAdapter';

export abstract class FundingAdapter extends DerivativesAdapter {
  // ============================================================================
  // DEPOSITS
  // ============================================================================

  async fetchDeposits(code?: string, since?: number, limit?: number): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchDeposits']) {
        throw new Error('fetchDeposits not supported by this exchange');
      }
      return await this.exchange.fetchDeposits(code, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchDepositAddress(code: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchDepositAddress']) {
        throw new Error('fetchDepositAddress not supported by this exchange');
      }
      return await this.exchange.fetchDepositAddress(code, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async createDepositAddress(code: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['createDepositAddress']) {
        throw new Error('createDepositAddress not supported by this exchange');
      }
      return await this.exchange.createDepositAddress(code, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchDepositMethods(code?: string): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchDepositMethods']) {
        throw new Error('fetchDepositMethods not supported by this exchange');
      }
      return await (this.exchange as any).fetchDepositMethods(code);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // WITHDRAWALS
  // ============================================================================

  async fetchWithdrawals(code?: string, since?: number, limit?: number): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchWithdrawals']) {
        throw new Error('fetchWithdrawals not supported by this exchange');
      }
      return await this.exchange.fetchWithdrawals(code, since, limit);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async withdraw(code: string, amount: number, address: string, tag?: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['withdraw']) {
        throw new Error('withdraw not supported by this exchange');
      }
      return await this.exchange.withdraw(code, amount, address, tag, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  // ============================================================================
  // TRANSFERS
  // ============================================================================

  async transfer(code: string, amount: number, fromAccount: string, toAccount: string, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['transfer']) {
        throw new Error('transfer not supported by this exchange');
      }
      return await this.exchange.transfer(code, amount, fromAccount, toAccount, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async transferOut(code: string, amount: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['transfer']) {
        throw new Error('transferOut not supported by this exchange');
      }
      return await this.exchange.transfer(code, amount, 'spot', 'external', params);
    } catch (error) {
      throw this.handleError(error);
    }
  }

  async fetchTransfers(code?: string, since?: number, limit?: number, params?: any): Promise<any> {
    try {
      await this.ensureAuthenticated();
      if (!this.exchange.has['fetchTransfers']) {
        throw new Error('fetchTransfers not supported by this exchange');
      }
      return await (this.exchange as any).fetchTransfers(code, since, limit, params);
    } catch (error) {
      throw this.handleError(error);
    }
  }
}
