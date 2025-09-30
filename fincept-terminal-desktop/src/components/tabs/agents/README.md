# Agents Tab - Node-Based Agent Workflow System

## Overview

The Agents Tab is a visual node-based workflow builder for creating and managing AI agent pipelines. It migrates all 32+ agents from the legacy Python terminal to the new Tauri+React application.

## Features

### 🤖 32+ Pre-Built Agents

#### Hedge Fund Agents (10)
- **Macro Cycle Agent** - Business cycle and economic phase analysis
- **Central Bank Policy Agent** - Federal Reserve and central bank policy monitoring
- **Geopolitical Risk Agent** - Geopolitical risk and market impact assessment
- **Market Sentiment Agent** - Multi-source market sentiment analysis
- **Institutional Flow Agent** - Institutional money flow tracking
- **Supply Chain Agent** - Supply chain disruption monitoring
- **Innovation Trends Agent** - Emerging technology identification
- **Currency Analysis Agent** - Currency and forex market analysis
- **Regulatory Impact Agent** - Regulatory change monitoring
- **Behavioral Psychology Agent** - Market psychology pattern analysis

#### Legendary Trader Agents (10)
- **Warren Buffett Style** - Value investing with economic moats
- **Charlie Munger Style** - Multidisciplinary thinking
- **Benjamin Graham Style** - Deep value with margin of safety
- **Seth Klarman Style** - Risk-averse value investing
- **Howard Marks Style** - Contrarian investing and cycle awareness
- **Joel Greenblatt Style** - Magic formula investing
- **David Einhorn Style** - Value with short selling
- **Bill Miller Style** - Concentrated value with growth
- **Jean-Marie Eveillard Style** - Global value investing
- **Marty Whitman Style** - Safe and cheap balance sheet focus

#### Analysis Agents (5)
- **Technical Analysis** - Technical indicators and patterns
- **Fundamental Analysis** - Deep financial analysis
- **Risk Assessment** - Portfolio risk and VaR calculation
- **Correlation Analysis** - Asset correlation and diversification
- **Scenario Analysis** - Stress testing and scenario modeling

#### Data Agents (3)
- **Data Collector** - Data collection from sources
- **Data Transformer** - Data transformation and normalization
- **Data Aggregator** - Multi-source data aggregation

#### Execution Agents (2)
- **Signal Generator** - Trading signal generation
- **Portfolio Manager** - Portfolio allocation and rebalancing

#### Monitoring Agents (2)
- **Alert Monitor** - Condition monitoring and alerts
- **Performance Tracker** - Strategy performance tracking

### 🎨 Visual Workflow Builder

- **Drag & Drop Interface** - Intuitive node-based editor
- **Visual Connections** - Animated edges showing data flow
- **Real-time Status** - Live agent execution status indicators
- **Category Filters** - Filter agents by category
- **Node Configuration** - Per-agent parameter customization

### 📊 Workflow Management

- **Save/Load** - Save workflows for reuse
- **Export/Import** - JSON-based workflow export/import
- **Execution Engine** - Run entire agent workflows
- **Scheduling** - Support for cron and interval-based execution
- **Version Control** - Track workflow changes

## Architecture

### File Structure

```
agents/
├── AgentsTab.tsx          # Main component with ReactFlow
├── types.ts               # TypeScript type definitions
├── agentConfigs.ts        # Agent configurations (32+ agents)
└── README.md             # This file
```

### Type System

```typescript
// Agent configuration
interface AgentConfig {
  id: string;
  name: string;
  type: string;
  category: AgentCategory;
  icon: string;
  color: string;
  description: string;
  hasInput: boolean;
  hasOutput: boolean;
  parameters: AgentParameter[];
  requiredDataSources?: string[];
}

// Workflow definition
interface AgentWorkflow {
  id: string;
  name: string;
  nodes: any[];
  edges: any[];
  schedule?: WorkflowSchedule;
  isActive: boolean;
}
```

## Usage

### Creating a Workflow

1. **Add Agents** - Click "ADD AGENT" and select from 32+ agents
2. **Configure** - Double-click node labels to rename, click settings to configure
3. **Connect** - Drag from output handles to input handles to create data flow
4. **Execute** - Click "EXECUTE" to run the workflow
5. **Save** - Click "SAVE" or "EXPORT" to persist the workflow

### Example Workflows

#### Value Investing Pipeline
```
Data Collector → Fundamental Analysis → Warren Buffett Agent → Signal Generator → Portfolio Manager
```

#### Hedge Fund Strategy
```
Data Collector → Macro Cycle Agent → Central Bank Agent → Risk Assessment → Portfolio Manager
                → Sentiment Agent ↗                    ↗
```

#### Multi-Strategy Aggregation
```
Data Collector → Technical Analysis → Signal Generator ↘
              → Fundamental Analysis → Signal Generator → Portfolio Manager
              → Sentiment Analysis → Signal Generator ↗
```

## Integration Points

### Data Sources Tab Integration
Agents can specify required data sources:
```typescript
requiredDataSources: ['market-data', 'financial-statements', 'news-feed']
```

### Execution Engine
Currently mock implementation. Production version should:
1. Validate workflow topology (no cycles in DAG)
2. Execute agents in dependency order
3. Pass data between connected agents
4. Handle errors and retries
5. Store execution results

### Scheduling System
Workflows can be scheduled:
```typescript
schedule: {
  type: 'interval',
  interval: 60, // minutes
  nextRun: '2025-01-15T10:00:00Z'
}
```

## Development

### Adding New Agents

1. Add configuration to `agentConfigs.ts`:
```typescript
{
  id: 'my-agent',
  name: 'My Agent',
  type: 'my-agent',
  category: 'analysis',
  icon: '🔬',
  color: '#10b981',
  description: 'Does something cool',
  hasInput: true,
  hasOutput: true,
  parameters: [
    {
      name: 'threshold',
      label: 'Threshold',
      type: 'slider',
      defaultValue: 0.5,
      min: 0,
      max: 1,
      step: 0.1
    }
  ]
}
```

2. Implement execution logic in execution engine
3. Add tests for agent behavior

### Testing

```bash
# Run in development mode
npm run dev

# Navigate to Agents tab in dashboard
# Create test workflow
# Click Execute to test
```

## Future Enhancements

### Phase 1 (Current)
- ✅ Visual node editor
- ✅ 32+ pre-configured agents
- ✅ Workflow save/export
- ✅ Basic execution (mock)

### Phase 2 (Planned)
- [ ] Real agent execution engine
- [ ] Parameter configuration UI
- [ ] Data source connections
- [ ] Workflow validation
- [ ] Error handling

### Phase 3 (Planned)
- [ ] Workflow scheduling
- [ ] Execution history
- [ ] Result visualization
- [ ] Agent marketplace
- [ ] Custom agent creation
- [ ] Backtesting support

### Phase 4 (Advanced)
- [ ] Real-time collaboration
- [ ] Agent performance metrics
- [ ] A/B testing workflows
- [ ] Auto-optimization
- [ ] Cloud deployment

## Technical Stack

- **ReactFlow** - Node-based editor
- **TypeScript** - Type safety
- **React 19** - UI framework
- **TailwindCSS** - Styling
- **Tauri** - Desktop application

## Migration from Python

All agents from the legacy Python terminal have been migrated:

| Python Agent | React Component | Status |
|-------------|----------------|---------|
| MacroCycleAgent | macro-cycle | ✅ Migrated |
| CentralBankAgent | central-bank | ✅ Migrated |
| SentimentAgent | sentiment | ✅ Migrated |
| WarrenBuffettAgent | warren-buffett | ✅ Migrated |
| [+28 more agents...] | [...] | ✅ Migrated |

## API Reference

### AgentsTab Component

```typescript
export default function AgentsTab(): JSX.Element
```

Main component that renders the agent workflow editor.

### Agent Node Component

```typescript
const AgentNode = ({ data, id, selected }: any): JSX.Element
```

Custom ReactFlow node component for agents.

### Helper Functions

```typescript
function getAgentConfig(agentId: string): AgentConfig | undefined
function getAgentsByCategory(category: AgentCategory): AgentConfig[]
```

## Contributing

To contribute new agents or features:

1. Follow the existing agent configuration pattern
2. Ensure TypeScript types are correct
3. Add appropriate icons and colors
4. Document parameters clearly
5. Test workflow execution

## License

Part of Fincept Terminal Desktop Application
