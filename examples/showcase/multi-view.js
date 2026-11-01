(() => {
    'use strict';

    const DEFAULT_TIMEOUT_MS = 10000;
    const NOTE_PATH = 'app://data/multi-view-demo/shared-note.txt';
    const NOTE_DIRECTORY = 'app://data/multi-view-demo';
    const POLL_INTERVAL_MS = 2500;

    class BridgeError extends Error {
        constructor(code, message) {
            super(message);
            this.name = 'BridgeError';
            this.code = code;
        }
    }

    function createTransport() {
        if (window.chrome?.webview) {
            return {
                post(message) {
                    window.chrome.webview.postMessage(message);
                },
                listen(listener) {
                    const handler = (event) => listener(event.data);
                    window.chrome.webview.addEventListener('message', handler);
                    return () => window.chrome.webview.removeEventListener('message', handler);
                }
            };
        }

        if (window.ksm) {
            return {
                post(message) {
                    window.ksm.postMessage(message);
                },
                listen(listener) {
                    window.ksm.onmessage = (event) => listener(event.data);
                    return () => {
                        window.ksm.onmessage = null;
                    };
                }
            };
        }

        return null;
    }

    function createBridge(transport, prefix) {
        const pending = new Map();
        let nextRequestId = 1;

        function parseMessage(raw) {
            if (typeof raw !== 'string') {
                return raw;
            }
            try {
                return JSON.parse(raw);
            } catch {
                return null;
            }
        }

        transport.listen((raw) => {
            const message = parseMessage(raw);
            if (message?.type !== 'response' || !pending.has(message.id)) {
                return;
            }
            const request = pending.get(message.id);
            pending.delete(message.id);
            window.clearTimeout(request.timer);
            if (message.ok) {
                request.resolve(message.data);
                return;
            }
            request.reject(new BridgeError(
                message.error?.code ?? 'INTERNAL_ERROR',
                message.error?.message ?? '宿主返回了错误。'
            ));
        });

        function invoke(command, args = {}, timeoutMs = DEFAULT_TIMEOUT_MS) {
            return new Promise((resolve, reject) => {
                const id = `${prefix}-${nextRequestId++}`;
                const timer = window.setTimeout(() => {
                    pending.delete(id);
                    try {
                        transport.post(JSON.stringify({ type: 'cancel', id }));
                    } catch {
                        // A host that stopped reading must not keep the promise alive.
                    }
                    reject(new BridgeError('TIMEOUT', '请求超时，操作已取消。'));
                }, timeoutMs);

                pending.set(id, { resolve, reject, timer });
                try {
                    transport.post(JSON.stringify({
                        type: 'request',
                        id,
                        cmd: command,
                        args,
                        timeoutMs
                    }));
                } catch (error) {
                    window.clearTimeout(timer);
                    pending.delete(id);
                    reject(error);
                }
            });
        }

        return { invoke };
    }

    const search = new URLSearchParams(window.location.search);
    const pane = search.get('pane') === 'right' ? 'right' : 'left';
    const paneConfig = pane === 'left'
        ? {
            code: 'VIEW A',
            kicker: 'LEFT SURFACE / WRITER',
            footer: 'VIEW A / LEFT',
            route: 'view-a-left',
            sample: '来自 VIEW A 的第一张便签。'
        }
        : {
            code: 'VIEW B',
            kicker: 'RIGHT SURFACE / READER',
            footer: 'VIEW B / RIGHT',
            route: 'view-b-right',
            sample: '来自 VIEW B 的第一张便签。'
        };

    document.documentElement.dataset.pane = pane;
    document.title = `${paneConfig.code} | Kasumi Multi-WebView Lab`;
    document.querySelector('#view-code').textContent = paneConfig.code;
    document.querySelector('#pane-kicker').textContent = paneConfig.kicker;
    document.querySelector('#footer-label').textContent = paneConfig.footer;
    document.querySelector('#route-value').textContent = paneConfig.route;
    document.querySelector('#note-input').value = paneConfig.sample;
    const title = document.querySelector('#view-title');
    const titleLead = document.createTextNode(
        pane === 'left' ? '同一扇窗，' : '同一份数据，'
    );
    const titleBreak = document.createElement('br');
    const titleEmphasis = document.createElement('em');
    titleEmphasis.textContent = pane === 'left' ? '两个独立页面。' : '两个独立通道。';
    title.replaceChildren(titleLead, titleBreak, titleEmphasis);
    document.querySelector('#pane-summary').textContent = pane === 'left'
        ? '这个 View 拥有自己的页面状态和消息路由。把便签写进共享沙箱，另一侧会通过自己的命令通道读取它。'
        : '这个 View 不依赖左侧页面的 DOM 或请求队列。它轮询同一个沙箱文件，因此写入和读取各走各的宿主通道。';

    const elements = {
        connection: document.querySelector('#connection'),
        connectionLabel: document.querySelector('#connection-label'),
        version: document.querySelector('#version-value'),
        focus: document.querySelector('#focus-value'),
        ping: document.querySelector('#ping-button'),
        pingResult: document.querySelector('#ping-result'),
        noteInput: document.querySelector('#note-input'),
        noteWrite: document.querySelector('#note-write-button'),
        noteRead: document.querySelector('#note-read-button'),
        noteStatus: document.querySelector('#note-status'),
        notePreview: document.querySelector('#note-preview'),
        localButton: document.querySelector('#local-button'),
        localCount: document.querySelector('#local-count')
    };

    const transport = createTransport();
    const bridge = transport ? createBridge(transport, paneConfig.route) : null;
    let connected = false;
    let localCount = 0;
    let lastNote = '';
    let pollTimer = 0;
    const multiViewMarker = search.get('multi-view-smoke');
    if (multiViewMarker) {
        void runMultiViewSmoke(multiViewMarker, search.get('view') ?? '0');
        return;
    }

    function setConnection(label, tone = 'idle') {
        elements.connectionLabel.textContent = label;
        elements.connection.dataset.tone = tone;
    }

    function setStatus(element, text, tone = 'idle') {
        element.textContent = text;
        element.dataset.tone = tone;
    }

    function errorText(error) {
        const code = error?.code ? ` · ${error.code}` : '';
        return `${error?.message ?? '操作失败。'}${code}`;
    }

    function requireBridge() {
        if (!bridge) {
            throw new BridgeError('NOT_SUPPORTED', '当前页面没有连接 Kasumi 宿主。');
        }
        return bridge;
    }

    function setBusy(button, busy) {
        button.disabled = busy || !connected;
        button.setAttribute('aria-busy', String(busy));
    }

    function applyConnected(nextConnected) {
        connected = nextConnected;
        for (const button of [
            elements.ping,
            elements.noteWrite,
            elements.noteRead
        ]) {
            button.disabled = !connected;
        }
    }

    function updateFocusState() {
        const focused = document.hasFocus();
        elements.focus.textContent = focused ? '当前 View' : '另一侧';
        elements.focus.dataset.tone = focused ? 'ready' : 'idle';
    }

    async function invoke(button, action) {
        setBusy(button, true);
        try {
            await action();
        } catch (error) {
            setConnection('宿主返回了错误', 'error');
            setStatus(elements.pingResult, errorText(error), 'error');
        } finally {
            setBusy(button, false);
        }
    }

    async function ping() {
        const result = await requireBridge().invoke('ping');
        if (result.pong !== true) {
            throw new BridgeError('INTERNAL_ERROR', '连接检查返回了意外结果。');
        }
        setStatus(elements.pingResult, `${paneConfig.code} 已在线`, 'ready');
        setConnection('当前 View 已连接', 'ready');
    }

    async function readSharedNote(showStatus = true) {
        const bus = requireBridge();
        const exists = await bus.invoke('fs.exists', { path: NOTE_PATH });
        if (!exists.exists) {
            if (showStatus) {
                setStatus(elements.noteStatus, '共享便签还不存在，请从任意一侧写入。');
            }
            return;
        }

        const result = await bus.invoke('fs.readText', { path: NOTE_PATH });
        const text = typeof result === 'string' ? result : String(result.content ?? '');
        if (text === lastNote && !showStatus) {
            return;
        }
        lastNote = text;
        elements.notePreview.textContent = text;
        if (showStatus) {
            setStatus(
                elements.noteStatus,
                `已通过 ${paneConfig.code} 读取，共 ${text.length} 个字符。`,
                'ready'
            );
        }
    }

    async function writeSharedNote() {
        const text = elements.noteInput.value.trim();
        if (!text) {
            throw new BridgeError('INVALID_ARGS', '先输入便签内容。');
        }
        const bus = requireBridge();
        await bus.invoke('fs.mkdir', {
            path: NOTE_DIRECTORY,
            recursive: true
        });
        await bus.invoke('fs.writeText', {
            path: NOTE_PATH,
            content: text
        });
        lastNote = text;
        elements.notePreview.textContent = text;
        setStatus(
            elements.noteStatus,
            `${paneConfig.code} 已写入，另一侧会在 2.5 秒内看到更新。`,
            'ready'
        );
    }

    function scheduleNotePoll() {
        window.clearTimeout(pollTimer);
        pollTimer = window.setTimeout(async () => {
            if (connected && document.visibilityState === 'visible') {
                try {
                    await readSharedNote(false);
                } catch {
                    // A transient host restart should not replace the current note.
                }
            }
            scheduleNotePoll();
        }, POLL_INTERVAL_MS);
    }

    async function initialize() {
        if (!bridge || !transport) {
            applyConnected(false);
            setConnection('当前页面未连接宿主', 'error');
            return;
        }

        try {
            const [pong, appInfo, capabilities] = await Promise.all([
                requireBridge().invoke('ping'),
                requireBridge().invoke('app.getInfo'),
                requireBridge().invoke('capabilities.query')
            ]);
            if (pong.pong !== true) {
                throw new BridgeError('INTERNAL_ERROR', '连接检查返回了意外结果。');
            }
            elements.version.textContent = appInfo.version;
            applyConnected(true);
            setConnection('当前 View 已连接', 'ready');
            setStatus(
                elements.pingResult,
                `${capabilities.commands.length} 项能力可用`,
                'ready'
            );
            await readSharedNote();
            scheduleNotePoll();
        } catch (error) {
            applyConnected(false);
            setConnection('无法连接宿主', 'error');
            setStatus(elements.pingResult, errorText(error), 'error');
        }
    }

    async function runMultiViewSmoke(marker, view) {
        try {
            await requireBridge().invoke('fs.writeText', {
                path: `app://data/multi-view-smoke/${marker}-${view}.txt`,
                content: 'ready'
            });
        } catch (error) {
            document.title = 'KSM_MULTI_VIEW_SMOKE_FAIL';
            console.error(errorText(error));
        }
    }

    elements.ping.addEventListener('click', () => {
        void invoke(elements.ping, ping);
    });
    elements.noteWrite.addEventListener('click', () => {
        void invoke(elements.noteWrite, writeSharedNote);
    });
    elements.noteRead.addEventListener('click', () => {
        void invoke(elements.noteRead, () => readSharedNote(true));
    });
    elements.localButton.addEventListener('click', () => {
        localCount += 1;
        elements.localCount.textContent = String(localCount);
    });

    window.addEventListener('focus', updateFocusState);
    window.addEventListener('blur', updateFocusState);
    window.addEventListener('pagehide', () => {
        window.clearTimeout(pollTimer);
    });

    updateFocusState();
    applyConnected(false);
    void initialize();
})();
