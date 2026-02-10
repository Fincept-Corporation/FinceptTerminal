import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../shared';
import { opsApi } from '../quantlibCoreApi';

export default function OperationsPanel() {
  // Discount Cashflows
  const dcf = useEndpoint();
  const [dcCf, setDcCf] = useState('100, 100, 100, 1100');
  const [dcT, setDcT] = useState('1, 2, 3, 4');
  const [dcDf, setDcDf] = useState('0.97, 0.94, 0.91, 0.88');

  // Black-Scholes
  const bs = useEndpoint();
  const [bsType, setBsType] = useState('call');
  const [bsSpot, setBsSpot] = useState('100');
  const [bsStrike, setBsStrike] = useState('100');
  const [bsRate, setBsRate] = useState('0.05');
  const [bsTime, setBsTime] = useState('1');
  const [bsVol, setBsVol] = useState('0.2');

  // Black76
  const b76 = useEndpoint();
  const [b76Type, setB76Type] = useState('call');
  const [b76Fwd, setB76Fwd] = useState('100');
  const [b76Strike, setB76Strike] = useState('100');
  const [b76Vol, setB76Vol] = useState('0.2');
  const [b76Time, setB76Time] = useState('1');
  const [b76Df, setB76Df] = useState('0.95');

  // VaR
  const varEp = useEndpoint();
  const [varMethod, setVarMethod] = useState('parametric');
  const [varReturns, setVarReturns] = useState('-0.02, 0.01, -0.03, 0.02, -0.01, 0.03, -0.015, 0.005');
  const [varPv, setVarPv] = useState('1000000');
  const [varConf, setVarConf] = useState('0.95');

  // Interpolate
  const interp = useEndpoint();
  const [intMethod, setIntMethod] = useState('linear');
  const [intX, setIntX] = useState('2.5');
  const [intXData, setIntXData] = useState('1, 2, 3, 4, 5');
  const [intYData, setIntYData] = useState('10, 20, 30, 40, 50');

  // Forward Rate
  const fwd = useEndpoint();
  const [fwdDf1, setFwdDf1] = useState('0.97');
  const [fwdDf2, setFwdDf2] = useState('0.94');
  const [fwdT1, setFwdT1] = useState('1');
  const [fwdT2, setFwdT2] = useState('2');

  // Zero Rate Convert
  const zrc = useEndpoint();
  const [zrcDir, setZrcDir] = useState('rate_to_df');
  const [zrcVal, setZrcVal] = useState('0.05');
  const [zrcT, setZrcT] = useState('1');

  // GBM Paths
  const gbm = useEndpoint();
  const [gbmSpot, setGbmSpot] = useState('100');
  const [gbmDrift, setGbmDrift] = useState('0.05');
  const [gbmVol, setGbmVol] = useState('0.2');
  const [gbmTime, setGbmTime] = useState('1');
  const [gbmPaths, setGbmPaths] = useState('10');
  const [gbmSteps, setGbmSteps] = useState('50');

  // Cholesky
  const chol = useEndpoint();
  const [cholMatrix, setCholMatrix] = useState('[[1, 0.5], [0.5, 1]]');

  // Covariance Matrix
  const cov = useEndpoint();
  const [covReturns, setCovReturns] = useState('[[0.01, 0.02, -0.01], [0.02, -0.01, 0.03]]');

  // Statistics
  const stats = useEndpoint();
  const [statsVals, setStatsVals] = useState('10, 20, 30, 40, 50');
  const [statsDdof, setStatsDdof] = useState('0');

  // Percentile
  const pct = useEndpoint();
  const [pctVals, setPctVals] = useState('10, 20, 30, 40, 50, 60, 70, 80, 90, 100');
  const [pctP, setPctP] = useState('75');

  return (
    <>
      <EndpointCard title="Discount Cashflows" description="PV of cashflow stream">
        <Row>
          <Field label="Cashflows (comma-sep)" width="200px"><Input value={dcCf} onChange={setDcCf} /></Field>
          <Field label="Times" width="200px"><Input value={dcT} onChange={setDcT} /></Field>
          <Field label="Discount Factors" width="200px"><Input value={dcDf} onChange={setDcDf} /></Field>
          <RunButton loading={dcf.loading} onClick={() => dcf.run(() =>
            opsApi.discountCashflows(dcCf.split(',').map(Number), dcT.split(',').map(Number), dcDf.split(',').map(Number))
          )} />
        </Row>
        <ResultPanel data={dcf.result} error={dcf.error} />
      </EndpointCard>

      <EndpointCard title="Black-Scholes" description="Option pricing">
        <Row>
          <Field label="Type"><Select value={bsType} onChange={setBsType} options={['call', 'put']} /></Field>
          <Field label="Spot"><Input value={bsSpot} onChange={setBsSpot} type="number" /></Field>
          <Field label="Strike"><Input value={bsStrike} onChange={setBsStrike} type="number" /></Field>
          <Field label="Rate"><Input value={bsRate} onChange={setBsRate} type="number" /></Field>
          <Field label="Time (Y)"><Input value={bsTime} onChange={setBsTime} type="number" /></Field>
          <Field label="Vol"><Input value={bsVol} onChange={setBsVol} type="number" /></Field>
          <RunButton loading={bs.loading} onClick={() => bs.run(() =>
            opsApi.blackScholes(bsType, Number(bsSpot), Number(bsStrike), Number(bsRate), Number(bsTime), Number(bsVol))
          )} />
        </Row>
        <ResultPanel data={bs.result} error={bs.error} />
      </EndpointCard>

      <EndpointCard title="Black76" description="Futures option pricing">
        <Row>
          <Field label="Type"><Select value={b76Type} onChange={setB76Type} options={['call', 'put']} /></Field>
          <Field label="Forward"><Input value={b76Fwd} onChange={setB76Fwd} type="number" /></Field>
          <Field label="Strike"><Input value={b76Strike} onChange={setB76Strike} type="number" /></Field>
          <Field label="Vol"><Input value={b76Vol} onChange={setB76Vol} type="number" /></Field>
          <Field label="Time"><Input value={b76Time} onChange={setB76Time} type="number" /></Field>
          <Field label="DF"><Input value={b76Df} onChange={setB76Df} type="number" /></Field>
          <RunButton loading={b76.loading} onClick={() => b76.run(() =>
            opsApi.black76(b76Type, Number(b76Fwd), Number(b76Strike), Number(b76Vol), Number(b76Time), Number(b76Df))
          )} />
        </Row>
        <ResultPanel data={b76.result} error={b76.error} />
      </EndpointCard>

      <EndpointCard title="Value at Risk (VaR)" description="Parametric / Historical VaR">
        <Row>
          <Field label="Method"><Select value={varMethod} onChange={setVarMethod} options={['parametric', 'historical', 'cornish_fisher']} /></Field>
          <Field label="Returns (comma-sep)" width="300px"><Input value={varReturns} onChange={setVarReturns} /></Field>
          <Field label="Portfolio Value"><Input value={varPv} onChange={setVarPv} type="number" /></Field>
          <Field label="Confidence"><Input value={varConf} onChange={setVarConf} type="number" /></Field>
          <RunButton loading={varEp.loading} onClick={() => varEp.run(() =>
            opsApi.var(varMethod, varReturns.split(',').map(Number), Number(varPv), undefined, Number(varConf))
          )} />
        </Row>
        <ResultPanel data={varEp.result} error={varEp.error} />
      </EndpointCard>

      <EndpointCard title="Interpolate" description="Linear, cubic, polynomial interpolation">
        <Row>
          <Field label="Method"><Select value={intMethod} onChange={setIntMethod} options={['linear', 'cubic', 'polynomial', 'nearest']} /></Field>
          <Field label="x"><Input value={intX} onChange={setIntX} type="number" /></Field>
          <Field label="x_data" width="200px"><Input value={intXData} onChange={setIntXData} /></Field>
          <Field label="y_data" width="200px"><Input value={intYData} onChange={setIntYData} /></Field>
          <RunButton loading={interp.loading} onClick={() => interp.run(() =>
            opsApi.interpolate(intMethod, Number(intX), intXData.split(',').map(Number), intYData.split(',').map(Number))
          )} />
        </Row>
        <ResultPanel data={interp.result} error={interp.error} />
      </EndpointCard>

      <EndpointCard title="Forward Rate" description="From discount factors">
        <Row>
          <Field label="DF\u2081"><Input value={fwdDf1} onChange={setFwdDf1} type="number" /></Field>
          <Field label="DF\u2082"><Input value={fwdDf2} onChange={setFwdDf2} type="number" /></Field>
          <Field label="T\u2081"><Input value={fwdT1} onChange={setFwdT1} type="number" /></Field>
          <Field label="T\u2082"><Input value={fwdT2} onChange={setFwdT2} type="number" /></Field>
          <RunButton loading={fwd.loading} onClick={() => fwd.run(() =>
            opsApi.forwardRate(Number(fwdDf1), Number(fwdDf2), Number(fwdT1), Number(fwdT2))
          )} />
        </Row>
        <ResultPanel data={fwd.result} error={fwd.error} />
      </EndpointCard>

      <EndpointCard title="Zero Rate Convert" description="Rate \u2194 Discount Factor">
        <Row>
          <Field label="Direction"><Select value={zrcDir} onChange={setZrcDir} options={['rate_to_df', 'df_to_rate']} /></Field>
          <Field label="Value"><Input value={zrcVal} onChange={setZrcVal} type="number" /></Field>
          <Field label="t"><Input value={zrcT} onChange={setZrcT} type="number" /></Field>
          <RunButton loading={zrc.loading} onClick={() => zrc.run(() =>
            opsApi.zeroRateConvert(zrcDir, Number(zrcVal), Number(zrcT))
          )} />
        </Row>
        <ResultPanel data={zrc.result} error={zrc.error} />
      </EndpointCard>

      <EndpointCard title="GBM Paths" description="Geometric Brownian Motion simulation">
        <Row>
          <Field label="Spot"><Input value={gbmSpot} onChange={setGbmSpot} type="number" /></Field>
          <Field label="Drift"><Input value={gbmDrift} onChange={setGbmDrift} type="number" /></Field>
          <Field label="Vol"><Input value={gbmVol} onChange={setGbmVol} type="number" /></Field>
          <Field label="Time"><Input value={gbmTime} onChange={setGbmTime} type="number" /></Field>
          <Field label="Paths"><Input value={gbmPaths} onChange={setGbmPaths} type="number" /></Field>
          <Field label="Steps"><Input value={gbmSteps} onChange={setGbmSteps} type="number" /></Field>
          <RunButton loading={gbm.loading} onClick={() => gbm.run(() =>
            opsApi.gbmPaths(Number(gbmSpot), Number(gbmDrift), Number(gbmVol), Number(gbmTime), Number(gbmPaths), Number(gbmSteps))
          )} />
        </Row>
        <ResultPanel data={gbm.result} error={gbm.error} />
      </EndpointCard>

      <EndpointCard title="Cholesky Decomposition" description="L such that A = LL\u1D40">
        <Row>
          <Field label="Matrix (JSON)" width="300px"><Input value={cholMatrix} onChange={setCholMatrix} /></Field>
          <RunButton loading={chol.loading} onClick={() => chol.run(() => opsApi.cholesky(JSON.parse(cholMatrix)))} />
        </Row>
        <ResultPanel data={chol.result} error={chol.error} />
      </EndpointCard>

      <EndpointCard title="Covariance Matrix" description="From return series">
        <Row>
          <Field label="Returns (JSON 2D array)" width="400px"><Input value={covReturns} onChange={setCovReturns} /></Field>
          <RunButton loading={cov.loading} onClick={() => cov.run(() => opsApi.covarianceMatrix(JSON.parse(covReturns)))} />
        </Row>
        <ResultPanel data={cov.result} error={cov.error} />
      </EndpointCard>

      <EndpointCard title="Array Statistics" description="Mean, std, skew, kurtosis, min, max">
        <Row>
          <Field label="Values (comma-sep)" width="300px"><Input value={statsVals} onChange={setStatsVals} /></Field>
          <Field label="ddof"><Input value={statsDdof} onChange={setStatsDdof} type="number" /></Field>
          <RunButton loading={stats.loading} onClick={() => stats.run(() =>
            opsApi.statistics(statsVals.split(',').map(Number), Number(statsDdof))
          )} />
        </Row>
        <ResultPanel data={stats.result} error={stats.error} />
      </EndpointCard>

      <EndpointCard title="Percentile" description="p-th percentile of values">
        <Row>
          <Field label="Values (comma-sep)" width="300px"><Input value={pctVals} onChange={setPctVals} /></Field>
          <Field label="Percentile"><Input value={pctP} onChange={setPctP} type="number" /></Field>
          <RunButton loading={pct.loading} onClick={() => pct.run(() =>
            opsApi.percentile(pctVals.split(',').map(Number), Number(pctP))
          )} />
        </Row>
        <ResultPanel data={pct.result} error={pct.error} />
      </EndpointCard>
    </>
  );
}
