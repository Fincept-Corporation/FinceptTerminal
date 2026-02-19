/**
 * mcpBridge.ts - TypeScript side of the Python ↔ TypeScript MCP tool bridge
 *
 * Listens for "mcp-tool-call" Tauri events emitted by mcp_bridge.rs when a Python
 * finagent_core agent calls a terminal tool. Executes the tool via mcpToolService
 * and delivers the result back via the register_mcp_tool_result Tauri command.
 *
 * Initialized once at app startup via initMCPBridge().
 */

import { listen, UnlistenFn } from '@tauri-apps/api/event';
import { invoke } from '@tauri-apps/api/core';
import { mcpToolService } from './mcpToolService';
import { INTERNAL_SERVER_ID } from './internal';

interface MCPToolCallEvent {
  id: string;
  tool_name: string;
  args: Record<string, any>;
}

let _unlisten: UnlistenFn | null = null;
let _initialized = false;

/**
 * Initialize the MCP bridge listener.
 * Safe to call multiple times — only registers once.
 */
export async function initMCPBridge(): Promise<void> {
  if (_initialized) return;
  _initialized = true;

  try {
    _unlisten = await listen<MCPToolCallEvent>('mcp-tool-call', async (event) => {
      const { id, tool_name, args } = event.payload;

      if (!id || !tool_name) {
        console.warn('[MCPBridge] Received mcp-tool-call with missing id or tool_name', event.payload);
        return;
      }

      let result: string;
      try {
        const execResult = await mcpToolService.executeToolDirect(
          INTERNAL_SERVER_ID,
          tool_name,
          args || {}
        );
        result = JSON.stringify({
          success: execResult.success,
          data: execResult.data,
          error: execResult.error,
          message: execResult.message,
        });
      } catch (err: any) {
        result = JSON.stringify({
          success: false,
          error: err?.message || String(err),
        });
      }

      // Deliver result back to the waiting Rust HTTP bridge handler
      try {
        await invoke('register_mcp_tool_result', { id, result });
      } catch (err: any) {
        console.error('[MCPBridge] Failed to deliver tool result:', err);
      }
    });

    console.log('[MCPBridge] Initialized — listening for mcp-tool-call events');
  } catch (err) {
    console.error('[MCPBridge] Failed to initialize:', err);
    _initialized = false;
  }
}

/**
 * Tear down the bridge listener (e.g. on app unmount).
 */
export function destroyMCPBridge(): void {
  if (_unlisten) {
    _unlisten();
    _unlisten = null;
  }
  _initialized = false;
}
