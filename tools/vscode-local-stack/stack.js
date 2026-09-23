'use strict';
const capabilities = ['Database', 'Shard', 'ControllerEligible', 'ClientIngress'];
function defaults() {
  return {
    program: '${workspaceFolder}/build/AtlasNetNode', cwd: '${workspaceFolder}/build',
    preLaunchCommand: 'cmake --build "${workspaceFolder}/build" --parallel',
    gdb: '/usr/bin/gdb', clusterPort: 42000, handshakePort: 43000,
    valgrind: false, valgrindProgram: '/usr/bin/valgrind',
    perf: false, perfProgram: '/usr/bin/perf', perfOutput: '${workspaceFolder}/build/perf-${index}.data',
    valkeyURI: 'tcp://127.0.0.1:46379', launchValkey: true,
    valkeyProgram: '${workspaceFolder}/external/valkey/src/valkey-server', valkeyPort: 46379,
    groups: [
      { name: 'Database', count: 3, capabilities: ['Database'], args: [] },
      { name: 'Shard', count: 2, capabilities: ['Shard'], args: [] },
      { name: 'Controller + Database + Client', count: 2, capabilities: ['ControllerEligible', 'Database', 'ClientIngress'], args: [] }
    ]
  };
}
function plan(config, root) {
  const fail = message => { throw new Error(message); };
  const string = (v, name) => typeof v === 'string' && v.trim() ? v : fail(`${name} is required`);
  const port = (v, name) => Number.isInteger(v) && v > 0 && v <= 65535 ? v : fail(`${name} must be 1–65535`);
  const expand = value => value.replaceAll('${workspaceFolder}', root);
  for (const key of ['program', 'cwd', 'gdb', 'valkeyURI']) string(config[key], key);
  if (config.valgrind) string(config.valgrindProgram, 'Valgrind executable');
  if (config.perf) {
    string(config.perfProgram, 'perf executable');
    string(config.perfOutput, 'perf output path');
  }
  if (!Array.isArray(config.groups)) fail('Groups must be an array');
  const nodes = [];
  config.groups.forEach((group, groupIndex) => {
    string(group.name, 'Group name');
    if (!Number.isInteger(group.count) || group.count < 0 || group.count > 100) fail('Counts must be integers from 0 to 100');
    if (!Array.isArray(group.capabilities) || !group.capabilities.length || group.capabilities.some(c => !capabilities.includes(c))) fail('Select valid capabilities');
    if (!Array.isArray(group.args) || group.args.some(a => typeof a !== 'string')) fail('Arguments must be a JSON array of strings');
    // Generated networking and capability settings have dedicated controls.
    if (group.args.some(a => /^--(cluster-port|handshake-port|capabilities|DB-host|DB-port|valkey-uri)(=|$)/.test(a))) fail('Use panel controls for ports, capabilities, registry, and Valkey URI');
    for (let i = 1; i <= group.count; i++) nodes.push({ group, groupIndex, instance: i, index: nodes.length });
  });
  if (!nodes.length || nodes.length > 100) fail('Stack must contain 1–100 nodes');
  const database = nodes.find(n => n.group.capabilities.includes('Database'));
  if (!database) fail('Include at least one Database node for the local registry');
  const used = new Set();
  for (const n of nodes) {
    n.cluster = port(config.clusterPort + n.index, 'Cluster port');
    n.handshake = port(config.handshakePort + n.index, 'Handshake port');
    for (const p of [n.cluster, n.handshake]) { if (used.has(p)) fail(`UDP port ${p} is duplicated`); used.add(p); }
  }
  const debug = (name, program, args, node) => {
    const setupCommands = [{ text: '-enable-pretty-printing', ignoreFailures: true }];
    if (node) setupCommands.push(
      // Retain forked workers as inferiors while keeping the node selected.
      // schedule-multiple makes Continue resume the node and every worker.
      { text: 'set follow-fork-mode parent' },
      { text: 'set detach-on-fork off' },
      { text: 'set schedule-multiple on' },
      { text: 'set follow-exec-mode same' },
      { text: 'set breakpoint pending on' }
    );
    if (node && (config.valgrind || config.perf)) {
      const wrapper = [];
      if (config.perf) {
        const output = expand(config.perfOutput).replaceAll('${index}', String(node.index)).replaceAll('${instance}', String(node.instance));
        wrapper.push(expand(config.perfProgram), 'record', '--call-graph', 'dwarf', '-o', output, '--');
      }
      if (config.valgrind) wrapper.push(expand(config.valgrindProgram), '--vgdb=no', '--leak-check=full', '--track-origins=yes');
      setupCommands.push({ text: 'set exec-wrapper ' + wrapper.map(gdbQuote).join(' ') });
    }
    return { name, type: 'cppdbg', request: 'launch', program: expand(program),
      cwd: expand(config.cwd), args, MIMode: 'gdb', miDebuggerPath: expand(config.gdb), stopAtEntry: false,
      externalConsole: false, setupCommands };
  };
  const result = [];
  if (config.launchValkey) {
    string(config.valkeyProgram, 'Valkey executable'); port(config.valkeyPort, 'Valkey port');
    if (config.valkeyURI !== `tcp://127.0.0.1:${config.valkeyPort}`) fail('Managed Valkey URI must match tcp://127.0.0.1:<Valkey port>');
    result.push(debug('AtlasNet / Valkey', config.valkeyProgram, ['--bind', '127.0.0.1', '--port', String(config.valkeyPort), '--save', '', '--appendonly', 'no', '--daemonize', 'no']));
  }
  // Database processes register directly in Valkey and must precede dependent nodes.
  nodes.sort((a, b) => Number(b.group.capabilities.includes('Database')) - Number(a.group.capabilities.includes('Database')));
  for (const n of nodes) {
    const isDB = n.group.capabilities.includes('Database');
    const args = ['--capabilities', n.group.capabilities.join(','), '--cluster-port', String(n.cluster), '--handshake-port', String(n.handshake)];
    if (isDB) args.push('--valkey-uri', config.valkeyURI);
    else args.push('--DB-host', '127.0.0.1', '--DB-port', String(database.handshake));
    args.push(...n.group.args.map(a => expand(a).replaceAll('${index}', String(n.index)).replaceAll('${instance}', String(n.instance))));
    result.push(debug(`AtlasNet / ${n.groupIndex + 1}: ${n.group.name} #${n.instance}`, config.program, args, n));
  }
  return result;
}
module.exports = { defaults, plan, capabilities };

function gdbQuote(value) {
  return JSON.stringify(String(value));
}
