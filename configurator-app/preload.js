'use strict';
const { contextBridge, ipcRenderer } = require('electron');

// Only the bundled connect screen (file://) gets the bridge. The page served by the board never does.
if (location.protocol === 'file:') {
  contextBridge.exposeInMainWorld('fc', {
    getHost: () => ipcRenderer.invoke('fc:get-host'),
    connect: (host) => ipcRenderer.invoke('fc:connect', host),
  });
}
