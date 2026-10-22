// Independent numerical oracle for SVG emitted by the production C++ renderer.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {JSDOM} from 'jsdom';
import * as d3 from 'd3-shape';

const lock = JSON.parse(readFileSync(new URL('package-lock.json', import.meta.url)));
assert.equal(lock.packages['node_modules/d3-shape'].version, '3.2.0');
assert.equal(JSON.parse(readFileSync(new URL('node_modules/d3-shape/package.json', import.meta.url))).version, '3.2.0');
const names = {linear: d3.curveLinear, natural: d3.curveNatural, cardinal: d3.curveCardinal,
  catmullRom: d3.curveCatmullRom, monotoneX: d3.curveMonotoneX, monotoneY: d3.curveMonotoneY,
  step: d3.curveStep, stepBefore: d3.curveStepBefore, stepAfter: d3.curveStepAfter,
  bumpX: d3.curveBumpX, bumpY: d3.curveBumpY};

function expected(edge) {
  const out = [];
  d3.line().curve(names[edge.curve || 'linear']).context({
    moveTo: (...p) => out.push(['M', ...p]), lineTo: (...p) => out.push(['L', ...p]),
    bezierCurveTo: (...p) => out.push(['C', ...p]), closePath: () => out.push(['Z']),
  })(edge.points);
  return out;
}
function tokens(path) {
  return (path.match(/[MLCZ]|[-+]?(?:\d*\.?\d+)(?:[eE][+-]?\d+)?/g) ?? [])
    .map(v => /^[MLCZ]$/.test(v) ? v : Number(v));
}
// Independent diagnostic sampling proves the known legacy refusal is a real
// intersection. Acceptance still requires the native conservative certificate.
function collisionCount(edges, boxes) {
  let hits = 0;
  for (const edge of edges) {
    if (!edge.curve || edge.curve === 'linear') continue;
    let current;
    for (const c of expected(edge)) {
      if (c[0] === 'M') { current = c.slice(1); continue; }
      if (c[0] === 'Z') continue;
      for (let i = 0; i <= 128; ++i) {
        const t = i / 128, u = 1-t;
        const p = c[0] === 'L'
          ? [current[0]*u+c[1]*t, current[1]*u+c[2]*t]
          : [current[0]*u*u*u+3*c[1]*u*u*t+3*c[3]*u*t*t+c[5]*t*t*t,
             current[1]*u*u*u+3*c[2]*u*u*t+3*c[4]*u*t*t+c[6]*t*t*t];
        if (boxes.some(([x,y,w,h]) => p[0] > x+1e-8 && p[0] < x+w-1e-8 && p[1] > y+1e-8 && p[1] < y+h-1e-8)) ++hits;
      }
      current = c.slice(-2);
    }
  }
  return hits;
}
let count = 0;
for (const base of process.argv.slice(2)) {
  const records = JSON.parse(readFileSync(base + '.json'));
  const commands = records.map(expected);
  const boxes = JSON.parse(readFileSync(base + '.nodes.json'));
  if (base.endsWith('.legacy')) assert.ok(collisionCount(records, boxes) > 0, 'legacy refusal must have independent collision evidence');
  if (base.endsWith('.professional')) assert.equal(collisionCount(records, boxes), 0, 'professional fixture curves must clear nodes');
  let minX = 0, minY = 0;
  for (const path of commands) for (const c of path) for (let i = 1; i < c.length; i += 2) {
    minX = Math.min(minX, c[i]); minY = Math.min(minY, c[i + 1]);
  }
  const dom = new JSDOM(readFileSync(base + '.svg', 'utf8'), {contentType: 'image/svg+xml'});
  const root = dom.window.document.documentElement;
  const [, , width, height] = root.getAttribute('viewBox').split(' ').map(Number);
  const paths = [...root.querySelectorAll('path.edge-path')];
  assert.equal(paths.length, records.length);
  for (let i = 0; i < records.length; ++i) {
    const edge = records[i], path = paths[i];
    assert.equal(path.getAttribute('data-edge-index'), String(i));
    assert.equal(path.getAttribute('data-edge-id'), edge.id);
    assert.equal(path.getAttribute('data-edge-curve'), edge.curve);
    const actual = tokens(path.getAttribute('d'));
    const want = commands[i].flatMap(c => c.map((v, j) => {
      if (!j) return v;
      return v + (j % 2 ? 50 - minX : 50 - minY);
    }));
    assert.equal(actual.length, want.length, `${base} ${edge.id}: command count`);
    for (let j = 0; j < want.length; ++j) {
      if (typeof want[j] === 'string') assert.equal(actual[j], want[j], `${edge.id}: command kind ${j}`);
      else {
        assert.ok(Number.isFinite(actual[j]));
        assert.ok(Math.abs(actual[j] - want[j]) <= 1e-10 * Math.max(1, Math.abs(want[j])),
          `${edge.id}: coordinate ${j}: ${actual[j]} vs upstream ${want[j]}`);
      }
    }
    // All controls lie inside the computed viewport, so every cubic's convex
    // hull does too. Curved overshoot must not be clipped by knot-only bounds.
    for (const c of commands[i]) for (let j = 1; j < c.length; j += 2) {
      assert.ok(c[j] + 50 - minX >= 0 && c[j] + 50 - minX < width);
      assert.ok(c[j + 1] + 50 - minY >= 0 && c[j + 1] + 50 - minY < height);
    }
    ++count;
  }
  dom.window.close();
}
assert.equal(process.argv.length, 5, 'Supply generated cases and both real-fixture layout artifacts');
assert.ok(count > 300, 'Expected broad generated geometry and both original-fixture layout modes');
console.log(`Pinned D3 3.2.0: ${count} production SVG paths match command types and all control/end coordinates`);
