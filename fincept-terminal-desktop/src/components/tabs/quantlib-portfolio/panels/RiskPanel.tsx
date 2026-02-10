import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { riskApi } from '../quantlibPortfolioApi';

export default function RiskPanel() {
  const sampleW = '[0.4, 0.35, 0.25]';
  const sampleCov = '[[0.04,0.006,0.002],[0.006,0.09,0.009],[0.002,0.009,0.01]]';
  const sampleRet = '0.01, -0.02, 0.03, -0.01, 0.005, -0.03, 0.02, 0.01, -0.005, 0.015';

  // Comprehensive
  const comp = useEndpoint();
  const [compW, setCompW] = useState(sampleW);
  const [compCov, setCompCov] = useState(sampleCov);

  // VaR
  const varE = useEndpoint();
  const [varW, setVarW] = useState(sampleW);
  const [varCov, setVarCov] = useState(sampleCov);
  const [varConf, setVarConf] = useState('0.95');
  const [varHorizon, setVarHorizon] = useState('1');
  const [varVal, setVarVal] = useState('1000000');
  const [varMethod, setVarMethod] = useState('parametric');

  // CVaR
  const cvarE = useEndpoint();
  const [cvarW, setCvarW] = useState(sampleW);
  const [cvarCov, setCvarCov] = useState(sampleCov);
  const [cvarConf, setCvarConf] = useState('0.95');

  // Contribution
  const contrib = useEndpoint();
  const [contW, setContW] = useState(sampleW);
  const [contCov, setContCov] = useState(sampleCov);

  // Ratios
  const ratios = useEndpoint();
  const [ratRet, setRatRet] = useState(sampleRet);
  const [ratRf, setRatRf] = useState('0');

  // Incremental VaR
  const incVar = useEndpoint();
  const [incW, setIncW] = useState(sampleW);
  const [incCov, setIncCov] = useState(sampleCov);
  const [incNew, setIncNew] = useState('0.05');
  const [incIdx, setIncIdx] = useState('0');

  // Inverse Vol
  const invVol = useEndpoint();
  const [ivCov, setIvCov] = useState(sampleCov);

  // Portfolio Comprehensive
  const pComp = useEndpoint();
  const [pcRet, setPcRet] = useState('[[0.01,-0.02,0.03],[0.02,0.01,-0.01],[-0.01,0.03,0.02]]');
  const [pcW, setPcW] = useState(sampleW);

  return (
    <>
      <EndpointCard title="Risk Comprehensive" description="Full portfolio risk metrics">
        <Row>
          <Field label="Weights (JSON)" width="200px"><Input value={compW} onChange={setCompW} /></Field>
          <Field label="Covariance (JSON 2D)" width="400px"><Input value={compCov} onChange={setCompCov} /></Field>
          <RunButton loading={comp.loading} onClick={() => comp.run(() => riskApi.comprehensive(JSON.parse(compW), JSON.parse(compCov)))} />
        </Row>
        <ResultPanel data={comp.result} error={comp.error} />
      </EndpointCard>

      <EndpointCard title="Value at Risk (VaR)" description="Parametric / Historical VaR">
        <Row>
          <Field label="Weights (JSON)" width="200px"><Input value={varW} onChange={setVarW} /></Field>
          <Field label="Cov (JSON 2D)" width="300px"><Input value={varCov} onChange={setVarCov} /></Field>
          <Field label="Confidence"><Input value={varConf} onChange={setVarConf} type="number" /></Field>
          <Field label="Horizon"><Input value={varHorizon} onChange={setVarHorizon} type="number" /></Field>
          <Field label="Port Value"><Input value={varVal} onChange={setVarVal} type="number" /></Field>
          <Field label="Method"><Select value={varMethod} onChange={setVarMethod} options={['parametric', 'historical', 'cornish_fisher']} /></Field>
          <RunButton loading={varE.loading} onClick={() => varE.run(() => riskApi.var(JSON.parse(varW), JSON.parse(varCov), Number(varConf), Number(varHorizon), Number(varVal), varMethod))} />
        </Row>
        <ResultPanel data={varE.result} error={varE.error} />
      </EndpointCard>

      <EndpointCard title="Conditional VaR (CVaR)" description="Expected Shortfall">
        <Row>
          <Field label="Weights (JSON)" width="200px"><Input value={cvarW} onChange={setCvarW} /></Field>
          <Field label="Cov (JSON 2D)" width="350px"><Input value={cvarCov} onChange={setCvarCov} /></Field>
          <Field label="Confidence"><Input value={cvarConf} onChange={setCvarConf} type="number" /></Field>
          <RunButton loading={cvarE.loading} onClick={() => cvarE.run(() => riskApi.cvar(JSON.parse(cvarW), JSON.parse(cvarCov), Number(cvarConf)))} />
        </Row>
        <ResultPanel data={cvarE.result} error={cvarE.error} />
      </EndpointCard>

      <EndpointCard title="Risk Contribution" description="Marginal and component risk contributions">
        <Row>
          <Field label="Weights (JSON)" width="200px"><Input value={contW} onChange={setContW} /></Field>
          <Field label="Cov (JSON 2D)" width="400px"><Input value={contCov} onChange={setContCov} /></Field>
          <RunButton loading={contrib.loading} onClick={() => contrib.run(() => riskApi.contribution(JSON.parse(contW), JSON.parse(contCov)))} />
        </Row>
        <ResultPanel data={contrib.result} error={contrib.error} />
      </EndpointCard>

      <EndpointCard title="Risk Ratios" description="Sharpe, Sortino, Calmar, etc.">
        <Row>
          <Field label="Returns (comma-sep)" width="400px"><Input value={ratRet} onChange={setRatRet} /></Field>
          <Field label="Rf Rate"><Input value={ratRf} onChange={setRatRf} type="number" /></Field>
          <RunButton loading={ratios.loading} onClick={() => ratios.run(() => riskApi.ratios(ratRet.split(',').map(Number), Number(ratRf)))} />
        </Row>
        <ResultPanel data={ratios.result} error={ratios.error} />
      </EndpointCard>

      <EndpointCard title="Incremental VaR" description="VaR impact of adding an asset">
        <Row>
          <Field label="Weights (JSON)" width="200px"><Input value={incW} onChange={setIncW} /></Field>
          <Field label="Cov (JSON 2D)" width="300px"><Input value={incCov} onChange={setIncCov} /></Field>
          <Field label="New Weight"><Input value={incNew} onChange={setIncNew} type="number" /></Field>
          <Field label="Asset Index"><Input value={incIdx} onChange={setIncIdx} type="number" /></Field>
          <RunButton loading={incVar.loading} onClick={() => incVar.run(() => riskApi.incrementalVar(JSON.parse(incW), JSON.parse(incCov), Number(incNew), Number(incIdx)))} />
        </Row>
        <ResultPanel data={incVar.result} error={incVar.error} />
      </EndpointCard>

      <EndpointCard title="Inverse Volatility Weights" description="Weights proportional to 1/vol">
        <Row>
          <Field label="Covariance (JSON 2D)" width="450px"><Input value={ivCov} onChange={setIvCov} /></Field>
          <RunButton loading={invVol.loading} onClick={() => invVol.run(() => riskApi.inverseVolWeights(JSON.parse(ivCov)))} />
        </Row>
        <ResultPanel data={invVol.result} error={invVol.error} />
      </EndpointCard>

      <EndpointCard title="Portfolio Comprehensive" description="Full risk analysis from returns matrix">
        <Row>
          <Field label="Returns (JSON 2D)" width="350px"><Input value={pcRet} onChange={setPcRet} /></Field>
          <Field label="Weights (JSON)" width="200px"><Input value={pcW} onChange={setPcW} /></Field>
          <RunButton loading={pComp.loading} onClick={() => pComp.run(() => riskApi.portfolioComprehensive(JSON.parse(pcRet), JSON.parse(pcW)))} />
        </Row>
        <ResultPanel data={pComp.result} error={pComp.error} />
      </EndpointCard>
    </>
  );
}
