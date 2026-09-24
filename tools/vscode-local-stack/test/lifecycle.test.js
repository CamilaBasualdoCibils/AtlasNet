'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const Module = require('node:module');
const { defaults } = require('../stack');
test('launch failure cleans up only stack sessions, and force stop cancels launch', async () => {
  const handlers = {};
  const stopped = [], messages = [];
  let starts = 0, fail = true, taskRuns = 0, taskExit = 0;
  const vscode = {
    Task: class { constructor(...args) { this.args = args; } },
    ShellExecution: class { constructor(command, options) { this.command = command; this.options = options; } },
    TaskRevealKind: { Always: 1 }, TaskPanelKind: { Dedicated: 1 },
    Uri: { joinPath: (base, ...parts) => ({ fsPath: require('node:path').join(base.fsPath, ...parts) }) },
    workspace: { isTrusted: true, workspaceFolders: [{ uri: { fsPath: '/tmp' } }], fs: { createDirectory: async () => {}, writeFile: async () => {} } },
    window: { createOutputChannel: () => ({ appendLine() {} }), registerWebviewViewProvider: (_, provider) => { handlers.provider = provider; } },
    commands: { registerCommand() {} },
    tasks: {
      onDidEndTaskProcess: cb => { handlers.taskEnd = cb; return { dispose() {} }; },
      executeTask: async task => { taskRuns++; handlers.task = task; const execution = { task, terminate() {} }; setImmediate(() => handlers.taskEnd({ execution, exitCode: taskExit })); return execution; }
    },
    debug: {
      onDidStartDebugSession: cb => { handlers.start = cb; },
      onDidTerminateDebugSession: cb => { handlers.end = cb; },
      startDebugging: async (_, configuration) => {
        starts++;
        if (fail && starts === 2) return false;
        handlers.start({ id: String(starts), name: configuration.name, configuration });
        if (!fail) await handlers.message({ type: 'forceStop' });
        return true;
      },
      stopDebugging: async s => { stopped.push(s.id); handlers.end(s); }
    }
  };
  const original = Module._load;
  Module._load = function(name, ...args) { return name === 'vscode' ? vscode : original.call(this, name, ...args); };
  let activate;
  try { ({ activate } = require('../extension')); } finally { Module._load = original; }
  activate({ subscriptions: [], extensionUri: { fsPath: require('node:path').resolve(__dirname, '..') } });
  await handlers.provider.resolveWebviewView({ webview: {
    asWebviewUri: uri => uri.fsPath, postMessage: m => messages.push(m),
    onDidReceiveMessage: cb => { handlers.message = cb; }
  } });
  const config = defaults(); config.launchValkey = false; config.program = process.execPath; config.gdb = process.execPath; config.cwd = '/tmp';
  handlers.start({ id: 'unrelated', configuration: {} });
  await handlers.message({ type: 'run', config });
  assert.deepEqual(stopped, ['1']);
  assert.ok(messages.some(m => m.type === 'error' && m.message.includes('could not launch')));
  fail = false; starts = 0; stopped.length = 0;
  await handlers.message({ type: 'run', config });
  assert.equal(starts, 1);
  assert.deepEqual(stopped, ['1']);
  assert.equal(taskRuns, 2);
  assert.match(handlers.task.args[4].command, /cmake --build "\/tmp\/build" --parallel/);
  taskExit = 2; starts = 0; stopped.length = 0;
  await handlers.message({ type: 'run', config });
  assert.equal(starts, 0);
  assert.ok(messages.some(m => m.type === 'error' && m.message.includes('exit code 2')));
});

test('graceful stop pauses nodes and asks GDB to deliver SIGTERM', async () => {
  const handlers = {}, requests = [], stopped = [];
  const vscode = {
    Uri: { joinPath: (base, ...parts) => ({ fsPath: require('node:path').join(base.fsPath, ...parts) }) },
    workspace: { isTrusted: true, workspaceFolders: [{ uri: { fsPath: '/tmp' } }], fs: { createDirectory: async () => {}, writeFile: async () => {}, readFile: async () => { const error = new Error(); error.code = 'ENOENT'; throw error; } } },
    window: { createOutputChannel: () => ({ appendLine() {} }), registerWebviewViewProvider: (_, provider) => { handlers.provider = provider; } },
    commands: { registerCommand() {} },
    debug: {
      onDidStartDebugSession: cb => { handlers.start = cb; }, onDidTerminateDebugSession: cb => { handlers.end = cb; },
      startDebugging: async (_, configuration) => { const session = { id: 'node', name: configuration.name, configuration, customRequest: async (command, args) => { requests.push([command, args]); return command === 'threads' ? { threads: [{ id: 7 }] } : {}; } }; handlers.start(session); return true; },
      stopDebugging: async session => stopped.push(session.id)
    }
  };
  const Module = require('node:module'), original = Module._load;
  Module._load = function(name, ...args) { return name === 'vscode' ? vscode : original.call(this, name, ...args); };
  delete require.cache[require.resolve('../extension')];
  let activate; try { ({ activate } = require('../extension')); } finally { Module._load = original; }
  activate({ subscriptions: [], extensionUri: { fsPath: require('node:path').resolve(__dirname, '..') } });
  await handlers.provider.resolveWebviewView({ webview: { asWebviewUri: uri => uri.fsPath, postMessage() {}, onDidReceiveMessage: cb => { handlers.message = cb; } } });
  const config = defaults(); config.preLaunchCommand = ''; config.launchValkey = false; config.program = process.execPath; config.gdb = process.execPath; config.cwd = '/tmp'; config.groups = [{ name: 'Database', count: 1, capabilities: ['Database'], args: [] }];
  await handlers.message({ type: 'run', config });
  await handlers.message({ type: 'gracefulStop' });
  assert.deepEqual(requests.map(r => r[0]), ['threads', 'pause', 'evaluate', 'evaluate']);
  assert.equal(requests[2][1].expression, '-exec inferior 1');
  assert.equal(requests[3][1].expression, '-exec signal SIGTERM');
  assert.deepEqual(stopped, []);
});
