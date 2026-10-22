// Independent parser evidence. Never rewrites fixtures or replaces Stun's
// strict golden gate. Dependencies are isolated from the C11 runtime.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync, writeFileSync} from 'node:fs';
import {JSDOM} from 'jsdom';

const here = new URL('./', import.meta.url);
const golden = new URL('../../../golden/flowchart/', here);
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const lockBytes = readFileSync(new URL('package-lock.json', here));
const lock = JSON.parse(lockBytes);
assert.equal(lock.packages['node_modules/mermaid'].version, '11.12.0');
assert.equal(lock.packages['node_modules/jsdom'].version, '26.1.0');
const installed = JSON.parse(readFileSync(new URL('node_modules/mermaid/package.json', here)));
assert.equal(installed.version, '11.12.0');

const dom = new JSDOM('<!doctype html><html><body></body></html>');
globalThis.window = dom.window;
globalThis.document = dom.window.document;
const {default: mermaid} = await import('mermaid');
mermaid.initialize({startOnLoad: false, securityLevel: 'strict'});

async function parse(source) {
  const diagram = await mermaid.mermaidAPI.getDiagramFromText(source);
  return {
    nodes: [...diagram.db.getVertices().keys()],
    edges: diagram.db.getEdges().map(edge => ({
      source: edge.start, target: edge.end, id: edge.id,
      curve: edge.interpolate ?? null, styles: edge.style ?? [],
    })),
  };
}
const endpoints = edges => edges.map(({source, target}) => [source, target]);
const cases = ['mermaid-024', 'mermaid-035', 'v2-mermaid-054',
  'v2-mermaid-104', 'mermaid-006', 'mermaid-005', 'mermaid-031'];
const evidence = {
  purpose: 'Independent diagnostic evidence; does not accept Stun golden differences',
  mermaidVersion: installed.version,
  mermaidIntegrity: lock.packages['node_modules/mermaid'].integrity,
  lockSha256: hash(lockBytes), cases: [],
};
for (const name of cases) {
  const results = {};
  for (const kind of ['input', 'output']) {
    const source = readFileSync(new URL(`${name}.${kind}.mmd`, golden));
    const expectedBytes = readFileSync(new URL(`${name}.${kind}.json`, golden));
    const expected = JSON.parse(expectedBytes);
    const actual = await parse(source.toString('utf8'));
    // Only endpoint order is compared here. A legacy input.json is not an
    // oracle for node metadata, edge IDs, styles or other Mermaid semantics.
    assert.deepEqual(endpoints(actual.edges), endpoints(expected.links),
      `${name}.${kind}: pinned Mermaid disagrees with legacy endpoint order`);
    results[kind] = {sourceSha256: hash(source), jsonSha256: hash(expectedBytes), ...actual};
  }
  assert.notDeepEqual(endpoints(results.input.edges), endpoints(results.output.edges),
    `${name}: expected historical rewrite order discrepancy disappeared`);
  evidence.cases.push({name, ...results});
}

const styled = evidence.cases.find(test => test.name === 'mermaid-024');
assert.deepEqual(endpoints(styled.input.edges.filter(e => e.styles.includes('stroke:greenyellow'))), [['B', 'C']]);
assert.deepEqual(endpoints(styled.output.edges.filter(e => e.styles.includes('stroke:greenyellow'))), [['C', 'D']]);
const named = evidence.cases.find(test => test.name === 'v2-mermaid-104');
assert.deepEqual(named.input.nodes, ['A', 'B', 'E', 'D', 'C', 'F']);
assert.deepEqual(named.input.edges.map(e => e.id), ['e1', 'e5', 'e7', 'e3', 'e2', 'e4', 'e6', 'e8']);
assert.deepEqual(named.input.edges.map(e => e.curve), ['natural', 'linear', 'cardinal', 'monotoneY', 'stepAfter', 'bumpY', 'catmullRom', null]);
assert.deepEqual(named.output.nodes.filter(id => /^e[1-7]$/.test(id)), ['e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7']);

// Match the native regression's lookup timing and unknown-ID behavior.
const timing = await parse('flowchart TD\nA e1@--> B\ne1@{ curve: natural }\ne2@{ curve: stepAfter }\n');
assert.deepEqual(timing.nodes, ['A', 'B', 'e2']);
assert.equal(timing.edges[0].curve, 'natural');
const before = await parse('flowchart TD\ne1@{ curve: natural }\nA e1@--> B\n');
assert.deepEqual(before.nodes, ['e1', 'A', 'B']);
assert.equal(before.edges[0].curve, null);
evidence.lookupTiming = {after: timing, before};

if (process.argv[2]) writeFileSync(process.argv[2], JSON.stringify(evidence, null, 2) + '\n');
console.log(`Mermaid ${installed.version}: verified 14 source/JSON endpoint orders, linkStyle retargeting, named-edge metadata and lookup timing.`);
console.log('Diagnostic evidence only. Stun strict golden and full runtime acceptance remain separate.');
dom.window.close();
