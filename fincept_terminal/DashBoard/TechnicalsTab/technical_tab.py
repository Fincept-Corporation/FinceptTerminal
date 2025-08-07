"""
Node Editor Tab for Fincept Terminal
Technical Analysis and Backtesting Node Editor
"""

import dearpygui.dearpygui as dpg
import yfinance as yf
import pandas as pd
import numpy as np
import ta
from datetime import datetime, timedelta
import json
from typing import Dict, List, Any, Optional
from dataclasses import dataclass, field
from fincept_terminal.Utils.base_tab import BaseTab
from fincept_terminal.Utils.Logging.logger import logger

@dataclass
class NodeData:
    """Data structure for node information"""
    node_id: str
    node_type: str
    inputs: Dict[str, Any] = field(default_factory=dict)
    outputs: Dict[str, Any] = field(default_factory=dict)
    parameters: Dict[str, Any] = field(default_factory=dict)
    position: tuple = (0, 0)

class NodeProcessor:
    """Processes node calculations and data flow"""

    def __init__(self):
        self.nodes: Dict[str, NodeData] = {}
        self.connections: List[tuple] = []  # (output_node, output_attr, input_node, input_attr)
        self.execution_order: List[str] = []

    def add_node(self, node_data: NodeData):
        """Add a node to the processor"""
        self.nodes[node_data.node_id] = node_data

    def add_connection(self, output_node: str, output_attr: str, input_node: str, input_attr: str):
        """Add a connection between nodes"""
        connection = (output_node, output_attr, input_node, input_attr)
        if connection not in self.connections:
            self.connections.append(connection)

    def calculate_execution_order(self):
        """Calculate the order in which nodes should be executed"""
        # Simple topological sort
        visited = set()
        temp_visited = set()
        order = []

        def visit(node_id: str):
            if node_id in temp_visited:
                return  # Cycle detected, skip
            if node_id in visited:
                return

            temp_visited.add(node_id)

            # Visit all nodes that depend on this node's output
            for conn in self.connections:
                if conn[0] == node_id:  # This node is the output
                    visit(conn[2])  # Visit the input node

            temp_visited.remove(node_id)
            visited.add(node_id)
            order.append(node_id)

        for node_id in self.nodes.keys():
            if node_id not in visited:
                visit(node_id)

        self.execution_order = order[::-1]  # Reverse for correct execution order

    def execute_nodes(self):
        """Execute all nodes in the correct order"""
        self.calculate_execution_order()

        for node_id in self.execution_order:
            node = self.nodes[node_id]
            try:
                self._execute_node(node)
            except Exception as e:
                logger.error(f"Error executing node {node_id}: {e}")

    def _execute_node(self, node: NodeData):
        """Execute a single node based on its type"""
        # First, collect inputs from connected nodes
        for conn in self.connections:
            if conn[2] == node.node_id:  # This node is the input
                output_node = self.nodes[conn[0]]
                if conn[1] in output_node.outputs:
                    node.inputs[conn[3]] = output_node.outputs[conn[1]]

        # Execute based on node type
        if node.node_type == "data_source":
            self._execute_data_source(node)
        elif node.node_type == "sma":
            self._execute_sma(node)
        elif node.node_type == "rsi":
            self._execute_rsi(node)
        elif node.node_type == "macd":
            self._execute_macd(node)
        elif node.node_type == "bollinger":
            self._execute_bollinger(node)
        elif node.node_type == "signal":
            self._execute_signal(node)
        elif node.node_type == "backtest":
            self._execute_backtest(node)

    def _execute_data_source(self, node: NodeData):
        """Execute data source node"""
        ticker = node.parameters.get('ticker', 'AAPL')
        period = node.parameters.get('period', '1y')

        try:
            data = yf.download(ticker, period=period)
            node.outputs['data'] = data
            node.outputs['close'] = data['Close']
            node.outputs['high'] = data['High']
            node.outputs['low'] = data['Low']
            node.outputs['volume'] = data['Volume']
        except Exception as e:
            logger.error(f"Error fetching data for {ticker}: {e}")

    def _execute_sma(self, node: NodeData):
        """Execute Simple Moving Average node"""
        if 'price' in node.inputs:
            window = node.parameters.get('window', 20)
            sma = node.inputs['price'].rolling(window=window).mean()
            node.outputs['sma'] = sma

    def _execute_rsi(self, node: NodeData):
        """Execute RSI node"""
        if 'price' in node.inputs:
            window = node.parameters.get('window', 14)
            rsi = ta.momentum.rsi(node.inputs['price'], window=window)
            node.outputs['rsi'] = rsi

    def _execute_macd(self, node: NodeData):
        """Execute MACD node"""
        if 'price' in node.inputs:
            fast = node.parameters.get('fast', 12)
            slow = node.parameters.get('slow', 26)
            signal = node.parameters.get('signal', 9)

            macd_line = ta.trend.macd(node.inputs['price'], window_fast=fast, window_slow=slow)
            macd_signal = ta.trend.macd_signal(node.inputs['price'], window_fast=fast, window_slow=slow, window_sign=signal)
            macd_histogram = ta.trend.macd_diff(node.inputs['price'], window_fast=fast, window_slow=slow, window_sign=signal)

            node.outputs['macd'] = macd_line
            node.outputs['signal'] = macd_signal
            node.outputs['histogram'] = macd_histogram

    def _execute_bollinger(self, node: NodeData):
        """Execute Bollinger Bands node"""
        if 'price' in node.inputs:
            window = node.parameters.get('window', 20)
            std = node.parameters.get('std', 2)

            upper = ta.volatility.bollinger_hband(node.inputs['price'], window=window, window_dev=std)
            lower = ta.volatility.bollinger_lband(node.inputs['price'], window=window, window_dev=std)
            middle = ta.volatility.bollinger_mavg(node.inputs['price'], window=window)

            node.outputs['upper'] = upper
            node.outputs['lower'] = lower
            node.outputs['middle'] = middle

    def _execute_signal(self, node: NodeData):
        """Execute signal generation node"""
        signal_type = node.parameters.get('type', 'crossover')

        if signal_type == 'crossover' and 'fast' in node.inputs and 'slow' in node.inputs:
            fast = node.inputs['fast']
            slow = node.inputs['slow']
            buy_signals = (fast > slow) & (fast.shift(1) <= slow.shift(1))
            sell_signals = (fast < slow) & (fast.shift(1) >= slow.shift(1))

            node.outputs['buy_signals'] = buy_signals
            node.outputs['sell_signals'] = sell_signals

        elif signal_type == 'threshold' and 'indicator' in node.inputs:
            indicator = node.inputs['indicator']
            buy_threshold = node.parameters.get('buy_threshold', 30)
            sell_threshold = node.parameters.get('sell_threshold', 70)

            buy_signals = indicator < buy_threshold
            sell_signals = indicator > sell_threshold

            node.outputs['buy_signals'] = buy_signals
            node.outputs['sell_signals'] = sell_signals

    def _execute_backtest(self, node: NodeData):
        """Execute backtest node"""
        if 'price' in node.inputs and 'buy_signals' in node.inputs and 'sell_signals' in node.inputs:
            price = node.inputs['price']
            buy_signals = node.inputs['buy_signals']
            sell_signals = node.inputs['sell_signals']

            # Simple backtest implementation
            position = 0
            portfolio_value = []
            initial_capital = node.parameters.get('initial_capital', 10000)
            current_capital = initial_capital
            shares = 0

            for i in range(len(price)):
                if buy_signals.iloc[i] and position == 0:
                    shares = current_capital / price.iloc[i]
                    current_capital = 0
                    position = 1
                elif sell_signals.iloc[i] and position == 1:
                    current_capital = shares * price.iloc[i]
                    shares = 0
                    position = 0

                total_value = current_capital + (shares * price.iloc[i])
                portfolio_value.append(total_value)

            portfolio_series = pd.Series(portfolio_value, index=price.index)
            total_return = (portfolio_series.iloc[-1] - initial_capital) / initial_capital * 100

            node.outputs['portfolio_value'] = portfolio_series
            node.outputs['total_return'] = total_return

class NodeEditorTab(BaseTab):
    """Node Editor Tab for Technical Analysis"""

    def __init__(self, app):
        super().__init__(app)
        self.node_processor = NodeProcessor()
        self.node_counter = 0
        self.selected_node = None

    def get_label(self):
        return "Node Editor"

    def create_content(self):
        """Create node editor content"""
        self.add_section_header("🔗 Technical Analysis Node Editor")

        # Create main layout
        with dpg.group(horizontal=True):
            # Left panel - Node palette
            self.create_node_palette()

            # Center - Node editor
            self.create_node_editor()

            # Right panel - Properties
            self.create_properties_panel()

        # Bottom panel - Controls and results
        self.create_control_panel()

    def create_node_palette(self):
        """Create node palette with available nodes"""
        with dpg.child_window(label="Node Palette", width=200, height=600, border=True):
            dpg.add_text("📊 Data Sources", color=[100, 255, 100])
            dpg.add_button(label="Stock Data", callback=lambda: self.add_node("data_source"), width=-1)

            dpg.add_separator()
            dpg.add_text("📈 Technical Indicators", color=[255, 255, 100])
            dpg.add_button(label="Simple MA", callback=lambda: self.add_node("sma"), width=-1)
            dpg.add_button(label="RSI", callback=lambda: self.add_node("rsi"), width=-1)
            dpg.add_button(label="MACD", callback=lambda: self.add_node("macd"), width=-1)
            dpg.add_button(label="Bollinger Bands", callback=lambda: self.add_node("bollinger"), width=-1)

            dpg.add_separator()
            dpg.add_text("⚡ Signal Generation", color=[255, 150, 100])
            dpg.add_button(label="Signal Generator", callback=lambda: self.add_node("signal"), width=-1)

            dpg.add_separator()
            dpg.add_text("🎯 Analysis", color=[150, 150, 255])
            dpg.add_button(label="Backtest", callback=lambda: self.add_node("backtest"), width=-1)

            dpg.add_separator()
            dpg.add_text("🔧 Actions", color=[255, 150, 255])
            dpg.add_button(label="Clear All", callback=self.clear_all_nodes, width=-1)
            dpg.add_button(label="Execute", callback=self.execute_graph, width=-1)

    def create_node_editor(self):
        """Create the main node editor area"""
        with dpg.child_window(label="Node Editor", width=600, height=600, border=True):
            # Node editor
            with dpg.node_editor(tag="node_editor", callback=self.node_editor_callback,
                               delink_callback=self.delink_callback):
                pass

    def create_properties_panel(self):
        """Create properties panel for selected node"""
        with dpg.child_window(label="Properties", width=250, height=600, border=True):
            dpg.add_text("Node Properties", color=[200, 200, 200])
            dpg.add_separator()

            with dpg.group(tag="properties_content"):
                dpg.add_text("Select a node to view properties")

    def create_control_panel(self):
        """Create control panel with execution controls and results"""
        dpg.add_separator()

        with dpg.group(horizontal=True):
            dpg.add_button(label="🚀 Execute Strategy", callback=self.execute_graph)
            dpg.add_button(label="💾 Save Strategy", callback=self.save_strategy)
            dpg.add_button(label="📁 Load Strategy", callback=self.load_strategy)
            dpg.add_button(label="🗑️ Clear All", callback=self.clear_all_nodes)

        # Results display
        with dpg.child_window(label="Results", height=200, border=True):
            dpg.add_text("Execution Results", color=[100, 255, 100])
            dpg.add_separator()
            with dpg.group(tag="results_content"):
                dpg.add_text("Execute the strategy to see results...")

    def add_node(self, node_type: str):
        """Add a new node to the editor"""
        self.node_counter += 1
        node_id = f"{node_type}_{self.node_counter}"

        # Create node data
        node_data = NodeData(
            node_id=node_id,
            node_type=node_type,
            position=(100 + self.node_counter * 20, 100 + self.node_counter * 20)
        )

        # Add to processor
        self.node_processor.add_node(node_data)

        # Create visual node
        with dpg.node(label=self.get_node_label(node_type), parent="node_editor", tag=node_id, pos=node_data.position):

            # Create inputs and outputs based on node type
            if node_type == "data_source":
                with dpg.node_attribute(label="Ticker", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_text(tag=f"{node_id}_ticker", default_value="AAPL", width=80)
                with dpg.node_attribute(label="Period", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_combo(["1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y"],
                                tag=f"{node_id}_period", default_value="1y", width=80)

                with dpg.node_attribute(label="Data", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_data_out"):
                    dpg.add_text("📊 Full Data")
                with dpg.node_attribute(label="Close", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_close_out"):
                    dpg.add_text("💰 Close Price")
                with dpg.node_attribute(label="High", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_high_out"):
                    dpg.add_text("📈 High Price")
                with dpg.node_attribute(label="Low", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_low_out"):
                    dpg.add_text("📉 Low Price")

            elif node_type == "sma":
                with dpg.node_attribute(label="Price", attribute_type=dpg.mvNode_Attr_Input, tag=f"{node_id}_price_in"):
                    dpg.add_text("💰 Price")
                with dpg.node_attribute(label="Window", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_int(tag=f"{node_id}_window", default_value=20, width=60, min_value=1, max_value=200)
                with dpg.node_attribute(label="SMA", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_sma_out"):
                    dpg.add_text("📊 SMA")

            elif node_type == "rsi":
                with dpg.node_attribute(label="Price", attribute_type=dpg.mvNode_Attr_Input, tag=f"{node_id}_price_in"):
                    dpg.add_text("💰 Price")
                with dpg.node_attribute(label="Window", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_int(tag=f"{node_id}_window", default_value=14, width=60, min_value=1, max_value=100)
                with dpg.node_attribute(label="RSI", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_rsi_out"):
                    dpg.add_text("⚡ RSI")

            elif node_type == "macd":
                with dpg.node_attribute(label="Price", attribute_type=dpg.mvNode_Attr_Input, tag=f"{node_id}_price_in"):
                    dpg.add_text("💰 Price")
                with dpg.node_attribute(label="Fast", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_int(tag=f"{node_id}_fast", default_value=12, width=50)
                with dpg.node_attribute(label="Slow", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_int(tag=f"{node_id}_slow", default_value=26, width=50)
                with dpg.node_attribute(label="Signal", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_int(tag=f"{node_id}_signal", default_value=9, width=50)

                with dpg.node_attribute(label="MACD", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_macd_out"):
                    dpg.add_text("📈 MACD")
                with dpg.node_attribute(label="Signal", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_signal_out"):
                    dpg.add_text("📊 Signal")
                with dpg.node_attribute(label="Histogram", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_histogram_out"):
                    dpg.add_text("📊 Histogram")

            elif node_type == "bollinger":
                with dpg.node_attribute(label="Price", attribute_type=dpg.mvNode_Attr_Input, tag=f"{node_id}_price_in"):
                    dpg.add_text("💰 Price")
                with dpg.node_attribute(label="Window", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_int(tag=f"{node_id}_window", default_value=20, width=50)
                with dpg.node_attribute(label="Std Dev", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_float(tag=f"{node_id}_std", default_value=2.0, width=50)

                with dpg.node_attribute(label="Upper", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_upper_out"):
                    dpg.add_text("📈 Upper")
                with dpg.node_attribute(label="Lower", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_lower_out"):
                    dpg.add_text("📉 Lower")
                with dpg.node_attribute(label="Middle", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_middle_out"):
                    dpg.add_text("📊 Middle")

            elif node_type == "signal":
                with dpg.node_attribute(label="Type", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_combo(["crossover", "threshold"], tag=f"{node_id}_type", default_value="crossover", width=100)

                with dpg.node_attribute(label="Fast/Indicator", attribute_type=dpg.mvNode_Attr_Input, tag=f"{node_id}_fast_in"):
                    dpg.add_text("⚡ Fast/Indicator")
                with dpg.node_attribute(label="Slow", attribute_type=dpg.mvNode_Attr_Input, tag=f"{node_id}_slow_in"):
                    dpg.add_text("🐌 Slow")

                with dpg.node_attribute(label="Buy Threshold", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_float(tag=f"{node_id}_buy_threshold", default_value=30.0, width=70)
                with dpg.node_attribute(label="Sell Threshold", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_float(tag=f"{node_id}_sell_threshold", default_value=70.0, width=70)

                with dpg.node_attribute(label="Buy Signals", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_buy_out"):
                    dpg.add_text("🟢 Buy")
                with dpg.node_attribute(label="Sell Signals", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_sell_out"):
                    dpg.add_text("🔴 Sell")

            elif node_type == "backtest":
                with dpg.node_attribute(label="Price", attribute_type=dpg.mvNode_Attr_Input, tag=f"{node_id}_price_in"):
                    dpg.add_text("💰 Price")
                with dpg.node_attribute(label="Buy Signals", attribute_type=dpg.mvNode_Attr_Input, tag=f"{node_id}_buy_in"):
                    dpg.add_text("🟢 Buy")
                with dpg.node_attribute(label="Sell Signals", attribute_type=dpg.mvNode_Attr_Input, tag=f"{node_id}_sell_in"):
                    dpg.add_text("🔴 Sell")

                with dpg.node_attribute(label="Initial Capital", attribute_type=dpg.mvNode_Attr_Static):
                    dpg.add_input_float(tag=f"{node_id}_capital", default_value=10000.0, width=80)

                with dpg.node_attribute(label="Portfolio", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_portfolio_out"):
                    dpg.add_text("💼 Portfolio")
                with dpg.node_attribute(label="Return %", attribute_type=dpg.mvNode_Attr_Output, tag=f"{node_id}_return_out"):
                    dpg.add_text("📊 Return")

    def get_node_label(self, node_type: str) -> str:
        """Get display label for node type"""
        labels = {
            "data_source": "📊 Stock Data",
            "sma": "📈 Simple MA",
            "rsi": "⚡ RSI",
            "macd": "📊 MACD",
            "bollinger": "📈 Bollinger",
            "signal": "🔔 Signal Gen",
            "backtest": "🎯 Backtest"
        }
        return labels.get(node_type, node_type)

    def node_editor_callback(self, sender, app_data):
        """Handle node editor events"""
        if app_data[0] == 1:  # Node link created
            link_info = app_data[1]
            self.node_processor.add_connection(
                link_info[0].split('_')[0] + '_' + link_info[0].split('_')[1],  # output node
                link_info[0].split('_')[-2],  # output attribute
                link_info[1].split('_')[0] + '_' + link_info[1].split('_')[1],  # input node
                link_info[1].split('_')[-2]   # input attribute
            )

    def delink_callback(self, sender, app_data):
        """Handle node delink events"""
        # Remove connection from processor
        # This would need more sophisticated tracking of link IDs
        pass

    def execute_graph(self):
        """Execute the node graph"""
        try:
            # Update node parameters from GUI
            self.update_node_parameters()

            # Execute nodes
            self.node_processor.execute_nodes()

            # Display results
            self.display_results()

        except Exception as e:
            logger.error(f"Error executing graph: {e}")
            self.show_error(f"Execution error: {str(e)}")

    def update_node_parameters(self):
        """Update node parameters from GUI inputs"""
        for node_id, node in self.node_processor.nodes.items():
            if node.node_type == "data_source":
                if dpg.does_item_exist(f"{node_id}_ticker"):
                    node.parameters['ticker'] = dpg.get_value(f"{node_id}_ticker")
                if dpg.does_item_exist(f"{node_id}_period"):
                    node.parameters['period'] = dpg.get_value(f"{node_id}_period")

            elif node.node_type == "sma":
                if dpg.does_item_exist(f"{node_id}_window"):
                    node.parameters['window'] = dpg.get_value(f"{node_id}_window")

            elif node.node_type == "rsi":
                if dpg.does_item_exist(f"{node_id}_window"):
                    node.parameters['window'] = dpg.get_value(f"{node_id}_window")

            elif node.node_type == "macd":
                if dpg.does_item_exist(f"{node_id}_fast"):
                    node.parameters['fast'] = dpg.get_value(f"{node_id}_fast")
                if dpg.does_item_exist(f"{node_id}_slow"):
                    node.parameters['slow'] = dpg.get_value(f"{node_id}_slow")
                if dpg.does_item_exist(f"{node_id}_signal"):
                    node.parameters['signal'] = dpg.get_value(f"{node_id}_signal")

            elif node.node_type == "bollinger":
                if dpg.does_item_exist(f"{node_id}_window"):
                    node.parameters['window'] = dpg.get_value(f"{node_id}_window")
                if dpg.does_item_exist(f"{node_id}_std"):
                    node.parameters['std'] = dpg.get_value(f"{node_id}_std")

            elif node.node_type == "signal":
                if dpg.does_item_exist(f"{node_id}_type"):
                    node.parameters['type'] = dpg.get_value(f"{node_id}_type")
                if dpg.does_item_exist(f"{node_id}_buy_threshold"):
                    node.parameters['buy_threshold'] = dpg.get_value(f"{node_id}_buy_threshold")
                if dpg.does_item_exist(f"{node_id}_sell_threshold"):
                    node.parameters['sell_threshold'] = dpg.get_value(f"{node_id}_sell_threshold")

            elif node.node_type == "backtest":
                if dpg.does_item_exist(f"{node_id}_capital"):
                    node.parameters['initial_capital'] = dpg.get_value(f"{node_id}_capital")

    def display_results(self):
        """Display execution results"""
        dpg.delete_item("results_content", children_only=True)

        with dpg.group(parent="results_content"):
            dpg.add_text("📊 Execution Results:", color=[100, 255, 100])
            dpg.add_separator()

            # Find backtest nodes and display their results
            for node_id, node in self.node_processor.nodes.items():
                if node.node_type == "backtest" and 'total_return' in node.outputs:
                    total_return = node.outputs['total_return']
                    dpg.add_text(f"💼 {node_id}: Total Return = {total_return:.2f}%",
                               color=[255, 255, 100] if total_return > 0 else [255, 100, 100])

                    if 'portfolio_value' in node.outputs:
                        portfolio = node.outputs['portfolio_value']
                        final_value = portfolio.iloc[-1]
                        initial_value = portfolio.iloc[0]
                        dpg.add_text(f"    💰 Final Portfolio Value: ${final_value:,.2f}")
                        dpg.add_text(f"    📈 Initial Value: ${initial_value:,.2f}")

                # Display data source info
                elif node.node_type == "data_source" and 'data' in node.outputs:
                    data = node.outputs['data']
                    ticker = node.parameters.get('ticker', 'Unknown')
                    dpg.add_text(f"📊 {ticker}: {len(data)} data points loaded")
                    dpg.add_text(f"    📅 From {data.index[0].strftime('%Y-%m-%d')} to {data.index[-1].strftime('%Y-%m-%d')}")

                # Display technical indicator info
                elif node.node_type in ["sma", "rsi", "macd", "bollinger"] and node.outputs:
                    indicator_name = self.get_node_label(node.node_type)
                    dpg.add_text(f"{indicator_name} calculated successfully")

    def clear_all_nodes(self):
        """Clear all nodes from the editor"""
        # Clear visual nodes
        dpg.delete_item("node_editor", children_only=True)

        # Clear processor data
        self.node_processor.nodes.clear()
        self.node_processor.connections.clear()
        self.node_processor.execution_order.clear()

        # Reset counter
        self.node_counter = 0

        # Clear results
        dpg.delete_item("results_content", children_only=True)
        with dpg.group(parent="results_content"):
            dpg.add_text("Execute the strategy to see results...")

        logger.info("All nodes cleared")

    def save_strategy(self):
        """Save the current strategy to a file"""
        try:
            strategy_data = {
                'nodes': {},
                'connections': self.node_processor.connections,
                'counter': self.node_counter
            }

            # Save node data
            for node_id, node in self.node_processor.nodes.items():
                strategy_data['nodes'][node_id] = {
                    'node_type': node.node_type,
                    'parameters': node.parameters,
                    'position': node.position
                }

            # Save to file (in a real application, you'd use a file dialog)
            filename = f"strategy_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
            with open(filename, 'w') as f:
                json.dump(strategy_data, f, indent=2)

            logger.info(f"Strategy saved to {filename}")

        except Exception as e:
            logger.error(f"Error saving strategy: {e}")
            self.show_error(f"Save error: {str(e)}")

    def load_strategy(self):
        """Load a strategy from a file"""
        # This would typically use a file dialog
        # For now, just a placeholder
        logger.info("Load strategy feature - file dialog would open here")

    def show_error(self, message: str):
        """Show error message in results"""
        dpg.delete_item("results_content", children_only=True)
        with dpg.group(parent="results_content"):
            dpg.add_text("❌ Error:", color=[255, 100, 100])
            dpg.add_text(message, color=[255, 150, 150])

    def cleanup(self):
        """Clean up resources"""
        self.clear_all_nodes()
        logger.info("Node Editor tab cleaned up")


# Example usage and preset strategies
class PresetStrategies:
    """Predefined strategies for quick setup"""

    @staticmethod
    def create_sma_crossover_strategy(tab: NodeEditorTab):
        """Create a simple SMA crossover strategy"""
        # Clear existing nodes
        tab.clear_all_nodes()

        # Add data source
        tab.add_node("data_source")

        # Add SMA nodes
        tab.add_node("sma")  # Fast SMA
        tab.add_node("sma")  # Slow SMA

        # Add signal generator
        tab.add_node("signal")

        # Add backtest
        tab.add_node("backtest")

        # Set parameters (this would need manual connection in GUI)
        logger.info("SMA Crossover strategy template created")

    @staticmethod
    def create_rsi_strategy(tab: NodeEditorTab):
        """Create an RSI-based strategy"""
        tab.clear_all_nodes()

        tab.add_node("data_source")
        tab.add_node("rsi")
        tab.add_node("signal")
        tab.add_node("backtest")

        logger.info("RSI strategy template created")

    @staticmethod
    def create_bollinger_bands_strategy(tab: NodeEditorTab):
        """Create a Bollinger Bands strategy"""
        tab.clear_all_nodes()

        tab.add_node("data_source")
        tab.add_node("bollinger")
        tab.add_node("signal")
        tab.add_node("backtest")

        logger.info("Bollinger Bands strategy template created")


# Enhanced Node Editor Tab with preset strategies
class EnhancedNodeEditorTab(NodeEditorTab):
    """Enhanced version with preset strategies and better UI"""

    def create_content(self):
        """Create enhanced node editor content"""
        self.add_section_header("🔗 Technical Analysis Node Editor")

        # Add preset strategy buttons
        with dpg.group(horizontal=True):
            dpg.add_text("🎯 Quick Strategies:")
            dpg.add_button(label="SMA Crossover",
                         callback=lambda: PresetStrategies.create_sma_crossover_strategy(self))
            dpg.add_button(label="RSI Strategy",
                         callback=lambda: PresetStrategies.create_rsi_strategy(self))
            dpg.add_button(label="Bollinger Bands",
                         callback=lambda: PresetStrategies.create_bollinger_bands_strategy(self))

        dpg.add_separator()

        # Create main layout
        with dpg.group(horizontal=True):
            # Left panel - Enhanced node palette
            self.create_enhanced_node_palette()

            # Center - Node editor
            self.create_node_editor()

            # Right panel - Properties and help
            self.create_enhanced_properties_panel()

        # Bottom panel - Enhanced controls and results
        self.create_enhanced_control_panel()

    def create_enhanced_node_palette(self):
        """Create enhanced node palette with descriptions"""
        with dpg.child_window(label="Node Palette", width=220, height=600, border=True):
            dpg.add_text("📊 Data Sources", color=[100, 255, 100])
            dpg.add_button(label="📈 Stock Data", callback=lambda: self.add_node("data_source"), width=-1)
            dpg.add_text("Load stock price data from Yahoo Finance",
                        wrap=200, color=[150, 150, 150])

            dpg.add_separator()
            dpg.add_text("📈 Technical Indicators", color=[255, 255, 100])

            dpg.add_button(label="📊 Simple MA", callback=lambda: self.add_node("sma"), width=-1)
            dpg.add_text("Moving average over N periods", wrap=200, color=[150, 150, 150])

            dpg.add_button(label="⚡ RSI", callback=lambda: self.add_node("rsi"), width=-1)
            dpg.add_text("Relative Strength Index (0-100)", wrap=200, color=[150, 150, 150])

            dpg.add_button(label="📊 MACD", callback=lambda: self.add_node("macd"), width=-1)
            dpg.add_text("Moving Average Convergence Divergence", wrap=200, color=[150, 150, 150])

            dpg.add_button(label="📈 Bollinger Bands", callback=lambda: self.add_node("bollinger"), width=-1)
            dpg.add_text("Price channels with standard deviation", wrap=200, color=[150, 150, 150])

            dpg.add_separator()
            dpg.add_text("⚡ Signal Generation", color=[255, 150, 100])
            dpg.add_button(label="🔔 Signal Generator", callback=lambda: self.add_node("signal"), width=-1)
            dpg.add_text("Generate buy/sell signals from indicators", wrap=200, color=[150, 150, 150])

            dpg.add_separator()
            dpg.add_text("🎯 Analysis", color=[150, 150, 255])
            dpg.add_button(label="💼 Backtest", callback=lambda: self.add_node("backtest"), width=-1)
            dpg.add_text("Test strategy performance on historical data", wrap=200, color=[150, 150, 150])

            dpg.add_separator()
            dpg.add_text("🔧 Actions", color=[255, 150, 255])
            dpg.add_button(label="🗑️ Clear All", callback=self.clear_all_nodes, width=-1)
            dpg.add_button(label="🚀 Execute", callback=self.execute_graph, width=-1)

    def create_enhanced_properties_panel(self):
        """Create enhanced properties panel with help"""
        with dpg.child_window(label="Properties & Help", width=280, height=600, border=True):
            dpg.add_text("Node Properties", color=[200, 200, 200])
            dpg.add_separator()

            with dpg.group(tag="properties_content"):
                dpg.add_text("Select a node to view properties")

            dpg.add_separator()
            dpg.add_text("💡 Quick Help", color=[255, 255, 100])

            with dpg.collapsing_header(label="How to Use"):
                dpg.add_text("1. Add nodes from the palette", wrap=250)
                dpg.add_text("2. Connect outputs to inputs by dragging", wrap=250)
                dpg.add_text("3. Configure node parameters", wrap=250)
                dpg.add_text("4. Click Execute to run strategy", wrap=250)

            with dpg.collapsing_header(label="Node Types"):
                dpg.add_text("📊 Data Source: Loads stock data", wrap=250, color=[150, 150, 150])
                dpg.add_text("📈 Indicators: Calculate technical values", wrap=250, color=[150, 150, 150])
                dpg.add_text("🔔 Signals: Generate trading signals", wrap=250, color=[150, 150, 150])
                dpg.add_text("💼 Backtest: Test strategy performance", wrap=250, color=[150, 150, 150])

            with dpg.collapsing_header(label="Strategy Tips"):
                dpg.add_text("• Use multiple timeframes for better signals", wrap=250, color=[100, 255, 150])
                dpg.add_text("• Combine different indicators for confirmation", wrap=250, color=[100, 255, 150])
                dpg.add_text("• Test on different market conditions", wrap=250, color=[100, 255, 150])
                dpg.add_text("• Consider transaction costs in real trading", wrap=250, color=[100, 255, 150])

    def create_enhanced_control_panel(self):
        """Create enhanced control panel with better visualization"""
        dpg.add_separator()

        # Control buttons with better layout
        with dpg.group(horizontal=True):
            with dpg.group():
                dpg.add_text("🎮 Strategy Controls:", color=[100, 255, 100])
                with dpg.group(horizontal=True):
                    dpg.add_button(label="🚀 Execute Strategy", callback=self.execute_graph)
                    dpg.add_button(label="⏹️ Stop", callback=self.stop_execution)
                    dpg.add_button(label="🔄 Reset", callback=self.clear_all_nodes)

            dpg.add_spacer(width=20)

            with dpg.group():
                dpg.add_text("💾 Strategy Management:", color=[255, 255, 100])
                with dpg.group(horizontal=True):
                    dpg.add_button(label="💾 Save", callback=self.save_strategy)
                    dpg.add_button(label="📁 Load", callback=self.load_strategy)
                    dpg.add_button(label="📤 Export", callback=self.export_results)

        # Enhanced results display with tabs
        dpg.add_separator()

        with dpg.tab_bar():
            with dpg.tab(label="📊 Results"):
                with dpg.child_window(height=200, border=True):
                    with dpg.group(tag="results_content"):
                        dpg.add_text("Execute the strategy to see results...")

            with dpg.tab(label="📈 Performance"):
                with dpg.child_window(height=200, border=True):
                    with dpg.group(tag="performance_content"):
                        dpg.add_text("Performance metrics will appear here...")

            with dpg.tab(label="🔍 Debug"):
                with dpg.child_window(height=200, border=True):
                    with dpg.group(tag="debug_content"):
                        dpg.add_text("Debug information and logs...")

    def stop_execution(self):
        """Stop strategy execution"""
        # Placeholder for stopping execution
        logger.info("Strategy execution stopped")

    def export_results(self):
        """Export strategy results"""
        try:
            # Export results to CSV or JSON
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"strategy_results_{timestamp}.json"

            results = {
                'timestamp': timestamp,
                'nodes': len(self.node_processor.nodes),
                'connections': len(self.node_processor.connections),
                'results': {}
            }

            # Collect results from backtest nodes
            for node_id, node in self.node_processor.nodes.items():
                if node.node_type == "backtest" and node.outputs:
                    results['results'][node_id] = {
                        'total_return': node.outputs.get('total_return'),
                        'final_value': node.outputs.get('portfolio_value', pd.Series()).iloc[-1] if 'portfolio_value' in node.outputs else None
                    }

            with open(filename, 'w') as f:
                json.dump(results, f, indent=2, default=str)

            logger.info(f"Results exported to {filename}")

        except Exception as e:
            logger.error(f"Error exporting results: {e}")
            self.show_error(f"Export error: {str(e)}")

    def display_results(self):
        """Enhanced results display with performance metrics"""
        # Clear existing results
        dpg.delete_item("results_content", children_only=True)
        dpg.delete_item("performance_content", children_only=True)
        dpg.delete_item("debug_content", children_only=True)

        # Results tab
        with dpg.group(parent="results_content"):
            dpg.add_text("📊 Execution Results:", color=[100, 255, 100])
            dpg.add_separator()

            execution_time = datetime.now().strftime("%H:%M:%S")
            dpg.add_text(f"⏰ Executed at: {execution_time}", color=[150, 150, 150])

            # Display results for each node type
            backtest_results = []

            for node_id, node in self.node_processor.nodes.items():
                if node.node_type == "backtest" and 'total_return' in node.outputs:
                    total_return = node.outputs['total_return']
                    backtest_results.append((node_id, total_return, node.outputs))

                    color = [100, 255, 100] if total_return > 0 else [255, 100, 100]
                    dpg.add_text(f"💼 {node_id}:", color=[255, 255, 100])
                    dpg.add_text(f"    📈 Total Return: {total_return:.2f}%", color=color)

                    if 'portfolio_value' in node.outputs:
                        portfolio = node.outputs['portfolio_value']
                        final_value = portfolio.iloc[-1]
                        initial_value = portfolio.iloc[0]
                        dpg.add_text(f"    💰 Final Value: ${final_value:,.2f}")
                        dpg.add_text(f"    💵 Initial Value: ${initial_value:,.2f}")

                elif node.node_type == "data_source" and 'data' in node.outputs:
                    data = node.outputs['data']
                    ticker = node.parameters.get('ticker', 'Unknown')
                    dpg.add_text(f"📊 {ticker}: {len(data)} data points loaded", color=[100, 200, 255])
                    dpg.add_text(f"    📅 Period: {data.index[0].strftime('%Y-%m-%d')} to {data.index[-1].strftime('%Y-%m-%d')}")

        # Performance tab
        with dpg.group(parent="performance_content"):
            dpg.add_text("📈 Performance Analysis:", color=[100, 255, 100])
            dpg.add_separator()

            if backtest_results:
                for node_id, total_return, outputs in backtest_results:
                    dpg.add_text(f"📊 Strategy: {node_id}", color=[255, 255, 100])

                    if 'portfolio_value' in outputs:
                        portfolio = outputs['portfolio_value']

                        # Calculate additional metrics
                        returns = portfolio.pct_change().dropna()
                        volatility = returns.std() * np.sqrt(252) * 100  # Annualized
                        max_drawdown = ((portfolio / portfolio.cummax()) - 1).min() * 100

                        dpg.add_text(f"    📈 Total Return: {total_return:.2f}%")
                        dpg.add_text(f"    📊 Volatility: {volatility:.2f}%")
                        dpg.add_text(f"    📉 Max Drawdown: {max_drawdown:.2f}%")

                        if volatility != 0:
                            sharpe_ratio = (total_return / 100) / (volatility / 100)
                            dpg.add_text(f"    ⚡ Sharpe Ratio: {sharpe_ratio:.2f}")

                        dpg.add_separator()
            else:
                dpg.add_text("No backtest results to analyze")

        # Debug tab
        with dpg.group(parent="debug_content"):
            dpg.add_text("🔍 Debug Information:", color=[100, 255, 100])
            dpg.add_separator()

            dpg.add_text(f"🔗 Nodes: {len(self.node_processor.nodes)}")
            dpg.add_text(f"🔗 Connections: {len(self.node_processor.connections)}")
            dpg.add_text(f"⚡ Execution Order: {', '.join(self.node_processor.execution_order)}")

            dpg.add_separator()
            dpg.add_text("Node Details:")
            for node_id, node in self.node_processor.nodes.items():
                dpg.add_text(f"  • {node_id} ({node.node_type}): {len(node.outputs)} outputs")


# Usage example:
# Replace the original NodeEditorTab with EnhancedNodeEditorTab in your application