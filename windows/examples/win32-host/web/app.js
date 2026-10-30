const bridge = (() => {
  const pending = new Map();
  let nextRequestId = 1;

  function post(message) {
    if (!window.chrome?.webview) {
      throw new Error('当前窗口未连接到 Kasumi。');
    }
    window.chrome.webview.postMessage(JSON.stringify(message));
  }

  function invoke(command, args, timeoutMs = 10000) {
    return new Promise((resolve, reject) => {
      const id = `demo-${nextRequestId++}`;
      const timer = window.setTimeout(() => {
        pending.delete(id);
        try {
          post({ type: 'cancel', id });
        } catch {
          // The timeout still rejects even if the host is gone.
        }
        reject(Object.assign(new Error('请求超时。'), { code: 'TIMEOUT' }));
      }, timeoutMs);

      pending.set(id, { resolve, reject, timer });
      try {
        post({ type: 'request', id, cmd: command, args, timeoutMs });
      } catch (error) {
        window.clearTimeout(timer);
        pending.delete(id);
        reject(error);
      }
    });
  }

  window.chrome?.webview?.addEventListener('message', (event) => {
    let message = event.data;
    if (typeof message === 'string') {
      try {
        message = JSON.parse(message);
      } catch {
        return;
      }
    }
    if (!message || message.type !== 'response' || !pending.has(message.id)) {
      return;
    }
    const request = pending.get(message.id);
    pending.delete(message.id);
    window.clearTimeout(request.timer);
    if (message.ok) {
      request.resolve(message.data);
      return;
    }
    const error = new Error(message.error?.message ?? '宿主返回了错误。');
    error.code = message.error?.code ?? 'INTERNAL_ERROR';
    request.reject(error);
  });

  return { invoke };
})();

const elements = {
  connection: document.querySelector('.connection'),
  connectionLabel: document.querySelector('#connection-label'),
  platform: document.querySelector('#platform-value'),
  version: document.querySelector('#version-value'),
  activityList: document.querySelector('#activity-list'),
  activityCount: document.querySelector('#activity-count'),
  emptyState: document.querySelector('#empty-state'),
  note: document.querySelector('#note-input'),
  width: document.querySelector('#width-input'),
  height: document.querySelector('#height-input'),
  ping: document.querySelector('#ping-button'),
  write: document.querySelector('#write-button'),
  read: document.querySelector('#read-button'),
  size: document.querySelector('#size-button'),
  external: document.querySelector('#external-button'),
  download: document.querySelector('#download-button')
};

let activityCount = 0;

function setConnection(label, tone = 'idle') {
  elements.connectionLabel.textContent = label;
  elements.connection.dataset.tone = tone;
}

function addActivity(message, tone = 'normal') {
  activityCount += 1;
  elements.activityCount.textContent = `${activityCount} 条`;
  elements.emptyState.hidden = true;
  const item = document.createElement('li');
  item.dataset.tone = tone;
  const time = document.createElement('time');
  time.dateTime = new Date().toISOString();
  time.textContent = new Intl.DateTimeFormat('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  }).format(new Date());
  const text = document.createElement('span');
  text.textContent = message;
  item.append(time, text);
  elements.activityList.prepend(item);
  while (elements.activityList.children.length > 20) {
    elements.activityList.lastElementChild?.remove();
  }
}

function errorText(error) {
  const code = error?.code ? ` ${error.code}` : '';
  return `${error?.message ?? '操作失败。'}${code}`;
}

async function run(button, action) {
  button.disabled = true;
  button.setAttribute('aria-busy', 'true');
  try {
    await action();
  } catch (error) {
    addActivity(errorText(error), 'error');
    setConnection('宿主返回了错误', 'error');
  } finally {
    button.disabled = false;
    button.removeAttribute('aria-busy');
  }
}

async function initialize() {
  if (!window.chrome?.webview) {
    setConnection('当前窗口无法连接 Kasumi', 'error');
    return;
  }
  try {
    const info = await bridge.invoke('app.getInfo', {});
    elements.platform.textContent = info.platform;
    elements.version.textContent = info.version;
    setConnection('已连接', 'ready');
    addActivity('连接成功，应用信息已读取。');
  } catch (error) {
    setConnection('无法读取应用信息', 'error');
    addActivity(errorText(error), 'error');
  }
}

elements.ping.addEventListener('click', () => run(elements.ping, async () => {
  const result = await bridge.invoke('ping', {});
  if (result.pong !== true) {
    throw new Error('连接检查返回了意外结果。');
  }
  setConnection('已连接', 'ready');
  addActivity('连接正常，操作已完成。');
}));

elements.write.addEventListener('click', () => run(elements.write, async () => {
  const content = elements.note.value.trim();
  if (!content) {
    throw new Error('先写一点内容，再保存。');
  }
  await bridge.invoke('fs.writeText', {
    path: 'app://data/notes.txt',
    content
  });
  addActivity(`笔记已保存，共 ${content.length} 个字符。`);
}));

elements.read.addEventListener('click', () => run(elements.read, async () => {
  const result = await bridge.invoke('fs.readText', {
    path: 'app://data/notes.txt'
  });
  const content = typeof result === 'string' ? result : result.file;
  elements.note.value = typeof result === 'string' ? result : '';
  addActivity(
    typeof result === 'string'
      ? `已载入笔记：${content.slice(0, 80)}${content.length > 80 ? '…' : ''}`
      : `内容较大，已保存到 ${content}。`
  );
}));

elements.size.addEventListener('click', () => run(elements.size, async () => {
  const width = Number(elements.width.value);
  const height = Number(elements.height.value);
  if (!Number.isSafeInteger(width) || !Number.isSafeInteger(height)) {
    throw new Error('宽度和高度必须是整数。');
  }
  await bridge.invoke('window.setSize', { width, height });
  addActivity(`窗口已调整为 ${width} × ${height} 像素。`);
}));

elements.external.addEventListener('click', () => run(elements.external, async () => {
  const result = await bridge.invoke('shell.openExternal', {
    url: 'https://example.com/'
  });
  addActivity(
    result.opened
      ? '示例站点已交给系统浏览器。'
    : '没有找到可打开此链接的应用。'
  );
}));

elements.download.addEventListener('click', () => run(elements.download, async () => {
  const result = await bridge.invoke(
    'http.download',
    {
      url: 'https://example.com/',
      path: 'app://data/example.html',
      overwrite: true
    },
    120000
  );
  addActivity(
    `下载完成：${result.bytes} 字节，保存到 ${result.path}。`
  );
}));

async function runSmokeTest() {
  try {
    const result = await bridge.invoke('ping', {});
    if (result.pong !== true) {
      throw new Error('Ping returned an unexpected result.');
    }
    await bridge.invoke('app.exit', {});
  } catch (error) {
    document.title = 'KSM_SMOKE_FAIL';
    console.error(errorText(error));
  }
}

if (new URLSearchParams(window.location.search).has('smoke')) {
  void runSmokeTest();
} else {
  void initialize();
}
