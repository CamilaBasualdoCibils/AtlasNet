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
  let view, busy = false, cancelled = false, runId, prelaunchExecution;
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
    try { return { ...defaults(), ...JSON.parse(Buffer.from(await vscode.workspace.fs.readFile(configUri())).toString()) }; }
    catch (error) { if (error.code === 'FileNotFound' || error.code === 'ENOENT') return defaults(); throw error; }
  }
  async function forceStop() {
    cancelled = true;
    prelaunchExecution?.terminate();
    await Promise.all([...sessions.values()].reverse().map(s => vscode.debug.stopDebugging(s)));
  }
  async function runPrelaunch(command, workspace) {
    if (!command || !command.trim()) return;
    const expanded = command.replaceAll('${workspaceFolder}', workspace.uri.fsPath);
    output.appendLine('Prelaunch: ' + expanded);
    const task = new vscode.Task(
      { type: 'atlasnet-local-stack' }, workspace, 'Prelaunch',
      'AtlasNet Local Stack',
      new vscode.ShellExecution(expanded, { cwd: workspace.uri.fsPath }));
    task.presentationOptions = { reveal: vscode.TaskRevealKind.Always, panel: vscode.TaskPanelKind.Dedicated };
    const exitCode = await new Promise(async (resolve, reject) => {
      const ended = vscode.tasks.onDidEndTaskProcess(event => {
        if (event.execution.task === task) { ended.dispose(); resolve(event.exitCode); }
      });
      try { prelaunchExecution = await vscode.tasks.executeTask(task); }
      catch (error) { ended.dispose(); reject(error); }
    });
    prelaunchExecution = undefined;
    if (cancelled) throw new Error('Prelaunch command cancelled.');
    if (exitCode !== 0) throw new Error('Prelaunch command failed with exit code ' + exitCode + '.');
  }
  async function gracefulStopOne(session) {
    if (session.name === 'AtlasNet / Valkey') return;
    const threads = await session.customRequest('threads');
    const threadId = threads?.threads?.[0]?.id;
    if (threadId !== undefined) {
      await session.customRequest('pause', { threadId });
      await new Promise(resolve => setTimeout(resolve, 100));
    }
    await session.customRequest('evaluate', { expression: '-exec signal SIGTERM', context: 'repl' });
  }
  async function gracefulStop() {
    cancelled = true;
    const nodes = [...sessions.values()].filter(s => s.name !== 'AtlasNet / Valkey');
    const results = await Promise.allSettled(nodes.map(gracefulStopOne));
    const failures = results.filter(r => r.status === 'rejected');
    if (failures.length) throw new Error('Could not signal ' + failures.length + ' process(es). Use Force stop. ' + failures[0].reason);
    send({ type: 'notice', message: 'SIGTERM sent. Waiting for nodes to shut down…' });
  }
  async function run(config) {
    if (busy || sessions.size) throw new Error('Stop the current stack before running another.');
    if (!vscode.workspace.isTrusted) throw new Error('Trust this workspace before running processes.');
    const workspace = folder();
    const launches = plan(config, workspace.uri.fsPath);
    busy = true; cancelled = false; runId = crypto.randomUUID(); status();
    try {
      await runPrelaunch(config.preLaunchCommand, workspace);
      const toolPaths = [config.valgrind && config.valgrindProgram, config.perf && config.perfProgram].filter(Boolean).map(p => p.replaceAll('${workspaceFolder}', workspace.uri.fsPath));
      for (const p of new Set([...launches.flatMap(c => [c.program, c.miDebuggerPath]), ...toolPaths])) await fs.access(p, fs.constants.X_OK);
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
        if (cancelled) { await forceStop(); break; }
        if (config.launchValkey && index === 0) {
          const deadline = Date.now() + 15000;
          while (!cancelled && !(await tcpOpen(config.valkeyPort))) {
            if (Date.now() > deadline) throw new Error('Valkey did not become ready within 15 seconds. See Debug Console.');
            await new Promise(resolve => setTimeout(resolve, 100));
          }
        }
      }
    } catch (error) { await forceStop(); throw error; }
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
      if (sessions.delete(session.id)) {
        output.appendLine(`Stopped ${session.name}`);
        const remaining = [...sessions.values()];
        if (cancelled && remaining.length === 1 && remaining[0].name === 'AtlasNet / Valkey')
          void vscode.debug.stopDebugging(remaining[0]);
        status();
      }
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
            } else if (message.type === 'forceStop') await forceStop();
            else if (message.type === 'gracefulStop') await gracefulStop();
            else if (message.type === 'forceStopOne') {
              const session = sessions.get(message.id);
              if (session) await vscode.debug.stopDebugging(session);
            } else if (message.type === 'gracefulStopOne') {
              const session = sessions.get(message.id);
              if (session) await gracefulStopOne(session);
            }
          } catch (error) { report(error); }
        }));
      }
    })
  );
}
module.exports = { activate };
