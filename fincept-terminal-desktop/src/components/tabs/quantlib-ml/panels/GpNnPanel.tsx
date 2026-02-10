import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { gpNnApi } from '../quantlibMlApi';

export default function GpNnPanel() {
  // GP Curve
  const gpC = useEndpoint();
  const [gpTenors, setGpTenors] = useState('0.25, 0.5, 1, 2, 5, 10');
  const [gpRates, setGpRates] = useState('0.01, 0.015, 0.02, 0.025, 0.03, 0.035');
  const [gpQuery, setGpQuery] = useState('0.75, 1.5, 3, 7');
  const [gpLS, setGpLS] = useState('1');
  const [gpNoise, setGpNoise] = useState('0.001');

  // GP Vol Surface
  const gpV = useEndpoint();
  const [gpvStrikes, setGpvStrikes] = useState('80, 90, 100, 110, 120');
  const [gpvExpiries, setGpvExpiries] = useState('0.25, 0.5, 1');
  const [gpvVols, setGpvVols] = useState('0.3, 0.25, 0.2, 0.22, 0.28');
  const [gpvQS, setGpvQS] = useState('85, 95, 105, 115');
  const [gpvQE, setGpvQE] = useState('0.25, 0.5, 0.75');

  // NN Curve
  const nnC = useEndpoint();
  const [nnTenors, setNnTenors] = useState('0.25, 0.5, 1, 2, 5, 10');
  const [nnRates, setNnRates] = useState('0.01, 0.015, 0.02, 0.025, 0.03, 0.035');
  const [nnQuery, setNnQuery] = useState('0.75, 1.5, 3, 7');
  const [nnLayers, setNnLayers] = useState('[32, 16]');
  const [nnEpochs, setNnEpochs] = useState('100');

  // NN Vol Surface
  const nnV = useEndpoint();
  const [nnvStrikes, setNnvStrikes] = useState('80, 90, 100, 110, 120');
  const [nnvExpiries, setNnvExpiries] = useState('0.25, 0.5, 1');
  const [nnvVols, setNnvVols] = useState('0.3, 0.25, 0.2, 0.22, 0.28');
  const [nnvQS, setNnvQS] = useState('85, 95, 105, 115');
  const [nnvQE, setNnvQE] = useState('0.25, 0.5, 0.75');
  const [nnvLayers, setNnvLayers] = useState('[32, 16]');
  const [nnvEpochs, setNnvEpochs] = useState('100');

  // NN Portfolio
  const nnP = useEndpoint();
  const [nnpReturns, setNnpReturns] = useState('[[0.01,-0.02,0.03],[0.02,0.01,-0.01],[-0.01,0.03,0.02]]');
  const [nnpRA, setNnpRA] = useState('1');
  const [nnpLayers, setNnpLayers] = useState('[32, 16]');

  return (
    <>
      <EndpointCard title="GP Yield Curve" description="Gaussian Process yield curve interpolation">
        <Row>
          <Field label="Tenors (comma-sep)" width="200px"><Input value={gpTenors} onChange={setGpTenors} /></Field>
          <Field label="Rates (comma-sep)" width="250px"><Input value={gpRates} onChange={setGpRates} /></Field>
          <Field label="Query Tenors" width="200px"><Input value={gpQuery} onChange={setGpQuery} /></Field>
          <Field label="Length Scale"><Input value={gpLS} onChange={setGpLS} type="number" /></Field>
          <Field label="Noise"><Input value={gpNoise} onChange={setGpNoise} type="number" /></Field>
          <RunButton loading={gpC.loading} onClick={() => gpC.run(() => gpNnApi.gpCurve(gpTenors.split(',').map(Number), gpRates.split(',').map(Number), gpQuery.split(',').map(Number), Number(gpLS), Number(gpNoise)))} />
        </Row>
        <ResultPanel data={gpC.result} error={gpC.error} />
      </EndpointCard>

      <EndpointCard title="GP Vol Surface" description="Gaussian Process volatility surface">
        <Row>
          <Field label="Strikes" width="200px"><Input value={gpvStrikes} onChange={setGpvStrikes} /></Field>
          <Field label="Expiries" width="150px"><Input value={gpvExpiries} onChange={setGpvExpiries} /></Field>
          <Field label="Vols" width="200px"><Input value={gpvVols} onChange={setGpvVols} /></Field>
          <Field label="Query Strikes" width="200px"><Input value={gpvQS} onChange={setGpvQS} /></Field>
          <Field label="Query Expiries" width="150px"><Input value={gpvQE} onChange={setGpvQE} /></Field>
          <RunButton loading={gpV.loading} onClick={() => gpV.run(() => gpNnApi.gpVolSurface(gpvStrikes.split(',').map(Number), gpvExpiries.split(',').map(Number), gpvVols.split(',').map(Number), gpvQS.split(',').map(Number), gpvQE.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={gpV.result} error={gpV.error} />
      </EndpointCard>

      <EndpointCard title="NN Yield Curve" description="Neural Network yield curve fitting">
        <Row>
          <Field label="Tenors" width="200px"><Input value={nnTenors} onChange={setNnTenors} /></Field>
          <Field label="Rates" width="250px"><Input value={nnRates} onChange={setNnRates} /></Field>
          <Field label="Query Tenors" width="200px"><Input value={nnQuery} onChange={setNnQuery} /></Field>
          <Field label="Layers (JSON)" width="100px"><Input value={nnLayers} onChange={setNnLayers} /></Field>
          <Field label="Epochs"><Input value={nnEpochs} onChange={setNnEpochs} type="number" /></Field>
          <RunButton loading={nnC.loading} onClick={() => nnC.run(() => gpNnApi.nnCurve(nnTenors.split(',').map(Number), nnRates.split(',').map(Number), nnQuery.split(',').map(Number), JSON.parse(nnLayers), Number(nnEpochs)))} />
        </Row>
        <ResultPanel data={nnC.result} error={nnC.error} />
      </EndpointCard>

      <EndpointCard title="NN Vol Surface" description="Neural Network volatility surface">
        <Row>
          <Field label="Strikes" width="180px"><Input value={nnvStrikes} onChange={setNnvStrikes} /></Field>
          <Field label="Expiries" width="130px"><Input value={nnvExpiries} onChange={setNnvExpiries} /></Field>
          <Field label="Vols" width="180px"><Input value={nnvVols} onChange={setNnvVols} /></Field>
          <Field label="Query Strikes" width="180px"><Input value={nnvQS} onChange={setNnvQS} /></Field>
          <Field label="Query Exp" width="130px"><Input value={nnvQE} onChange={setNnvQE} /></Field>
          <Field label="Layers" width="80px"><Input value={nnvLayers} onChange={setNnvLayers} /></Field>
          <Field label="Epochs"><Input value={nnvEpochs} onChange={setNnvEpochs} type="number" /></Field>
          <RunButton loading={nnV.loading} onClick={() => nnV.run(() => gpNnApi.nnVolSurface(nnvStrikes.split(',').map(Number), nnvExpiries.split(',').map(Number), nnvVols.split(',').map(Number), nnvQS.split(',').map(Number), nnvQE.split(',').map(Number), JSON.parse(nnvLayers), Number(nnvEpochs)))} />
        </Row>
        <ResultPanel data={nnV.result} error={nnV.error} />
      </EndpointCard>

      <EndpointCard title="NN Portfolio" description="Neural Network portfolio optimization">
        <Row>
          <Field label="Returns (JSON 2D)" width="350px"><Input value={nnpReturns} onChange={setNnpReturns} /></Field>
          <Field label="Risk Aversion"><Input value={nnpRA} onChange={setNnpRA} type="number" /></Field>
          <Field label="Layers (JSON)" width="100px"><Input value={nnpLayers} onChange={setNnpLayers} /></Field>
          <RunButton loading={nnP.loading} onClick={() => nnP.run(() => gpNnApi.nnPortfolio(JSON.parse(nnpReturns), Number(nnpRA), JSON.parse(nnpLayers)))} />
        </Row>
        <ResultPanel data={nnP.result} error={nnP.error} />
      </EndpointCard>
    </>
  );
}
