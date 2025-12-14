# Lightweight Charts Plugin System

A comprehensive plugin architecture for extending lightweight-charts with custom primitives, drawing tools, indicators, and interactive features in the Fincept Terminal.

## Table of Contents

- [Overview](#overview)
- [Architecture](#architecture)
- [Built-in Plugins](#built-in-plugins)
- [Usage](#usage)
- [Creating Custom Plugins](#creating-custom-plugins)
- [API Reference](#api-reference)
- [Examples](#examples)

## Overview

The plugin system allows you to extend the lightweight-charts library with:

- **Drawing Tools**: Interactive tools for technical analysis (trend lines, shapes, annotations)
- **Indicators**: Custom technical indicators and overlays
- **Annotations**: Text labels, vertical lines, markers
- **Interactive Features**: Tooltips, crosshair enhancements, custom interactions
- **Overlays**: Background shading, session highlighting, custom visualizations

### Features

✅ **Type-safe**: Full TypeScript support with comprehensive type definitions
✅ **Modular**: Plugin-based architecture for easy extension
✅ **Performance**: Built on lightweight-charts primitives for optimal rendering
✅ **Interactive**: Support for mouse events and drawing interactions
✅ **Customizable**: Extensive options for styling and behavior
✅ **Production-ready**: Designed for professional financial terminal use

## Architecture

### Core Components

```
plugins/
├── types.ts                  # Type definitions and interfaces
├── BasePlugin.ts             # Abstract base class for all plugins
├── PluginManager.ts          # Manages plugin lifecycle and coordination
├── TooltipPlugin.ts          # Interactive tooltip display
├── TrendLinePlugin.ts        # Trend line drawing tool
├── VerticalLinePlugin.ts     # Vertical time markers
├── index.ts                  # Main export and plugin registry
└── README.md                 # This file
```

### Plugin Lifecycle

1. **Creation**: Plugin instance created with configuration options
2. **Registration**: Plugin registered with PluginManager
3. **Initialization**: Plugin receives chart and series context
4. **Attachment**: Primitive attached to series for rendering
5. **Updates**: Plugin responds to option changes and events
6. **Destruction**: Cleanup when plugin is removed

## Built-in Plugins

### 1. TooltipPlugin

Displays customizable tooltips with price and time information on hover.

**Features**:
- OHLC data display
- Volume information
- Custom formatting
- Follow cursor mode
- Configurable styling

**Example**:
```typescript
import { TooltipPlugin } from './plugins';

const tooltip = new TooltipPlugin({
  showOHLC: true,
  showVolume: true,
  backgroundColor: 'rgba(0, 0, 0, 0.85)',
  fontSize: 11,
});
```

### 2. TrendLinePlugin

Interactive trend line drawing tool for technical analysis.

**Features**:
- Two-point click drawing
- Line extension (left/right)
- Custom colors and styles
- Multiple lines support
- Edit and remove lines

**Example**:
```typescript
import { TrendLinePlugin } from './plugins';

const trendLine = new TrendLinePlugin({
  color: '#2196F3',
  lineWidth: 2,
  extendRight: true,
});

// Activate drawing mode
trendLine.startDrawing();

// Add programmatically
trendLine.addLine(
  { time: 1234567890, price: 100 },
  { time: 1234567900, price: 110 }
);
```

### 3. VerticalLinePlugin

Marks specific time points with vertical lines.

**Features**:
- Time-based markers
- Optional labels
- Customizable styling
- Dashed/solid lines
- Event marking

**Example**:
```typescript
import { VerticalLinePlugin } from './plugins';

const vLine = new VerticalLinePlugin({
  showLabel: true,
  dashArray: [5, 5],
});

// Mark current time
vLine.markNow('Market Open');

// Add custom marker
vLine.addMarker(timestamp, 'Event', '#ff9800');
```

## Usage

### Basic Setup

```typescript
import { PluginManager, TooltipPlugin, TrendLinePlugin } from './plugins';
import { createChart, CandlestickSeries } from 'lightweight-charts';

// Create chart and series
const chart = createChart(container);
const series = chart.addSeries(CandlestickSeries);

// Initialize plugin manager
const pluginManager = new PluginManager(chart, series);

// Register plugins
pluginManager.register(new TooltipPlugin());
pluginManager.register(new TrendLinePlugin());

// Get plugin statistics
console.log(pluginManager.getStats());
// { total: 2, enabled: 2, byCategory: { interactive: 1, drawing: 1 } }
```

### Using with TradingChartWithPlugins

```typescript
import { TradingChartWithPlugins } from './charts/TradingChartWithPlugins';

function MyChart() {
  return (
    <TradingChartWithPlugins
      symbol="BTC/USD"
      timeframe="1h"
      enablePlugins={true}
    />
  );
}
```

### Managing Plugins

```typescript
// Enable/disable plugins
pluginManager.togglePlugin('tooltip', false);

// Get specific plugin
const trendLine = pluginManager.getPlugin('trendline');

// Get plugins by category
const drawingTools = pluginManager.getPluginsByCategory(PluginCategory.DRAWING);

// Clear all plugins
pluginManager.clear();
```

## Creating Custom Plugins

### Step 1: Define Plugin Class

```typescript
import { BasePlugin } from './BasePlugin';
import { PluginCategory, PluginOptions } from './types';
import type { ISeriesPrimitive, Time } from 'lightweight-charts';

interface MyPluginOptions extends PluginOptions {
  customOption?: string;
}

export class MyCustomPlugin extends BasePlugin<Time> {
  constructor(options: Partial<MyPluginOptions> = {}) {
    super(
      {
        id: 'my-plugin',
        name: 'My Custom Plugin',
        description: 'Does something awesome',
        category: PluginCategory.INDICATOR,
        version: '1.0.0',
        enabled: true
      },
      options
    );
  }

  protected createPrimitive(): ISeriesPrimitive<Time> | null {
    // Create and return your primitive
    return new MyCustomPrimitive(this.options);
  }
}
```

### Step 2: Implement Primitive

```typescript
import type {
  ISeriesPrimitive,
  ISeriesPrimitivePaneView,
  SeriesAttachedParameter,
  Time
} from 'lightweight-charts';

class MyCustomPrimitive implements ISeriesPrimitive<Time> {
  private _paneViews: MyPaneView[];

  constructor(options: any) {
    this._paneViews = [new MyPaneView(options)];
  }

  attached(param: SeriesAttachedParameter<Time>): void {
    // Called when primitive is attached
  }

  updateAllViews(param: SeriesAttachedParameter<Time>): void {
    // Update all views with new data
    this._paneViews.forEach(view => view.update(param));
  }

  paneViews(): readonly ISeriesPrimitivePaneView[] {
    return this._paneViews;
  }
}
```

### Step 3: Implement Pane View

```typescript
import type {
  ISeriesPrimitivePaneView,
  SeriesAttachedParameter,
  CanvasRenderingTarget2D,
  Time
} from 'lightweight-charts';

class MyPaneView implements ISeriesPrimitivePaneView {
  private _options: any;

  constructor(options: any) {
    this._options = options;
  }

  update(param: SeriesAttachedParameter<Time>): void {
    // Update view data
  }

  renderer(): any {
    return {
      draw: (target: CanvasRenderingTarget2D) => {
        const ctx = target.context;

        // Your custom drawing code here
        ctx.fillStyle = this._options.color;
        ctx.fillRect(10, 10, 100, 100);
      }
    };
  }
}
```

### Step 4: Register Plugin

```typescript
import { PLUGIN_REGISTRY } from './index';
import { MyCustomPlugin } from './MyCustomPlugin';

// Add to registry
PLUGIN_REGISTRY['my-plugin'] = () => new MyCustomPlugin();

// Or register directly
pluginManager.register(new MyCustomPlugin({
  customOption: 'value'
}));
```

## API Reference

### PluginManager

#### Methods

- `register(plugin: IChartPlugin): void` - Register a plugin
- `unregister(pluginId: string): void` - Unregister a plugin
- `getPlugins(): IChartPlugin[]` - Get all plugins
- `getPlugin(pluginId: string): IChartPlugin | undefined` - Get specific plugin
- `getPluginsByCategory(category: PluginCategory): IChartPlugin[]` - Get plugins by category
- `togglePlugin(pluginId: string, enabled: boolean): void` - Enable/disable plugin
- `getStats()` - Get plugin statistics
- `clear(): void` - Remove all plugins
- `destroy(): void` - Cleanup manager

### BasePlugin

#### Properties

- `metadata: PluginMetadata` - Plugin metadata
- `options: PluginOptions` - Plugin options

#### Methods

- `initialize(chart, series): void` - Initialize plugin
- `applyOptions(options): void` - Update options
- `getPrimitive(): ISeriesPrimitive | null` - Get primitive
- `setEnabled(enabled: boolean): void` - Enable/disable
- `destroy(): void` - Cleanup

#### Protected Methods (for subclasses)

- `createPrimitive(): ISeriesPrimitive | null` - Create primitive
- `getDefaultOptions(): PluginOptions` - Get defaults
- `onOptionsChanged(): void` - Handle option changes
- `attachPrimitive(): void` - Attach primitive to series
- `detachPrimitive(): void` - Detach primitive

### Plugin Types

#### PluginCategory

```typescript
enum PluginCategory {
  DRAWING = 'drawing',
  INDICATOR = 'indicator',
  ANNOTATION = 'annotation',
  INTERACTIVE = 'interactive',
  OVERLAY = 'overlay',
}
```

#### PluginOptions

```typescript
interface PluginOptions {
  color?: string;
  lineWidth?: number;
  lineStyle?: number;
  visible?: boolean;
  interactive?: boolean;
  [key: string]: any;
}
```

## Examples

### Example 1: Multi-Plugin Setup

```typescript
const pluginManager = new PluginManager(chart, series);

// Add tooltip
const tooltip = new TooltipPlugin({
  showOHLC: true,
  showVolume: true,
});
pluginManager.register(tooltip);

// Add trend lines
const trendLine = new TrendLinePlugin({
  color: '#2196F3',
  extendRight: true,
});
pluginManager.register(trendLine);

// Add vertical markers
const vLine = new VerticalLinePlugin();
pluginManager.register(vLine);

// Mark important events
vLine.addMarker(1234567890, 'Event A', '#ff9800');
vLine.addMarker(1234567900, 'Event B', '#4caf50');
```

### Example 2: Interactive Drawing

```typescript
const trendLine = new TrendLinePlugin();
pluginManager.register(trendLine);

// Activate drawing mode
function startDrawing() {
  trendLine.startDrawing();
  console.log('Click two points to draw a trend line');
}

// Handle chart clicks
chartContainer.addEventListener('click', (event) => {
  if (trendLine.isDrawing()) {
    const point = getChartPoint(event); // Your coordinate conversion
    trendLine.handleClick(point);
  }
});

// Stop drawing
function stopDrawing() {
  trendLine.stopDrawing();
}

// Clear all lines
function clearAll() {
  trendLine.clear();
}
```

### Example 3: Custom Plugin with Events

```typescript
class AlertPlugin extends BasePlugin<Time> {
  private alerts: Map<string, Alert> = new Map();

  addAlert(time: Time, price: number, message: string) {
    const id = `alert-${Date.now()}`;
    this.alerts.set(id, { time, price, message });

    // Emit event
    this.emitEvent({
      type: PluginEventType.CUSTOM,
      data: { id, time, price, message },
      timestamp: Date.now()
    });

    return id;
  }

  protected createPrimitive(): ISeriesPrimitive<Time> {
    return new AlertPrimitive(this.alerts);
  }
}

// Usage with event listeners
const alertPlugin = new AlertPlugin();
alertPlugin.addEventListener(PluginEventType.CUSTOM, (event) => {
  console.log('Alert added:', event.data);
  // Trigger notification, sound, etc.
});

pluginManager.register(alertPlugin);
```

## Best Practices

1. **Type Safety**: Always use TypeScript for type checking
2. **Performance**: Minimize drawing operations, use memoization
3. **Cleanup**: Always destroy plugins when done
4. **Error Handling**: Handle edge cases and invalid data
5. **Documentation**: Document custom plugins and options
6. **Testing**: Test plugins with various data scenarios
7. **Modularity**: Keep plugins focused on single responsibility

## Resources

- [Lightweight Charts Documentation](https://tradingview.github.io/lightweight-charts/)
- [Plugin Examples](https://tradingview.github.io/lightweight-charts/plugin-examples/)
- [API Reference](https://tradingview.github.io/lightweight-charts/docs/api)
- [Primitives Guide](https://tradingview.github.io/lightweight-charts/docs/plugins/intro)

## Contributing

To add new plugins:

1. Create plugin class extending `BasePlugin`
2. Implement required methods
3. Add to `PLUGIN_REGISTRY` in `index.ts`
4. Add documentation and examples
5. Test with `TradingChartWithPlugins`

## License

MIT - Part of Fincept Terminal

---

**Version**: 1.0.0
**Last Updated**: 2025-01-14
**Maintained By**: Fincept Corporation
