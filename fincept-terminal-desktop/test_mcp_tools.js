// Quick test script
const { invoke } = require('@tauri-apps/api/core');

async function test() {
  console.log('Testing MCP tools availability...');
  
  // Simulate what ChatTab does
  const tools = [
    {
      serverId: 'fincept-terminal',
      name: 'list_notes',
      description: 'List all saved notes',
      inputSchema: { type: 'object', properties: {} }
    }
  ];
  
  const formatted = tools.map(tool => ({
    type: 'function',
    function: {
      name: `${tool.serverId}__${tool.name}`,
      description: tool.description,
      parameters: tool.inputSchema
    }
  }));
  
  console.log('Formatted tools:', JSON.stringify(formatted, null, 2));
}

test();
