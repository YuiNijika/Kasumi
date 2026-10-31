import { KSM } from 'kasumi-bridge';

import './style.css';

const elements = {
  message: requiredElement('message'),
  platform: requiredElement('platform'),
  version: requiredElement('version'),
  capabilities: requiredElement('capabilities'),
  button: requiredElement<HTMLButtonElement>('ping'),
  statusDot: requiredElement('status-dot')
};

KSM.init();

async function connect(): Promise<void> {
  elements.button.disabled = true;
  elements.button.textContent = '检查中…';
  elements.message.textContent = '正在连接宿主…';
  elements.statusDot.dataset.state = 'pending';
  try {
    const [pong, capabilities, appInfo] = await Promise.all([
      KSM.ping(),
      KSM.get(),
      KSM.app.getInfo()
    ]);
    if (!pong.pong) {
      throw new Error('宿主返回了无效的连接检查结果。');
    }
    elements.platform.textContent = capabilities.platform;
    elements.version.textContent = appInfo.version;
    elements.capabilities.textContent = String(capabilities.commands.length);
    elements.message.textContent = '已连接宿主，可以开始开发。';
    elements.statusDot.dataset.state = 'ready';
  } catch (error) {
    elements.message.textContent =
      error instanceof Error ? error.message : '连接宿主失败。';
    elements.statusDot.dataset.state = 'error';
  } finally {
    elements.button.disabled = false;
    elements.button.textContent = '重新检查';
  }
}

function requiredElement<T extends HTMLElement = HTMLElement>(id: string): T {
  const element = document.getElementById(id);
  if (!element) {
    throw new Error(`Missing element: ${id}`);
  }
  return element as T;
}

elements.button.addEventListener('click', () => {
  void connect();
});
void connect();
