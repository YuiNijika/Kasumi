import { KSM } from 'kasumi-bridge';

import './style.css';

const elements = {
  message: requiredElement('message'),
  platform: requiredElement('platform'),
  version: requiredElement('version'),
  capabilities: requiredElement('capabilities'),
  button: requiredElement<HTMLButtonElement>('ping'),
  statusBadge: requiredElement('status-badge'),
  statusLabel: requiredElement('status-label')
};

KSM.init();

async function connect(): Promise<void> {
  elements.button.disabled = true;
  elements.button.textContent = '检查中…';
  elements.statusBadge.dataset.state = 'pending';
  elements.statusLabel.textContent = '连接中';
  elements.message.textContent = '正在连接宿主…';
  elements.platform.textContent = '检测中';
  elements.version.textContent = '检测中';
  elements.capabilities.textContent = '检测中';

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
    elements.message.textContent = '已连接 Kasumi 运行时，可以开始编辑。';
    elements.statusBadge.dataset.state = 'ready';
    elements.statusLabel.textContent = '已连接';
  } catch (error) {
    elements.platform.textContent = '不可用';
    elements.version.textContent = '不可用';
    elements.capabilities.textContent = '不可用';
    elements.message.textContent =
      error instanceof Error ? error.message : '连接宿主失败。';
    elements.statusBadge.dataset.state = 'error';
    elements.statusLabel.textContent = '未连接';
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

if (new URLSearchParams(window.location.search).has('smoke')) {
  void runSmokeTest();
} else {
  void connect();
}

async function runSmokeTest(): Promise<void> {
  const pong = await KSM.ping();
  if (!pong.pong) {
    throw new Error('宿主返回了无效的连接检查结果。');
  }
  await KSM.app.exit();
}
