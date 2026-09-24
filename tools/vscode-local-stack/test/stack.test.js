'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { defaults, plan } = require('../stack');
test('example stack launches Valkey then five database-capable nodes then two shards', () => {
  const result = plan(defaults(), '/repo');
  assert.equal(result.length, 8);
  assert.equal(result[0].program, '/repo/external/valkey/src/valkey-server');
  assert.deepEqual(result.slice(1, 6).map(c => c.args.includes('--valkey-uri')), [true, true, true, true, true]);
  for (const launch of result.slice(6)) {
    assert.equal(launch.args[launch.args.indexOf('--DB-port') + 1], '43000');
    assert.equal(launch.type, 'cppdbg');
    assert.equal(launch.MIMode, 'gdb');
    assert.deepEqual(launch.setupCommands.slice(1, 6).map(c => c.text), [
      'set follow-fork-mode parent', 'set detach-on-fork off',
      'set schedule-multiple on', 'set follow-exec-mode same',
      'set breakpoint pending on'
    ]);
  }
  const ports = result.slice(1).flatMap(c => [c.args[3], c.args[5]]);
  assert.equal(new Set(ports).size, 14);
});
test('arguments preserve whitespace and expand replica placeholders without a shell', () => {
  const config = defaults();
  config.groups[0].args = ['two words', '${workspaceFolder}/${instance}/${index}', '$(touch /tmp/nope)'];
  assert.deepEqual(plan(config, '/repo')[2].args.slice(-3), ['two words', '/repo/2/1', '$(touch /tmp/nope)']);
});
test('invalid counts, collisions, missing database, and generated flag overrides are rejected', () => {
  for (const mutate of [c => c.groups[0].count = -1, c => c.handshakePort = c.clusterPort,
    c => c.clusterPort = 65535, c => c.groups = [c.groups[1]],
    c => c.groups[0].args = ['--DB-port=1'], c => c.groups[0].args = [5],
    c => c.valkeyPort = 1234]) {
    const config = defaults(); mutate(config); assert.throws(() => plan(config, '/repo'));
  }
});
test('external Valkey is not launched and zero-count groups are omitted', () => {
  const config = defaults(); config.launchValkey = false; config.groups[1].count = 0;
  assert.equal(plan(config, '/repo').length, 5);
});

test('Valgrind and perf can wrap every node together while Valkey remains direct', () => {
  const config = defaults(); config.valgrind = true; config.perf = true;
  const result = plan(config, '/repo');
  assert.equal(result[0].setupCommands.length, 1);
  for (const launch of result.slice(1)) {
    const wrapper = launch.setupCommands.find(c => c.text.startsWith('set exec-wrapper ')).text;
    assert.match(wrapper, /^set exec-wrapper /);
    assert.match(wrapper, /\/usr\/bin\/perf.*record.*--.*\/usr\/bin\/valgrind/);
    assert.match(wrapper, /perf-[0-9]+\.data/);
  }
});
