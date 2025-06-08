import os
import pandas as pd
from pathlib import Path

from textual.widgets import Input, Button, Static, RichLog
from textual.containers import VerticalScroll, Container, Horizontal
from textual import on, work
from textual.app import ComposeResult

class PredictorTab(Container):
    """Auto Quant Research Workflow Tab using QLib"""

    def __init__(self):
        super().__init__()
        # Column mapping for flexible CSV formats
        self.column_aliases = {
            'date': ['date', 'datetime', 'timestamp', 'time', 'trading_date'],
            'close': ['close', 'close_price', 'prev_close_price', 'closing_price', 'adj_close', 'adjusted_close', 'price'],
            'open': ['open', 'opening_price', 'open_price'],
            'high': ['high', 'high_price', 'highest'],
            'low': ['low', 'low_price', 'lowest'],
            'volume': ['volume', 'vol', 'trading_volume', 'shares_traded']
        }
        self.temp_data = {}
    
    def compose(self) -> ComposeResult:
        yield Static("📈 QLib Auto Quant Research Workflow", classes="header")
        yield Static("🔎 Enter path to CSV file:", id="search_label")
        yield Input(placeholder="Enter path to CSV file...", id="csv_path")

        with Horizontal(id="predictor_buttons_section"):
            yield Button("Validate CSV", id="validate_button", variant="default")
            yield Button("Run QLib Workflow", id="run_workflow", variant="primary")
            yield Button("Clear Results", id="clear_results", variant="warning")
        
        # Results section
        with Container(id="results_container"):
            yield Static("Results:", id="results_label", classes="hidden")
            yield RichLog(id="results_log", auto_scroll=False, highlight=True, markup=True)


    @on(Button.Pressed, "#validate_button")
    def on_validate_pressed(self, event: Button.Pressed) -> None:
        """Handle validate button press"""
        path = self.query_one("#csv_path", Input).value.strip()
        
        if not path:
            self.log_result("❌ Please enter a CSV file path", "error")
            return
        
        self.validate_csv(path)
    
    @on(Button.Pressed, "#run_workflow")
    def run_qlib_workflow(self, event):
        """Run the QLib Auto Quant Research Workflow"""
        csv_path = self.query_one("#csv_path", Input).value.strip()
        
        if not csv_path:
            self.log_result(f"Please enter a CSV file path", "error")
            return
            
        if not os.path.exists(csv_path):
            self.log_result(f"File not found: {csv_path}", "error")
            return
        
        # Show results elements
        self.query_one("#results_label").remove_class("hidden")
        self.query_one("#results_log").remove_class("hidden")
        
        # Start the workflow
        self.execute_workflow(csv_path)
    
    @on(Button.Pressed, "#clear_results")
    def clear_results(self, event):
        """Clear all results and hide elements"""
        self.query_one("#results_label").add_class("hidden")
        self.query_one("#results_log").add_class("hidden")
        self.query_one("#results_log", RichLog).clear()
    
    def execute_workflow(self, csv_path: str):
        """Execute the QLib workflow steps"""        
        try:
            # Check QLib availability first
            self.log_result("🔧 Step 1: Checking QLib installation...", "info")
            if not self.check_qlib_availability():
                return
            self.log_result("✅ QLib is available", "success")
            
            # Step 2: Validate and prepare data
            self.log_result("📊 Step 2: Validating CSV data...", "info")
            
            df = self.validate_csv(csv_path)
            if df is None:
                return
                
            self.log_result(f"✅ CSV loaded successfully", "success")
            
            # Step 3: Initialize QLib
            self.log_result("🔧 Step 3: Initializing QLib...", "info")
            self.initialize_qlib()
            self.log_result("✅ QLib initialized", "success")
            
            # Step 4: Prepare data for QLib
            self.log_result("📊 Step 4: Converting CSV data to QLib format...", "info")
            self.prepare_qlib_data(df, csv_path)
            self.log_result("✅ Data prepared for QLib", "success")
            
            # Step 5: Setup QLib components
            self.log_result("⚙️ Step 5: Setting up QLib components...", "info")
            components = self.setup_qlib_components()
            self.log_result("✅ QLib components initialized", "success")
            
            # Step 6: Train model and generate predictions
            self.log_result("🚀 Step 6: Training model and generating predictions...", "info")
            results = self.run_qlib_pipeline(components)
            self.log_result("✅ Model training and prediction completed", "success")
            
            # Step 7: Display results
            self.log_result("📈 Step 7: Processing results...", "info")
            self.display_results(results)
            self.log_result("🎉 Workflow completed successfully!", "success")
            
        except Exception as e:
            self.log_result(f"❌ Error: {str(e)}", "error")
            self.log_result("💡 Tip: Make sure QLib is properly installed with: pip install qlib", "info")
            self.log_result("💡 Tip: and required datasets are installed too.", "info")
    
    def check_qlib_availability(self) -> bool:
        """Check if QLib is properly installed and available"""
        try:
            import qlib
            from qlib.contrib.model.gbdt import LGBModel
            from qlib.contrib.data.handler import Alpha158
            from qlib.data.dataset import DatasetH
            from qlib.contrib.strategy.signal_strategy import TopkDropoutStrategy
            from qlib.contrib.evaluate import risk_analysis
            from qlib.backtest import backtest
            from qlib.config import REG_CN
            return True
        except ImportError as e:
            self.log_result("❌ QLib is not properly installed or configured", "error")
            self.log_result("📋 Missing dependencies detected:", "error")
            self.log_result(f"   - {str(e)}", "error")
            self.log_result("", "info")
            self.log_result("🔧 To install QLib, run:", "info")
            self.log_result("   pip install qlib", "info")
            self.log_result("", "info")
            self.log_result("📚 For detailed installation instructions, visit:", "info")
            self.log_result("   https://qlib.readthedocs.io/en/latest/start/installation.html", "info")
            return False
        except Exception as e:
            self.log_result(f"❌ Error checking QLib: {str(e)}", "error")
            self.log_result("💡 Please ensure QLib is properly installed and configured", "error")
            return False
    
    def find_column_mapping(self, df_columns):
        """Auto-detect and map column names to standard format"""
        mapping = {}
        df_columns_lower = [col.lower() for col in df_columns]
        
        for standard_col, aliases in self.column_aliases.items():
            found = False
            for alias in aliases:
                if alias.lower() in df_columns_lower:
                    original_col = df_columns[df_columns_lower.index(alias.lower())]
                    mapping[original_col] = standard_col
                    found = True
                    break
            
            if not found and standard_col != 'volume':  # volume is optional
                # Try partial matches for required columns
                for col in df_columns:
                    if standard_col in col.lower():
                        mapping[col] = standard_col
                        found = True
                        break
                
                if not found:
                    return None, f"Could not find column for '{standard_col}'"
        
        return mapping, None

    def validate_csv(self, file_path: str) -> pd.DataFrame:
        """Validate CSV file format with flexible column detection"""
        try:
            if not os.path.exists(file_path):
                self.log_result(f"❌ File not found: {file_path}", "error")
                return None
            
            # Read CSV
            df = pd.read_csv(file_path)
            
            if df.empty:
                self.log_result("❌ CSV file is empty", "error")
                return None
            
            # Auto-detect column mapping
            column_mapping, error = self.find_column_mapping(df.columns.tolist())
            
            if column_mapping is None:
                self.log_result(f"❌ {error}", "error")
                self.log_result(f"Available columns: {list(df.columns)}", "info")
                self.log_result("Expected columns (or similar): date, open, high, low, close", "info")
                return None
            
            # Apply column mapping
            df = df.rename(columns=column_mapping)
            
            # Check data types and ranges for required columns
            required_cols = ['open', 'high', 'low', 'close']
            for col in required_cols:
                if col in df.columns:
                    if not pd.api.types.is_numeric_dtype(df[col]):
                        self.log_result(f"❌ Column '{col}' should be numeric", "error")
                        return None
            
            # Basic OHLC validation
            if all(col in df.columns for col in required_cols):
                invalid_ohlc = (df['high'] < df['low']) | \
                              (df['open'] < 0) | (df['close'] < 0)
                if invalid_ohlc.any():
                    self.log_result("⚠️ Some OHLC data appears invalid (high < low or negative prices)", "warning")
            
            # Convert date column to datetime if exists
            if 'date' in df.columns:
                df['date'] = pd.to_datetime(df['date'])
            
            self.log_result(f"✅ CSV validation passed", "success")
            self.log_result(f"📊 Data shape: {df.shape}", "info")
            
            # Show column mapping
            self.log_result("📋 Column mapping:", "info")
            for orig, mapped in column_mapping.items():
                self.log_result(f"  '{orig}' → '{mapped}'", "info")
            
            if 'date' in df.columns:
                self.log_result(f"📅 Date range: {df['date'].min()} to {df['date'].max()}", "info")
            
            return df
            
        except Exception as e:
            self.log_result(f"❌ Error validating CSV: {str(e)}", "error")
            return None
    
    def initialize_qlib(self):
        """Initialize QLib with basic configuration"""
        try:
            import qlib
            from qlib.config import REG_CN
            
            # Create data directory if it doesn't exist
            data_dir = os.path.expanduser("~/.qlib/qlib_data/cn_data")
            os.makedirs(data_dir, exist_ok=True)
            
            # Initialize QLib
            qlib.init(region=REG_CN, provider_uri=data_dir)
            
        except Exception as e:
            raise Exception(f"QLib initialization failed: {str(e)}")
    
    def prepare_qlib_data(self, df: pd.DataFrame, csv_path: str):
        """Convert CSV data to QLib format and store it"""
        try:
            # Create a temporary instrument name from file
            instrument = Path(csv_path).stem.upper()
            
            # Prepare data in QLib format
            qlib_data = df.copy()
            if 'date' in qlib_data.columns:
                qlib_data = qlib_data.set_index('date')
            
            # Ensure columns are in the right format
            qlib_data.columns = [col.upper() for col in qlib_data.columns]
            
            # Calculate additional features if we have the required columns
            if all(col in qlib_data.columns for col in ['HIGH', 'LOW', 'CLOSE']):
                qlib_data['VWAP'] = (qlib_data['HIGH'] + qlib_data['LOW'] + qlib_data['CLOSE']) / 3
                qlib_data['RETURN'] = qlib_data['CLOSE'].pct_change()
            
            # Remove NaN values
            qlib_data = qlib_data.dropna()
            
            # Store this data temporarily
            self.temp_data = {instrument: qlib_data}
            
        except Exception as e:
            raise Exception(f"Data preparation failed: {str(e)}")
    
    def setup_qlib_components(self) -> dict:
        """Setup QLib components"""
        try:
            # Get data info from temp data
            instrument = list(self.temp_data.keys())[0]
            data = self.temp_data[instrument]
            start_date = data.index.min().strftime('%Y-%m-%d')
            end_date = data.index.max().strftime('%Y-%m-%d')
            
            # Calculate split dates
            total_days = (data.index.max() - data.index.min()).days
            train_end_days = int(total_days * 0.6)
            valid_end_days = int(total_days * 0.8)
            
            train_end = (data.index.min() + pd.Timedelta(days=train_end_days)).strftime('%Y-%m-%d')
            valid_end = (data.index.min() + pd.Timedelta(days=valid_end_days)).strftime('%Y-%m-%d')
            
            # Import QLib components
            from qlib.contrib.model.gbdt import LGBModel
            from qlib.contrib.data.handler import Alpha158
            from qlib.data.dataset import DatasetH
            from qlib.contrib.strategy.signal_strategy import TopkDropoutStrategy
            
            # Setup model with optimized hyperparameters
            model = LGBModel(
                loss="mse",
                colsample_bytree=0.8879,
                learning_rate=0.0421,
                subsample=0.8789,
                lambda_l1=205.6999,
                lambda_l2=580.9768,
                max_depth=8,
                num_leaves=210,
                num_threads=20
            )
            
            # Setup data handler
            data_handler = Alpha158(
                instruments=instrument,
                start_time=start_date,
                end_time=end_date,
                fit_start_time=start_date,
                fit_end_time=train_end
            )
            
            # Setup dataset
            dataset = DatasetH(
                handler=data_handler,
                segments={
                    "train": (start_date, train_end),
                    "valid": (train_end, valid_end),
                    "test": (valid_end, end_date)
                }
            )
            
            # Setup strategy
            strategy = TopkDropoutStrategy(
                signal="<PRED>",
                topk=50,
                n_drop=5
            )
            
            return {
                "model": model,
                "dataset": dataset,
                "strategy": strategy,
                "data_handler": data_handler,
                "dates": {
                    "start": start_date,
                    "train_end": train_end,
                    "valid_end": valid_end,
                    "end": end_date
                },
                "instrument": instrument
            }
            
        except Exception as e:
            raise Exception(f"QLib components setup failed: {str(e)}")
    
    def run_qlib_pipeline(self, components: dict) -> dict:
        """Run the complete QLib pipeline"""
        try:
            model = components["model"]
            dataset = components["dataset"]
            strategy = components["strategy"]
            
            # 1. Train the model
            model.fit(dataset)
            
            # 2. Generate predictions
            predictions = model.predict(dataset)
            
            # 3. Calculate metrics
            from qlib.contrib.evaluate import risk_analysis
            
            # Get test data for evaluation
            test_data = dataset.prepare("test", col_set="label")
            pred_data = predictions.loc[test_data.index] if len(predictions) > 0 else pd.DataFrame()
            
            # Calculate IC metrics
            if len(pred_data) > 0 and len(test_data) > 0:
                ic_data = pred_data.join(test_data, how='inner')
                if len(ic_data.columns) > 1:
                    ic_corr = ic_data.corr(method='pearson').iloc[0, 1]
                    rank_ic_corr = ic_data.corr(method='spearman').iloc[0, 1]
                else:
                    ic_corr = 0.0
                    rank_ic_corr = 0.0
            else:
                ic_corr = 0.0
                rank_ic_corr = 0.0
            
            # 4. Run backtest
            from qlib.backtest import backtest
            portfolio_metric_dict = backtest(predictions, strategy=strategy)
            analysis_result = risk_analysis(portfolio_metric_dict)
            
            # Extract metrics
            excess_return_without_cost = analysis_result.get('excess_return_without_cost', {})
            excess_return_with_cost = analysis_result.get('excess_return_with_cost', {})
            
            results = {
                "model_performance": {
                    "ic_mean": ic_corr,
                    "ic_std": ic_data.corr(method='pearson').iloc[0, 1] if len(ic_data.columns) > 1 else 0.0,
                    "rank_ic_mean": rank_ic_corr,
                    "rank_ic_std": ic_data.corr(method='spearman').iloc[0, 1] if len(ic_data.columns) > 1 else 0.0
                },
                "backtest_performance": {
                    "excess_return_without_cost": excess_return_without_cost,
                    "excess_return_with_cost": excess_return_with_cost
                },
                "components_used": {
                    "model": "LightGBM with Alpha158 features",
                    "strategy": "TopkDropoutStrategy (top 50, drop 5)",
                    "data_handler": "Alpha158 technical indicators"
                },
                "predictions": predictions,
                "dates_used": components["dates"]
            }
            
            return results
                
        except Exception as e:
            raise Exception(f"QLib pipeline execution failed: {str(e)}")
    
    def display_results(self, results: dict):
        """Display workflow results"""
        
        self.log_result("=" * 60, "info")
        self.log_result("📊 QLIB AUTO QUANT RESEARCH RESULTS", "info")
        self.log_result("=" * 60, "info")
        
        # Display date ranges used
        if "dates_used" in results:
            dates = results["dates_used"]
            self.log_result("\n📅 Data Periods:", "info")
            self.log_result("-" * 40, "info")
            self.log_result(f"Training Period:     {dates.get('start', 'N/A')} to {dates.get('train_end', 'N/A')}", "info")
            self.log_result(f"Validation Period:   {dates.get('train_end', 'N/A')} to {dates.get('valid_end', 'N/A')}", "info")
            self.log_result(f"Testing Period:      {dates.get('valid_end', 'N/A')} to {dates.get('end', 'N/A')}", "info")
        
        # Display components used
        self.log_result("\n🔧 QLib Components Used:", "info")
        self.log_result("-" * 40, "info")
        components = results["components_used"]
        self.log_result(f"Model:        {components['model']}", "info")
        self.log_result(f"Strategy:     {components['strategy']}", "info")
        self.log_result(f"Features:     {components['data_handler']}", "info")
        
        # Display model performance
        self.log_result("\n🎯 Model Performance (IC Analysis):", "info")
        self.log_result("-" * 40, "info")
        model_perf = results["model_performance"]
        self.log_result(f"IC Mean:             {model_perf['ic_mean']:.4f}", "info")
        self.log_result(f"IC Std:              {model_perf['ic_std']:.4f}", "info")
        self.log_result(f"Rank IC Mean:        {model_perf['rank_ic_mean']:.4f}", "info")
        self.log_result(f"Rank IC Std:         {model_perf['rank_ic_std']:.4f}", "info")
        
        # Interpret IC results
        ic_interpretation = self.interpret_ic_score(model_perf['ic_mean'])
        self.log_result(f"IC Interpretation:   {ic_interpretation}", "info")
        
        # Display backtest performance
        backtest_results = results["backtest_performance"]
        
        # Display excess return without cost
        self.log_result("\n🔵 Backtest Performance (Without Cost):", "info")
        self.log_result("-" * 40, "info")
        no_cost = backtest_results["excess_return_without_cost"]
        self.log_result(f"Mean Return:          {no_cost.get('mean', 0):.6f}", "info")
        self.log_result(f"Standard Deviation:   {no_cost.get('std', 0):.6f}", "info")
        self.log_result(f"Annualized Return:    {no_cost.get('annualized_return', 0):.4f} ({no_cost.get('annualized_return', 0)*100:.2f}%)", "info")
        self.log_result(f"Information Ratio:    {no_cost.get('information_ratio', 0):.4f}", "info")
        self.log_result(f"Max Drawdown:         {no_cost.get('max_drawdown', 0):.4f} ({no_cost.get('max_drawdown', 0)*100:.2f}%)", "info")
        
        # Display excess return with cost
        self.log_result("\n🔴 Backtest Performance (With Cost):", "info")
        self.log_result("-" * 40, "info")
        with_cost = backtest_results["excess_return_with_cost"]
        self.log_result(f"Mean Return:          {with_cost.get('mean', 0):.6f}", "info")
        self.log_result(f"Standard Deviation:   {with_cost.get('std', 0):.6f}", "info")
        self.log_result(f"Annualized Return:    {with_cost.get('annualized_return', 0):.4f} ({with_cost.get('annualized_return', 0)*100:.2f}%)", "info")
        self.log_result(f"Information Ratio:    {with_cost.get('information_ratio', 0):.4f}", "info")
        self.log_result(f"Max Drawdown:         {with_cost.get('max_drawdown', 0):.4f} ({with_cost.get('max_drawdown', 0)*100:.2f}%)", "info")
        
        # Display performance interpretation
        perf_interpretation = self.interpret_performance(with_cost.get('information_ratio', 0), 
                                                        with_cost.get('max_drawdown', 0))
        self.log_result(f"\nPerformance Summary:  {perf_interpretation}", "success")
        
        self.log_result("\n" + "=" * 60, "info")
        self.log_result("💡 Next Steps:", "info")
        self.log_result("• Fine-tune model hyperparameters for better IC", "info")
        self.log_result("• Experiment with different strategies (e.g., WeightStrategyBase)", "info")
        self.log_result("• Add custom features to Alpha158 handler", "info")
        self.log_result("• Run extended backtesting periods", "info")
        self.log_result("• Consider ensemble methods for improved predictions", "info")
    
    def interpret_ic_score(self, ic: float) -> str:
        """Interpret Information Coefficient score"""
        if abs(ic) >= 0.1:
            return "Excellent predictive power"
        elif abs(ic) >= 0.05:
            return "Good predictive power"
        elif abs(ic) >= 0.02:
            return "Moderate predictive power"
        else:
            return "Weak predictive power"
    
    def interpret_performance(self, ir: float, max_dd: float) -> str:
        """Interpret overall performance"""
        if ir > 1.5 and max_dd > -0.1:
            return "Strong performance with controlled risk"
        elif ir > 1.0:
            return "Good risk-adjusted returns"
        elif ir > 0.5:
            return "Moderate performance"
        else:
            return "Needs improvement"
    
    def log_result(self, message: str, level: str = "info"):
        """Log results to the RichLog widget"""
        results_log = self.query_one("#results_log", RichLog)
        
        colors = {
            "error": "red",
            "warning": "yellow",
            "success": "green",
            "info": "blue"
        }
        
        color = colors.get(level, "white")
        results_log.write(f"[{color}]{message}[/{color}]")