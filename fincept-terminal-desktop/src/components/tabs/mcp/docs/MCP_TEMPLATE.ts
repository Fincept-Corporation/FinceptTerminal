/**
 * MCP Server Template
 *
 * Copy this template to add a new MCP server to the marketplace.
 * Replace all placeholder values marked with <...>
 */

import { MCPServerDefinition } from '../marketplace/serverDefinitions';

export const YOUR_MCP_TEMPLATE: MCPServerDefinition = {
  // Unique identifier (lowercase, hyphenated)
  id: '<your-mcp-id>',

  // Display name
  name: '<Your MCP Name>',

  // Brief description (shown in marketplace)
  description: '<What this MCP server does in one line>',

  // Category: 'data' | 'web' | 'productivity' | 'dev' | 'custom'
  category: 'data',

  // Emoji icon (single emoji)
  icon: '🔧',

  // Command to run (usually 'bunx' for npm packages)
  command: 'bunx',

  // Command arguments
  args: [
    '-y',                              // Force install without prompt
    '@<your-org>/<your-mcp-package>'   // npm package name
  ],

  // Environment variables (optional)
  // Set to empty object {} if not needed
  env: {
    // 'API_KEY': '',
    // 'ENDPOINT_URL': ''
  },

  // CLI argument (optional)
  // Use this if server expects connection string as argument
  // Examples: 'DATABASE_URL', 'CONNECTION_STRING', 'API_ENDPOINT'
  // Leave undefined if not needed
  requiresArg: undefined,  // or 'DATABASE_URL'

  // Whether this server needs configuration
  // true = shows configuration form before install
  // false = installs directly
  requiresConfig: true,

  // List of tools provided by this MCP server
  // Get this from the MCP server's documentation
  tools: [
    'tool_name_1',
    'tool_name_2',
    'tool_name_3'
  ],

  // Link to MCP server documentation (optional)
  documentation: 'https://github.com/<your-org>/<your-mcp-repo>'
};

/**
 * Example 1: Simple MCP (no configuration)
 */
export const EXAMPLE_SIMPLE: MCPServerDefinition = {
  id: 'time-server',
  name: 'Time Server',
  description: 'Get current time in any timezone',
  category: 'productivity',
  icon: '⏰',
  command: 'bunx',
  args: ['-y', '@modelcontextprotocol/server-time'],
  env: {},
  requiresConfig: false,  // No config needed
  tools: ['get_time', 'convert_timezone', 'list_timezones'],
  documentation: 'https://github.com/modelcontextprotocol/servers/tree/main/src/time'
};

/**
 * Example 2: API-based MCP (requires API key)
 */
export const EXAMPLE_API: MCPServerDefinition = {
  id: 'weather-api',
  name: 'Weather API',
  description: 'Get weather data and forecasts',
  category: 'web',
  icon: '🌤️',
  command: 'bunx',
  args: ['-y', '@example/weather-mcp'],
  env: {
    OPENWEATHER_API_KEY: ''  // User will fill this in
  },
  requiresConfig: true,  // Uses GenericForm
  tools: ['get_current_weather', 'get_forecast', 'get_historical'],
  documentation: 'https://docs.example.com/weather-mcp'
};

/**
 * Example 3: Database MCP (requires connection string)
 */
export const EXAMPLE_DATABASE: MCPServerDefinition = {
  id: 'mongodb',
  name: 'MongoDB',
  description: 'Connect to MongoDB databases for queries and updates',
  category: 'data',
  icon: '🍃',
  command: 'bunx',
  args: ['-y', '@example/mongodb-mcp'],
  requiresArg: 'MONGODB_URI',  // Connection string as CLI argument
  requiresConfig: true,  // Needs custom MongoDBForm
  tools: ['find', 'insert', 'update', 'delete', 'aggregate', 'list_collections'],
  documentation: 'https://docs.example.com/mongodb-mcp'
};

/**
 * Example 4: Complex MCP (mixed configuration)
 */
export const EXAMPLE_COMPLEX: MCPServerDefinition = {
  id: 'analytics',
  name: 'Analytics Platform',
  description: 'Connect to analytics platform for data insights',
  category: 'data',
  icon: '📊',
  command: 'bunx',
  args: ['-y', '@example/analytics-mcp'],
  env: {
    API_KEY: '',
    WORKSPACE_ID: ''
  },
  requiresArg: 'ENDPOINT_URL',  // Also needs CLI argument
  requiresConfig: true,  // Needs custom AnalyticsForm
  tools: [
    'get_metrics',
    'run_query',
    'create_dashboard',
    'export_data',
    'list_datasets'
  ],
  documentation: 'https://docs.example.com/analytics-mcp'
};

/**
 * USAGE:
 *
 * 1. Copy one of the examples above that matches your needs
 * 2. Customize all fields
 * 3. Add to MARKETPLACE_SERVERS array in serverDefinitions.ts:
 *
 *    export const MARKETPLACE_SERVERS: MCPServerDefinition[] = [
 *      // ... existing servers
 *      YOUR_NEW_SERVER,  // Add here
 *    ];
 *
 * 4. If requiresConfig = true and you need a custom form:
 *    - Create src/components/tabs/mcp/marketplace/forms/YourMCPForm.tsx
 *    - Register in MCPMarketplace.tsx renderConfigForm() switch statement
 *
 * 5. Test:
 *    npm run tauri dev
 */
