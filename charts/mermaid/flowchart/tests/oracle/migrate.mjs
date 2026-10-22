// A separately reviewed test-expectation migration. The seven historical
// output snapshots and all original Mermaid source files remain untouched.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync, readdirSync, writeFileSync} from 'node:fs';
import {JSDOM} from 'jsdom';

const golden = new URL('../../../golden/flowchart/', import.meta.url);
const lock = JSON.parse(readFileSync(new URL('package-lock.json', import.meta.url)));
assert.equal(lock.packages['node_modules/mermaid'].version, '11.12.0');
assert.equal(JSON.parse(readFileSync(new URL('node_modules/mermaid/package.json', import.meta.url))).version, '11.12.0');
const write = process.argv.includes('--write');
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const read = path => readFileSync(new URL(path, golden));
const selected = new Set(['mermaid-005', 'mermaid-006', 'mermaid-024',
  'mermaid-031', 'mermaid-035', 'v2-mermaid-054', 'v2-mermaid-104']);
const dom = new JSDOM('<!doctype html><html><body></body></html>');
globalThis.window = dom.window;
globalThis.document = dom.window.document;
const {default: mermaid} = await import('mermaid');
mermaid.initialize({startOnLoad: false, securityLevel: 'strict'});

// This is the pre-existing sparse JSON adapter's arrow projection, not a
// claim that its legacy schema captures double-ended arrow semantics.
const legacyType = type => type === 'double_arrow_point' ? 'arrow_point' : type;
// The legacy adapter always reports dotted-arrow length as 1. Retain that
// existing limitation explicitly; this migration changes order, not geometry.
const legacyLength = edge => edge.stroke === 'dotted' ? 1 : edge.length;
const manifest = {schema: 'stun-flowchart-test-contract/1', cases: {}};
const receipt = {mermaid: '11.12.0',
  integrity: lock.packages['node_modules/mermaid'].integrity,
  lockSha256: hash(readFileSync(new URL('package-lock.json', import.meta.url))),
  adapterPolicy: {doubleArrowPoint: 'arrow_point', dottedLength: 1,
    edgeIdsAndCurves: 'retained in AST accessors; omitted by legacy JSON'},
  changes: []};
const inputs = readdirSync(golden).filter(name => name.endsWith('.input.mmd')).sort();
assert.equal(inputs.length, 95, 'Review corpus changes explicitly');
for (const filename of inputs) {
  const name = filename.slice(0, -'.input.mmd'.length);
  const source = read(filename);
  const historical = read(`${name}.output.json`);
  let expected = historical;
  if (selected.has(name)) {
    const diagram = await mermaid.mermaidAPI.getDiagramFromText(source.toString('utf8'));
    const old = JSON.parse(historical);
    const pending = [...old.links];
    const links = diagram.db.getEdges().map(edge => {
      const matches = pending.filter(link => link.source === edge.start &&
        link.target === edge.end && link.length === legacyLength(edge) &&
        link.stroke === edge.stroke && link.type === legacyType(edge.type) &&
        (link.text?.text ?? '') === edge.text);
      assert.equal(matches.length, 1, `${name}: ambiguous or changed edge facts`);
      const link = matches[0];
      pending.splice(pending.indexOf(link), 1);
      return link; // Preserve the ENTIRE historical link object, including metadata.
    });
    assert.equal(pending.length, 0, `${name}: lost/extra legacy edges`);
    const ids = [...diagram.db.getVertices().keys()];
    assert(ids.every(id => Object.hasOwn(old.nodes, id)), `${name}: added node`);
    const removedNodes = Object.keys(old.nodes).filter(id => !ids.includes(id));
    const namedIds = new Set(diagram.db.getEdges().filter(e => e.isUserDefinedId).map(e => e.id));
    assert(removedNodes.every(id => namedIds.has(id)), `${name}: non-edge node removed`);
    const nodes = Object.fromEntries(Object.entries(old.nodes).filter(([id]) => ids.includes(id)));
    const migrated = {...old, links, nodes};
    assert.notDeepEqual(migrated, old, `${name}: unnecessary migration`);
    expected = Buffer.from(JSON.stringify(migrated, null, 2) + '\n');
    const target = `${name}.source-v1.json`;
    if (write) writeFileSync(new URL(target, golden), expected);
    else assert.equal(read(target).toString(), expected.toString(), `${name}: stale independent expectation`);
    receipt.changes.push({name, inputSha256: hash(source), historicalSha256: hash(historical),
      expectedSha256: hash(expected), removedNodes,
      originalOrder: old.links.map(e => [e.source, e.target]),
      sourceOrder: links.map(e => [e.source, e.target])});
  }
  manifest.cases[name] = {expected: selected.has(name) ? 'source-v1' : 'output',
    inputSha256: hash(source), expectedSha256: hash(expected)};
}
for (const [name, data] of [['contracts.json', manifest], ['source-v1-provenance.json', receipt]]) {
  const bytes = JSON.stringify(data, null, 2) + '\n';
  if (write) writeFileSync(new URL(name, golden), bytes);
  else assert.equal(read(name).toString(), bytes, `${name}: stale contract/provenance`);
}
console.log('Verified explicit 95-case contract; 7 source-derived expectations, no edge facts dropped, historical snapshots unchanged.');
dom.window.close();
