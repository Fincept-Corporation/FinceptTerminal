import React, { useState } from 'react';
import { PortfolioSummary, portfolioService } from '../../../services/portfolioService';
import { BLOOMBERG_COLORS, formatCurrency, formatPercent } from './utils';
import { Shield, Target, TrendingDown, AlertTriangle, DollarSign } from 'lucide-react';

interface AdvancedAnalyticsViewProps {
  portfolioSummary: PortfolioSummary;
}

const AdvancedAnalyticsView: React.FC<AdvancedAnalyticsViewProps> = ({ portfolioSummary }) => {
  const { ORANGE, WHITE, RED, GREEN, GRAY, CYAN, YELLOW, BLUE } = BLOOMBERG_COLORS;
  const currency = portfolioSummary.portfolio.currency;

  // State for each analytics module
  const [riskMetrics, setRiskMetrics] = useState<any>(null);
  const [assetAllocation, setAssetAllocation] = useState<any>(null);
  const [retirementPlan, setRetirementPlan] = useState<any>(null);
  const [behavioralBiases, setBehavioralBiases] = useState<any>(null);
  const [loadingRisk, setLoadingRisk] = useState(false);
  const [loadingAllocation, setLoadingAllocation] = useState(false);
  const [loadingRetirement, setLoadingRetirement] = useState(false);
  const [loadingBehavioral, setLoadingBehavioral] = useState(false);

  // Input state for user-configurable parameters
  const [age, setAge] = useState(35);
  const [riskTolerance, setRiskTolerance] = useState<'conservative' | 'moderate' | 'aggressive'>('moderate');
  const [yearsToRetirement, setYearsToRetirement] = useState(30);
  const [currentAge, setCurrentAge] = useState(35);
  const [retirementAge, setRetirementAge] = useState(65);
  const [currentSavings, setCurrentSavings] = useState(100000);
  const [annualContribution, setAnnualContribution] = useState(10000);

  const loadRiskMetrics = async () => {
    setLoadingRisk(true);
    try {
      const metrics = await portfolioService.calculateRiskMetrics(portfolioSummary.portfolio.id);
      setRiskMetrics(metrics);
    } catch (error) {
      console.error('Error loading risk metrics:', error);
    } finally {
      setLoadingRisk(false);
    }
  };

  const loadAssetAllocation = async () => {
    setLoadingAllocation(true);
    try {
      const allocation = await portfolioService.generateAssetAllocation(age, riskTolerance, yearsToRetirement);
      setAssetAllocation(allocation);
    } catch (error) {
      console.error('Error loading asset allocation:', error);
    } finally {
      setLoadingAllocation(false);
    }
  };

  const loadRetirementPlan = async () => {
    setLoadingRetirement(true);
    try {
      const plan = await portfolioService.calculateRetirementPlan(currentAge, retirementAge, currentSavings, annualContribution);
      setRetirementPlan(plan);
    } catch (error) {
      console.error('Error loading retirement plan:', error);
    } finally {
      setLoadingRetirement(false);
    }
  };

  const loadBehavioralAnalysis = async () => {
    setLoadingBehavioral(true);
    try {
      const biases = await portfolioService.analyzeBehavioralBiases(portfolioSummary.portfolio.id);
      setBehavioralBiases(biases);
    } catch (error) {
      console.error('Error loading behavioral analysis:', error);
    } finally {
      setLoadingBehavioral(false);
    }
  };

  return (
    <div>
      <div style={{
        color: ORANGE,
        fontSize: '12px',
        fontWeight: 'bold',
        marginBottom: '16px'
      }}>
        ADVANCED PORTFOLIO MANAGEMENT
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
        {/* RISK METRICS PANEL */}
        <div style={{
          backgroundColor: 'rgba(255,0,0,0.05)',
          border: `1px solid ${RED}`,
          padding: '16px',
          borderRadius: '4px'
        }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '12px'
          }}>
            <div style={{ color: RED, fontSize: '11px', fontWeight: 'bold', display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Shield size={14} />
              RISK METRICS
            </div>
            <button
              onClick={loadRiskMetrics}
              disabled={loadingRisk}
              style={{
                background: loadingRisk ? GRAY : RED,
                color: 'white',
                border: 'none',
                padding: '4px 12px',
                fontSize: '9px',
                fontWeight: 'bold',
                cursor: loadingRisk ? 'not-allowed' : 'pointer'
              }}
            >
              {loadingRisk ? 'LOADING...' : 'CALCULATE'}
            </button>
          </div>

          {riskMetrics ? (
            <div style={{ display: 'grid', gap: '8px' }}>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                <div>
                  <div style={{ color: GRAY, fontSize: '8px' }}>VaR (95%)</div>
                  <div style={{ color: RED, fontSize: '12px', fontWeight: 'bold' }}>
                    {(riskMetrics.var_95 * 100).toFixed(2)}%
                  </div>
                </div>
                <div>
                  <div style={{ color: GRAY, fontSize: '8px' }}>CVaR (95%)</div>
                  <div style={{ color: RED, fontSize: '12px', fontWeight: 'bold' }}>
                    {(riskMetrics.cvar_95 * 100).toFixed(2)}%
                  </div>
                </div>
                <div>
                  <div style={{ color: GRAY, fontSize: '8px' }}>VOLATILITY</div>
                  <div style={{ color: YELLOW, fontSize: '12px', fontWeight: 'bold' }}>
                    {(riskMetrics.volatility * 100).toFixed(2)}%
                  </div>
                </div>
                <div>
                  <div style={{ color: GRAY, fontSize: '8px' }}>DOWNSIDE VOL</div>
                  <div style={{ color: ORANGE, fontSize: '12px', fontWeight: 'bold' }}>
                    {(riskMetrics.downside_volatility * 100).toFixed(2)}%
                  </div>
                </div>
                <div>
                  <div style={{ color: GRAY, fontSize: '8px' }}>MAX DRAWDOWN</div>
                  <div style={{ color: RED, fontSize: '12px', fontWeight: 'bold' }}>
                    {(riskMetrics.max_drawdown * 100).toFixed(2)}%
                  </div>
                </div>
                <div>
                  <div style={{ color: GRAY, fontSize: '8px' }}>SKEWNESS</div>
                  <div style={{ color: CYAN, fontSize: '12px', fontWeight: 'bold' }}>
                    {riskMetrics.skewness.toFixed(2)}
                  </div>
                </div>
              </div>
              <div style={{ marginTop: '8px', fontSize: '8px', color: GRAY, borderTop: `1px solid ${GRAY}`, paddingTop: '8px' }}>
                VaR = Value at Risk | CVaR = Conditional VaR (Expected Shortfall)
              </div>
            </div>
          ) : (
            <div style={{ textAlign: 'center', color: GRAY, fontSize: '10px', padding: '20px' }}>
              Click CALCULATE to analyze portfolio risk
            </div>
          )}
        </div>

        {/* ASSET ALLOCATION PANEL */}
        <div style={{
          backgroundColor: 'rgba(0,255,255,0.05)',
          border: `1px solid ${CYAN}`,
          padding: '16px',
          borderRadius: '4px'
        }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '12px'
          }}>
            <div style={{ color: CYAN, fontSize: '11px', fontWeight: 'bold', display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Target size={14} />
              ASSET ALLOCATION
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '8px', marginBottom: '12px' }}>
            <div>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '2px' }}>AGE</div>
              <input
                type="number"
                value={age}
                onChange={(e) => setAge(Number(e.target.value))}
                style={{
                  width: '100%',
                  background: 'rgba(0,0,0,0.3)',
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '4px',
                  fontSize: '10px'
                }}
              />
            </div>
            <div>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '2px' }}>RISK</div>
              <select
                value={riskTolerance}
                onChange={(e) => setRiskTolerance(e.target.value as any)}
                style={{
                  width: '100%',
                  background: 'rgba(0,0,0,0.3)',
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '4px',
                  fontSize: '10px'
                }}
              >
                <option value="conservative">Conservative</option>
                <option value="moderate">Moderate</option>
                <option value="aggressive">Aggressive</option>
              </select>
            </div>
            <div>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '2px' }}>YEARS TO RET</div>
              <input
                type="number"
                value={yearsToRetirement}
                onChange={(e) => setYearsToRetirement(Number(e.target.value))}
                style={{
                  width: '100%',
                  background: 'rgba(0,0,0,0.3)',
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '4px',
                  fontSize: '10px'
                }}
              />
            </div>
          </div>

          <button
            onClick={loadAssetAllocation}
            disabled={loadingAllocation}
            style={{
              width: '100%',
              background: loadingAllocation ? GRAY : CYAN,
              color: 'black',
              border: 'none',
              padding: '6px',
              fontSize: '9px',
              fontWeight: 'bold',
              cursor: loadingAllocation ? 'not-allowed' : 'pointer',
              marginBottom: '12px'
            }}
          >
            {loadingAllocation ? 'GENERATING...' : 'GENERATE ALLOCATION PLAN'}
          </button>

          {assetAllocation && (
            <div style={{ fontSize: '9px' }}>
              <div style={{ color: GREEN, fontWeight: 'bold', marginBottom: '8px' }}>
                RECOMMENDED ALLOCATION
              </div>
              <div style={{ display: 'grid', gap: '4px' }}>
                <div style={{ color: GRAY }}>
                  Total Equity: <span style={{ color: GREEN, fontWeight: 'bold' }}>{assetAllocation.summary.total_equity.toFixed(1)}%</span>
                </div>
                <div style={{ color: GRAY }}>
                  Total Fixed Income: <span style={{ color: YELLOW, fontWeight: 'bold' }}>{assetAllocation.summary.total_fixed_income.toFixed(1)}%</span>
                </div>
                <div style={{ fontSize: '8px', color: GRAY, marginTop: '4px', paddingTop: '4px', borderTop: `1px solid ${GRAY}` }}>
                  Equity: {assetAllocation.equities.domestic.toFixed(1)}% Domestic, {assetAllocation.equities.international.toFixed(1)}% Intl
                </div>
              </div>
            </div>
          )}
        </div>

        {/* RETIREMENT PLANNING PANEL */}
        <div style={{
          backgroundColor: 'rgba(0,200,0,0.05)',
          border: `1px solid ${GREEN}`,
          padding: '16px',
          borderRadius: '4px'
        }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '12px'
          }}>
            <div style={{ color: GREEN, fontSize: '11px', fontWeight: 'bold', display: 'flex', alignItems: 'center', gap: '6px' }}>
              <TrendingDown size={14} />
              RETIREMENT PLANNING
            </div>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '12px' }}>
            <div>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '2px' }}>CURRENT AGE</div>
              <input
                type="number"
                value={currentAge}
                onChange={(e) => setCurrentAge(Number(e.target.value))}
                style={{
                  width: '100%',
                  background: 'rgba(0,0,0,0.3)',
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '4px',
                  fontSize: '10px'
                }}
              />
            </div>
            <div>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '2px' }}>RETIREMENT AGE</div>
              <input
                type="number"
                value={retirementAge}
                onChange={(e) => setRetirementAge(Number(e.target.value))}
                style={{
                  width: '100%',
                  background: 'rgba(0,0,0,0.3)',
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '4px',
                  fontSize: '10px'
                }}
              />
            </div>
            <div>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '2px' }}>CURRENT SAVINGS</div>
              <input
                type="number"
                value={currentSavings}
                onChange={(e) => setCurrentSavings(Number(e.target.value))}
                style={{
                  width: '100%',
                  background: 'rgba(0,0,0,0.3)',
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '4px',
                  fontSize: '10px'
                }}
              />
            </div>
            <div>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '2px' }}>ANNUAL CONTRIB</div>
              <input
                type="number"
                value={annualContribution}
                onChange={(e) => setAnnualContribution(Number(e.target.value))}
                style={{
                  width: '100%',
                  background: 'rgba(0,0,0,0.3)',
                  border: `1px solid ${GRAY}`,
                  color: WHITE,
                  padding: '4px',
                  fontSize: '10px'
                }}
              />
            </div>
          </div>

          <button
            onClick={loadRetirementPlan}
            disabled={loadingRetirement}
            style={{
              width: '100%',
              background: loadingRetirement ? GRAY : GREEN,
              color: 'black',
              border: 'none',
              padding: '6px',
              fontSize: '9px',
              fontWeight: 'bold',
              cursor: loadingRetirement ? 'not-allowed' : 'pointer',
              marginBottom: '12px'
            }}
          >
            {loadingRetirement ? 'CALCULATING...' : 'CALCULATE RETIREMENT PLAN'}
          </button>

          {retirementPlan && (
            <div style={{ fontSize: '9px' }}>
              <div style={{ display: 'grid', gap: '4px' }}>
                <div style={{ color: GRAY }}>
                  Projected Savings: <span style={{ color: GREEN, fontWeight: 'bold' }}>
                    {formatCurrency(retirementPlan.projected_savings, currency)}
                  </span>
                </div>
                <div style={{ color: GRAY }}>
                  Annual Income (4%): <span style={{ color: YELLOW, fontWeight: 'bold' }}>
                    {formatCurrency(retirementPlan.annual_income_4pct_rule, currency)}
                  </span>
                </div>
                <div style={{ color: GRAY }}>
                  Monthly Income: <span style={{ color: CYAN, fontWeight: 'bold' }}>
                    {formatCurrency(retirementPlan.monthly_income, currency)}
                  </span>
                </div>
                <div style={{ fontSize: '8px', color: GRAY, marginTop: '4px', paddingTop: '4px', borderTop: `1px solid ${GRAY}` }}>
                  Years to retirement: {retirementPlan.years_to_retirement} | Assumes 7% return, 3% inflation
                </div>
              </div>
            </div>
          )}
        </div>

        {/* BEHAVIORAL BIASES PANEL */}
        <div style={{
          backgroundColor: 'rgba(255,165,0,0.05)',
          border: `1px solid ${ORANGE}`,
          padding: '16px',
          borderRadius: '4px'
        }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '12px'
          }}>
            <div style={{ color: ORANGE, fontSize: '11px', fontWeight: 'bold', display: 'flex', alignItems: 'center', gap: '6px' }}>
              <AlertTriangle size={14} />
              BEHAVIORAL BIASES
            </div>
            <button
              onClick={loadBehavioralAnalysis}
              disabled={loadingBehavioral}
              style={{
                background: loadingBehavioral ? GRAY : ORANGE,
                color: 'black',
                border: 'none',
                padding: '4px 12px',
                fontSize: '9px',
                fontWeight: 'bold',
                cursor: loadingBehavioral ? 'not-allowed' : 'pointer'
              }}
            >
              {loadingBehavioral ? 'ANALYZING...' : 'ANALYZE'}
            </button>
          </div>

          {behavioralBiases ? (
            <div>
              <div style={{
                fontSize: '10px',
                color: behavioralBiases.bias_count === 0 ? GREEN : YELLOW,
                fontWeight: 'bold',
                marginBottom: '8px'
              }}>
                Score: {behavioralBiases.overall_score}
              </div>

              {behavioralBiases.bias_count === 0 ? (
                <div style={{ color: GRAY, fontSize: '9px' }}>
                  No significant biases detected
                </div>
              ) : (
                <div style={{ display: 'grid', gap: '6px' }}>
                  {behavioralBiases.biases_detected.map((bias: any, idx: number) => (
                    <div
                      key={idx}
                      style={{
                        padding: '8px',
                        backgroundColor: bias.severity === 'High' ? 'rgba(255,0,0,0.1)' : 'rgba(255,165,0,0.1)',
                        border: `1px solid ${bias.severity === 'High' ? RED : ORANGE}`,
                        borderRadius: '2px'
                      }}
                    >
                      <div style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        marginBottom: '4px'
                      }}>
                        <span style={{ color: WHITE, fontSize: '9px', fontWeight: 'bold' }}>
                          {bias.bias}
                        </span>
                        <span style={{
                          color: bias.severity === 'High' ? RED : YELLOW,
                          fontSize: '8px',
                          fontWeight: 'bold'
                        }}>
                          {bias.severity}
                        </span>
                      </div>
                      <div style={{ color: GRAY, fontSize: '8px' }}>
                        {bias.description}
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          ) : (
            <div style={{ textAlign: 'center', color: GRAY, fontSize: '10px', padding: '20px' }}>
              Click ANALYZE to detect behavioral biases
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default AdvancedAnalyticsView;
