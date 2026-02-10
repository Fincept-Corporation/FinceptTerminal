import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { gameApi } from '../quantlibEconomicsApi';

const sampleP1 = '[[3,0],[5,1]]';
const sampleP2 = '[[3,5],[0,1]]';

export default function GameTheoryPanel() {
  // Create Game
  const cg = useEndpoint();
  const [cgP1, setCgP1] = useState(sampleP1);
  const [cgP2, setCgP2] = useState(sampleP2);

  // Classic Game
  const cl = useEndpoint();
  const [clName, setClName] = useState('prisoners_dilemma');

  // Nash Check
  const nc = useEndpoint();
  const [ncP1, setNcP1] = useState(sampleP1);
  const [ncP2, setNcP2] = useState(sampleP2);
  const [ncS1, setNcS1] = useState('1, 0');
  const [ncS2, setNcS2] = useState('1, 0');

  // Mixed Nash
  const mn = useEndpoint();
  const [mnP1, setMnP1] = useState(sampleP1);
  const [mnP2, setMnP2] = useState(sampleP2);

  // Fictitious Play
  const fp = useEndpoint();
  const [fpP1, setFpP1] = useState(sampleP1);
  const [fpP2, setFpP2] = useState(sampleP2);
  const [fpIter, setFpIter] = useState('1000');

  // Best Response
  const br = useEndpoint();
  const [brP1, setBrP1] = useState(sampleP1);
  const [brP2, setBrP2] = useState(sampleP2);
  const [brS1, setBrS1] = useState('0.5, 0.5');
  const [brS2, setBrS2] = useState('0.5, 0.5');

  // Eliminate Dominated
  const ed = useEndpoint();
  const [edP1, setEdP1] = useState(sampleP1);
  const [edP2, setEdP2] = useState(sampleP2);

  return (
    <>
      <EndpointCard title="Create Game" description="Analyze a two-player normal-form game">
        <Row>
          <Field label="Payoff Matrix 1 (JSON)" width="250px"><Input value={cgP1} onChange={setCgP1} /></Field>
          <Field label="Payoff Matrix 2 (JSON)" width="250px"><Input value={cgP2} onChange={setCgP2} /></Field>
          <RunButton loading={cg.loading} onClick={() => cg.run(() => gameApi.create(JSON.parse(cgP1), JSON.parse(cgP2)))} />
        </Row>
        <ResultPanel data={cg.result} error={cg.error} />
      </EndpointCard>

      <EndpointCard title="Classic Game" description="Load a classic game by name">
        <Row>
          <Field label="Game Name"><Select value={clName} onChange={setClName} options={['prisoners_dilemma', 'battle_of_sexes', 'matching_pennies', 'stag_hunt', 'chicken']} /></Field>
          <RunButton loading={cl.loading} onClick={() => cl.run(() => gameApi.classic(clName))} />
        </Row>
        <ResultPanel data={cl.result} error={cl.error} />
      </EndpointCard>

      <EndpointCard title="Nash Equilibrium Check" description="Check if a strategy profile is a Nash equilibrium">
        <Row>
          <Field label="Payoff 1 (JSON)" width="200px"><Input value={ncP1} onChange={setNcP1} /></Field>
          <Field label="Payoff 2 (JSON)" width="200px"><Input value={ncP2} onChange={setNcP2} /></Field>
          <Field label="Strategy 1" width="120px"><Input value={ncS1} onChange={setNcS1} /></Field>
          <Field label="Strategy 2" width="120px"><Input value={ncS2} onChange={setNcS2} /></Field>
          <RunButton loading={nc.loading} onClick={() => nc.run(() => gameApi.nashCheck(JSON.parse(ncP1), JSON.parse(ncP2), ncS1.split(',').map(Number), ncS2.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={nc.result} error={nc.error} />
      </EndpointCard>

      <EndpointCard title="Mixed Nash" description="Compute mixed-strategy Nash equilibria">
        <Row>
          <Field label="Payoff Matrix 1 (JSON)" width="250px"><Input value={mnP1} onChange={setMnP1} /></Field>
          <Field label="Payoff Matrix 2 (JSON)" width="250px"><Input value={mnP2} onChange={setMnP2} /></Field>
          <RunButton loading={mn.loading} onClick={() => mn.run(() => gameApi.mixedNash(JSON.parse(mnP1), JSON.parse(mnP2)))} />
        </Row>
        <ResultPanel data={mn.result} error={mn.error} />
      </EndpointCard>

      <EndpointCard title="Fictitious Play" description="Simulate fictitious play convergence">
        <Row>
          <Field label="Payoff Matrix 1 (JSON)" width="220px"><Input value={fpP1} onChange={setFpP1} /></Field>
          <Field label="Payoff Matrix 2 (JSON)" width="220px"><Input value={fpP2} onChange={setFpP2} /></Field>
          <Field label="Iterations"><Input value={fpIter} onChange={setFpIter} type="number" /></Field>
          <RunButton loading={fp.loading} onClick={() => fp.run(() => gameApi.fictitiousPlay(JSON.parse(fpP1), JSON.parse(fpP2), Number(fpIter)))} />
        </Row>
        <ResultPanel data={fp.result} error={fp.error} />
      </EndpointCard>

      <EndpointCard title="Best Response" description="Compute best response to opponent strategy">
        <Row>
          <Field label="Payoff 1 (JSON)" width="200px"><Input value={brP1} onChange={setBrP1} /></Field>
          <Field label="Payoff 2 (JSON)" width="200px"><Input value={brP2} onChange={setBrP2} /></Field>
          <Field label="Strategy 1" width="120px"><Input value={brS1} onChange={setBrS1} /></Field>
          <Field label="Strategy 2" width="120px"><Input value={brS2} onChange={setBrS2} /></Field>
          <RunButton loading={br.loading} onClick={() => br.run(() => gameApi.bestResponse(JSON.parse(brP1), JSON.parse(brP2), brS1.split(',').map(Number), brS2.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={br.result} error={br.error} />
      </EndpointCard>

      <EndpointCard title="Eliminate Dominated" description="Iteratively eliminate dominated strategies">
        <Row>
          <Field label="Payoff Matrix 1 (JSON)" width="250px"><Input value={edP1} onChange={setEdP1} /></Field>
          <Field label="Payoff Matrix 2 (JSON)" width="250px"><Input value={edP2} onChange={setEdP2} /></Field>
          <RunButton loading={ed.loading} onClick={() => ed.run(() => gameApi.eliminateDominated(JSON.parse(edP1), JSON.parse(edP2)))} />
        </Row>
        <ResultPanel data={ed.result} error={ed.error} />
      </EndpointCard>
    </>
  );
}
