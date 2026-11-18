const board = document.querySelector('#board');
const info = document.querySelector('#board-info');
const status = document.querySelector('#status');
const installer = document.querySelector('esp-web-install-button');
const profiles = {
  original: {
    manifest: './firmware/manifest.json',
    detail: '3.2″ ST7789 · 320 × 240. Original QD3223 / E32R32P projector firmware.'
  },
  freenove: {
    manifest: './firmware/freenove35/manifest.json',
    detail: '3.5″ ST7796 TN · 480 × 320. Freenove ESP32-32E resistive touch. First hardware test pending.'
  }
};
let ready = false;
let loadFailed = false;
function update() {
  const profile = profiles[board.value];
  info.textContent = profile?.detail || 'Match the screen size and driver to your board or order listing.';
  installer.hidden = true;
  installer.removeAttribute('manifest');
  status.hidden = false;
  if (!profile) {
    status.textContent = 'Select your board to enable USB installation.';
  } else if (!window.isSecureContext || !('serial' in navigator)) {
    status.textContent = 'Open the HTTPS installer in a separate desktop Chrome or Edge tab to use USB.';
  } else if (!ready) {
    status.textContent = loadFailed ? 'The installer could not load. Refresh to retry.' : 'Loading USB installer…';
  } else {
    installer.setAttribute('manifest', profile.manifest);
    installer.hidden = false;
    status.hidden = true;
  }
}
board.addEventListener('change', update);
update();
if (window.isSecureContext && 'serial' in navigator) {
  try {
    await import('./vendor/install-button.js');
    await customElements.whenDefined('esp-web-install-button');
    ready = true;
  } catch {
    loadFailed = true;
  }
  update();
}
