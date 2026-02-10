import React, { useState } from 'react';
import { EndpointCard, Row, Field, Input, Select, RunButton, ResultPanel, useEndpoint } from '../../quantlib-core/shared';
import { thermoApi, isingApi } from '../quantlibPhysicsApi';

export default function ThermoPanel() {
  // Free Energy
  const fe = useEndpoint();
  const [feU, setFeU] = useState('1000');
  const [feT, setFeT] = useState('300');
  const [feS, setFeS] = useState('10');
  const [feP, setFeP] = useState('0');
  const [feV, setFeV] = useState('0');
  const [feType, setFeType] = useState('gibbs');

  // Carnot
  const car = useEndpoint();
  const [carHot, setCarHot] = useState('600');
  const [carCold, setCarCold] = useState('300');
  const [carQ, setCarQ] = useState('1000');

  // Van der Waals
  const vdw = useEndpoint();
  const [vdwA, setVdwA] = useState('0.3643');
  const [vdwB, setVdwB] = useState('0.00004267');
  const [vdwT, setVdwT] = useState('300');
  const [vdwV, setVdwV] = useState('0.0224');

  // Maxwell Relations
  const mx = useEndpoint();

  // Ideal Gas
  const ig = useEndpoint();
  const [igN, setIgN] = useState('1');
  const [igT, setIgT] = useState('300');
  const [igV, setIgV] = useState('1');

  // Clausius-Clapeyron
  const cc = useEndpoint();
  const [ccDH, setCcDH] = useState('40700');
  const [ccT, setCcT] = useState('373.15');

  // Joule-Thomson
  const jt = useEndpoint();
  const [jtCp, setJtCp] = useState('29.1');
  const [jtV, setJtV] = useState('0.0224');
  const [jtT, setJtT] = useState('300');

  // Ising Model
  const ising = useEndpoint();
  const [isSize, setIsSize] = useState('10');
  const [isJ, setIsJ] = useState('1');
  const [isH, setIsH] = useState('0');
  const [isT, setIsT] = useState('2');
  const [isSteps, setIsSteps] = useState('1000');

  // Ising Critical Temp
  const isCrit = useEndpoint();
  const [icJ, setIcJ] = useState('1');

  return (
    <>
      <EndpointCard title="Free Energy" description="Gibbs / Helmholtz free energy">
        <Row>
          <Field label="U (internal)"><Input value={feU} onChange={setFeU} type="number" /></Field>
          <Field label="T"><Input value={feT} onChange={setFeT} type="number" /></Field>
          <Field label="S (entropy)"><Input value={feS} onChange={setFeS} type="number" /></Field>
          <Field label="P"><Input value={feP} onChange={setFeP} type="number" /></Field>
          <Field label="V"><Input value={feV} onChange={setFeV} type="number" /></Field>
          <Field label="Type"><Select value={feType} onChange={setFeType} options={['gibbs', 'helmholtz']} /></Field>
          <RunButton loading={fe.loading} onClick={() => fe.run(() => thermoApi.freeEnergy(Number(feU), Number(feT), Number(feS), Number(feP), Number(feV), feType))} />
        </Row>
        <ResultPanel data={fe.result} error={fe.error} />
      </EndpointCard>

      <EndpointCard title="Carnot Cycle" description="Carnot efficiency and work output">
        <Row>
          <Field label="T_hot"><Input value={carHot} onChange={setCarHot} type="number" /></Field>
          <Field label="T_cold"><Input value={carCold} onChange={setCarCold} type="number" /></Field>
          <Field label="Q_hot"><Input value={carQ} onChange={setCarQ} type="number" /></Field>
          <RunButton loading={car.loading} onClick={() => car.run(() => thermoApi.carnot(Number(carHot), Number(carCold), Number(carQ)))} />
        </Row>
        <ResultPanel data={car.result} error={car.error} />
      </EndpointCard>

      <EndpointCard title="Van der Waals" description="Real gas equation of state">
        <Row>
          <Field label="a"><Input value={vdwA} onChange={setVdwA} type="number" /></Field>
          <Field label="b"><Input value={vdwB} onChange={setVdwB} type="number" /></Field>
          <Field label="T"><Input value={vdwT} onChange={setVdwT} type="number" /></Field>
          <Field label="V"><Input value={vdwV} onChange={setVdwV} type="number" /></Field>
          <RunButton loading={vdw.loading} onClick={() => vdw.run(() => thermoApi.vanDerWaals(Number(vdwA), Number(vdwB), Number(vdwT), Number(vdwV)))} />
        </Row>
        <ResultPanel data={vdw.result} error={vdw.error} />
      </EndpointCard>

      <EndpointCard title="Maxwell Relations" description="Thermodynamic Maxwell relations reference">
        <Row>
          <RunButton loading={mx.loading} onClick={() => mx.run(() => thermoApi.maxwellRelations())} />
        </Row>
        <ResultPanel data={mx.result} error={mx.error} />
      </EndpointCard>

      <EndpointCard title="Ideal Gas Law" description="PV = nRT calculations">
        <Row>
          <Field label="n (moles)"><Input value={igN} onChange={setIgN} type="number" /></Field>
          <Field label="T (K)"><Input value={igT} onChange={setIgT} type="number" /></Field>
          <Field label="V (m3)"><Input value={igV} onChange={setIgV} type="number" /></Field>
          <RunButton loading={ig.loading} onClick={() => ig.run(() => thermoApi.idealGas(Number(igN), Number(igT), Number(igV)))} />
        </Row>
        <ResultPanel data={ig.result} error={ig.error} />
      </EndpointCard>

      <EndpointCard title="Clausius-Clapeyron" description="Phase transition pressure-temperature relation">
        <Row>
          <Field label="Delta H (J/mol)"><Input value={ccDH} onChange={setCcDH} type="number" /></Field>
          <Field label="T (K)"><Input value={ccT} onChange={setCcT} type="number" /></Field>
          <RunButton loading={cc.loading} onClick={() => cc.run(() => thermoApi.clausiusClapeyron(Number(ccDH), Number(ccT)))} />
        </Row>
        <ResultPanel data={cc.result} error={cc.error} />
      </EndpointCard>

      <EndpointCard title="Joule-Thomson" description="Joule-Thomson coefficient for real gases">
        <Row>
          <Field label="Cp"><Input value={jtCp} onChange={setJtCp} type="number" /></Field>
          <Field label="V"><Input value={jtV} onChange={setJtV} type="number" /></Field>
          <Field label="T"><Input value={jtT} onChange={setJtT} type="number" /></Field>
          <RunButton loading={jt.loading} onClick={() => jt.run(() => thermoApi.jouleThomson(Number(jtCp), Number(jtV), Number(jtT)))} />
        </Row>
        <ResultPanel data={jt.result} error={jt.error} />
      </EndpointCard>

      <EndpointCard title="Ising Model" description="2D Ising model Monte Carlo simulation">
        <Row>
          <Field label="Size"><Input value={isSize} onChange={setIsSize} type="number" /></Field>
          <Field label="J"><Input value={isJ} onChange={setIsJ} type="number" /></Field>
          <Field label="h"><Input value={isH} onChange={setIsH} type="number" /></Field>
          <Field label="T"><Input value={isT} onChange={setIsT} type="number" /></Field>
          <Field label="Steps"><Input value={isSteps} onChange={setIsSteps} type="number" /></Field>
          <RunButton loading={ising.loading} onClick={() => ising.run(() => isingApi.simulate(Number(isSize), Number(isJ), Number(isH), Number(isT), Number(isSteps)))} />
        </Row>
        <ResultPanel data={ising.result} error={ising.error} />
      </EndpointCard>

      <EndpointCard title="Ising Critical Temperature" description="Exact critical temperature for 2D Ising">
        <Row>
          <Field label="J"><Input value={icJ} onChange={setIcJ} type="number" /></Field>
          <RunButton loading={isCrit.loading} onClick={() => isCrit.run(() => isingApi.criticalTemperature(Number(icJ)))} />
        </Row>
        <ResultPanel data={isCrit.result} error={isCrit.error} />
      </EndpointCard>
    </>
  );
}
