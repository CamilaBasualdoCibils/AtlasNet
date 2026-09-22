'use strict';
const vscode = require('vscode');
const fs = require('node:fs/promises');
const crypto = require('node:crypto');
const net = require('node:net');
const { defaults, plan } = require('./stack');

function tcpOpen(port) {
  return new Promise(resolve => {
    const socket = net.connect({ host: '127.0.0.1', port });
    const finish = value => { socket.destroy(); resolve(value); };
    socket.setTimeout(200);
    socket.once('connect', () => finish(true));
    socket.once('error', () => finish(false));
    socket.once('timeout', () => finish(false));
  });
}
function activate(context) {
  let view, busy = false, cancelled = false, runId;
  const sessions = new Map();
  const output = vscode.window.createOutputChannel('AtlasNet Local Stack');
  const folder = () => {
    const folders = vscode.workspace.workspaceFolders;
    if (!folders || folders.length !== 1) throw new Error('Open a single AtlasNet workspace folder.');
    return folders[0];
  };
  const configUri = () => vscode.Uri.joinPath(folder().uri, '.vscode', 'atlasnet-stack.json');
  const send = message => view?.webview.postMessage(message);
  const status = () => send({ type: 'status', busy, sessions: [...sessions.values()].map(s => ({ id: s.id, name: s.name })) });
  const report = error => { output.appendLine(String(error)); send({ type: 'error', message: error.message || String(error) }); };
  async function read() {
    try { return JSON.parse(Buffer.from(await vscode.workspace.fs.readFile(configUri())).toString()); }
    catch (error) { if (error.code === 'FileNotFound' || error.code === 'ENOENT') return defaults(); throw error; }
  }
  async function stop() {
    cancelled = true;
    await Promise.all([...sessions.values()].reverse().map(s => vscode.debug.stopDebugging(s)));
  }
  async function run(config) {
    if (busy || sessions.size) throw new Error('Stop the current stack before running another.');
    if (!vscode.workspace.isTrusted) throw new Error('Trust this workspace before running processes.');
    const workspace = folder();
    const launches = plan(config, workspace.uri.fsPath);
    busy = true; cancelled = false; runId = crypto.randomUUID(); status();
    try {
      for (const p of new Set(launches.flatMap(c => [c.program, c.miDebuggerPath]))) await fs.access(p, fs.constants.X_OK);
      await fs.access(launches[0].cwd);
      if (config.launchValkey && await tcpOpen(config.valkeyPort)) throw new Error(`Valkey port ${config.valkeyPort} is already in use. Choose another port or disable managed Valkey.`);
      await vscode.workspace.fs.createDirectory(vscode.Uri.joinPath(workspace.uri, '.vscode'));
      await vscode.workspace.fs.writeFile(configUri(), Buffer.from(JSON.stringify(config, null, 2) + '\n'));
      for (const [index, launch] of launches.entries()) {
        if (cancelled) break;
        launch.__atlasnetRun = runId;
        output.appendLine(`Launching ${launch.name}`);
        const started = await vscode.debug.startDebugging(workspace, launch);
        if (!started) throw new Error(`Debugger could not launch ${launch.name}. See Debug Console.`);
        if (cancelled) { await stop(); break; }
        if (config.launchValkey && index === 0) {
          const deadline = Date.now() + 15000;
          while (!cancelled && !(await tcpOpen(config.valkeyPort))) {
            if (Date.now() > deadline) throw new Error('Valkey did not become ready within 15 seconds. See Debug Console.');
            await new Promise(resolve => setTimeout(resolve, 100));
          }
        }
      }
    } catch (error) { await stop(); throw error; }
    finally { busy = false; status(); }
  }
  context.subscriptions.push(output,
    vscode.debug.onDidStartDebugSession(session => {
      if (session.configuration.__atlasnetRun === runId && runId) {
        sessions.set(session.id, session); status();
        if (cancelled) void vscode.debug.stopDebugging(session);
      }
    }),
    vscode.debug.onDidTerminateDebugSession(session => {
      if (sessions.delete(session.id)) { output.appendLine(`Stopped ${session.name}`); status(); }
    }),
    vscode.commands.registerCommand('atlasnetLocalStack.open', () => vscode.commands.executeCommand('atlasnetLocalStack.panel.focus')),
    vscode.window.registerWebviewViewProvider('atlasnetLocalStack.panel', {
      async resolveWebviewView(webviewView) {
        view = webviewView;
        const webview = view.webview;
        const media = vscode.Uri.joinPath(context.extensionUri, 'media');
        webview.options = { enableScripts: true, localResourceRoots: [media] };
        const nonce = crypto.randomBytes(16).toString('hex');
        const html = await fs.readFile(vscode.Uri.joinPath(media, 'panel.html').fsPath, 'utf8');
        webview.html = html.replaceAll('NONCE', nonce).replaceAll('SCRIPT_URI', webview.asWebviewUri(vscode.Uri.joinPath(media, 'panel.js')).toString());
        context.subscriptions.push(webview.onDidReceiveMessage(async message => {
          try {
            if (message.type === 'ready') { send({ type: 'config', config: await read() }); status(); }
            else if (message.type === 'run') await run(message.config);
            else if (message.type === 'save') {
              plan(message.config, folder().uri.fsPath);
              await vscode.workspace.fs.createDirectory(vscode.Uri.joinPath(folder().uri, '.vscode'));
              await vscode.workspace.fs.writeFile(configUri(), Buffer.from(JSON.stringify(message.config, null, 2) + '\n'));
              send({ type: 'notice', message: 'Saved .vscode/atlasnet-stack.json' });
            } else if (message.type === 'stop') await stop();
            else if (message.type === 'stopOne') {
              const session = sessions.get(message.id);
              if (session) await vscode.debug.stopDebugging(session);
            }
          } catch (error) { report(error); }
        }));
      }
    })
  );
}
module.exports = { activate };
