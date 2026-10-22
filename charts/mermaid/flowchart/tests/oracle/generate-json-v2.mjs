// Explicitly generated review artifact. CI verifies; --write is opt-in.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync, readdirSync, writeFileSync} from 'node:fs';
import {JSDOM} from 'jsdom';

const golden = new URL('../../../golden/flowchart/', import.meta.url);
const output = new URL('../json-v2-contract.json', import.meta.url);
const read = name => readFileSync(new URL(name, golden));
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const lockBytes = readFileSync(new URL('package-lock.json', import.meta.url));
const lock = JSON.parse(lockBytes);
assert.equal(lock.packages['node_modules/mermaid'].version, '11.12.0');
assert.equal(JSON.parse(readFileSync(new URL('node_modules/mermaid/package.json', import.meta.url))).version, '11.12.0');
const legacy = JSON.parse(read('contracts.json'));
assert.equal(legacy.schema, 'stun-flowchart-test-contract/1');
const dom = new JSDOM('<html><body></body></html>');
globalThis.window = dom.window; globalThis.document = dom.window.document;
const {default: mermaid} = await import('mermaid');
mermaid.initialize({startOnLoad: false, securityLevel: 'strict'});

async function edges(source) {
  const diagram = await mermaid.mermaidAPI.getDiagramFromText(source);
  return diagram.db.getEdges().map((e, i) => {
    const head = e.type === 'arrow_open' ? 'none' : e.type.replace(/^double_/, '');
    const link = {declarationIndex: i, source: e.start, target: e.end,
      length: e.length, stroke: e.stroke, type: e.type,
      arrowTypeStart: e.type.startsWith('double_') ? head : 'none', arrowTypeEnd: head,
      id: e.isUserDefinedId ? e.id : null, curve: e.interpolate ?? null};
    if (e.text) link.text = {text: e.text, type: 'text'};
    return link;
  });
}

const manifest = {schema: 'stun-flowchart-json-v2-test/1',
  outputSchema: 'stun.flowchart/2', mermaidVersion: '11.12.0',
  mermaidIntegrity: lock.packages['node_modules/mermaid'].integrity,
  lockSha256: hash(lockBytes),
  policy: 'Non-edge fields and source label objects retain the explicit legacy corpus contract; ordered edge identity, type, stroke and length come from pinned upstream semantics.',
  cases: {}, regressions: {}};
const inputs = readdirSync(golden).filter(n => n.endsWith('.input.mmd')).sort();
assert.equal(inputs.length, 95);
for (const file of inputs) {
  const name = file.slice(0, -'.input.mmd'.length), source = read(file);
  const contract = legacy.cases[name];
  assert.equal(hash(source), contract.inputSha256);
  assert.ok(['output', 'source-v1'].includes(contract.expected));
  const baseline = read(`${name}.${contract.expected}.json`);
  assert.equal(hash(baseline), contract.expectedSha256);
  const expected = JSON.parse(baseline);
  const pending = [...expected.links];
  expected.links = (await edges(source.toString('utf8'))).map(link => {
    const endpoints = pending.filter(old => old.source === link.source && old.target === link.target);
    // A bijection preserves source label bytes (e.g. <br/> versus <br>).
    // Where endpoints repeat, exact labels must uniquely identify the edge.
    const matches = endpoints.length > 1 ? endpoints.filter(old =>
      (old.text?.text ?? '') === (link.text?.text ?? '')) : endpoints;
    assert.equal(matches.length, 1, `${name}: ambiguous/missing legacy edge-label identity`);
    const old = matches[0];
    assert.ok(Object.keys(old).every(k => ['source','target','length','stroke','type','text'].includes(k)), `${name}: unreviewed legacy edge facts`);
    pending.splice(pending.indexOf(old), 1);
    delete link.text;
    if (old.text) link.text = old.text;
    return link;
  });
  assert.equal(pending.length, 0, `${name}: discarded legacy edge`);
  expected.schema = 'stun.flowchart/2';
  manifest.cases[name] = {sourceSha256: hash(source), baselineSha256: hash(baseline), expected};
}

const ops = ['-->', '---', '<-->', '==>', '===', '<==>', '.->', '<.->',
  '-.-', '-.->', '<-.->', '--o', 'o--o', '--x', 'x--x', 'o--x', '~~~'];
for (let i = 1; i <= 13; ++i) ops.push('-' + '.'.repeat(i) + '->',
  '<-' + '.'.repeat(i) + '->', '-'.repeat(i + 1) + '>', '='.repeat(i + 1) + '>');
const regressions = {
  strokes: 'flowchart LR\n' + ops.map((op, i) => `A${i} ${op} B${i}`).join('\n') + '\n',
  labels: 'flowchart LR\nA -- label ---> B\nB <-- second label ---> C\nC -->|quoted| D\n',
  named: 'flowchart TD\nA first@--> B second@==> C\nA third@-.-> B\nA fourth@<--> B\n' +
    'first@{ curve: natural }\nthird@{ curve: stepAfter }\nlinkStyle 2 stroke:red,stroke-width:3px\n',
  duplicate_ids: 'flowchart TD\nA same@--> B\nA same@--> B\nsame@{ curve: natural }\n',
  timing: 'flowchart TD\ne1@{ curve: natural }\nA e1@--> B\ne1@{ curve: linear }\n',
  escaping: 'flowchart LR\nA -->|"hello & world"| B\n',
};
for (const [name, source] of Object.entries(regressions))
  manifest.regressions[name] = {source, sourceSha256: hash(source), expectedLinks: await edges(source)};

// One full graph per line keeps the generated artifact compact; arrays retain
// declaration order. Nothing is derived from Stun's actual output.
const lines = ['{'];
for (const [key, value] of Object.entries(manifest)) {
  const comma = key === 'regressions' ? '' : ',';
  if (key === 'cases' || key === 'regressions') {
    lines.push(`  ${JSON.stringify(key)}: {`);
    const entries = Object.entries(value);
    entries.forEach(([k, v], i) => lines.push(`    ${JSON.stringify(k)}: ${JSON.stringify(v)}${i+1 < entries.length ? ',' : ''}`));
    lines.push('  }' + comma);
  } else lines.push(`  ${JSON.stringify(key)}: ${JSON.stringify(value)},`);
}
lines.push('}', '');
const bytes = lines.join('\n');
if (process.argv.includes('--write')) writeFileSync(output, bytes);
else assert.equal(readFileSync(output, 'utf8'), bytes, 'Regenerate the explicit JSON v2 contract for review');
console.log(`Pinned Mermaid 11.12.0 JSON v2: 95 complete graph contracts and ${Object.keys(regressions).length} edge regression groups verified`);
dom.window.close();
