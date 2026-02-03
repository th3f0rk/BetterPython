import * as path from 'path';
import * as vscode from 'vscode';
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
} from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: vscode.ExtensionContext) {
  // Get configuration
  const config = vscode.workspace.getConfiguration('betterpython');
  const compilerPath = config.get<string>('compilerPath') || 'bpcc';
  const vmPath = config.get<string>('vmPath') || 'bpvm';
  let lspPath = config.get<string>('lspPath');
  
  // Default LSP path
  if (!lspPath) {
    lspPath = path.join(context.extensionPath, '..', '..', 'lsp-server', 'server.py');
  }
  
  // Register commands
  context.subscriptions.push(
    vscode.commands.registerCommand('betterpython.run', runFile),
    vscode.commands.registerCommand('betterpython.compile', compileFile)
  );
  
  // Start LSP client
  const serverOptions: ServerOptions = {
    command: 'python3',
    args: [lspPath],
  };
  
  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: 'file', language: 'betterpython' }],
  };
  
  client = new LanguageClient(
    'betterpython',
    'BetterPython Language Server',
    serverOptions,
    clientOptions
  );
  
  client.start();
}

export function deactivate(): Thenable<void> | undefined {
  if (!client) {
    return undefined;
  }
  return client.stop();
}

async function runFile() {
  const editor = vscode.window.activeTextEditor;
  if (!editor) {
    vscode.window.showErrorMessage('No active editor');
    return;
  }
  
  const filePath = editor.document.fileName;
  if (!filePath.endsWith('.bp')) {
    vscode.window.showErrorMessage('Not a BetterPython file');
    return;
  }
  
  await editor.document.save();
  
  const config = vscode.workspace.getConfiguration('betterpython');
  const compilerPath = config.get<string>('compilerPath') || 'bpcc';
  const vmPath = config.get<string>('vmPath') || 'bpvm';
  
  const terminal = vscode.window.createTerminal('BetterPython');
  terminal.show();
  terminal.sendText(`${compilerPath} "${filePath}" -o /tmp/temp.bpc && ${vmPath} /tmp/temp.bpc`);
}

async function compileFile() {
  const editor = vscode.window.activeTextEditor;
  if (!editor) {
    vscode.window.showErrorMessage('No active editor');
    return;
  }
  
  const filePath = editor.document.fileName;
  if (!filePath.endsWith('.bp')) {
    vscode.window.showErrorMessage('Not a BetterPython file');
    return;
  }
  
  await editor.document.save();
  
  const config = vscode.workspace.getConfiguration('betterpython');
  const compilerPath = config.get<string>('compilerPath') || 'bpcc';
  
  const outputPath = filePath.replace(/\.bp$/, '.bpc');
  
  const terminal = vscode.window.createTerminal('BetterPython Compile');
  terminal.show();
  terminal.sendText(`${compilerPath} "${filePath}" -o "${outputPath}"`);
}
