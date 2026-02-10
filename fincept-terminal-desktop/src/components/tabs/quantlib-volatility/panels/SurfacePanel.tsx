import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { surfaceApi } from '../quantlibVolatilityApi';

export default function SurfacePanel() {
  // Flat
  const flat = useEndpoint();
  const [flatVol, setFlatVol] = useState('0.2');
  const [flatExp, setFlatExp] = useState('1');
  const [flatStrike, setFlatStrike] = useState('100');

  // Term Structure
  const ts = useEndpoint();
  const [tsExpiries, setTsExpiries] = useState('0.25, 0.5, 1, 2');
  const [tsVols, setTsVols] = useState('0.18, 0.19, 0.2, 0.21');
  const [tsQuery, setTsQuery] = useState('0.75');

  // Grid
  const grid = useEndpoint();
  const [gridExpiries, setGridExpiries] = useState('0.25, 0.5, 1');
  const [gridStrikes, setGridStrikes] = useState('90, 100, 110');
  const [gridMatrix, setGridMatrix] = useState('[[0.22,0.20,0.21],[0.21,0.19,0.20],[0.20,0.18,0.19]]');
  const [gridQExp, setGridQExp] = useState('0.5');
  const [gridQStrike, setGridQStrike] = useState('100');

  // Smile
  const smile = useEndpoint();
  const [smExpiries, setSmExpiries] = useState('0.25, 0.5, 1');
  const [smStrikes, setSmStrikes] = useState('90, 100, 110');
  const [smMatrix, setSmMatrix] = useState('[[0.22,0.20,0.21],[0.21,0.19,0.20],[0.20,0.18,0.19]]');
  const [smQExp, setSmQExp] = useState('0.5');
  const [smQStrikes, setSmQStrikes] = useState('85, 90, 95, 100, 105, 110, 115');

  // Total Variance
  const tv = useEndpoint();
  const [tvExpiries, setTvExpiries] = useState('0.25, 0.5, 1');
  const [tvStrikes, setTvStrikes] = useState('90, 100, 110');
  const [tvMatrix, setTvMatrix] = useState('[[0.22,0.20,0.21],[0.21,0.19,0.20],[0.20,0.18,0.19]]');
  const [tvQExp, setTvQExp] = useState('0.5');
  const [tvQStrike, setTvQStrike] = useState('100');

  // From Points
  const fp = useEndpoint();
  const [fpTenors, setFpTenors] = useState('0.25, 0.5, 1');
  const [fpStrikes, setFpStrikes] = useState('90, 100, 110');
  const [fpVols, setFpVols] = useState('0.22, 0.20, 0.19');

  return (
    <>
      <EndpointCard title="Flat Volatility" description="Constant vol surface query">
        <Row>
          <Field label="Volatility"><Input value={flatVol} onChange={setFlatVol} type="number" /></Field>
          <Field label="Query Expiry"><Input value={flatExp} onChange={setFlatExp} type="number" /></Field>
          <Field label="Query Strike"><Input value={flatStrike} onChange={setFlatStrike} type="number" /></Field>
          <RunButton loading={flat.loading} onClick={() => flat.run(() =>
            surfaceApi.flat(Number(flatVol), Number(flatExp), Number(flatStrike))
          )} />
        </Row>
        <ResultPanel data={flat.result} error={flat.error} />
      </EndpointCard>

      <EndpointCard title="Term Structure Vol" description="Volatility term structure interpolation">
        <Row>
          <Field label="Expiries" width="200px"><Input value={tsExpiries} onChange={setTsExpiries} /></Field>
          <Field label="Volatilities" width="200px"><Input value={tsVols} onChange={setTsVols} /></Field>
          <Field label="Query Expiry"><Input value={tsQuery} onChange={setTsQuery} type="number" /></Field>
          <RunButton loading={ts.loading} onClick={() => ts.run(() =>
            surfaceApi.termStructure(tsExpiries.split(',').map(Number), tsVols.split(',').map(Number), Number(tsQuery))
          )} />
        </Row>
        <ResultPanel data={ts.result} error={ts.error} />
      </EndpointCard>

      <EndpointCard title="Surface Grid Query" description="Bilinear interpolation on expiry-strike grid">
        <Row>
          <Field label="Expiries" width="180px"><Input value={gridExpiries} onChange={setGridExpiries} /></Field>
          <Field label="Strikes" width="180px"><Input value={gridStrikes} onChange={setGridStrikes} /></Field>
          <Field label="Vol Matrix (JSON)" width="300px"><Input value={gridMatrix} onChange={setGridMatrix} /></Field>
        </Row>
        <Row>
          <Field label="Query Expiry"><Input value={gridQExp} onChange={setGridQExp} type="number" /></Field>
          <Field label="Query Strike"><Input value={gridQStrike} onChange={setGridQStrike} type="number" /></Field>
          <RunButton loading={grid.loading} onClick={() => grid.run(() =>
            surfaceApi.grid(
              gridExpiries.split(',').map(Number), gridStrikes.split(',').map(Number),
              JSON.parse(gridMatrix), Number(gridQExp), Number(gridQStrike)
            )
          )} />
        </Row>
        <ResultPanel data={grid.result} error={grid.error} />
      </EndpointCard>

      <EndpointCard title="Surface Smile" description="Volatility smile at given expiry">
        <Row>
          <Field label="Expiries" width="180px"><Input value={smExpiries} onChange={setSmExpiries} /></Field>
          <Field label="Strikes" width="180px"><Input value={smStrikes} onChange={setSmStrikes} /></Field>
          <Field label="Vol Matrix (JSON)" width="300px"><Input value={smMatrix} onChange={setSmMatrix} /></Field>
        </Row>
        <Row>
          <Field label="Query Expiry"><Input value={smQExp} onChange={setSmQExp} type="number" /></Field>
          <Field label="Query Strikes" width="300px"><Input value={smQStrikes} onChange={setSmQStrikes} /></Field>
          <RunButton loading={smile.loading} onClick={() => smile.run(() =>
            surfaceApi.smile(
              smExpiries.split(',').map(Number), smStrikes.split(',').map(Number),
              JSON.parse(smMatrix), Number(smQExp), smQStrikes.split(',').map(Number)
            )
          )} />
        </Row>
        <ResultPanel data={smile.result} error={smile.error} />
      </EndpointCard>

      <EndpointCard title="Surface Total Variance" description="Total variance at point on surface">
        <Row>
          <Field label="Expiries" width="180px"><Input value={tvExpiries} onChange={setTvExpiries} /></Field>
          <Field label="Strikes" width="180px"><Input value={tvStrikes} onChange={setTvStrikes} /></Field>
          <Field label="Vol Matrix (JSON)" width="300px"><Input value={tvMatrix} onChange={setTvMatrix} /></Field>
        </Row>
        <Row>
          <Field label="Query Expiry"><Input value={tvQExp} onChange={setTvQExp} type="number" /></Field>
          <Field label="Query Strike"><Input value={tvQStrike} onChange={setTvQStrike} type="number" /></Field>
          <RunButton loading={tv.loading} onClick={() => tv.run(() =>
            surfaceApi.totalVariance(
              tvExpiries.split(',').map(Number), tvStrikes.split(',').map(Number),
              JSON.parse(tvMatrix), Number(tvQExp), Number(tvQStrike)
            )
          )} />
        </Row>
        <ResultPanel data={tv.result} error={tv.error} />
      </EndpointCard>

      <EndpointCard title="Surface From Points" description="Build surface from scattered data points">
        <Row>
          <Field label="Tenors" width="200px"><Input value={fpTenors} onChange={setFpTenors} /></Field>
          <Field label="Strikes" width="200px"><Input value={fpStrikes} onChange={setFpStrikes} /></Field>
          <Field label="Vols" width="200px"><Input value={fpVols} onChange={setFpVols} /></Field>
          <RunButton loading={fp.loading} onClick={() => fp.run(() =>
            surfaceApi.fromPoints(fpTenors.split(',').map(Number), fpStrikes.split(',').map(Number), fpVols.split(',').map(Number))
          )} />
        </Row>
        <ResultPanel data={fp.result} error={fp.error} />
      </EndpointCard>
    </>
  );
}
