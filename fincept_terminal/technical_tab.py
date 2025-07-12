import dearpygui.dearpygui as dpg
import pandas as pd
import threading
import datetime
from base_tab import BaseTab

try:
    import yfinance as yf
    YFINANCE_AVAILABLE = True
except ImportError:
    YFINANCE_AVAILABLE = False

class TechnicalAnalysisTab(BaseTab):
    def __init__(self, app):
        super().__init__(app)
        self.tag_prefix = "ta_"
        self.market_data = None
        self.current_symbol = "AAPL"
        self.node_counter = 1
        self.nodes = {}
        self.connections = {}
        self.link_ids = set()
        self.signals = None
        self.backtest_results = None
        self.node_editor_tag = f"{self.tag_prefix}editor"

    def get_label(self):
        return "📈 Technical Analysis"

    def create_content(self):
        if not YFINANCE_AVAILABLE:
            dpg.add_text("❌ yfinance not available!")
            return

        with dpg.group(horizontal=True):
            dpg.add_input_text(label="Symbol", default_value=self.current_symbol,
                               tag=f"{self.tag_prefix}symbol", width=100)
            dpg.add_button(label="📊 Fetch Data", callback=self.fetch_data)
            dpg.add_text("No Data", tag=f"{self.tag_prefix}status", color=(255, 100, 100))

        dpg.add_separator()

        with dpg.group(horizontal=True):
            dpg.add_button(label="🔢 Data", callback=self.add_data_node)
            dpg.add_button(label="📊 SMA", callback=self.add_sma_node)
            dpg.add_button(label="📈 EMA", callback=self.add_ema_node)
            dpg.add_button(label="⚡ Cross", callback=self.add_cross_node)
            dpg.add_button(label="🚀 Execute", callback=self.execute_strategy)
            dpg.add_button(label="🧹 Clear", callback=self.clear_nodes)

        with dpg.child_window(height=400):
            with dpg.node_editor(tag=self.node_editor_tag,
                                 callback=self.on_link,
                                 delink_callback=self.on_delink):
                pass

        # Handler for Delete key to remove selected nodes and links
        with dpg.handler_registry():
            dpg.add_key_press_handler(key=dpg.mvKey_Delete, callback=self.on_delete_selected)

        dpg.add_separator()
        with dpg.group(horizontal=True):
            dpg.add_text("Trades: 0", tag=f"{self.tag_prefix}trades")
            dpg.add_text("Win Rate: 0%", tag=f"{self.tag_prefix}winrate")
            dpg.add_text("Return: 0%", tag=f"{self.tag_prefix}return")

        with dpg.child_window(height=100):
            dpg.add_text("Ready...", tag=f"{self.tag_prefix}log")

    def fetch_data(self):
        def fetch():
            try:
                symbol = dpg.get_value(f"{self.tag_prefix}symbol")
                self.log(f"Fetching {symbol}...")

                ticker = yf.Ticker(symbol)
                data = ticker.history(period="1y", interval="1d")

                if data.empty:
                    self.log("❌ No data found")
                    return

                data.columns = [col.lower() for col in data.columns]
                self.market_data = data
                self.current_symbol = symbol

                dpg.set_value(f"{self.tag_prefix}status", f"✅ {len(data)} records")
                dpg.configure_item(f"{self.tag_prefix}status", color=(100, 255, 100))
                self.log(f"✅ Loaded {len(data)} records")

                for node_id, node_info in self.nodes.items():
                    if node_info["type"] == "data":
                        node_info["data"] = data['close']
                        node_info["calculated"] = True
                        self.invalidate_downstream(node_id)

            except Exception as e:
                self.log(f"❌ Error: {str(e)}")

        threading.Thread(target=fetch, daemon=True).start()

    def add_data_node(self):
        node_id = f"data_{self.node_counter}"
        self.node_counter += 1

        with dpg.node(label="🔢 Data Source", parent=self.node_editor_tag,
                      tag=node_id, pos=[50, 50]):
            with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Static):
                dpg.add_text(f"Symbol: {self.current_symbol}")
                if self.market_data is not None:
                    dpg.add_text(f"Records: {len(self.market_data)}", color=(100, 255, 100))
                else:
                    dpg.add_text("No Data", color=(255, 100, 100))

            with dpg.node_attribute(tag=f"{node_id}_out", attribute_type=dpg.mvNode_Attr_Output):
                dpg.add_text("Price →")

        self.nodes[node_id] = {
            "type": "data",
            "data": self.market_data['close'] if self.market_data is not None else None,
            "calculated": self.market_data is not None
        }
        self.log(f"Added data node: {node_id}")

    def add_sma_node(self):
        node_id = f"sma_{self.node_counter}"
        self.node_counter += 1

        with dpg.node(label="📊 SMA", parent=self.node_editor_tag,
                      tag=node_id, pos=[250, 50]):
            with dpg.node_attribute(tag=f"{node_id}_in", attribute_type=dpg.mvNode_Attr_Input):
                dpg.add_text("← Data")

            with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Static):
                dpg.add_input_int(label="Period", default_value=20, tag=f"{node_id}_period", width=80)
                dpg.add_button(label="Calculate", callback=lambda: self.calc_sma(node_id), width=100)
                dpg.add_text("Not Calculated", tag=f"{node_id}_status", color=(255, 255, 100))

            with dpg.node_attribute(tag=f"{node_id}_out", attribute_type=dpg.mvNode_Attr_Output):
                dpg.add_text("SMA →")

        self.nodes[node_id] = {
            "type": "sma",
            "data": None,
            "calculated": False
        }
        self.log(f"Added SMA node: {node_id}")

    def add_ema_node(self):
        node_id = f"ema_{self.node_counter}"
        self.node_counter += 1

        with dpg.node(label="📈 EMA", parent=self.node_editor_tag,
                      tag=node_id, pos=[250, 150]):
            with dpg.node_attribute(tag=f"{node_id}_in", attribute_type=dpg.mvNode_Attr_Input):
                dpg.add_text("← Data")

            with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Static):
                dpg.add_input_int(label="Period", default_value=12, tag=f"{node_id}_period", width=80)
                dpg.add_button(label="Calculate", callback=lambda: self.calc_ema(node_id), width=100)
                dpg.add_text("Not Calculated", tag=f"{node_id}_status", color=(255, 255, 100))

            with dpg.node_attribute(tag=f"{node_id}_out", attribute_type=dpg.mvNode_Attr_Output):
                dpg.add_text("EMA →")

        self.nodes[node_id] = {
            "type": "ema",
            "data": None,
            "calculated": False
        }
        self.log(f"Added EMA node: {node_id}")

    def add_cross_node(self):
        node_id = f"cross_{self.node_counter}"
        self.node_counter += 1

        with dpg.node(label="⚡ Crossover", parent=self.node_editor_tag,
                      tag=node_id, pos=[450, 100]):
            with dpg.node_attribute(tag=f"{node_id}_in1", attribute_type=dpg.mvNode_Attr_Input):
                dpg.add_text("← Fast")

            with dpg.node_attribute(tag=f"{node_id}_in2", attribute_type=dpg.mvNode_Attr_Input):
                dpg.add_text("← Slow")

            with dpg.node_attribute(attribute_type=dpg.mvNode_Attr_Static):
                dpg.add_button(label="Generate", callback=lambda: self.calc_crossover(node_id), width=100)
                dpg.add_text("No Signals", tag=f"{node_id}_status", color=(255, 255, 100))

            with dpg.node_attribute(tag=f"{node_id}_out", attribute_type=dpg.mvNode_Attr_Output):
                dpg.add_text("Signals →")

        self.nodes[node_id] = {
            "type": "crossover",
            "data": None,
            "calculated": False
        }
        self.log(f"Added crossover node: {node_id}")

    def on_link(self, sender, app_data):
        output_attr, input_attr = app_data

        if not isinstance(output_attr, str):
            output_attr = dpg.get_item_alias(output_attr)
        if not isinstance(input_attr, str):
            input_attr = dpg.get_item_alias(input_attr)

        output_node = output_attr.rsplit('_', 1)[0]
        input_node = input_attr.rsplit('_', 1)[0]

        if input_node not in self.connections:
            self.connections[input_node] = {}

        if input_attr.endswith('_in1'):
            self.connections[input_node]['input1'] = output_node
        elif input_attr.endswith('_in2'):
            self.connections[input_node]['input2'] = output_node
        elif input_attr.endswith('_in'):
            self.connections[input_node]['input'] = output_node

        link_id = dpg.add_node_link(output_attr, input_attr, parent=self.node_editor_tag)
        self.link_ids.add(link_id)

        self.invalidate_downstream(input_node)
        self.log(f"Connected: {output_node} → {input_node}")

    def on_delink(self, sender, app_data):
        link_id = app_data
        if dpg.does_item_exist(link_id):
            dpg.delete_item(link_id)
        if link_id in self.link_ids:
            self.link_ids.remove(link_id)
        self.log(f"Disconnected link")

    def on_delete_selected(self, sender, app_data):
        # Delete selected links
        for link in dpg.get_selected_links(self.node_editor_tag):
            dpg.delete_item(link)
            if link in self.link_ids:
                self.link_ids.remove(link)
        # Delete selected nodes
        for node in dpg.get_selected_nodes(self.node_editor_tag):
            dpg.delete_item(node)
            if node in self.nodes:
                del self.nodes[node]
            if node in self.connections:
                del self.connections[node]
        self.log("Deleted selected nodes/links")

    def invalidate_downstream(self, node_id):
        for target, conns in self.connections.items():
            for _, src in conns.items():
                if src == node_id:
                    if self.nodes[target]["calculated"]:
                        self.nodes[target]["calculated"] = False
                        try:
                            dpg.set_value(f"{target}_status", "Not Calculated")
                            dpg.configure_item(f"{target}_status", color=(255, 255, 100))
                        except Exception:
                            pass
                        self.invalidate_downstream(target)

    def ensure_input_calculated(self, input_node):
        if not self.nodes[input_node]["calculated"]:
            node_type = self.nodes[input_node]["type"]
            if node_type == "sma":
                self.calc_sma(input_node)
            elif node_type == "ema":
                self.calc_ema(input_node)
            elif node_type == "crossover":
                self.calc_crossover(input_node)

    def calc_sma(self, node_id):
        if self.market_data is None:
            self.log("❌ No data available")
            return

        try:
            if node_id in self.connections and 'input' in self.connections[node_id]:
                input_node = self.connections[node_id]['input']
                self.ensure_input_calculated(input_node)
                input_data = self.nodes[input_node]['data']
            else:
                self.log("❌ SMA node not connected to data")
                return

            period = dpg.get_value(f"{node_id}_period")
            sma = input_data.rolling(window=period).mean()

            self.nodes[node_id]['data'] = sma
            self.nodes[node_id]['calculated'] = True

            dpg.set_value(f"{node_id}_status", f"SMA({period})")
            dpg.configure_item(f"{node_id}_status", color=(100, 255, 100))

            self.log(f"✅ Calculated SMA({period})")
            self.invalidate_downstream(node_id)

        except Exception as e:
            self.log(f"❌ SMA error: {str(e)}")

    def calc_ema(self, node_id):
        if self.market_data is None:
            self.log("❌ No data available")
            return

        try:
            if node_id in self.connections and 'input' in self.connections[node_id]:
                input_node = self.connections[node_id]['input']
                self.ensure_input_calculated(input_node)
                input_data = self.nodes[input_node]['data']
            else:
                self.log("❌ EMA node not connected to data")
                return

            period = dpg.get_value(f"{node_id}_period")
            ema = input_data.ewm(span=period).mean()

            self.nodes[node_id]['data'] = ema
            self.nodes[node_id]['calculated'] = True

            dpg.set_value(f"{node_id}_status", f"EMA({period})")
            dpg.configure_item(f"{node_id}_status", color=(100, 255, 100))

            self.log(f"✅ Calculated EMA({period})")
            self.invalidate_downstream(node_id)

        except Exception as e:
            self.log(f"❌ EMA error: {str(e)}")

    def calc_crossover(self, node_id):
        try:
            if node_id not in self.connections:
                self.log("❌ Crossover node has no connections")
                return

            conn = self.connections[node_id]
            if 'input1' not in conn or 'input2' not in conn:
                self.log("❌ Crossover needs both inputs connected")
                return

            self.ensure_input_calculated(conn['input1'])
            self.ensure_input_calculated(conn['input2'])
            fast_data = self.nodes[conn['input1']]['data']
            slow_data = self.nodes[conn['input2']]['data']

            if fast_data is None or slow_data is None:
                self.log("❌ Connected nodes not calculated")
                return

            signals = pd.Series(0, index=fast_data.index, dtype=int)
            bullish = (fast_data > slow_data) & (fast_data.shift(1) <= slow_data.shift(1))
            bearish = (fast_data < slow_data) & (fast_data.shift(1) >= slow_data.shift(1))

            signals[bullish] = 1
            signals[bearish] = -1

            signal_count = len(signals[signals != 0])
            self.nodes[node_id]['data'] = signals
            self.nodes[node_id]['calculated'] = True
            self.signals = signals

            dpg.set_value(f"{node_id}_status", f"{signal_count} Signals")
            dpg.configure_item(f"{node_id}_status", color=(100, 255, 100))

            self.log(f"✅ Generated {signal_count} crossover signals")
            self.invalidate_downstream(node_id)

        except Exception as e:
            self.log(f"❌ Crossover error: {str(e)}")

    def execute_strategy(self):
        if self.signals is None:
            self.log("❌ No signals available")
            return

        try:
            self.log("🚀 Running backtest...")
            position = 0
            entry_price = 0
            trades = []

            for date, signal in self.signals.items():
                if date not in self.market_data.index:
                    continue
                current_price = self.market_data.loc[date, 'close']
                if signal == 1 and position == 0:
                    position = 1
                    entry_price = current_price
                    trades.append({'entry': date, 'entry_price': entry_price, 'exit': None, 'pnl': None})
                    self.log(f"BUY at {date.date()} ${entry_price:.2f}")
                elif signal == -1 and position == 1:
                    position = 0
                    exit_price = current_price
                    pnl_pct = (exit_price - entry_price) / entry_price * 100
                    if trades:
                        trades[-1].update({'exit': date, 'exit_price': exit_price, 'pnl': pnl_pct})
                        self.log(f"SELL at {date.date()} ${exit_price:.2f} | PnL: {pnl_pct:.2f}%")

            completed_trades = [t for t in trades if t['pnl'] is not None]
            winning_trades = [t for t in completed_trades if t['pnl'] > 0]
            total_trades = len(completed_trades)
            win_rate = (len(winning_trades) / total_trades * 100) if total_trades > 0 else 0
            total_return = sum([t['pnl'] for t in completed_trades])

            dpg.set_value(f"{self.tag_prefix}trades", f"Trades: {total_trades}")
            dpg.set_value(f"{self.tag_prefix}winrate", f"Win Rate: {win_rate:.1f}%")
            dpg.set_value(f"{self.tag_prefix}return", f"Return: {total_return:.2f}%")

            self.backtest_results = {
                'trades': completed_trades,
                'total_return': total_return,
                'win_rate': win_rate
            }

            self.log(f"✅ Backtest complete: {total_trades} trades, {win_rate:.1f}% win rate, {total_return:.2f}% return")

        except Exception as e:
            self.log(f"❌ Backtest error: {str(e)}")

    def clear_nodes(self):
        children = dpg.get_item_children(self.node_editor_tag)
        if children and len(children) > 1:
            for child in children[1]:
                if dpg.does_item_exist(child):
                    dpg.delete_item(child)
        self.nodes.clear()
        self.connections.clear()
        self.signals = None
        self.backtest_results = None
        self.link_ids.clear()
        self.node_counter = 1
        dpg.set_value(f"{self.tag_prefix}trades", "Trades: 0")
        dpg.set_value(f"{self.tag_prefix}winrate", "Win Rate: 0%")
        dpg.set_value(f"{self.tag_prefix}return", "Return: 0%")
        self.log("🧹 Cleared all nodes")

    def log(self, message):
        try:
            timestamp = datetime.datetime.now().strftime("%H:%M:%S")
            current = dpg.get_value(f"{self.tag_prefix}log")
            new_log = f"[{timestamp}] {message}\n{current}"
            lines = new_log.split('\n')[:10]
            dpg.set_value(f"{self.tag_prefix}log", '\n'.join(lines))
        except Exception:
            pass

    def cleanup(self):
        try:
            self.market_data = None
            self.nodes.clear()
            self.connections.clear()
            self.signals = None
            self.backtest_results = None
            self.link_ids.clear()
        except Exception:
            pass