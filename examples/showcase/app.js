(() => {
    'use strict';

    const DEFAULT_TIMEOUT_MS = 10000;
    const DOWNLOAD_TIMEOUT_MS = 120000;
    const MAX_ACTIVITY_ITEMS = 24;
    const SAMPLE_FILE_PATH = 'app://data/showcase/notes.txt';
    const SAMPLE_DIRECTORY_PATH = 'app://data/showcase';

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
                    const handler = (event) => {
                        if (event.source === window) {
                            listener(event.data);
                        }
                    };
                    window.ksm.onmessage = (event) => listener(event.data);
                    window.addEventListener('message', handler);
                    return () => {
                        window.ksm.onmessage = null;
                        window.removeEventListener('message', handler);
                    };
                }
            };
        }

        return null;
    }

    function createBridge(transport) {
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

        function settle(message) {
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
            request.reject(new BridgeError(
                message.error?.code ?? 'INTERNAL_ERROR',
                message.error?.message ?? '宿主返回了错误。'
            ));
        }

        transport.listen((raw) => {
            const message = parseMessage(raw);
            if (message?.type === 'event') {
                emitEvent(message.event, message.payload);
                return;
            }
            settle(message);
        });

        function invoke(command, args, timeoutMs = DEFAULT_TIMEOUT_MS) {
            return new Promise((resolve, reject) => {
                const id = `showcase-${nextRequestId++}`;
                const timer = window.setTimeout(() => {
                    pending.delete(id);
                    try {
                        transport.post(JSON.stringify({ type: 'cancel', id }));
                    } catch {
                        // A lost host must not prevent the timeout from settling.
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

    const transport = createTransport();
    const eventListeners = new Map();

    function emitEvent(event, payload) {
        for (const listener of eventListeners.get(event) ?? []) {
            try {
                listener(payload);
            } catch {
                // A broken listener must not stop the remaining event handlers.
            }
        }
    }

    const transportAdapter = transport
        ? {
            post(message) {
                transport.post(message);
            },
            listen(listener) {
                return transport.listen((raw) => {
                    let message = raw;
                    if (typeof message === 'string') {
                        try {
                            message = JSON.parse(message);
                        } catch {
                            return;
                        }
                    }
                    if (message?.type === 'event') {
                        emitEvent(message.event, message.payload);
                        return;
                    }
                    listener(message);
                });
            }
        }
        : null;

    const bridge = transportAdapter ? createBridge(transportAdapter) : null;

    const elements = {
        connection: document.querySelector('#connection'),
        connectionLabel: document.querySelector('#connection-label'),
        platform: document.querySelector('#platform-value'),
        version: document.querySelector('#version-value'),
        capabilityCount: document.querySelector('#capability-value'),
        appName: document.querySelector('#app-name'),
        appLanguage: document.querySelector('#app-language'),
        activityList: document.querySelector('#activity-list'),
        activityCount: document.querySelector('#activity-count'),
        emptyState: document.querySelector('#empty-state'),
        clearActivity: document.querySelector('#clear-activity-button'),
        ping: document.querySelector('#ping-button'),
        refreshApp: document.querySelector('#refresh-app-button'),
        exit: document.querySelector('#exit-button'),
        fileContent: document.querySelector('#file-content'),
        fileRoundtrip: document.querySelector('#file-roundtrip-button'),
        fileList: document.querySelector('#file-list-button'),
        fileClear: document.querySelector('#file-clear-button'),
        fileResult: document.querySelector('#file-result'),
        dialogMessage: document.querySelector('#dialog-message-button'),
        dialogConfirm: document.querySelector('#dialog-confirm-button'),
        dialogOpen: document.querySelector('#dialog-open-button'),
        dialogSave: document.querySelector('#dialog-save-button'),
        dialogResult: document.querySelector('#dialog-result'),
        clipboardInput: document.querySelector('#clipboard-input'),
        clipboardWrite: document.querySelector('#clipboard-write-button'),
        clipboardRead: document.querySelector('#clipboard-read-button'),
        notificationPermission: document.querySelector('#notification-permission-button'),
        notificationShow: document.querySelector('#notification-show-button'),
        notificationState: document.querySelector('#notification-state'),
        windowModule: document.querySelector('#window-title').closest('.module'),
        windowBadge: document.querySelector('#window-badge'),
        windowCopy: document.querySelector('#window-copy'),
        windowWidth: document.querySelector('#window-width'),
        windowHeight: document.querySelector('#window-height'),
        windowTitleButton: document.querySelector('#window-title-button'),
        windowSizeButton: document.querySelector('#window-size-button'),
        windowMinimize: document.querySelector('#window-minimize-button'),
        windowMaximize: document.querySelector('#window-maximize-button'),
        windowClose: document.querySelector('#window-close-button'),
        windowEventState: document.querySelector('#window-event-state'),
        downloadPath: document.querySelector('#download-path'),
        download: document.querySelector('#download-button'),
        external: document.querySelector('#external-button'),
        deviceModule: document.querySelector('#device-module'),
        deviceBadge: document.querySelector('#device-badge'),
        deviceButton: document.querySelector('#device-button'),
        deviceDetails: document.querySelector('#device-details'),
        resizeValue: document.querySelector('#resize-value'),
        reloadValue: document.querySelector('#reload-value'),
        reload: document.querySelector('#reload-button')
    };

    let activityCount = 0;
    let capabilities = null;
    let supportedCommands = new Set();
    const notificationPermission = 'notification.show';

    function setConnection(label, tone = 'idle') {
        elements.connectionLabel.textContent = label;
        elements.connection.dataset.tone = tone;
    }

    function addActivity(message, tone = 'normal') {
        activityCount += 1;
        elements.activityCount.textContent = `${activityCount}条`;
        elements.emptyState.hidden = true;
        const item = document.createElement('li');
        item.dataset.tone = tone;
        const time = document.createElement('time');
        const now = new Date();
        time.dateTime = now.toISOString();
        time.textContent = new Intl.DateTimeFormat('zh-CN', {
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
        }).format(now);
        const text = document.createElement('span');
        text.textContent = message;
        item.append(time, text);
        elements.activityList.prepend(item);
        while (elements.activityList.children.length > MAX_ACTIVITY_ITEMS) {
            elements.activityList.lastElementChild?.remove();
        }
    }

    function errorText(error) {
        const code = error?.code ? ` · ${error.code}` : '';
        return `${error?.message ?? '操作失败。'}${code}`;
    }

    function setInlineStatus(element, message, tone = 'idle') {
        element.textContent = message;
        element.dataset.tone = tone;
    }

    function setResult(element, value) {
        element.textContent = typeof value === 'string'
            ? value
            : JSON.stringify(value, null, 2);
    }

    function requireBridge() {
        if (!bridge) {
            throw new BridgeError('NOT_SUPPORTED', '当前窗口没有连接 Kasumi宿主。');
        }
        return bridge;
    }

    function isSupported(command) {
        return supportedCommands.has(command);
    }

    async function run(button, action) {
        if (button.disabled) {
            return;
        }
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
            applyCapabilities();
        }
    }

    function applyCapabilities() {
        for (const button of document.querySelectorAll('[data-command]')) {
            const command = button.dataset.command;
            if (!command || command === 'window.reload') {
                continue;
            }
            const enabled = isSupported(command);
            button.disabled = !enabled;
            button.title = enabled ? '' : '当前平台不提供此能力';
        }

        const windowSupported = [
            'window.setTitle',
            'window.setSize',
            'window.minimize',
            'window.maximize'
        ].some(isSupported);
        elements.windowModule.dataset.disabled = String(!windowSupported);
        elements.windowBadge.textContent = windowSupported ? 'Windows能力' : '部分可用';
        elements.windowCopy.textContent = windowSupported
            ? '外框尺寸会按当前 DPI换算并夹取到显示器工作区；最大化或最小化时调整尺寸会被拒绝。'
            : 'Android只提供关闭窗口；标题、最小化、最大化和尺寸调整在 Windows上可用。';

        const deviceSupported = isSupported('device.getInfo');
        elements.deviceModule.dataset.disabled = String(!deviceSupported);
        elements.deviceBadge.textContent = deviceSupported ? 'Android专属' : '仅 Android';
        elements.deviceButton.title = deviceSupported
            ? ''
            : 'device.getInfo只在 Android宿主中提供';
    }

    function updateRuntimeInfo() {
        if (!capabilities) {
            return;
        }
        elements.platform.textContent = capabilities.platform === 'windows' ? 'Windows' : 'Android';
        elements.capabilityCount.textContent = `${capabilities.commands.length}项命令`;
    }

    async function loadAppInfo() {
        const info = await requireBridge().invoke('app.getInfo', {});
        elements.appName.textContent = info.name;
        elements.version.textContent = info.version;
        elements.appLanguage.textContent = info.language;
        addActivity(`应用信息已更新：${info.name} ${info.version}。`);
        return info;
    }

    async function loadCapabilities() {
        const result = await requireBridge().invoke('capabilities.query', {});
        capabilities = result;
        supportedCommands = new Set(result.commands);
        if (result.permissions.includes(notificationPermission)) {
            setInlineStatus(elements.notificationState, '通知权限当前可用。', 'ready');
        } else {
            setInlineStatus(elements.notificationState, '尚未请求通知权限。');
        }
        updateRuntimeInfo();
        applyCapabilities();
        return result;
    }

    async function initialize() {
        if (!bridge || !transport) {
            setConnection('当前页面未连接 Kasumi', 'error');
            addActivity('找不到 Windows或 Android宿主消息通道。', 'error');
            return;
        }

        try {
            const [info, currentCapabilities] = await Promise.all([
                loadAppInfo(),
                loadCapabilities()
            ]);
            elements.platform.textContent = info.platform === 'windows' ? 'Windows' : 'Android';
            setConnection('已连接宿主', 'ready');
            addActivity(`连接正常，发现 ${currentCapabilities.commands.length}项能力。`);
            subscribeToEvents();
        } catch (error) {
            setConnection('无法连接宿主', 'error');
            addActivity(errorText(error), 'error');
        }
    }

    async function subscribeToEvents() {
        const bus = requireBridge();
        eventListeners.set('window.resized', [(payload) => {
            const width = Number(payload?.width);
            const height = Number(payload?.height);
            if (!Number.isFinite(width) || !Number.isFinite(height)) {
                return;
            }
            elements.resizeValue.textContent = `${width} × ${height} px`;
            setInlineStatus(
                elements.windowEventState,
                `内容区现在为 ${width} × ${height}物理像素。`,
                'ready'
            );
        }]);
        eventListeners.set('webview.reloaded', [() => {
            const now = new Intl.DateTimeFormat('zh-CN', {
                hour: '2-digit',
                minute: '2-digit',
                second: '2-digit'
            }).format(new Date());
            elements.reloadValue.textContent = now;
        }]);

        try {
            await bus.invoke('event.subscribe', { event: 'window.resized' });
            await bus.invoke('event.subscribe', { event: 'webview.reloaded' });
            addActivity('已订阅窗口尺寸与页面重载事件。');
        } catch (error) {
            addActivity(`事件订阅失败：${errorText(error)}`, 'error');
        }
    }

    async function runFileRoundtrip() {
        const bus = requireBridge();
        const content = `${elements.fileContent.value.trim()} ${new Date().toISOString()}`;
        await bus.invoke('fs.mkdir', {
            path: SAMPLE_DIRECTORY_PATH,
            recursive: true
        });
        await bus.invoke('fs.writeText', {
            path: SAMPLE_FILE_PATH,
            content
        });
        const exists = await bus.invoke('fs.exists', { path: SAMPLE_FILE_PATH });
        const readResult = await bus.invoke('fs.readText', { path: SAMPLE_FILE_PATH });
        const stat = await bus.invoke('fs.stat', { path: SAMPLE_FILE_PATH });
        const directory = await bus.invoke('fs.readDir', { path: SAMPLE_DIRECTORY_PATH });
        const text = typeof readResult === 'string' ? readResult : `较大内容已写入 ${readResult.file}`;
        setResult(elements.fileResult, {
            exists: exists.exists,
            content: text,
            stat,
            entries: directory.entries
        });
        addActivity(`文件往返完成，目录中有 ${directory.entries.length}项。`);
    }

    async function listSampleDirectory() {
        const result = await requireBridge().invoke('fs.readDir', {
            path: SAMPLE_DIRECTORY_PATH
        });
        setResult(elements.fileResult, result);
        addActivity(`数据目录中有 ${result.entries.length}项。`);
    }

    async function removeSampleFile() {
        const removed = await requireBridge().invoke('fs.remove', {
            path: SAMPLE_FILE_PATH
        });
        setResult(elements.fileResult, removed);
        addActivity(removed.removed ? '示例文件已删除。' : '示例文件本来就不存在。');
    }

    async function showDialog(command) {
        const bus = requireBridge();
        const result = await bus.invoke(command, {
            title: 'Kasumi示例',
            message: '这是由当前系统绘制的原生对话框。'
        });
        addActivity(
            command === 'dialog.confirm'
                ? `确认对话框返回 ${result.confirmed ? '确定' : '取消'}。`
                : '消息对话框已关闭。'
        );
    }

    async function openPickedFile() {
        const bus = requireBridge();
        const picked = await bus.invoke('dialog.openFile', {
            filters: ['txt', 'md', 'json']
        });
        if (!picked.file) {
            setResult(elements.dialogResult, '没有选择文件。');
            addActivity('文件选择已取消。');
            return;
        }

        let preview;
        try {
            const value = await bus.invoke('fs.readText', { path: picked.file });
            preview = typeof value === 'string' ? value : `内容已暂存到 ${value.file}`;
        } catch (error) {
            preview = `无法读取选中文件：${errorText(error)}`;
        }
        setResult(elements.dialogResult, {
            handle: picked.file,
            preview: preview.slice(0, 4000)
        });
        addActivity(`已选择 ${picked.file}。`);
    }

    async function savePickedFile() {
        const result = await requireBridge().invoke('dialog.saveFile', {
            suggestedName: 'kasumi-note.txt'
        });
        if (!result.file) {
            setResult(elements.dialogResult, '已取消另存为。');
            addActivity('另存为已取消。');
            return;
        }
        await requireBridge().invoke('fs.writeText', {
            path: result.file,
            content: elements.fileContent.value
        });
        setResult(elements.dialogResult, {
            handle: result.file,
            written: true
        });
        addActivity(`内容已写入 ${result.file}。`);
    }

    async function writeClipboard() {
        const text = elements.clipboardInput.value;
        if (!text) {
            throw new BridgeError('INVALID_ARGS', '先输入要复制的文字。');
        }
        const result = await requireBridge().invoke('clipboard.writeText', { text });
        addActivity(result.written ? '文字已写入系统剪贴板。' : '剪贴板拒绝写入。');
    }

    async function readClipboard() {
        const result = await requireBridge().invoke('clipboard.readText', {});
        elements.clipboardInput.value = result.text;
        addActivity(
            result.text
                ? `已从剪贴板读回 ${result.text.length}个字符。`
                : '剪贴板当前没有文本。'
        );
    }

    async function requestNotificationPermission() {
        const result = await requireBridge().invoke('permission.request', {
            permission: notificationPermission
        });
        setInlineStatus(
            elements.notificationState,
            result.granted ? '通知权限已授予。' : '通知权限被拒绝。',
            result.granted ? 'ready' : 'error'
        );
        addActivity(result.granted ? '通知权限已授予。' : '通知权限被拒绝。', result.granted ? 'normal' : 'error');
    }

    async function showNotification() {
        const result = await requireBridge().invoke('notification.show', {
            title: 'Kasumi能力示例',
            body: '通知来自真实的宿主服务。'
        });
        if (!result.shown) {
            throw new BridgeError('PERMISSION_DENIED', '通知没有显示，请先检查权限。');
        }
        addActivity('系统通知已发送。');
    }

    async function setWindowTitle() {
        const result = await requireBridge().invoke('window.setTitle', {
            title: 'Kasumi能力示例'
        });
        addActivity(result.updated ? '窗口标题已更新。' : '窗口标题没有变化。');
    }

    async function resizeWindow() {
        const width = Number(elements.windowWidth.value);
        const height = Number(elements.windowHeight.value);
        if (!Number.isSafeInteger(width) || !Number.isSafeInteger(height)) {
            throw new BridgeError('INVALID_ARGS', '宽度和高度必须是整数。');
        }
        const result = await requireBridge().invoke('window.setSize', {
            width,
            height
        });
        addActivity(result.updated ? `窗口外框已调整为 ${width} × ${height}。` : '窗口尺寸没有变化。');
    }

    async function minimizeWindow() {
        await requireBridge().invoke('window.minimize', {});
        addActivity('窗口已最小化。');
    }

    async function maximizeWindow() {
        await requireBridge().invoke('window.maximize', {});
        addActivity('窗口已最大化。');
    }

    async function closeWindow() {
        const result = await requireBridge().invoke('window.close', {});
        addActivity(result.closed ? '关闭请求已发送。' : '窗口没有关闭。');
    }

    async function downloadExample() {
        const path = elements.downloadPath.value.trim();
        if (!path) {
            throw new BridgeError('INVALID_ARGS', '请输入下载目标路径。');
        }
        const result = await requireBridge().invoke(
            'http.download',
            {
                url: 'https://example.com/',
                path,
                overwrite: true
            },
            DOWNLOAD_TIMEOUT_MS
        );
        addActivity(`下载完成，${result.bytes}字节已写入 ${result.path}。`);
    }

    async function openExternal() {
        const result = await requireBridge().invoke('shell.openExternal', {
            url: 'https://example.com/'
        });
        addActivity(result.opened ? '示例站点已交给系统浏览器。' : '系统没有找到可打开的浏览器。');
    }

    async function loadDeviceInfo() {
        const info = await requireBridge().invoke('device.getInfo', {});
        const labels = {
            platform: '平台',
            osVersion: '系统版本',
            model: '机型',
            manufacturer: '制造商',
            appVersion: '应用版本',
            language: '语言',
            screenWidth: '屏幕宽度',
            screenHeight: '屏幕高度',
            density: '显示密度',
            deviceName: '设备名称',
            timezone: '时区',
            architecture: '架构'
        };
        elements.deviceDetails.replaceChildren();
        for (const [key, label] of Object.entries(labels)) {
            const item = document.createElement('div');
            const term = document.createElement('dt');
            const definition = document.createElement('dd');
            term.textContent = label;
            definition.textContent = String(info[key] ?? '未知');
            item.append(term, definition);
            elements.deviceDetails.append(item);
        }
        addActivity('设备运行环境信息已读取。');
    }

    async function reloadPage() {
        await requireBridge().invoke('event.unsubscribe', { event: 'window.resized' });
        await requireBridge().invoke('event.unsubscribe', { event: 'webview.reloaded' });
        window.location.reload();
    }

    async function exitApp() {
        await requireBridge().invoke('app.exit', {});
        addActivity('退出请求已发送。');
    }

    function bind(button, action) {
        button.addEventListener('click', () => run(button, action));
    }

    bind(elements.ping, async () => {
        const result = await requireBridge().invoke('ping', {});
        if (result.pong !== true) {
            throw new BridgeError('INTERNAL_ERROR', '连接检查返回了意外结果。');
        }
        await Promise.all([loadAppInfo(), loadCapabilities()]);
        setConnection('已连接宿主', 'ready');
        addActivity('连接检查通过。');
    });
    bind(elements.refreshApp, loadAppInfo);
    bind(elements.exit, exitApp);
    bind(elements.fileRoundtrip, runFileRoundtrip);
    bind(elements.fileList, listSampleDirectory);
    bind(elements.fileClear, removeSampleFile);
    bind(elements.dialogMessage, () => showDialog('dialog.message'));
    bind(elements.dialogConfirm, () => showDialog('dialog.confirm'));
    bind(elements.dialogOpen, openPickedFile);
    bind(elements.dialogSave, savePickedFile);
    bind(elements.clipboardWrite, writeClipboard);
    bind(elements.clipboardRead, readClipboard);
    bind(elements.notificationPermission, requestNotificationPermission);
    bind(elements.notificationShow, showNotification);
    bind(elements.windowTitleButton, setWindowTitle);
    bind(elements.windowSizeButton, resizeWindow);
    bind(elements.windowMinimize, minimizeWindow);
    bind(elements.windowMaximize, maximizeWindow);
    bind(elements.windowClose, closeWindow);
    bind(elements.download, downloadExample);
    bind(elements.external, openExternal);
    bind(elements.deviceButton, loadDeviceInfo);
    bind(elements.reload, reloadPage);

    elements.clearActivity.addEventListener('click', () => {
        elements.activityList.replaceChildren();
        activityCount = 0;
        elements.activityCount.textContent = '0条';
        elements.emptyState.hidden = false;
    });

    async function runSmokeTest() {
        try {
            const result = await requireBridge().invoke('ping', {});
            if (result.pong !== true) {
                throw new Error('Ping returned an unexpected result.');
            }
            await requireBridge().invoke('app.exit', {});
        } catch (error) {
            document.title = 'KSM_SMOKE_FAIL';
            console.error(errorText(error));
        }
    }

    if (new URLSearchParams(window.location.search).has('smoke')) {
        void runSmokeTest();
        return;
    }

    applyCapabilities();
    void initialize();
})();
