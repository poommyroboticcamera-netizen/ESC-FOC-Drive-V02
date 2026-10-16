'use strict';

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('satNative', Object.freeze({
  listComPorts: () => ipcRenderer.invoke('serial:list-com-ports'),
  selectComPort: (portName) => ipcRenderer.invoke('serial:select-com-port', portName)
}));
