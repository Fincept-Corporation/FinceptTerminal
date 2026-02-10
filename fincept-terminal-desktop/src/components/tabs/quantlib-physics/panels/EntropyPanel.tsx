import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { entropyApi, divergenceApi, boltzmannApi, maxEntropyApi } from '../quantlibPhysicsApi';

export default function EntropyPanel() {
  // Shannon
  const shan = useEndpoint();
  const [shanP, setShanP] = useState('0.25, 0.25, 0.25, 0.25');

  // Renyi
  const ren = useEndpoint();
  const [renP, setRenP] = useState('0.25, 0.25, 0.25, 0.25');
  const [renAlpha, setRenAlpha] = useState('2');

  // Tsallis
  const tsa = useEndpoint();
  const [tsaP, setTsaP] = useState('0.25, 0.25, 0.25, 0.25');
  const [tsaQ, setTsaQ] = useState('2');

  // Cross Entropy
  const cross = useEndpoint();
  const [crossP, setCrossP] = useState('0.25, 0.25, 0.25, 0.25');
  const [crossQ, setCrossQ] = useState('0.1, 0.2, 0.3, 0.4');

  // Joint Entropy
  const joint = useEndpoint();
  const [jointProb, setJointProb] = useState('[[0.1,0.2],[0.3,0.4]]');

  // Conditional Entropy
  const cond = useEndpoint();
  const [condProb, setCondProb] = useState('[[0.1,0.2],[0.3,0.4]]');

  // Mutual Information
  const mi = useEndpoint();
  const [miProb, setMiProb] = useState('[[0.1,0.2],[0.3,0.4]]');

  // Transfer Entropy
  const te = useEndpoint();
  const [teX, setTeX] = useState('1, 2, 3, 4, 5, 4, 3, 2, 1, 2, 3');
  const [teY, setTeY] = useState('2, 3, 4, 5, 6, 5, 4, 3, 2, 3, 4');
  const [teLag, setTeLag] = useState('1');

  // Markov Entropy Rate
  const mer = useEndpoint();
  const [merMat, setMerMat] = useState('[[0.7,0.3],[0.4,0.6]]');

  // Differential Entropy
  const de = useEndpoint();
  const [deDist, setDeDist] = useState('gaussian');
  const [deSigma, setDeSigma] = useState('1');

  // Fisher Information
  const fi = useEndpoint();
  const [fiDist, setFiDist] = useState('gaussian');
  const [fiTheta, setFiTheta] = useState('1');

  // KL Divergence
  const kl = useEndpoint();
  const [klP, setKlP] = useState('0.25, 0.25, 0.25, 0.25');
  const [klQ, setKlQ] = useState('0.1, 0.2, 0.3, 0.4');

  // JS Divergence
  const js = useEndpoint();
  const [jsP, setJsP] = useState('0.25, 0.25, 0.25, 0.25');
  const [jsQ, setJsQ] = useState('0.1, 0.2, 0.3, 0.4');

  // Boltzmann
  const boltz = useEndpoint();
  const [boltzE, setBoltzE] = useState('0, 1, 2, 3, 4');
  const [boltzT, setBoltzT] = useState('1');

  // Max Entropy
  const maxE = useEndpoint();
  const [maxN, setMaxN] = useState('4');
  const [maxC, setMaxC] = useState('[0.5, 1.0]');

  return (
    <>
      <EndpointCard title="Shannon Entropy" description="H(X) = -sum p*log(p)">
        <Row>
          <Field label="Probabilities (comma-sep)" width="350px"><Input value={shanP} onChange={setShanP} /></Field>
          <RunButton loading={shan.loading} onClick={() => shan.run(() => entropyApi.shannon(shanP.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={shan.result} error={shan.error} />
      </EndpointCard>

      <EndpointCard title="Renyi Entropy" description="Generalized entropy of order alpha">
        <Row>
          <Field label="Probabilities (comma-sep)" width="300px"><Input value={renP} onChange={setRenP} /></Field>
          <Field label="Alpha"><Input value={renAlpha} onChange={setRenAlpha} type="number" /></Field>
          <RunButton loading={ren.loading} onClick={() => ren.run(() => entropyApi.renyi(renP.split(',').map(Number), Number(renAlpha)))} />
        </Row>
        <ResultPanel data={ren.result} error={ren.error} />
      </EndpointCard>

      <EndpointCard title="Tsallis Entropy" description="Non-extensive entropy (q-entropy)">
        <Row>
          <Field label="Probabilities (comma-sep)" width="300px"><Input value={tsaP} onChange={setTsaP} /></Field>
          <Field label="q"><Input value={tsaQ} onChange={setTsaQ} type="number" /></Field>
          <RunButton loading={tsa.loading} onClick={() => tsa.run(() => entropyApi.tsallis(tsaP.split(',').map(Number), Number(tsaQ)))} />
        </Row>
        <ResultPanel data={tsa.result} error={tsa.error} />
      </EndpointCard>

      <EndpointCard title="Cross Entropy" description="H(p, q) = -sum p*log(q)">
        <Row>
          <Field label="p (comma-sep)" width="250px"><Input value={crossP} onChange={setCrossP} /></Field>
          <Field label="q (comma-sep)" width="250px"><Input value={crossQ} onChange={setCrossQ} /></Field>
          <RunButton loading={cross.loading} onClick={() => cross.run(() => entropyApi.cross(crossP.split(',').map(Number), crossQ.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={cross.result} error={cross.error} />
      </EndpointCard>

      <EndpointCard title="Joint Entropy" description="H(X, Y) from joint probability table">
        <Row>
          <Field label="Joint Prob (JSON 2D)" width="350px"><Input value={jointProb} onChange={setJointProb} /></Field>
          <RunButton loading={joint.loading} onClick={() => joint.run(() => entropyApi.joint(JSON.parse(jointProb)))} />
        </Row>
        <ResultPanel data={joint.result} error={joint.error} />
      </EndpointCard>

      <EndpointCard title="Conditional Entropy" description="H(Y|X) from joint probability table">
        <Row>
          <Field label="Joint Prob (JSON 2D)" width="350px"><Input value={condProb} onChange={setCondProb} /></Field>
          <RunButton loading={cond.loading} onClick={() => cond.run(() => entropyApi.conditional(JSON.parse(condProb)))} />
        </Row>
        <ResultPanel data={cond.result} error={cond.error} />
      </EndpointCard>

      <EndpointCard title="Mutual Information" description="I(X;Y) from joint probability table">
        <Row>
          <Field label="Joint Prob (JSON 2D)" width="350px"><Input value={miProb} onChange={setMiProb} /></Field>
          <RunButton loading={mi.loading} onClick={() => mi.run(() => entropyApi.mutualInformation(JSON.parse(miProb)))} />
        </Row>
        <ResultPanel data={mi.result} error={mi.error} />
      </EndpointCard>

      <EndpointCard title="Transfer Entropy" description="Directed information transfer between series">
        <Row>
          <Field label="X History (comma-sep)" width="250px"><Input value={teX} onChange={setTeX} /></Field>
          <Field label="Y History (comma-sep)" width="250px"><Input value={teY} onChange={setTeY} /></Field>
          <Field label="Lag"><Input value={teLag} onChange={setTeLag} type="number" /></Field>
          <RunButton loading={te.loading} onClick={() => te.run(() => entropyApi.transfer(teX.split(',').map(Number), teY.split(',').map(Number), Number(teLag)))} />
        </Row>
        <ResultPanel data={te.result} error={te.error} />
      </EndpointCard>

      <EndpointCard title="Markov Entropy Rate" description="Entropy rate of a Markov chain">
        <Row>
          <Field label="Transition Matrix (JSON 2D)" width="350px"><Input value={merMat} onChange={setMerMat} /></Field>
          <RunButton loading={mer.loading} onClick={() => mer.run(() => entropyApi.markovRate(JSON.parse(merMat)))} />
        </Row>
        <ResultPanel data={mer.result} error={mer.error} />
      </EndpointCard>

      <EndpointCard title="Differential Entropy" description="Continuous distribution entropy">
        <Row>
          <Field label="Distribution"><Select value={deDist} onChange={setDeDist} options={['gaussian', 'exponential', 'uniform']} /></Field>
          <Field label="Sigma / Rate"><Input value={deSigma} onChange={setDeSigma} type="number" /></Field>
          <RunButton loading={de.loading} onClick={() => de.run(() => entropyApi.differential(deDist, Number(deSigma)))} />
        </Row>
        <ResultPanel data={de.result} error={de.error} />
      </EndpointCard>

      <EndpointCard title="Fisher Information" description="Fisher information for distributions">
        <Row>
          <Field label="Distribution"><Select value={fiDist} onChange={setFiDist} options={['gaussian', 'exponential', 'poisson']} /></Field>
          <Field label="Theta"><Input value={fiTheta} onChange={setFiTheta} type="number" /></Field>
          <RunButton loading={fi.loading} onClick={() => fi.run(() => entropyApi.fisherInformation(fiDist, Number(fiTheta)))} />
        </Row>
        <ResultPanel data={fi.result} error={fi.error} />
      </EndpointCard>

      <EndpointCard title="KL Divergence" description="Kullback-Leibler divergence D_KL(p||q)">
        <Row>
          <Field label="p (comma-sep)" width="250px"><Input value={klP} onChange={setKlP} /></Field>
          <Field label="q (comma-sep)" width="250px"><Input value={klQ} onChange={setKlQ} /></Field>
          <RunButton loading={kl.loading} onClick={() => kl.run(() => divergenceApi.kl(klP.split(',').map(Number), klQ.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={kl.result} error={kl.error} />
      </EndpointCard>

      <EndpointCard title="JS Divergence" description="Jensen-Shannon divergence">
        <Row>
          <Field label="p (comma-sep)" width="250px"><Input value={jsP} onChange={setJsP} /></Field>
          <Field label="q (comma-sep)" width="250px"><Input value={jsQ} onChange={setJsQ} /></Field>
          <RunButton loading={js.loading} onClick={() => js.run(() => divergenceApi.js(jsP.split(',').map(Number), jsQ.split(',').map(Number)))} />
        </Row>
        <ResultPanel data={js.result} error={js.error} />
      </EndpointCard>

      <EndpointCard title="Boltzmann Distribution" description="Boltzmann probabilities from energy levels">
        <Row>
          <Field label="Energies (comma-sep)" width="300px"><Input value={boltzE} onChange={setBoltzE} /></Field>
          <Field label="Temperature"><Input value={boltzT} onChange={setBoltzT} type="number" /></Field>
          <RunButton loading={boltz.loading} onClick={() => boltz.run(() => boltzmannApi.distribution(boltzE.split(',').map(Number), Number(boltzT)))} />
        </Row>
        <ResultPanel data={boltz.result} error={boltz.error} />
      </EndpointCard>

      <EndpointCard title="Max Entropy Distribution" description="Maximum entropy distribution with constraints">
        <Row>
          <Field label="N States"><Input value={maxN} onChange={setMaxN} type="number" /></Field>
          <Field label="Constraints (JSON)" width="200px"><Input value={maxC} onChange={setMaxC} /></Field>
          <RunButton loading={maxE.loading} onClick={() => maxE.run(() => maxEntropyApi.distribution(Number(maxN), JSON.parse(maxC)))} />
        </Row>
        <ResultPanel data={maxE.result} error={maxE.error} />
      </EndpointCard>
    </>
  );
}
