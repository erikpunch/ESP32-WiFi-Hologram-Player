const status = document.querySelector('#status');
const installer = document.querySelector('esp-web-install-button');
async function start() {
  if (!window.isSecureContext || !('serial' in navigator)) {
    status.textContent = 'Open the HTTPS installer in a separate desktop Chrome or Edge tab to use USB.';
    return;
  }
  try {
    await import('./vendor/install-button.js');
    await customElements.whenDefined('esp-web-install-button');
    installer.hidden = false;
    status.hidden = true;
  } catch {
    status.textContent = 'The USB installer could not load. Refresh the page to try again.';
  }
}
start();
