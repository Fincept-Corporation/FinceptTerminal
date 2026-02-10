import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { interpApi, linalgApi } from '../quantlibNumericalApi';

const sampleX = '1, 2, 3, 4, 5';
const sampleY = '2.1, 3.9, 6.2, 7.8, 10.1';

export default function InterpLinalgPanel() {
  // Interpolate
  const interp = useEndpoint();
  const [intX, setIntX] = useState(sampleX);
  const [intY, setIntY] = useState(sampleY);
  const [intXi, setIntXi] = useState('2.5');
  const [intM, setIntM] = useState('linear');

  // Spline Curve
  const sc = useEndpoint();
  const [scX, setScX] = useState(sampleX);
  const [scY, setScY] = useState(sampleY);
  const [scXi, setScXi] = useState('1.5, 2.5, 3.5, 4.5');
  const [scBC, setScBC] = useState('natural');

  // Spline Derivative
  const sd = useEndpoint();
  const [sdX, setSdX] = useState(sampleX);
  const [sdY, setSdY] = useState(sampleY);
  const [sdXi, setSdXi] = useState('3');

  // Matmul
  const mm = useEndpoint();
  const [mmA, setMmA] = useState('[[1,2],[3,4]]');
  const [mmB, setMmB] = useState('[[5,6],[7,8]]');

  // Matvec
  const mv = useEndpoint();
  const [mvA, setMvA] = useState('[[1,2],[3,4]]');
  const [mvX, setMvX] = useState('1, 2');

  // Dot
  const dot = useEndpoint();
  const [dotA, setDotA] = useState('1, 2, 3');
  const [dotB, setDotB] = useState('4, 5, 6');

  // Norm
  const nm = useEndpoint();
  const [nmV, setNmV] = useState('3, 4');
  const [nmP, setNmP] = useState('2');

  // Outer
  const out = useEndpoint();
  const [outA, setOutA] = useState('1, 2, 3');
  const [outB, setOutB] = useState('4, 5');

  // Transpose
  const tr = useEndpoint();
  const [trA, setTrA] = useState('[[1,2,3],[4,5,6]]');

  // Decompose
  const dec = useEndpoint();
  const [decA, setDecA] = useState('[[4,3],[6,3]]');
  const [decM, setDecM] = useState('lu');

  // Solve
  const sol = useEndpoint();
  const [solA, setSolA] = useState('[[2,1],[5,3]]');
  const [solB, setSolB] = useState('4, 7');

  // Least Squares
  const ls = useEndpoint();
  const [lsA, setLsA] = useState('[[1,1],[1,2],[1,3]]');
  const [lsB, setLsB] = useState('1, 2, 2');

  // Inverse
  const inv = useEndpoint();
  const [invA, setInvA] = useState('[[4,7],[2,6]]');

  return (
    <>
      <EndpointCard title="Interpolate" description="1D interpolation at a point">
        <Row>
          <Field label="x data" width="200px"><Input value={intX} onChange={setIntX} /></Field>
          <Field label="y data" width="200px"><Input value={intY} onChange={setIntY} /></Field>
          <Field label="xi"><Input value={intXi} onChange={setIntXi} type="number" /></Field>
          <Field label="Method"><Select value={intM} onChange={setIntM} options={['linear', 'cubic', 'nearest']} /></Field>
          <RunButton loading={interp.loading} onClick={() => interp.run(() => interpApi.evaluate(intX.split(',').map(Number), intY.split(',').map(Number), Number(intXi), intM))} />
        </Row>
        <ResultPanel data={interp.result} error={interp.error} />
      </EndpointCard>

      <EndpointCard title="Spline Curve" description="Evaluate spline at multiple points">
        <Row>
          <Field label="x data" width="200px"><Input value={scX} onChange={setScX} /></Field>
          <Field label="y data" width="200px"><Input value={scY} onChange={setScY} /></Field>
          <Field label="xi (comma-sep)" width="200px"><Input value={scXi} onChange={setScXi} /></Field>
          <Field label="BC"><Select value={scBC} onChange={setScBC} options={['natural', 'clamped', 'not-a-knot']} /></Field>
          <RunButton loading={sc.loading} onClick={() => sc.run(() => interpApi.splineCurve(scX.split(',').map(Number), scY.split(',').map(Number), scXi.split(',').map(Number), scBC))} />
        </Row>
        <ResultPanel data={sc.result} error={sc.error} />
      </EndpointCard>

      <EndpointCard title="Spline Derivative" description="Spline derivative at a point">
        <Row>
          <Field label="x data" width="200px"><Input value={sdX} onChange={setSdX} /></Field>
          <Field label="y data" width="200px"><Input value={sdY} onChange={setSdY} /></Field>
          <Field label="xi"><Input value={sdXi} onChange={setSdXi} type="number" /></Field>
          <RunButton loading={sd.loading} onClick={() => sd.run(() => interpApi.splineDerivative(sdX.split(',').map(Number), sdY.split(',').map(Number), Number(sdXi)))} />
        </Row>
        <ResultPanel data={sd.result} error={sd.error} />
      </EndpointCard>

      <EndpointCard title="Matrix Multiply" description="A × B matrix product">
        <Row>
          <Field label="A (JSON)" width="200px"><Input value={mmA} onChange={setMmA} /></Field>
          <Field label="B (JSON)" width="200px"><Input value={mmB} onChange={setMmB} /></Field>
          <RunButton loading={mm.loading} onClick={() => mm.run(() => linalgApi.matmul(JSON.parse(mmA), JSON.parse(mmB)))} />
        </Row>
        <ResultPanel data={mm.result} error={mm.error} />
      </EndpointCard>

      <EndpointCard title="Matrix-Vector" description="A × x product">
        <Row>
          <Field label="A (JSON)" width="200px"><Input value={mvA} onChange={setMvA} /></Field>
          <Field label="x" width="150px"><Input value={mvX} onChange={setMvX} /></Field>
          <RunButton loading={mv.loading} onClick={() => mv.run(() => linalgApi.matvec(JSON.parse(mvA), mvX.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={mv.result} error={mv.error} />
      </EndpointCard>

      <EndpointCard title="Dot Product" description="Vector dot product">
        <Row>
          <Field label="a" width="200px"><Input value={dotA} onChange={setDotA} /></Field>
          <Field label="b" width="200px"><Input value={dotB} onChange={setDotB} /></Field>
          <RunButton loading={dot.loading} onClick={() => dot.run(() => linalgApi.dot(dotA.split(',').map(Number), dotB.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={dot.result} error={dot.error} />
      </EndpointCard>

      <EndpointCard title="Vector Norm" description="Lp norm of a vector">
        <Row>
          <Field label="v" width="200px"><Input value={nmV} onChange={setNmV} /></Field>
          <Field label="p"><Input value={nmP} onChange={setNmP} type="number" /></Field>
          <RunButton loading={nm.loading} onClick={() => nm.run(() => linalgApi.norm(nmV.split(',').map(Number), Number(nmP)))} />
        </Row>
        <ResultPanel data={nm.result} error={nm.error} />
      </EndpointCard>

      <EndpointCard title="Outer Product" description="a ⊗ b outer product">
        <Row>
          <Field label="a" width="200px"><Input value={outA} onChange={setOutA} /></Field>
          <Field label="b" width="200px"><Input value={outB} onChange={setOutB} /></Field>
          <RunButton loading={out.loading} onClick={() => out.run(() => linalgApi.outer(outA.split(',').map(Number), outB.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={out.result} error={out.error} />
      </EndpointCard>

      <EndpointCard title="Transpose" description="Matrix transpose">
        <Row>
          <Field label="A (JSON)" width="300px"><Input value={trA} onChange={setTrA} /></Field>
          <RunButton loading={tr.loading} onClick={() => tr.run(() => linalgApi.transpose(JSON.parse(trA)))} />
        </Row>
        <ResultPanel data={tr.result} error={tr.error} />
      </EndpointCard>

      <EndpointCard title="Matrix Decompose" description="LU / QR / Cholesky / SVD decomposition">
        <Row>
          <Field label="A (JSON)" width="200px"><Input value={decA} onChange={setDecA} /></Field>
          <Field label="Method"><Select value={decM} onChange={setDecM} options={['lu', 'qr', 'cholesky', 'svd', 'eigen']} /></Field>
          <RunButton loading={dec.loading} onClick={() => dec.run(() => linalgApi.decompose(JSON.parse(decA), decM))} />
        </Row>
        <ResultPanel data={dec.result} error={dec.error} />
      </EndpointCard>

      <EndpointCard title="Linear Solve" description="Solve Ax = b">
        <Row>
          <Field label="A (JSON)" width="200px"><Input value={solA} onChange={setSolA} /></Field>
          <Field label="b" width="150px"><Input value={solB} onChange={setSolB} /></Field>
          <RunButton loading={sol.loading} onClick={() => sol.run(() => linalgApi.solve(JSON.parse(solA), solB.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={sol.result} error={sol.error} />
      </EndpointCard>

      <EndpointCard title="Least Squares" description="Solve Ax ≈ b in least squares sense">
        <Row>
          <Field label="A (JSON)" width="250px"><Input value={lsA} onChange={setLsA} /></Field>
          <Field label="b" width="150px"><Input value={lsB} onChange={setLsB} /></Field>
          <RunButton loading={ls.loading} onClick={() => ls.run(() => linalgApi.lstsq(JSON.parse(lsA), lsB.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={ls.result} error={ls.error} />
      </EndpointCard>

      <EndpointCard title="Matrix Inverse" description="Compute A⁻¹">
        <Row>
          <Field label="A (JSON)" width="250px"><Input value={invA} onChange={setInvA} /></Field>
          <RunButton loading={inv.loading} onClick={() => inv.run(() => linalgApi.inverse(JSON.parse(invA)))} />
        </Row>
        <ResultPanel data={inv.result} error={inv.error} />
      </EndpointCard>
    </>
  );
}
