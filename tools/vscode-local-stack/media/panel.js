'use strict';
const vscode = acquireVsCodeApi();
const $ = id => document.getElementById(id);
const fields = ['program', 'cwd', 'gdb', 'clusterPort', 'handshakePort', 'valkeyURI', 'launchValkey', 'valkeyProgram', 'valkeyPort'];
let config, locked = false;
function notice(message, error = false) { $('message').textContent = message; $('message').className = error ? 'error' : ''; }
function collect() {
  const result = { ...config, groups: [] };
  for (const key of fields) { const e = $(key); result[key] = e.type === 'checkbox' ? e.checked : e.type === 'number' ? Number(e.value) : e.value; }
  for (const row of $('groups').children) result.groups.push({ name: row.querySelector('.name').value, count: Number(row.querySelector('.count').value), capabilities: [...row.querySelectorAll('input[type=checkbox]:checked')].map(e => e.value), args: JSON.parse(row.querySelector('textarea').value) });
  return result;
}
function render(value) {
  config = value;
  for (const key of fields) { if ($(key).type === 'checkbox') $(key).checked = value[key]; else $(key).value = value[key]; }
  $('groups').replaceChildren();
  for (const group of value.groups) addGroup(group);
}
function addGroup(group) {
  const row = document.createElement('fieldset');
  function inputLabel(title, type, value, cls) {
    const label = document.createElement('label'); label.textContent = title + ' ';
    const input = document.createElement('input'); input.type = type; input.value = value; input.className = cls; label.append(input); row.append(label); return input;
  }
  inputLabel('Name', 'text', group.name, 'name');
  const count = inputLabel('Count', 'number', group.count, 'count'); count.min = 0; count.max = 100;
  for (const capability of ['Database', 'Shard', 'ControllerEligible', 'ClientIngress']) {
    const input = inputLabel(capability, 'checkbox', capability, 'capability'); input.checked = group.capabilities.includes(capability); input.parentElement.className = 'check';
  }
  const label = document.createElement('label'); label.textContent = 'Extra arguments (JSON array)';
  const args = document.createElement('textarea'); args.value = JSON.stringify(group.args); label.append(args); row.append(label);
  const remove = document.createElement('button'); remove.textContent = 'Remove group'; remove.onclick = () => row.remove(); row.append(remove);
  $('groups').append(row);
}
for (const type of ['run', 'save']) $(type).onclick = () => { try { const value = collect(); notice(''); vscode.postMessage({ type, config: value }); } catch (error) { notice('Arguments must be valid JSON arrays: ' + error.message, true); } };
$('stop').onclick = () => vscode.postMessage({ type: 'stop' });
$('add').onclick = () => { if (!locked) addGroup({ name: 'Node', count: 1, capabilities: ['Shard'], args: [] }); };
window.addEventListener('message', ({ data }) => {
  if (data.type === 'config') render(data.config);
  if (data.type === 'error' || data.type === 'notice') notice(data.message, data.type === 'error');
  if (data.type === 'status') {
    locked = data.busy || data.sessions.length > 0;
    document.querySelectorAll('#settings input, #groups input, #groups textarea, #groups button, #run, #add, #save').forEach(e => { e.disabled = locked; });
    $('stop').disabled = !locked;
    $('summary').textContent = `${data.sessions.length} debug sessions${data.busy ? ' • starting…' : ''}`;
    $('sessions').replaceChildren();
    for (const session of data.sessions) { const button = document.createElement('button'); button.textContent = '■ Stop ' + session.name; button.onclick = () => vscode.postMessage({ type: 'stopOne', id: session.id }); $('sessions').append(button); }
  }
});
vscode.postMessage({ type: 'ready' });
