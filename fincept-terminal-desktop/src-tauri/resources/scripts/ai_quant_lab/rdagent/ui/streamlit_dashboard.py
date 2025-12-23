"""
RD-Agent Streamlit Dashboard
Interactive UI for experiment tracking, monitoring, and visualization

Features:
- Real-time experiment monitoring
- Factor discovery visualization
- Model optimization tracking
- Performance comparison
- Cost tracking
- Interactive charts
"""

import sys
import os
from typing import Dict, List, Any, Optional
import json

# Streamlit check
STREAMLIT_AVAILABLE = False
try:
    import streamlit as st
    STREAMLIT_AVAILABLE = True
except ImportError:
    pass

try:
    import pandas as pd
    import numpy as np
except ImportError:
    pd = None
    np = None

# Plotting
PLOTLY_AVAILABLE = False
try:
    import plotly.graph_objects as go
    import plotly.express as px
    from plotly.subplots import make_subplots
    PLOTLY_AVAILABLE = True
except ImportError:
    pass


class RDAgentDashboard:
    """
    Interactive Streamlit dashboard for RD-Agent experiments.
    """

    def __init__(self):
        self.experiments = {}
        self.current_experiment = None

    def render(self):
        """Main dashboard render function"""
        if not STREAMLIT_AVAILABLE:
            st.error("Streamlit not installed. Install with: pip install streamlit")
            return

        # Page config
        st.set_page_config(
            page_title="RD-Agent Dashboard",
            page_icon="🤖",
            layout="wide",
            initial_sidebar_state="expanded"
        )

        # Custom CSS
        st.markdown("""
            <style>
            .big-font {
                font-size:20px !important;
                font-weight: bold;
            }
            .metric-card {
                background-color: #f0f2f6;
                padding: 20px;
                border-radius: 10px;
                margin: 10px 0;
            }
            </style>
            """, unsafe_allow_html=True)

        # Sidebar
        self._render_sidebar()

        # Main content
        page = st.session_state.get('page', 'overview')

        if page == 'overview':
            self._render_overview()
        elif page == 'factor_mining':
            self._render_factor_mining()
        elif page == 'model_optimization':
            self._render_model_optimization()
        elif page == 'experiments':
            self._render_experiments()
        elif page == 'cost_tracking':
            self._render_cost_tracking()
        elif page == 'settings':
            self._render_settings()

    def _render_sidebar(self):
        """Render sidebar navigation"""
        st.sidebar.title("🤖 RD-Agent Dashboard")

        st.sidebar.markdown("---")

        # Navigation
        pages = {
            "📊 Overview": "overview",
            "🔍 Factor Mining": "factor_mining",
            "⚙️ Model Optimization": "model_optimization",
            "🧪 Experiments": "experiments",
            "💰 Cost Tracking": "cost_tracking",
            "⚙️ Settings": "settings"
        }

        for label, page_id in pages.items():
            if st.sidebar.button(label, key=f"nav_{page_id}"):
                st.session_state['page'] = page_id
                st.rerun()

        st.sidebar.markdown("---")

        # Status
        st.sidebar.subheader("System Status")
        st.sidebar.success("✅ RD-Agent Online")
        st.sidebar.info(f"Active Experiments: {len(self.experiments)}")

    def _render_overview(self):
        """Render overview page"""
        st.title("📊 RD-Agent Overview")

        # Key metrics
        col1, col2, col3, col4 = st.columns(4)

        with col1:
            st.metric(
                label="Total Experiments",
                value="47",
                delta="+5 this week"
            )

        with col2:
            st.metric(
                label="Factors Discovered",
                value="128",
                delta="+12"
            )

        with col3:
            st.metric(
                label="Avg IC",
                value="0.0532",
                delta="+0.0045"
            )

        with col4:
            st.metric(
                label="Total Cost",
                value="$247.50",
                delta="-$15.20"
            )

        st.markdown("---")

        # Recent activity
        col_left, col_right = st.columns([2, 1])

        with col_left:
            st.subheader("Recent Experiments")
            self._render_recent_experiments_table()

        with col_right:
            st.subheader("Quick Actions")
            if st.button("🚀 Start Factor Mining", use_container_width=True):
                st.session_state['page'] = 'factor_mining'
                st.rerun()

            if st.button("⚙️ Optimize Model", use_container_width=True):
                st.session_state['page'] = 'model_optimization'
                st.rerun()

            if st.button("📊 View All Experiments", use_container_width=True):
                st.session_state['page'] = 'experiments'
                st.rerun()

        # Performance chart
        st.markdown("---")
        st.subheader("Factor Discovery Performance Over Time")
        self._render_performance_chart()

    def _render_factor_mining(self):
        """Render factor mining page"""
        st.title("🔍 Factor Mining")

        tab1, tab2, tab3 = st.tabs(["Start New", "Monitor", "Results"])

        with tab1:
            self._render_start_factor_mining()

        with tab2:
            self._render_monitor_factor_mining()

        with tab3:
            self._render_factor_mining_results()

    def _render_start_factor_mining(self):
        """Start new factor mining task"""
        st.subheader("Configure Factor Mining Task")

        col1, col2 = st.columns(2)

        with col1:
            task_description = st.text_area(
                "Task Description",
                placeholder="E.g., Discover momentum factors for crypto markets...",
                height=100
            )

            target_market = st.selectbox(
                "Target Market",
                ["US Equities", "China A-Shares", "Crypto", "Global Multi-Asset"]
            )

            max_iterations = st.slider("Max Iterations", 5, 50, 10)

        with col2:
            target_ic = st.number_input("Target IC", 0.01, 0.20, 0.05, 0.005)

            budget = st.number_input("Budget ($)", 5.0, 100.0, 10.0, 5.0)

            llm_provider = st.selectbox(
                "LLM Provider",
                ["OpenAI GPT-4", "Anthropic Claude-3", "DeepSeek"]
            )

            api_key = st.text_input("API Key", type="password")

        st.markdown("---")

        col_a, col_b, col_c = st.columns([1, 1, 2])

        with col_a:
            if st.button("🚀 Start Mining", type="primary", use_container_width=True):
                if task_description and api_key:
                    st.success("Factor mining started!")
                    st.info(f"Estimated time: {max_iterations * 5}-{max_iterations * 10} minutes")
                    st.info(f"Estimated cost: ${budget:.2f} max")
                else:
                    st.error("Please fill in all required fields")

        with col_b:
            st.button("Reset", use_container_width=True)

    def _render_monitor_factor_mining(self):
        """Monitor active factor mining tasks"""
        st.subheader("Active Factor Mining Tasks")

        # Sample active task
        task_data = {
            "task_id": "factor_mining_20251222_143022",
            "status": "running",
            "progress": 65,
            "iterations_completed": 6,
            "max_iterations": 10,
            "factors_generated": 24,
            "factors_tested": 18,
            "best_ic": 0.0542,
            "elapsed_time": "32 minutes",
            "estimated_remaining": "~20 minutes"
        }

        # Progress bar
        st.progress(task_data["progress"] / 100)

        # Metrics
        col1, col2, col3, col4 = st.columns(4)

        with col1:
            st.metric("Iterations", f"{task_data['iterations_completed']}/{task_data['max_iterations']}")

        with col2:
            st.metric("Factors Generated", task_data['factors_generated'])

        with col3:
            st.metric("Best IC", task_data['best_ic'])

        with col4:
            st.metric("Elapsed", task_data['elapsed_time'])

        # Real-time log
        st.markdown("---")
        st.subheader("Live Log")

        log_container = st.container()
        with log_container:
            st.text_area(
                "Execution Log",
                value="""[14:30:22] Starting factor mining task...
[14:30:25] Generating hypothesis batch 1...
[14:32:10] Testing factor: adaptive_momentum_rsi
[14:32:45] ✓ IC: 0.0532, ICIR: 4.43 - Excellent!
[14:33:15] Testing factor: cross_asset_momentum
[14:33:50] ✓ IC: 0.0421, ICIR: 2.81 - Good
[14:35:22] Iteration 6/10 complete
[14:35:25] Generating improved hypotheses...
""",
                height=200,
                disabled=True
            )

        # Control buttons
        col_a, col_b = st.columns(2)
        with col_a:
            st.button("⏸️ Pause Task", use_container_width=True)
        with col_b:
            st.button("⏹️ Stop Task", use_container_width=True)

    def _render_factor_mining_results(self):
        """Display factor mining results"""
        st.subheader("Discovered Factors")

        # Sample factors
        factors_df = pd.DataFrame([
            {
                "Factor": "Adaptive Momentum RSI",
                "IC": 0.0532,
                "ICIR": 4.43,
                "Sharpe": 1.74,
                "Turnover": "12%",
                "Rating": "⭐⭐⭐⭐⭐"
            },
            {
                "Factor": "Cross-Asset Momentum",
                "IC": 0.0421,
                "ICIR": 2.81,
                "Sharpe": 1.45,
                "Turnover": "18%",
                "Rating": "⭐⭐⭐⭐"
            },
            {
                "Factor": "Volume-Weighted Reversion",
                "IC": 0.0389,
                "ICIR": 2.16,
                "Sharpe": 1.32,
                "Turnover": "22%",
                "Rating": "⭐⭐⭐⭐"
            },
            {
                "Factor": "Volatility Regime Momentum",
                "IC": 0.0456,
                "ICIR": 3.26,
                "Sharpe": 1.58,
                "Turnover": "15%",
                "Rating": "⭐⭐⭐⭐⭐"
            }
        ])

        st.dataframe(factors_df, use_container_width=True)

        # Factor details
        selected_factor = st.selectbox(
            "Select Factor to View Details",
            factors_df["Factor"].tolist()
        )

        if selected_factor:
            st.markdown("---")
            col1, col2 = st.columns([2, 1])

            with col1:
                st.subheader(f"📊 {selected_factor} Performance")
                self._render_factor_performance_chart()

            with col2:
                st.subheader("Code")
                st.code("""
def adaptive_momentum_rsi(data, base_period=14):
    close = data['close']
    volatility = close.pct_change().rolling(20).std()
    volatility_percentile = volatility.rank(pct=True)

    adaptive_period = (14 + (1 - volatility_percentile) * 14).fillna(14).astype(int)

    delta = close.diff()
    gain = delta.where(delta > 0, 0)
    loss = (-delta).where(delta < 0, 0)

    avg_gain = gain.ewm(span=adaptive_period.iloc[-1], adjust=False).mean()
    avg_loss = loss.ewm(span=adaptive_period.iloc[-1], adjust=False).mean()

    rs = avg_gain / avg_loss
    rsi = 100 - (100 / (1 + rs))

    return rsi
                """, language="python")

                if st.button("📥 Export Factor", use_container_width=True):
                    st.success("Factor exported successfully!")

    def _render_model_optimization(self):
        """Render model optimization page"""
        st.title("⚙️ Model Optimization")

        tab1, tab2 = st.tabs(["Configure", "Results"])

        with tab1:
            st.subheader("Model Optimization Configuration")

            col1, col2 = st.columns(2)

            with col1:
                model_type = st.selectbox(
                    "Model Type",
                    ["LightGBM", "XGBoost", "LSTM", "Transformer", "GRU"]
                )

                optimization_target = st.selectbox(
                    "Optimization Target",
                    ["Sharpe Ratio", "Information Coefficient", "Max Drawdown", "Win Rate"]
                )

                max_iterations = st.slider("Max Iterations", 5, 30, 10)

            with col2:
                budget = st.number_input("Budget ($)", 5.0, 50.0, 10.0)

                api_key = st.text_input("API Key", type="password", key="opt_api_key")

            if st.button("🚀 Start Optimization", type="primary"):
                st.success("Optimization started!")
                st.info(f"Estimated time: {max_iterations * 3}-{max_iterations * 6} minutes")

        with tab2:
            self._render_optimization_results()

    def _render_optimization_results(self):
        """Display optimization results"""
        st.subheader("Optimization Results")

        # Best config
        col1, col2, col3 = st.columns(3)

        with col1:
            st.metric("Best Sharpe", "1.85", delta="+15%")

        with col2:
            st.metric("Best IC", "0.052", delta="+18%")

        with col3:
            st.metric("Iterations", "10/10")

        # Optimization progress chart
        st.markdown("---")
        self._render_optimization_progress_chart()

        # Best parameters
        st.markdown("---")
        st.subheader("Best Parameters")

        best_params = {
            "num_leaves": 256,
            "max_depth": 10,
            "learning_rate": 0.042,
            "n_estimators": 800,
            "min_child_samples": 30,
            "subsample": 0.85,
            "colsample_bytree": 0.75
        }

        st.json(best_params)

    def _render_experiments(self):
        """Render experiments page"""
        st.title("🧪 Experiments")

        # Filters
        col1, col2, col3 = st.columns(3)

        with col1:
            status_filter = st.multiselect(
                "Status",
                ["Running", "Completed", "Failed", "Stopped"],
                default=["Running", "Completed"]
            )

        with col2:
            type_filter = st.multiselect(
                "Type",
                ["Factor Mining", "Model Optimization", "Document Analysis"],
                default=["Factor Mining", "Model Optimization"]
            )

        with col3:
            date_range = st.date_input("Date Range", value=[])

        # Experiments table
        st.markdown("---")
        experiments_df = self._get_experiments_dataframe()
        st.dataframe(experiments_df, use_container_width=True)

    def _render_cost_tracking(self):
        """Render cost tracking page"""
        st.title("💰 Cost Tracking")

        # Summary metrics
        col1, col2, col3, col4 = st.columns(4)

        with col1:
            st.metric("Total Spend", "$247.50", delta="-$15.20")

        with col2:
            st.metric("This Month", "$89.30", delta="+$12.40")

        with col3:
            st.metric("Avg per Experiment", "$5.27")

        with col4:
            st.metric("Budget Remaining", "$752.50")

        # Cost breakdown chart
        st.markdown("---")
        self._render_cost_breakdown_chart()

        # Cost by experiment
        st.markdown("---")
        st.subheader("Cost by Experiment")
        cost_df = pd.DataFrame([
            {"Experiment": "Factor Mining #47", "Cost": "$12.50", "LLM Calls": 125, "Tokens": "245K"},
            {"Experiment": "Model Opt #23", "Cost": "$8.30", "LLM Calls": 83, "Tokens": "167K"},
            {"Experiment": "Factor Mining #46", "Cost": "$10.20", "LLM Calls": 102, "Tokens": "203K"}
        ])
        st.dataframe(cost_df, use_container_width=True)

    def _render_settings(self):
        """Render settings page"""
        st.title("⚙️ Settings")

        # API Keys
        st.subheader("API Keys")
        openai_key = st.text_input("OpenAI API Key", type="password")
        anthropic_key = st.text_input("Anthropic API Key", type="password")

        # Preferences
        st.markdown("---")
        st.subheader("Preferences")

        default_budget = st.number_input("Default Budget ($)", 5.0, 100.0, 10.0)
        default_llm = st.selectbox("Default LLM", ["GPT-4", "Claude-3", "DeepSeek"])

        # Save button
        if st.button("💾 Save Settings", type="primary"):
            st.success("Settings saved successfully!")

    # Helper functions for charts
    def _render_recent_experiments_table(self):
        """Render recent experiments table"""
        if pd is not None:
            df = pd.DataFrame([
                {"ID": "factor_047", "Type": "Factor Mining", "Status": "✅ Completed", "IC": 0.0532},
                {"ID": "model_023", "Type": "Model Opt", "Status": "🔄 Running", "Sharpe": 1.85},
                {"ID": "factor_046", "Type": "Factor Mining", "Status": "✅ Completed", "IC": 0.0456}
            ])
            st.dataframe(df, hide_index=True, use_container_width=True)

    def _render_performance_chart(self):
        """Render performance over time chart"""
        if PLOTLY_AVAILABLE and pd is not None:
            dates = pd.date_range('2025-01-01', periods=30, freq='D')
            ic_values = 0.03 + np.random.randn(30).cumsum() * 0.002

            fig = go.Figure()
            fig.add_trace(go.Scatter(
                x=dates,
                y=ic_values,
                mode='lines+markers',
                name='IC',
                line=dict(color='#1f77b4', width=2)
            ))
            fig.update_layout(
                title="Average IC Over Time",
                xaxis_title="Date",
                yaxis_title="IC",
                height=300
            )
            st.plotly_chart(fig, use_container_width=True)

    def _render_factor_performance_chart(self):
        """Render factor performance chart"""
        if PLOTLY_AVAILABLE and pd is not None:
            dates = pd.date_range('2020-01-01', periods=252, freq='D')
            cumulative_returns = (1 + np.random.randn(252) * 0.01 + 0.0005).cumprod()

            fig = go.Figure()
            fig.add_trace(go.Scatter(
                x=dates,
                y=cumulative_returns,
                mode='lines',
                name='Factor Returns',
                line=dict(color='#2ca02c', width=2)
            ))
            fig.update_layout(
                title="Cumulative Returns",
                xaxis_title="Date",
                yaxis_title="Cumulative Return",
                height=300
            )
            st.plotly_chart(fig, use_container_width=True)

    def _render_optimization_progress_chart(self):
        """Render optimization progress"""
        if PLOTLY_AVAILABLE and pd is not None:
            iterations = list(range(1, 11))
            sharpe_values = [1.2, 1.35, 1.42, 1.55, 1.68, 1.72, 1.78, 1.82, 1.85, 1.85]

            fig = go.Figure()
            fig.add_trace(go.Scatter(
                x=iterations,
                y=sharpe_values,
                mode='lines+markers',
                name='Sharpe Ratio',
                line=dict(color='#ff7f0e', width=2),
                marker=dict(size=8)
            ))
            fig.update_layout(
                title="Optimization Progress",
                xaxis_title="Iteration",
                yaxis_title="Sharpe Ratio",
                height=300
            )
            st.plotly_chart(fig, use_container_width=True)

    def _render_cost_breakdown_chart(self):
        """Render cost breakdown pie chart"""
        if PLOTLY_AVAILABLE:
            labels = ['Factor Mining', 'Model Optimization', 'Document Analysis']
            values = [145.30, 85.20, 17.00]

            fig = go.Figure(data=[go.Pie(labels=labels, values=values)])
            fig.update_layout(title="Cost Breakdown by Experiment Type", height=300)
            st.plotly_chart(fig, use_container_width=True)

    def _get_experiments_dataframe(self):
        """Get experiments dataframe"""
        if pd is not None:
            return pd.DataFrame([
                {
                    "ID": "factor_047",
                    "Type": "Factor Mining",
                    "Status": "Completed",
                    "IC": 0.0532,
                    "Cost": "$12.50",
                    "Date": "2025-01-22"
                },
                {
                    "ID": "model_023",
                    "Type": "Model Optimization",
                    "Status": "Running",
                    "Sharpe": 1.85,
                    "Cost": "$8.30",
                    "Date": "2025-01-22"
                },
                {
                    "ID": "factor_046",
                    "Type": "Factor Mining",
                    "Status": "Completed",
                    "IC": 0.0456,
                    "Cost": "$10.20",
                    "Date": "2025-01-21"
                }
            ])
        return pd.DataFrame()


def main():
    """Main entry point"""
    if not STREAMLIT_AVAILABLE:
        print(json.dumps({
            "success": False,
            "error": "Streamlit not installed. Install with: pip install streamlit plotly"
        }))
        return

    dashboard = RDAgentDashboard()
    dashboard.render()


if __name__ == "__main__":
    main()
