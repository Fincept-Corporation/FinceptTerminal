import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { preprocessingApi } from '../quantlibMlApi';

export default function PreprocessingPanel() {
  // Scale
  const sc = useEndpoint();
  const [scX, setScX] = useState('[[1,200],[2,400],[3,600],[4,800]]');
  const [scMethod, setScMethod] = useState('standard');

  // Outliers
  const out = useEndpoint();
  const [outX, setOutX] = useState('[[1,2],[1.5,1.8],[5,8],[100,200],[1,0.6]]');
  const [outMethod, setOutMethod] = useState('zscore');
  const [outThresh, setOutThresh] = useState('3');

  // Stationarity
  const stat = useEndpoint();
  const [statData, setStatData] = useState('100, 102, 104, 103, 105, 107, 110, 108, 112');
  const [statTrans, setStatTrans] = useState('difference');

  // Transform
  const tr = useEndpoint();
  const [trX, setTrX] = useState('[[1,2],[3,4],[5,6],[7,8]]');
  const [trMethod, setTrMethod] = useState('log');

  // Winsorize
  const win = useEndpoint();
  const [winData, setWinData] = useState('1, 2, 3, 4, 5, 100, 200, 6, 7, 8');
  const [winLow, setWinLow] = useState('0.05');
  const [winUp, setWinUp] = useState('0.95');

  return (
    <>
      <EndpointCard title="Scale" description="Feature scaling (Standard / MinMax / Robust)">
        <Row>
          <Field label="X (JSON 2D)" width="300px"><Input value={scX} onChange={setScX} /></Field>
          <Field label="Method"><Select value={scMethod} onChange={setScMethod} options={['standard', 'minmax', 'robust', 'maxabs']} /></Field>
          <RunButton loading={sc.loading} onClick={() => sc.run(() => preprocessingApi.scale(JSON.parse(scX), scMethod))} />
        </Row>
        <ResultPanel data={sc.result} error={sc.error} />
      </EndpointCard>

      <EndpointCard title="Outlier Detection" description="Identify outliers (Z-score / IQR / Isolation)">
        <Row>
          <Field label="X (JSON 2D)" width="300px"><Input value={outX} onChange={setOutX} /></Field>
          <Field label="Method"><Select value={outMethod} onChange={setOutMethod} options={['zscore', 'iqr', 'isolation_forest']} /></Field>
          <Field label="Threshold"><Input value={outThresh} onChange={setOutThresh} type="number" /></Field>
          <RunButton loading={out.loading} onClick={() => out.run(() => preprocessingApi.outliers(JSON.parse(outX), outMethod, Number(outThresh)))} />
        </Row>
        <ResultPanel data={out.result} error={out.error} />
      </EndpointCard>

      <EndpointCard title="Stationarity" description="ADF test + optional differencing transform">
        <Row>
          <Field label="Data (comma-sep)" width="350px"><Input value={statData} onChange={setStatData} /></Field>
          <Field label="Transform"><Select value={statTrans} onChange={setStatTrans} options={['difference', 'log_difference', 'none']} /></Field>
          <RunButton loading={stat.loading} onClick={() => stat.run(() => preprocessingApi.stationarity(statData.split(',').map(Number), statTrans))} />
        </Row>
        <ResultPanel data={stat.result} error={stat.error} />
      </EndpointCard>

      <EndpointCard title="Transform" description="Data transformation (Log / Box-Cox / Yeo-Johnson)">
        <Row>
          <Field label="X (JSON 2D)" width="300px"><Input value={trX} onChange={setTrX} /></Field>
          <Field label="Method"><Select value={trMethod} onChange={setTrMethod} options={['log', 'boxcox', 'yeo_johnson', 'sqrt']} /></Field>
          <RunButton loading={tr.loading} onClick={() => tr.run(() => preprocessingApi.transform(JSON.parse(trX), trMethod))} />
        </Row>
        <ResultPanel data={tr.result} error={tr.error} />
      </EndpointCard>

      <EndpointCard title="Winsorize" description="Clip extreme values at percentiles">
        <Row>
          <Field label="Data (comma-sep)" width="350px"><Input value={winData} onChange={setWinData} /></Field>
          <Field label="Lower"><Input value={winLow} onChange={setWinLow} type="number" /></Field>
          <Field label="Upper"><Input value={winUp} onChange={setWinUp} type="number" /></Field>
          <RunButton loading={win.loading} onClick={() => win.run(() => preprocessingApi.winsorize(winData.split(',').map(Number), Number(winLow), Number(winUp)))} />
        </Row>
        <ResultPanel data={win.result} error={win.error} />
      </EndpointCard>
    </>
  );
}
