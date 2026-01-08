// Financial Analysis Panel
import React, { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { AlertCircle, CheckCircle, BarChart3, DollarSign, PieChart, Activity, FileText, ChevronDown, ChevronUp, Loader2 } from 'lucide-react';

interface FinancialsInput { revenue?: number; net_income?: number; total_assets?: number; operating_cash_flow?: number; }
interface FinancialDataInput { company: { ticker: string; name: string; sector?: string; country?: string }; period: { period_end: string; fiscal_year: number }; financials: FinancialsInput; }
interface AnalysisResult { success: boolean; metrics?: Record<string, any>; summary?: string; error?: string; results?: any[]; }
type AnalysisType = 'income' | 'balance' | 'cashflow' | 'comprehensive' | 'metrics';

const ANALYSIS_TYPES = [
  { type: 'income' as AnalysisType, label: 'Income', icon: <DollarSign className="w-4 h-4" /> },
  { type: 'balance' as AnalysisType, label: 'Balance', icon: <PieChart className="w-4 h-4" /> },
  { type: 'cashflow' as AnalysisType, label: 'Cash Flow', icon: <Activity className="w-4 h-4" /> },
  { type: 'comprehensive' as AnalysisType, label: 'Full', icon: <FileText className="w-4 h-4" /> },
  { type: 'metrics' as AnalysisType, label: 'Metrics', icon: <BarChart3 className="w-4 h-4" /> },
];

export const FinancialAnalysisPanel: React.FC<{ ticker?: string; companyName?: string; }> = ({ ticker = 'AAPL', companyName = 'Apple Inc.' }) => {
  const [selected, setSelected] = useState<AnalysisType>('comprehensive');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState<AnalysisResult | null>(null);
  const [expanded, setExpanded] = useState(true);

  const run = useCallback(async () => {
    setLoading(true);
    try {
      const cmds: Record<AnalysisType, string> = { income: 'analyze_income_statement', balance: 'analyze_balance_sheet', cashflow: 'analyze_cash_flow', comprehensive: 'analyze_financial_statements', metrics: 'get_financial_key_metrics' };
      const res = await invoke<string>(cmds[selected], { data: { company: { ticker, name: companyName }, period: { period_end: new Date().toISOString().split('T')[0], fiscal_year: new Date().getFullYear() }, financials: {} } });
      const p = JSON.parse(res);
      setResult({ success: p.success !== false, summary: p.summary || p.error, results: p.results, metrics: p.metrics, error: p.error });
    } catch (e) { setResult({ success: false, error: e instanceof Error ? e.message : 'Failed' }); }
    finally { setLoading(false); }
  }, [selected, ticker, companyName]);

  return (
    <div className="bg-gray-900 rounded-xl border border-gray-700/50 overflow-hidden">
      <div className="flex items-center justify-between p-4 bg-gray-800/50 cursor-pointer" onClick={() => setExpanded(!expanded)}>
        <div className="flex items-center gap-3"><BarChart3 className="w5 h-5 text-blue-400" /><h3 className="font-semibold text-white">Financial Analysis</h3></div>
        {expanded ? <ChevronUp className="w-5 h-5 text-gray-400" /> : <ChevronDown className="w5 h-5 text-gray-400" />}
      </div>
      {expanded && (
        <div className="p-4 space-y-4">
          <div className="flex flex-wrap gap-2">{ANALYSIS_TYPES.map(({ type, label, icon }) => (<button key={type} onClick={() => setSelected(type)} className={selected === type ? 'flex items-center gap-2 px-3 py-2 rounded-lg text-sm bg-blue-600 text-white' : 'flex items-center gap-2 px-3 py-2 rounded-lg text-sm bg-gray-800 text-gray-300'}>{icon}<span>{label}</span></button>))}</div>
          <button onClick={run} disabled={loading} className="w-full py-3 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-700 text-white rounded-lg flex items-center justify-center gap-2">{loading ? <><Loader2 className="w-5 h-5 animate-spin" />Analyzing...</> : <><BarChart3 className="w-5 h-5" />Run Analysis</>}</button>
          {result && (
            <div className={result.success ? "p-4 bg-green-900/20 border border-green-700/30 rounded-lg" : "p-4 bg-red-900/20 border border-red-700/30 rounded-lg"}>
              <div className="flex items-center gap-2 mb-2">
                {result.success ? <CheckCircle className="w-5 h-5 text-green-400" /> : <AlertCircle className="w-5 h-5 text-red-400" />}
                <span className={result.success ? "text-green-400 font-bold" : "text-red-400 font-bold"}>
                  {result.success ? 'Analysis Complete' : 'Analysis Failed'}
                </span>
              </div>
              {result.summary && <div className="text-gray-300 text-sm mb-2">{result.summary}</div>}
              {result.error && !result.success && <div className="text-red-400 text-sm">{result.error}</div>}
              {result.results && result.results.length > 0 && (
                <div className="mt-2 max-h-40 overflow-y-auto">
                  <div className="text-gray-500 text-xs mb-1">{result.results.length} metrics analyzed</div>
                  {result.results.slice(0,5).map((r: any, i: number) => (
                    <div key={i} className="flex justify-between text-xs py-1 border-b border-gray-700">
                      <span className="text-cyan-400">{r.metric_name}</span>
                      <span className={r.risk_level === 'low' ? 'text-green-400' : r.risk_level === 'high' ? 'text-red-400' : 'text-yellow-400'}>
                        {typeof r.value === 'number' ? r.value.toFixed(4) : r.value}
                      </span>
                    </div>
                  ))}
                </div>
              )}
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default FinancialAnalysisPanel;