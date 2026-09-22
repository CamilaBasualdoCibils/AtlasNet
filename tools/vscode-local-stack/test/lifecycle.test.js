'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const Module = require('node:module');
const { defaults } = require('../stack');
test('launch failure cleans up only stack sessions, and Stop cancels launch', async () => {
  const handlers = {};
  const stopped = [], messages = [];
  let starts = 0, fail = true;
  const vscode = {
    Uri: { joinPath: (base, ...parts) => ({ fsPath: require('node:path').join(base.fsPath, ...parts) }) },
    workspace: { isTrusted: true, workspaceFolders: [{ uri: { fsPath: '/tmp' } }], fs: { createDirectory: async () => {}, writeFile: async () => {} } },
    window: { createOutputChannel: () => ({ appendLine() {} }), registerWebviewViewProvider: (_, provider) => { handlers.provider = provider; } },
    commands: { registerCommand() {} },
    debug: {
      onDidStartDebugSession: cb => { handlers.start = cb; },
      onDidTerminateDebugSession: cb => { handlers.end = cb; },
      startDebugging: async (_, configuration) => {
        starts++;
        if (fail && starts === 2) return false;
        handlers.start({ id: String(starts), name: configuration.name, configuration });
        if (!fail) await handlers.message({ type: 'stop' });
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
});
