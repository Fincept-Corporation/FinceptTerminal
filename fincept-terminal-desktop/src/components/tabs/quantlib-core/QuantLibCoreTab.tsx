// File: src/components/tabs/quantlib-core/QuantLibCoreTab.tsx
// Unified QuantLib tab — single sidebar with all modules, auth-aware

import React, { useState, useEffect } from 'react';
import {
  Hash, Calendar, Zap, Activity, Sigma, Calculator, Layers, Clock,
  Shield, CreditCard, FileCheck, Droplets, AlertTriangle,
  BarChart3, Waves, Focus,
  TrendingUp, GitBranch, ChevronDown, ChevronRight,
  Shuffle, Target, Cpu, Gauge, Binary,
  Brain, PieChart, Grid3X3, Filter, LineChart, CheckCircle, Timer, Network, Boxes,
  BarChart2, Dice1, Milestone,
  Briefcase, Scale, ShieldCheck, CalendarDays, CalendarClock, Ruler,
  Thermometer, Atom, FlaskConical,
  Spline, ArrowRightLeft, Percent, Globe2,
  DollarSign, BarChartHorizontal, Replace, Grid,
  ShieldAlert, Crosshair, Move,
  Landmark, Swords, Gavel, Heart,
  Banknote, CircleDollarSign, Coins,
  Diff, Radical, Box,
  FileText, Repeat, Store, ShieldQuestion,
  Building2, BarChart4, TrendingDown, Gem, Rocket, Scan, FlaskRound, Combine,
} from 'lucide-react';
import { F, FONT } from './shared';
import { useAuth } from '@/contexts/AuthContext';
import { setQuantLibApiKey } from './quantlibCoreApi';
import { setRegulatoryApiKey } from '../quantlib-regulatory/quantlibRegulatoryApi';
import { setVolatilityApiKey } from '../quantlib-volatility/quantlibVolatilityApi';
import { setModelsApiKey } from '../quantlib-models/quantlibModelsApi';
import { setStochasticApiKey } from '../quantlib-stochastic/quantlibStochasticApi';
import { setMlApiKey } from '../quantlib-ml/quantlibMlApi';
import { setStatisticsApiKey } from '../quantlib-statistics/quantlibStatisticsApi';
import { setPortfolioApiKey } from '../quantlib-portfolio/quantlibPortfolioApi';
import { setSchedulingApiKey } from '../quantlib-scheduling/quantlibSchedulingApi';
import { setPhysicsApiKey } from '../quantlib-physics/quantlibPhysicsApi';
import { setCurvesApiKey } from '../quantlib-curves/quantlibCurvesApi';
import { setPricingApiKey } from '../quantlib-pricing/quantlibPricingApi';
import { setRiskModApiKey } from '../quantlib-risk/quantlibRiskApi';
import { setEconomicsApiKey } from '../quantlib-economics/quantlibEconomicsApi';
import { setSolverApiKey } from '../quantlib-solver/quantlibSolverApi';
import { setNumericalApiKey } from '../quantlib-numerical/quantlibNumericalApi';
import { setInstrumentsApiKey } from '../quantlib-instruments/quantlibInstrumentsApi';
import { setAnalysisApiKey } from '../quantlib-analysis/quantlibAnalysisApi';

// Core panels
import TypesPanel from './panels/TypesPanel';
import ConventionsPanel from './panels/ConventionsPanel';
import AutoDiffPanel from './panels/AutoDiffPanel';
import DistributionsPanel from './panels/DistributionsPanel';
import MathPanel from './panels/MathPanel';
import OperationsPanel from './panels/OperationsPanel';
import LegsPanel from './panels/LegsPanel';
import PeriodsPanel from './panels/PeriodsPanel';

// Regulatory panels
import BaselPanel from '../quantlib-regulatory/panels/BaselPanel';
import SaccrPanel from '../quantlib-regulatory/panels/SaccrPanel';
import Ifrs9Panel from '../quantlib-regulatory/panels/Ifrs9Panel';
import LiquidityPanel from '../quantlib-regulatory/panels/LiquidityPanel';
import StressPanel from '../quantlib-regulatory/panels/StressPanel';

// Volatility panels
import SurfacePanel from '../quantlib-volatility/panels/SurfacePanel';
import SabrPanel from '../quantlib-volatility/panels/SabrPanel';
import LocalVolPanel from '../quantlib-volatility/panels/LocalVolPanel';

// Models panels
import ShortRatePanel from '../quantlib-models/panels/ShortRatePanel';
import HullWhitePanel from '../quantlib-models/panels/HullWhitePanel';
import HestonPanel from '../quantlib-models/panels/HestonPanel';
import JumpDiffusionPanel from '../quantlib-models/panels/JumpDiffusionPanel';
import VolModelsPanel from '../quantlib-models/panels/VolModelsPanel';

// Stochastic panels
import ProcessesPanel from '../quantlib-stochastic/panels/ProcessesPanel';
import ExactPanel from '../quantlib-stochastic/panels/ExactPanel';
import SimulationPanel from '../quantlib-stochastic/panels/SimulationPanel';
import SamplingPanel from '../quantlib-stochastic/panels/SamplingPanel';
import TheoryPanel from '../quantlib-stochastic/panels/TheoryPanel';

// ML panels
import MlCreditPanel from '../quantlib-ml/panels/CreditPanel';
import MlRegressionPanel from '../quantlib-ml/panels/RegressionPanel';
import MlClusteringPanel from '../quantlib-ml/panels/ClusteringPanel';
import MlPreprocessingPanel from '../quantlib-ml/panels/PreprocessingPanel';
import MlFeaturesPanel from '../quantlib-ml/panels/FeaturesPanel';
import MlValidationPanel from '../quantlib-ml/panels/ValidationPanel';
import MlTimeSeriesPanel from '../quantlib-ml/panels/TimeSeriesPanel';
import MlGpNnPanel from '../quantlib-ml/panels/GpNnPanel';
import MlFactorPanel from '../quantlib-ml/panels/FactorPanel';

// Statistics panels
import ContinuousDistPanel from '../quantlib-statistics/panels/ContinuousDistPanel';
import DiscreteDistPanel from '../quantlib-statistics/panels/DiscreteDistPanel';
import StatTimeSeriesPanel from '../quantlib-statistics/panels/TimeSeriesPanel';

// Portfolio panels
import OptimizePanel from '../quantlib-portfolio/panels/OptimizePanel';
import PortfolioRiskPanel from '../quantlib-portfolio/panels/RiskPanel';

// Scheduling panels
import CalendarPanel from '../quantlib-scheduling/panels/CalendarPanel';
import DayCountPanel from '../quantlib-scheduling/panels/DayCountPanel';

// Physics panels
import EntropyPanel from '../quantlib-physics/panels/EntropyPanel';
import ThermoPanel from '../quantlib-physics/panels/ThermoPanel';

// Curves panels
import CurveBuildPanel from '../quantlib-curves/panels/CurveBuildPanel';
import TransformPanel from '../quantlib-curves/panels/TransformPanel';
import FittingPanel from '../quantlib-curves/panels/FittingPanel';
import SpecializedPanel from '../quantlib-curves/panels/SpecializedPanel';

// Pricing panels
import BlackScholesPanel from '../quantlib-pricing/panels/BlackScholesPanel';
import Black76Panel from '../quantlib-pricing/panels/Black76Panel';
import BachelierPanel from '../quantlib-pricing/panels/BachelierPanel';
import NumericalPanel from '../quantlib-pricing/panels/NumericalPanel';

// Risk panels
import VaRPanel from '../quantlib-risk/panels/VaRPanel';
import EvtXvaPanel from '../quantlib-risk/panels/EvtXvaPanel';
import SensitivitiesPanel from '../quantlib-risk/panels/SensitivitiesPanel';

// Economics panels
import EquilibriumPanel from '../quantlib-economics/panels/EquilibriumPanel';
import GameTheoryPanel from '../quantlib-economics/panels/GameTheoryPanel';
import AuctionPanel from '../quantlib-economics/panels/AuctionPanel';
import UtilityPanel from '../quantlib-economics/panels/UtilityPanel';

// Solver panels
import BondPanel from '../quantlib-solver/panels/BondPanel';
import RatesPanel from '../quantlib-solver/panels/RatesPanel';
import CashflowPanel from '../quantlib-solver/panels/CashflowPanel';

// Numerical panels
import DiffFftPanel from '../quantlib-numerical/panels/DiffFftPanel';
import InterpLinalgPanel from '../quantlib-numerical/panels/InterpLinalgPanel';
import NumSolversPanel from '../quantlib-numerical/panels/SolversPanel';

// Instruments panels
import InstrBondsPanel from '../quantlib-instruments/panels/BondsPanel';
import InstrSwapsPanel from '../quantlib-instruments/panels/SwapsPanel';
import InstrMarketsPanel from '../quantlib-instruments/panels/MarketsPanel';
import InstrCreditFuturesPanel from '../quantlib-instruments/panels/CreditFuturesPanel';

// Analysis panels
import FundamentalsPanel from '../quantlib-analysis/panels/FundamentalsPanel';
import RatioProfitPanel from '../quantlib-analysis/panels/RatioProfitPanel';
import RatioEfficiencyPanel from '../quantlib-analysis/panels/RatioEfficiencyPanel';
import RatioLeverageCfPanel from '../quantlib-analysis/panels/RatioLeverageCfPanel';
import RatioValQualPanel from '../quantlib-analysis/panels/RatioValQualPanel';
import IndustryPanel from '../quantlib-analysis/panels/IndustryPanel';
import ValuationDcfPanel from '../quantlib-analysis/panels/ValuationDcfPanel';
import ValuationModelsPanel from '../quantlib-analysis/panels/ValuationModelsPanel';

// ── Section types ────────────────────────────────────────────────
type SectionId =
  // core
  | 'types' | 'conventions' | 'autodiff' | 'distributions'
  | 'math' | 'operations' | 'legs' | 'periods'
  // regulatory
  | 'basel' | 'saccr' | 'ifrs9' | 'liquidity' | 'stress'
  // volatility
  | 'surface' | 'sabr' | 'localvol'
  // models
  | 'shortrate' | 'hullwhite' | 'heston' | 'jumpdiff' | 'volmodels'
  // stochastic
  | 'processes' | 'exact' | 'simulation' | 'sampling' | 'theory'
  // ml
  | 'ml-credit' | 'ml-regression' | 'ml-clustering' | 'ml-preprocessing'
  | 'ml-features' | 'ml-validation' | 'ml-timeseries' | 'ml-gpnn' | 'ml-factor'
  // statistics
  | 'stat-continuous' | 'stat-discrete' | 'stat-timeseries'
  // portfolio
  | 'port-optimize' | 'port-risk'
  // scheduling
  | 'sched-calendar' | 'sched-daycount'
  // physics
  | 'phys-entropy' | 'phys-thermo'
  // curves
  | 'curves-build' | 'curves-transform' | 'curves-fitting' | 'curves-specialized'
  // pricing
  | 'pricing-bs' | 'pricing-black76' | 'pricing-bachelier' | 'pricing-numerical'
  // risk
  | 'risk-var' | 'risk-evt-xva' | 'risk-sensitivities'
  // economics
  | 'econ-equilibrium' | 'econ-gametheory' | 'econ-auctions' | 'econ-utility'
  // solver
  | 'solver-bond' | 'solver-rates' | 'solver-cashflow'
  // numerical
  | 'num-difffft' | 'num-interplinalg' | 'num-solvers'
  // instruments
  | 'instr-bonds' | 'instr-swaps' | 'instr-markets' | 'instr-creditfut'
  // analysis
  | 'analysis-fundamentals' | 'analysis-profit-liq' | 'analysis-eff-growth'
  | 'analysis-lev-cf' | 'analysis-val-qual' | 'analysis-industry'
  | 'analysis-dcf' | 'analysis-models';

type ModuleId = 'core' | 'regulatory' | 'volatility' | 'models' | 'stochastic' | 'ml' | 'statistics' | 'portfolio' | 'scheduling' | 'physics' | 'curves' | 'pricing' | 'risk' | 'economics' | 'solver' | 'numerical' | 'instruments' | 'analysis';

interface SectionItem { id: SectionId; label: string; icon: React.ReactNode; }
interface ModuleItem { id: ModuleId; label: string; sections: SectionItem[]; endpoints: number; }

const MODULES: ModuleItem[] = [
  {
    id: 'core', label: 'CORE', endpoints: 51, sections: [
      { id: 'types',         label: 'Types',         icon: <Hash size={12} /> },
      { id: 'conventions',   label: 'Conventions',   icon: <Calendar size={12} /> },
      { id: 'autodiff',      label: 'AutoDiff',      icon: <Zap size={12} /> },
      { id: 'distributions', label: 'Distributions', icon: <Activity size={12} /> },
      { id: 'math',          label: 'Math',          icon: <Sigma size={12} /> },
      { id: 'operations',    label: 'Operations',    icon: <Calculator size={12} /> },
      { id: 'legs',          label: 'Legs',          icon: <Layers size={12} /> },
      { id: 'periods',       label: 'Periods',       icon: <Clock size={12} /> },
    ],
  },
  {
    id: 'regulatory', label: 'REGULATORY', endpoints: 11, sections: [
      { id: 'basel',     label: 'Basel III',    icon: <Shield size={12} /> },
      { id: 'saccr',     label: 'SA-CCR',       icon: <CreditCard size={12} /> },
      { id: 'ifrs9',     label: 'IFRS 9',       icon: <FileCheck size={12} /> },
      { id: 'liquidity', label: 'Liquidity',    icon: <Droplets size={12} /> },
      { id: 'stress',    label: 'Stress Test',  icon: <AlertTriangle size={12} /> },
    ],
  },
  {
    id: 'volatility', label: 'VOLATILITY', endpoints: 14, sections: [
      { id: 'surface',  label: 'Surface',    icon: <BarChart3 size={12} /> },
      { id: 'sabr',     label: 'SABR',       icon: <Waves size={12} /> },
      { id: 'localvol', label: 'Local Vol',  icon: <Focus size={12} /> },
    ],
  },
  {
    id: 'models', label: 'MODELS', endpoints: 14, sections: [
      { id: 'shortrate',  label: 'Short Rate',      icon: <TrendingUp size={12} /> },
      { id: 'hullwhite',  label: 'Hull-White',       icon: <GitBranch size={12} /> },
      { id: 'heston',     label: 'Heston',           icon: <Activity size={12} /> },
      { id: 'jumpdiff',   label: 'Jump Diffusion',   icon: <Zap size={12} /> },
      { id: 'volmodels',  label: 'Dupire / SVI',     icon: <BarChart3 size={12} /> },
    ],
  },
  {
    id: 'stochastic', label: 'STOCHASTIC', endpoints: 36, sections: [
      { id: 'processes',  label: 'Processes',     icon: <Shuffle size={12} /> },
      { id: 'exact',      label: 'Exact',         icon: <Target size={12} /> },
      { id: 'simulation', label: 'Simulation',    icon: <Cpu size={12} /> },
      { id: 'sampling',   label: 'Sampling',      icon: <Gauge size={12} /> },
      { id: 'theory',     label: 'Theory',        icon: <Binary size={12} /> },
    ],
  },
  {
    id: 'ml', label: 'MACHINE LEARNING', endpoints: 48, sections: [
      { id: 'ml-credit',        label: 'Credit',          icon: <Brain size={12} /> },
      { id: 'ml-regression',    label: 'Regression',      icon: <LineChart size={12} /> },
      { id: 'ml-clustering',    label: 'Clustering',      icon: <Grid3X3 size={12} /> },
      { id: 'ml-preprocessing', label: 'Preprocessing',   icon: <Filter size={12} /> },
      { id: 'ml-features',      label: 'Features',        icon: <Boxes size={12} /> },
      { id: 'ml-validation',    label: 'Validation',      icon: <CheckCircle size={12} /> },
      { id: 'ml-timeseries',    label: 'Time Series',     icon: <Timer size={12} /> },
      { id: 'ml-gpnn',          label: 'GP / Neural Net', icon: <Network size={12} /> },
      { id: 'ml-factor',        label: 'Factor / Cov',    icon: <PieChart size={12} /> },
    ],
  },
  {
    id: 'statistics', label: 'STATISTICS', endpoints: 52, sections: [
      { id: 'stat-continuous',  label: 'Continuous Dist', icon: <BarChart2 size={12} /> },
      { id: 'stat-discrete',   label: 'Discrete Dist',   icon: <Dice1 size={12} /> },
      { id: 'stat-timeseries', label: 'Time Series',     icon: <Milestone size={12} /> },
    ],
  },
  {
    id: 'portfolio', label: 'PORTFOLIO', endpoints: 15, sections: [
      { id: 'port-optimize', label: 'Optimization',  icon: <Briefcase size={12} /> },
      { id: 'port-risk',     label: 'Risk Metrics',  icon: <ShieldCheck size={12} /> },
    ],
  },
  {
    id: 'scheduling', label: 'SCHEDULING', endpoints: 14, sections: [
      { id: 'sched-calendar', label: 'Calendars',  icon: <CalendarDays size={12} /> },
      { id: 'sched-daycount', label: 'Day Count',  icon: <CalendarClock size={12} /> },
    ],
  },
  {
    id: 'physics', label: 'PHYSICS', endpoints: 24, sections: [
      { id: 'phys-entropy', label: 'Entropy',        icon: <Atom size={12} /> },
      { id: 'phys-thermo',  label: 'Thermodynamics', icon: <Thermometer size={12} /> },
    ],
  },
  {
    id: 'curves', label: 'CURVES', endpoints: 31, sections: [
      { id: 'curves-build',       label: 'Build & Query',   icon: <Spline size={12} /> },
      { id: 'curves-transform',   label: 'Transforms',      icon: <ArrowRightLeft size={12} /> },
      { id: 'curves-fitting',     label: 'NS / NSS Fitting', icon: <Percent size={12} /> },
      { id: 'curves-specialized', label: 'Specialized',     icon: <Globe2 size={12} /> },
    ],
  },
  {
    id: 'pricing', label: 'PRICING', endpoints: 29, sections: [
      { id: 'pricing-bs',        label: 'Black-Scholes',  icon: <DollarSign size={12} /> },
      { id: 'pricing-black76',   label: 'Black76',        icon: <BarChartHorizontal size={12} /> },
      { id: 'pricing-bachelier', label: 'Bachelier',      icon: <Replace size={12} /> },
      { id: 'pricing-numerical', label: 'Numerical',      icon: <Grid size={12} /> },
    ],
  },
  {
    id: 'risk', label: 'RISK', endpoints: 25, sections: [
      { id: 'risk-var',           label: 'VaR / Stress',    icon: <ShieldAlert size={12} /> },
      { id: 'risk-evt-xva',       label: 'EVT / XVA',       icon: <Crosshair size={12} /> },
      { id: 'risk-sensitivities', label: 'Sensitivities',   icon: <Move size={12} /> },
    ],
  },
  {
    id: 'economics', label: 'ECONOMICS', endpoints: 25, sections: [
      { id: 'econ-equilibrium', label: 'Equilibrium',   icon: <Landmark size={12} /> },
      { id: 'econ-gametheory',  label: 'Game Theory',   icon: <Swords size={12} /> },
      { id: 'econ-auctions',   label: 'Auctions',      icon: <Gavel size={12} /> },
      { id: 'econ-utility',    label: 'Utility Theory', icon: <Heart size={12} /> },
    ],
  },
  {
    id: 'solver', label: 'SOLVER', endpoints: 25, sections: [
      { id: 'solver-bond',     label: 'Bond Analytics', icon: <Banknote size={12} /> },
      { id: 'solver-rates',    label: 'Rates / IV',     icon: <CircleDollarSign size={12} /> },
      { id: 'solver-cashflow', label: 'Cashflows',      icon: <Coins size={12} /> },
    ],
  },
  {
    id: 'numerical', label: 'NUMERICAL', endpoints: 28, sections: [
      { id: 'num-difffft',      label: 'Diff / FFT / Int', icon: <Diff size={12} /> },
      { id: 'num-interplinalg', label: 'Interp / LinAlg',  icon: <Radical size={12} /> },
      { id: 'num-solvers',      label: 'ODE / Roots / Opt', icon: <Box size={12} /> },
    ],
  },
  {
    id: 'instruments', label: 'INSTRUMENTS', endpoints: 26, sections: [
      { id: 'instr-bonds',     label: 'Bonds',          icon: <FileText size={12} /> },
      { id: 'instr-swaps',     label: 'Swaps / FRA',    icon: <Repeat size={12} /> },
      { id: 'instr-markets',   label: 'Markets',        icon: <Store size={12} /> },
      { id: 'instr-creditfut', label: 'Credit / Fut',   icon: <ShieldQuestion size={12} /> },
    ],
  },
  {
    id: 'analysis', label: 'ANALYSIS', endpoints: 122, sections: [
      { id: 'analysis-fundamentals', label: 'Fundamentals',     icon: <Building2 size={12} /> },
      { id: 'analysis-profit-liq',   label: 'Profit / Liquidity', icon: <BarChart4 size={12} /> },
      { id: 'analysis-eff-growth',   label: 'Efficiency / Growth', icon: <TrendingDown size={12} /> },
      { id: 'analysis-lev-cf',       label: 'Leverage / CF',    icon: <Scale size={12} /> },
      { id: 'analysis-val-qual',     label: 'Val Ratios / Quality', icon: <Gem size={12} /> },
      { id: 'analysis-industry',     label: 'Industry',         icon: <Combine size={12} /> },
      { id: 'analysis-dcf',          label: 'DCF Valuation',    icon: <Scan size={12} /> },
      { id: 'analysis-models',       label: 'Valuation Models', icon: <FlaskRound size={12} /> },
    ],
  },
];

// ── Panel renderer ───────────────────────────────────────────────
function renderPanel(id: SectionId) {
  switch (id) {
    // core
    case 'types':         return <TypesPanel />;
    case 'conventions':   return <ConventionsPanel />;
    case 'autodiff':      return <AutoDiffPanel />;
    case 'distributions': return <DistributionsPanel />;
    case 'math':          return <MathPanel />;
    case 'operations':    return <OperationsPanel />;
    case 'legs':          return <LegsPanel />;
    case 'periods':       return <PeriodsPanel />;
    // regulatory
    case 'basel':     return <BaselPanel />;
    case 'saccr':     return <SaccrPanel />;
    case 'ifrs9':     return <Ifrs9Panel />;
    case 'liquidity': return <LiquidityPanel />;
    case 'stress':    return <StressPanel />;
    // volatility
    case 'surface':  return <SurfacePanel />;
    case 'sabr':     return <SabrPanel />;
    case 'localvol': return <LocalVolPanel />;
    // models
    case 'shortrate':  return <ShortRatePanel />;
    case 'hullwhite':  return <HullWhitePanel />;
    case 'heston':     return <HestonPanel />;
    case 'jumpdiff':   return <JumpDiffusionPanel />;
    case 'volmodels':  return <VolModelsPanel />;
    // stochastic
    case 'processes':  return <ProcessesPanel />;
    case 'exact':      return <ExactPanel />;
    case 'simulation': return <SimulationPanel />;
    case 'sampling':   return <SamplingPanel />;
    case 'theory':     return <TheoryPanel />;
    // ml
    case 'ml-credit':        return <MlCreditPanel />;
    case 'ml-regression':    return <MlRegressionPanel />;
    case 'ml-clustering':    return <MlClusteringPanel />;
    case 'ml-preprocessing': return <MlPreprocessingPanel />;
    case 'ml-features':      return <MlFeaturesPanel />;
    case 'ml-validation':    return <MlValidationPanel />;
    case 'ml-timeseries':    return <MlTimeSeriesPanel />;
    case 'ml-gpnn':          return <MlGpNnPanel />;
    case 'ml-factor':        return <MlFactorPanel />;
    // statistics
    case 'stat-continuous':  return <ContinuousDistPanel />;
    case 'stat-discrete':   return <DiscreteDistPanel />;
    case 'stat-timeseries': return <StatTimeSeriesPanel />;
    // portfolio
    case 'port-optimize': return <OptimizePanel />;
    case 'port-risk':     return <PortfolioRiskPanel />;
    // scheduling
    case 'sched-calendar': return <CalendarPanel />;
    case 'sched-daycount': return <DayCountPanel />;
    // physics
    case 'phys-entropy': return <EntropyPanel />;
    case 'phys-thermo':  return <ThermoPanel />;
    // curves
    case 'curves-build':       return <CurveBuildPanel />;
    case 'curves-transform':   return <TransformPanel />;
    case 'curves-fitting':     return <FittingPanel />;
    case 'curves-specialized': return <SpecializedPanel />;
    // pricing
    case 'pricing-bs':        return <BlackScholesPanel />;
    case 'pricing-black76':   return <Black76Panel />;
    case 'pricing-bachelier': return <BachelierPanel />;
    case 'pricing-numerical': return <NumericalPanel />;
    // risk
    case 'risk-var':           return <VaRPanel />;
    case 'risk-evt-xva':       return <EvtXvaPanel />;
    case 'risk-sensitivities': return <SensitivitiesPanel />;
    // economics
    case 'econ-equilibrium': return <EquilibriumPanel />;
    case 'econ-gametheory':  return <GameTheoryPanel />;
    case 'econ-auctions':    return <AuctionPanel />;
    case 'econ-utility':     return <UtilityPanel />;
    // solver
    case 'solver-bond':     return <BondPanel />;
    case 'solver-rates':    return <RatesPanel />;
    case 'solver-cashflow': return <CashflowPanel />;
    // numerical
    case 'num-difffft':      return <DiffFftPanel />;
    case 'num-interplinalg': return <InterpLinalgPanel />;
    case 'num-solvers':      return <NumSolversPanel />;
    // instruments
    case 'instr-bonds':     return <InstrBondsPanel />;
    case 'instr-swaps':     return <InstrSwapsPanel />;
    case 'instr-markets':   return <InstrMarketsPanel />;
    case 'instr-creditfut': return <InstrCreditFuturesPanel />;
    // analysis
    case 'analysis-fundamentals': return <FundamentalsPanel />;
    case 'analysis-profit-liq':   return <RatioProfitPanel />;
    case 'analysis-eff-growth':   return <RatioEfficiencyPanel />;
    case 'analysis-lev-cf':       return <RatioLeverageCfPanel />;
    case 'analysis-val-qual':     return <RatioValQualPanel />;
    case 'analysis-industry':     return <IndustryPanel />;
    case 'analysis-dcf':          return <ValuationDcfPanel />;
    case 'analysis-models':       return <ValuationModelsPanel />;
  }
}

// ── Main component ───────────────────────────────────────────────
export default function QuantLibCoreTab() {
  const { session } = useAuth();
  const [activeSection, setActiveSection] = useState<SectionId>('types');
  const [expandedModules, setExpandedModules] = useState<Set<ModuleId>>(new Set(['core']));

  // Push API key into all modules whenever it changes
  useEffect(() => {
    const key = session?.api_key ?? null;
    setQuantLibApiKey(key);
    setRegulatoryApiKey(key);
    setVolatilityApiKey(key);
    setModelsApiKey(key);
    setStochasticApiKey(key);
    setMlApiKey(key);
    setStatisticsApiKey(key);
    setPortfolioApiKey(key);
    setSchedulingApiKey(key);
    setPhysicsApiKey(key);
    setCurvesApiKey(key);
    setPricingApiKey(key);
    setRiskModApiKey(key);
    setEconomicsApiKey(key);
    setSolverApiKey(key);
    setNumericalApiKey(key);
    setInstrumentsApiKey(key);
    setAnalysisApiKey(key);
  }, [session?.api_key]);

  const toggleModule = (moduleId: ModuleId) => {
    setExpandedModules(prev => {
      const next = new Set(prev);
      if (next.has(moduleId)) next.delete(moduleId);
      else next.add(moduleId);
      return next;
    });
  };

  // Find which module the active section belongs to
  const activeModule = MODULES.find(m => m.sections.some(s => s.id === activeSection));

  return (
    <div style={{ display: 'flex', height: '100%', backgroundColor: F.DARK_BG, fontFamily: FONT }}>
      {/* Sidebar */}
      <div style={{
        width: 210, borderRight: `1px solid ${F.BORDER}`, backgroundColor: F.PANEL_BG,
        display: 'flex', flexDirection: 'column', flexShrink: 0, overflow: 'auto',
      }}>
        <div style={{ padding: '12px', backgroundColor: F.HEADER_BG, borderBottom: `2px solid ${F.ORANGE}` }}>
          <div style={{ color: F.ORANGE, fontSize: '12px', fontWeight: 700, letterSpacing: '0.5px' }}>QUANTLIB</div>
          <div style={{ color: F.GRAY, fontSize: '9px', marginTop: 4 }}>590 ENDPOINTS &middot; 18 MODULES</div>
        </div>

        {MODULES.map(mod => {
          const isExpanded = expandedModules.has(mod.id);
          return (
            <div key={mod.id}>
              {/* Module header */}
              <div
                onClick={() => toggleModule(mod.id)}
                style={{
                  display: 'flex', alignItems: 'center', gap: 6,
                  padding: '8px 10px', cursor: 'pointer',
                  backgroundColor: activeModule?.id === mod.id ? `${F.ORANGE}08` : 'transparent',
                  borderBottom: `1px solid ${F.BORDER}`,
                }}
              >
                {isExpanded
                  ? <ChevronDown size={10} color={F.ORANGE} />
                  : <ChevronRight size={10} color={F.GRAY} />}
                <span style={{
                  color: isExpanded ? F.ORANGE : F.GRAY,
                  fontSize: '9px', fontWeight: 700, letterSpacing: '0.8px',
                  flex: 1,
                }}>{mod.label}</span>
                <span style={{ color: F.MUTED, fontSize: '8px' }}>{mod.endpoints}</span>
              </div>

              {/* Section items */}
              {isExpanded && mod.sections.map(s => (
                <div
                  key={s.id}
                  onClick={() => setActiveSection(s.id)}
                  style={{
                    display: 'flex', alignItems: 'center', gap: 8,
                    padding: '7px 12px 7px 24px', cursor: 'pointer', transition: 'all 0.15s',
                    backgroundColor: activeSection === s.id ? `${F.ORANGE}15` : 'transparent',
                    color: activeSection === s.id ? F.ORANGE : F.GRAY,
                    fontSize: '9px', fontWeight: 600, letterSpacing: '0.3px',
                    textTransform: 'uppercase',
                    borderLeft: activeSection === s.id ? `2px solid ${F.ORANGE}` : '2px solid transparent',
                  }}
                >
                  {s.icon}
                  {s.label}
                </div>
              ))}
            </div>
          );
        })}
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: 16 }}>
        <div style={{ maxWidth: 900 }}>
          {renderPanel(activeSection)}
        </div>
      </div>
    </div>
  );
}
