(() => {
  const status = document.getElementById('status');
  if (!status || !window.ksm) {
    if (status) {
      status.textContent = '宿主桥接不可用。';
    }
    return;
  }

  const requestId = 'boot-ping';
  window.ksm.onmessage = (event) => {
    try {
      const response = JSON.parse(event.data);
      if (response.type === 'response' && response.id === requestId) {
        status.textContent = response.ok ? '桥接已就绪。' : '桥接初始化失败。';
      }
    } catch {
      status.textContent = '桥接返回了无效消息。';
    }
  };
  window.ksm.postMessage(JSON.stringify({
    type: 'request',
    id: requestId,
    cmd: 'ping',
    args: {},
    timeoutMs: 5000
  }));
})();
