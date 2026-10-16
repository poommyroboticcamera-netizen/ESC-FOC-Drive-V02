'use strict';

const { app, BrowserWindow, dialog, ipcMain, net, protocol, screen } = require('electron');
const { execFile } = require('node:child_process');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

protocol.registerSchemesAsPrivileged([
  { scheme: 'sat', privileges: { standard: true, secure: true, supportFetchAPI: true, corsEnabled: true } }
]);

let mainWindow;
let selectedComPort = '';

function listComPorts() {
  return new Promise((resolve) => {
    const script = '[System.IO.Ports.SerialPort]::GetPortNames() | ConvertTo-Json -Compress';
    execFile('powershell.exe', ['-NoLogo', '-NoProfile', '-NonInteractive', '-Command', script], {
      windowsHide: true,
      timeout: 5000,
      maxBuffer: 32 * 1024
    }, (error, stdout) => {
      if (error || !stdout.trim()) return resolve([]);
      try {
        const decoded = JSON.parse(stdout.trim());
        const ports = (Array.isArray(decoded) ? decoded : [decoded])
          .filter((name) => typeof name === 'string' && /^COM\d+$/i.test(name));
        ports.sort((a, b) => Number(a.slice(3)) - Number(b.slice(3)));
        resolve([...new Set(ports.map((name) => name.toUpperCase()))]);
      } catch {
        resolve([]);
      }
    });
  });
}

function matchesComPort(port, comPort) {
  if (!comPort) return false;
  const escaped = comPort.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  return new RegExp(`(^|[^A-Z0-9])${escaped}($|[^0-9])`, 'i')
    .test(`${port.portName || ''} ${port.displayName || ''}`);
}

function installAppProtocol() {
  const webRoot = path.join(app.getAppPath(), 'web');
  protocol.handle('sat', (request) => {
    let requestedPath;
    try {
      requestedPath = decodeURIComponent(new URL(request.url).pathname).replace(/^\/+/, '');
    } catch {
      return new Response('Bad path', { status: 400 });
    }
    const filePath = path.resolve(webRoot, requestedPath || 'index.html');
    if (filePath !== webRoot && !filePath.startsWith(`${webRoot}${path.sep}`)) {
      return new Response('Forbidden', { status: 403 });
    }
    return net.fetch(pathToFileURL(filePath).toString());
  });
}

function configureSerialAccess(appSession) {
  const isAppOrigin = (origin) => {
    try {
      const url = new URL(origin);
      return url.protocol === 'sat:' && url.hostname === 'app';
    } catch {
      return false;
    }
  };

  appSession.setPermissionCheckHandler((_contents, permission, origin) =>
    permission === 'serial' && isAppOrigin(origin)
  );

  ipcMain.handle('serial:list-com-ports', async (event) => {
    if (event.sender !== mainWindow?.webContents || !event.senderFrame?.url.startsWith('sat://app/')) return [];
    return listComPorts();
  });
  ipcMain.handle('serial:select-com-port', (event, portName) => {
    if (event.sender !== mainWindow?.webContents || !event.senderFrame?.url.startsWith('sat://app/')) return false;
    if (typeof portName !== 'string' || !/^COM\d+$/i.test(portName)) return false;
    selectedComPort = portName.toUpperCase();
    return true;
  });

  appSession.on('select-serial-port', async (event, ports, contents, callback) => {
    event.preventDefault();
    if (contents !== mainWindow?.webContents) {
      callback('');
      return;
    }
    const requestedComPort = selectedComPort;
    selectedComPort = '';
    const selected = ports.find((port) => matchesComPort(port, requestedComPort));
    if (!selected) {
      callback('');
      await dialog.showMessageBox(mainWindow, {
        type: 'warning',
        title: 'COM port unavailable',
        message: ports.length
          ? `${requestedComPort || 'The selected COM port'} is not available to the app.`
          : 'No serial devices are available to the app.',
        detail: 'Refresh the COM list, close any other program using the port, and try again.',
        buttons: ['OK']
      });
      return;
    }
    // Let Electron retain the permission granted by this device-selection callback.
    callback(selected.portId);
  });
}

function createWindow() {
  const workArea = screen.getPrimaryDisplay().workArea;
  const width = Math.min(1500, workArea.width);
  const height = Math.min(960, workArea.height);
  mainWindow = new BrowserWindow({
    width,
    height,
    minWidth: Math.min(900, workArea.width),
    minHeight: Math.min(640, workArea.height),
    x: workArea.x + Math.floor((workArea.width - width) / 2),
    y: workArea.y + Math.floor((workArea.height - height) / 2),
    title: 'SAT motor pilot',
    icon: path.join(app.getAppPath(), 'web', 'sat_motor_pilot.ico'),
    autoHideMenuBar: true,
    backgroundColor: '#080b10',
    webPreferences: {
      preload: path.join(app.getAppPath(), 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      webSecurity: true
    }
  });
  configureSerialAccess(mainWindow.webContents.session);
  mainWindow.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  mainWindow.webContents.on('will-navigate', (event, url) => {
    if (!url.startsWith('sat://app/')) event.preventDefault();
  });
  mainWindow.loadURL('sat://app/index.html');
  mainWindow.on('closed', () => { mainWindow = null; });
}

app.whenReady().then(() => {
  installAppProtocol();
  createWindow();
  app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
});

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') app.quit();
});
