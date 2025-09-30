# Multi-Connection Agent System

## Overview

The Agents Tab now supports **multiple simultaneous connections** for both inputs and outputs, allowing you to create complex data pipelines and multi-strategy workflows.

## Connection Capabilities by Agent Category

### 📊 Data Agents (3 inputs, 3 outputs)
- **Can receive from**: Multiple data sources simultaneously
- **Can send to**: Multiple analysis/strategy agents
- **Use case**: Aggregate data from multiple sources and distribute to multiple consumers

```
Data Source 1 ──┐
Data Source 2 ──┼─► Data Aggregator ──┬─► Analysis Agent 1
Data Source 3 ──┘                      ├─► Analysis Agent 2
                                       └─► Analysis Agent 3
```

### 📈 Analysis Agents (3 inputs, 2 outputs)
- **Can receive from**: Multiple data agents or other analysis agents
- **Can send to**: Multiple strategy/execution agents
- **Use case**: Combine multiple data streams for comprehensive analysis

```
Market Data ────┐
News Data ───────┼─► Fundamental Analysis ──┬─► Strategy Agent
Economic Data ───┘                           └─► Risk Assessment
```

### 💡 Hedge Fund Agents (2 inputs, 3 outputs)
- **Can receive from**: Data and analysis agents
- **Can send to**: Multiple execution/monitoring agents
- **Use case**: Generate signals for multiple portfolio strategies

```
Technical Data ──┐
Fundamental ─────┼─► Macro Cycle Agent ──┬─► Portfolio Manager 1
                 │                        ├─► Portfolio Manager 2
                                         └─► Signal Generator
```

### 🎯 Trader Style Agents (2 inputs, 3 outputs)
- **Can receive from**: Data and analysis agents
- **Can send to**: Multiple signal generators or portfolio managers
- **Use case**: Apply legendary investor strategies to multiple portfolios

```
Financial Data ──┐
Market Sentiment ┼─► Warren Buffett Agent ──┬─► Long-term Portfolio
                 │                           ├─► Value Portfolio
                                            └─► Quality Portfolio
```

### ⚡ Execution Agents (2 inputs, 2 outputs)
- **Can receive from**: Multiple strategy agents
- **Can send to**: Monitoring and portfolio management
- **Use case**: Aggregate signals from multiple strategies

```
Strategy 1 ──┐
Strategy 2 ──┼─► Signal Generator ──┬─► Portfolio Manager
Strategy 3 ──┘                       └─► Alert Monitor
```

### 🔔 Monitoring Agents (2 inputs, 2 outputs)
- **Can receive from**: Execution and portfolio agents
- **Can send to**: Other monitoring or alerting systems
- **Use case**: Multi-level monitoring and alerting

## Visual Connection Guide

### Connection Colors & Types

| Source Category | Edge Color | Label | Stroke Width | Purpose |
|----------------|------------|-------|--------------|---------|
| Data Agents | 🟢 Green (#84cc16) | 📊 Data | 2.5px | Raw data flow |
| Analysis Agents | 🟣 Purple (#8b5cf6) | 📈 Analysis | 2.5px | Analyzed insights |
| Trader/Hedge Fund | 🔵 Blue (#3b82f6) | 💡 Signal | 2.5px | Trading signals |
| Execution Agents | 🟢 Green (#10b981) | ⚡ Execute | 3px | Execution commands |
| Other | 🟠 Orange (#ea580c) | - | 2px | Generic data flow |

### Handle Positioning

Each agent node displays multiple handles distributed evenly:

```
INPUT HANDLES (Left Side)          OUTPUT HANDLES (Right Side)
        ○ Input 1                           Output 1 ○
        ○ Input 2                           Output 2 ○
        ○ Input 3                           Output 3 ○
```

The handles are positioned at:
- **2 handles**: 33%, 66% from top
- **3 handles**: 25%, 50%, 75% from top

## Example Workflows

### 1. Multi-Source Data Aggregation

```
Yahoo Finance ──────┐
Alpha Vantage ──────┼─► Data Aggregator ──► Fundamental Analysis
Polygon.io ─────────┘
```

### 2. Multi-Strategy Signal Generation

```
                     ┌─► Warren Buffett Agent ─┐
Market Data ────────┤                          ├─► Signal Aggregator ──► Portfolio Manager
                     └─► Joel Greenblatt ──────┘
```

### 3. Hedge Fund Pipeline

```
Economic Data ──┐
Market Data ────┼─► Macro Cycle Agent ───┬─► Risk Assessment ──► Portfolio Manager
News Feed ──────┘                         └─► Alert Monitor
```

### 4. Multi-Level Analysis

```
Price Data ──┐
Volume Data ─┼─► Technical Analysis ──┐
Indicators ──┘                        ├─► Signal Generator ──► Execution Agent
                                      │
Financials ──┐                        │
Earnings ────┼─► Fundamental Analysis ┘
Balance Sheet┘
```

### 5. Diversified Portfolio Strategy

```
                  ┌─► Value Portfolio
Data Collector ──┼─► Growth Portfolio
                  ├─► Income Portfolio
                  └─► Performance Tracker (monitors all)
```

## Advanced Patterns

### Fan-Out Pattern (1 → Many)
One agent distributes data to multiple consumers:

```
Single Data Source ──► Agent ──┬─► Consumer 1
                               ├─► Consumer 2
                               └─► Consumer 3
```

### Fan-In Pattern (Many → 1)
Multiple sources aggregate into one agent:

```
Source 1 ──┐
Source 2 ──┼─► Aggregator ──► Output
Source 3 ──┘
```

### Pipeline Pattern
Sequential processing with multiple stages:

```
Data ──► Collector ──► Transformer ──► Analyzer ──► Generator ──► Executor
```

### Diamond Pattern
Split and rejoin for parallel processing:

```
         ┌─► Fast Analysis ───┐
Input ──┤                      ├─► Combiner ──► Output
         └─► Deep Analysis ────┘
```

### Hub-and-Spoke Pattern
Central coordinator with multiple satellites:

```
        ┌─► Specialist 1 ──┐
        ├─► Specialist 2 ──┤
Hub ───┼─► Specialist 3 ──┼─► Hub ──► Output
        ├─► Specialist 4 ──┤
        └─► Specialist 5 ──┘
```

## Best Practices

### 1. Connection Organization
- **Group by purpose**: Keep related agents close together
- **Color coding**: Use edge colors to identify data flow types
- **Label connections**: Connection labels show data type

### 2. Performance Optimization
- **Limit depth**: Keep workflow depth under 6-7 levels
- **Parallel execution**: Use fan-out for concurrent processing
- **Caching**: Reuse data from upstream agents when possible

### 3. Workflow Design
- **Modular design**: Create reusable sub-workflows
- **Error handling**: Include monitoring agents at critical points
- **Validation**: Test each connection before full execution

### 4. Connection Limits
While technically unlimited, recommended maximums:
- **Data Agents**: Up to 5-6 outputs for distribution
- **Analysis Agents**: Up to 3-4 inputs for aggregation
- **Strategy Agents**: Up to 3-4 outputs for diversification
- **Execution Agents**: Up to 2-3 inputs for signal consensus

## Connection Validation

The system automatically validates:
- ✅ Source and target nodes exist
- ✅ Appropriate handle types (source → target)
- ✅ No self-connections
- ✅ Unique connection IDs

## Troubleshooting

### Connection Won't Create
1. **Check handle visibility**: Ensure handles are visible on both nodes
2. **Verify compatibility**: Some agents may have category restrictions
3. **Clear existing**: Delete conflicting connections

### Too Many Connections
1. **Use aggregator agents**: Combine multiple inputs before processing
2. **Create sub-workflows**: Break complex flows into manageable pieces
3. **Use fan-in/fan-out**: Organize connections systematically

### Performance Issues
1. **Reduce connection count**: Limit to essential connections
2. **Simplify workflow**: Break into smaller workflows
3. **Use caching**: Enable data caching for repeated queries

## Future Enhancements

### Planned Features
- [ ] Connection grouping (bundle multiple connections)
- [ ] Conditional connections (activate based on criteria)
- [ ] Connection weights (priority/importance)
- [ ] Connection filters (data transformation on edge)
- [ ] Connection templates (save common patterns)
- [ ] Connection validation rules (custom constraints)
- [ ] Connection analytics (data flow metrics)

### Advanced Capabilities
- [ ] Dynamic connection routing
- [ ] Load balancing across connections
- [ ] Connection failover and redundancy
- [ ] Real-time connection health monitoring
- [ ] Connection-level error handling

## API Reference

### Connection Object
```typescript
{
  id: string;                    // Unique connection ID
  source: string;                // Source node ID
  sourceHandle: string;          // Source handle ID (e.g., "output-0")
  target: string;                // Target node ID
  targetHandle: string;          // Target handle ID (e.g., "input-1")
  animated: boolean;             // Animation state
  style: {
    stroke: string;              // Edge color
    strokeWidth: number;         // Edge thickness
  };
  label: string;                 // Connection label
  markerEnd: {
    type: MarkerType;            // Arrow type
    color: string;               // Arrow color
  };
}
```

### Creating Connections Programmatically
```typescript
const connection = {
  source: 'agent_1',
  sourceHandle: 'output-0',
  target: 'agent_2',
  targetHandle: 'input-1',
};
onConnect(connection);
```

## Keyboard Shortcuts

- **Delete**: Remove selected connections
- **Backspace**: Remove selected connections
- **Ctrl+A**: Select all (nodes and connections)
- **Ctrl+C**: Copy selected (future feature)
- **Ctrl+V**: Paste (future feature)

## Tips & Tricks

1. **Visual Organization**: Use different handle positions to avoid crossing connections
2. **Connection Labels**: Hover over connections to see their type
3. **Color Coding**: Quickly identify data flow by connection color
4. **Handle Tooltips**: Hover over handles to see "Input N" or "Output N"
5. **Multi-Select**: Select multiple nodes to see all their connections highlighted

---

*Multi-connection support enables sophisticated agentic workflows with parallel processing, data aggregation, and strategy diversification.*
