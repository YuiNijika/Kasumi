export type NavigationTarget =
  | 'features'
  | 'platforms'
  | 'stack'
  | 'workflow'
  | 'comparison'

export type NavigationItem = {
  label: string
  target: NavigationTarget
}

export type HeroMetric = {
  value: string
  label: string
}

export type CodeLanguage = 'typescript' | 'cpp' | 'kotlin' | 'json'

export type CodeExample = {
  id: string
  label: string
  method: string
  icon: FeatureIconName
  files: CodeExampleFile[]
}

export type CodeExampleFile = {
  path: string
  language: CodeLanguage
  code: string
}

export type PlatformCard = {
  id: 'windows' | 'android'
  name: string
  shortName: string
  runtime: string
  description: string
  features: string[]
}

export type TechStackItem = {
  name: string
  role: string
  description: string
  mark: string
}

export type WorkflowStep = {
  step: string
  title: string
  description: string
  command: string
}

export type FeatureIconName =
  | 'bell'
  | 'folder'
  | 'window'
  | 'layers'
  | 'plugin'
  | 'panels'
  | 'settings'
  | 'package'

export type AdvantageCard = {
  icon: FeatureIconName
  title: string
  description: string
}

export type CapabilityStage = {
  label: string
  detail: string
}

export type CapabilityOverview = {
  label: string
  web: CapabilityStage
  bridge: CapabilityStage
  platforms: CapabilityStage[]
}

export type ComparisonRow = {
  criterion: string
  kasumi: string
  tauri: string
  electron: string
}

export type FooterLink = {
  label: string
  href: string
}

export type SiteMeta = {
  name: string
  description: string
  repository: string
  documentation: string
}

export const siteMeta = {
  name: 'Kasumi',
  description: '为 Windows 与 Android 构建轻量、清晰、可扩展的应用。',
  repository: 'https://github.com/YuiNijika/Kasumi',
  documentation: 'https://blog.miomoe.cn/docs/kasumi',
} satisfies SiteMeta

export const navigation: NavigationItem[] = [
  { label: '能力', target: 'features' },
  { label: '平台', target: 'platforms' },
  { label: '技术栈', target: 'stack' },
  { label: '开发方式', target: 'workflow' },
  { label: '对比', target: 'comparison' },
]

export const hero = {
  eyebrow: 'Windows · Android',
  titleLead: '渐进式',
  titleAccent: 'WebView 应用框架',
  description:
    '一套 Web 前端构建更原生、轻量、快速的应用程序。',
  command: 'pnpm create kasumi',
  primaryAction: {
    label: '阅读文档',
    href: 'https://blog.miomoe.cn/docs/kasumi',
  },
  secondaryAction: {
    label: '查看源码',
    href: 'https://github.com/YuiNijika/Kasumi',
  },
  metrics: [
    { value: '2', label: '目标平台' },
    { value: '48', label: '协议命令' },
    { value: '14', label: '内置插件' },
  ] satisfies HeroMetric[],
} as const

export const heroCodeExamples: CodeExample[] = [
  {
    id: 'app-info',
    label: '应用信息',
    method: 'app.getInfo',
    icon: 'window',
    files: [
      {
        path: 'src/app-info.ts',
        language: 'typescript',
        code: `import { KSM } from '@kasumi/bridge'

await KSM.initialize()

const app = await KSM.app.getInfo()

console.log(app.name, app.version)
console.log(app.platform, app.language)`,
      },
      {
        path: 'kasumi.config.json',
        language: 'json',
        code: `{
  "$schema": "https://raw.githubusercontent.com/YuiNijika/Kasumi/main/protocol/kasumi.config.schema.json",
  "windows": {
    "packageName": "com.example.kirakira",
    "version": "0.1.0",
    "commands": ["app.getInfo", "fs.readDir"]
  },
  "android": {
    "packageName": "com.example.kirakira",
    "version": "0.1.0",
    "commands": ["app.getInfo", "fs.readDir"]
  }
}`,
      },
      {
        path: 'windows/main.cpp',
        language: 'cpp',
        code: `#include <kasumi/ksm_webview2_host.h>

auto host = KSMWebView2Host(
    hwnd, fileSystem, {}, webRoot, userData);

host.Initialize([](bool ok, const std::string& error) {
  if (!ok) {
    OutputDebugStringA(error.c_str());
  }
});`,
      },
      {
        path: 'MainActivity.kt',
        language: 'kotlin',
        code: `val host = KSMWebViewHost(
    activity = this,
    fileSystem = fileSystem,
    hostServices = services,
    entryUrl = "https://app.local/index.html"
)

host.start()`,
      },
    ],
  },
  {
    id: 'files',
    label: '文件系统',
    method: 'fs.readDir',
    icon: 'folder',
    files: [
      {
        path: 'src/files.ts',
        language: 'typescript',
        code: `const { entries } = await KSM.fs.readDir({
  path: 'app://data',
})

const files = entries.filter(
  (entry) => entry.kind === 'file',
)

console.table(files)`,
      },
      {
        path: 'kasumi.config.json',
        language: 'json',
        code: `{
  "windows": {
    "commands": ["fs.readDir"]
  },
  "android": {
    "commands": ["fs.readDir"]
  }
}`,
      },
    ],
  },
  {
    id: 'notification',
    label: '通知',
    method: 'notification.show',
    icon: 'bell',
    files: [
      {
        path: 'src/notify.ts',
        language: 'typescript',
        code: `const result = await KSM.notification.show({
  title: '构建完成',
  body: '构建已经完成。',
})

if (!result.shown) {
  console.warn('通知未显示')
}`,
      },
      {
        path: 'windows/main.cpp',
        language: 'cpp',
        code: `services.ShowNotification(
    L"构建完成",
    L"构建已经完成。");`,
      },
    ],
  },
  {
    id: 'window',
    label: '窗口',
    method: 'window.setTitle',
    icon: 'panels',
    files: [
      {
        path: 'src/window.ts',
        language: 'typescript',
        code: `const result = await KSM.window.setTitle({
  title: '我的应用',
})

console.log(result.updated)`,
      },
      {
        path: 'windows/window.cpp',
        language: 'cpp',
        code: `services.SetWindowTitle(
    "我的应用");`,
      },
    ],
  },
]

export const platforms: PlatformCard[] = [
  {
    id: 'windows',
    name: 'Windows',
    shortName: 'WIN',
    runtime: 'Win32 + WebView2',
    description:
      '窗口、文件、下载、通知和多 WebView 都在原生宿主中完成，界面继续使用你熟悉的 Web 工程。',
    features: ['透明标题栏', '多 WebView', 'DPI 与工作区适配', '签名发布包'],
  },
  {
    id: 'android',
    name: 'Android',
    shortName: 'AND',
    runtime: 'Kotlin + Android WebView',
    description:
      '面向移动端的沉浸式页面容器，补齐权限、生命周期、返回导航和低内存处理。',
    features: ['全屏 WebView', '安全导航策略', '原生权限桥接', '低内存处理'],
  },
]

export const techStack: TechStackItem[] = [
  {
    name: 'TypeScript',
    role: 'Frontend',
    description: '类型安全的桥接接口、事件生命周期和能力查询。',
    mark: 'TS',
  },
  {
    name: 'C++ / Win32',
    role: 'Windows Host',
    description: '窗口系统、WebView2、下载和平台插件的原生运行时。',
    mark: 'C++',
  },
  {
    name: 'Kotlin',
    role: 'Android Host',
    description: 'Activity 生命周期、权限、WebView 和 Android 插件。',
    mark: 'KT',
  },
  {
    name: 'JSON Protocol',
    role: 'Contract',
    description: '命令、事件、权限与生成契约的单一事实源。',
    mark: '{}',
  },
]

export const workflowSteps: WorkflowStep[] = [
  {
    step: '01',
    title: '选择模板',
    description: 'Basic、React、Vue、Next.js 或 Nuxt，保留原来的工程习惯。',
    command: 'pnpm create kasumi',
  },
  {
    step: '02',
    title: '启动开发',
    description: '前端热更新与原生窗口同时启动，修改页面即可看到结果。',
    command: 'pnpm ksm dev',
  },
  {
    step: '03',
    title: '构建应用',
    description: '选择 Windows、Android 或一次构建两端。',
    command: 'pnpm ksm build --all',
  },
  {
    step: '04',
    title: '生成发布包',
    description: '输出安装包所需的可执行文件、资源和运行说明。',
    command: 'pnpm ksm package --all',
  },
]

export const templates = ['Basic', 'React', 'Vue', 'Next.js', 'Nuxt']

export const advantages: AdvantageCard[] = [
  {
    icon: 'window',
    title: '不用重写界面',
    description: '同一份 Web 页面同时服务 Windows 与 Android，原生能力通过稳定接口连接。',
  },
  {
    icon: 'layers',
    title: '边界清楚',
    description: '前端、Windows 宿主和 Android 宿主分开维护，排错时能沿真实路径定位。',
  },
  {
    icon: 'plugin',
    title: '按需添加能力',
    description: '文件、对话框、窗口、网络、SQL 等能力按插件组织，默认只开放需要的方法。',
  },
  {
    icon: 'panels',
    title: '多窗口隔离',
    description: '每个窗口拥有独立请求、事件和生命周期，适合多面板、浏览器和复杂工作区。',
  },
  {
    icon: 'settings',
    title: '精简配置',
    description: '一份 JSON 管理两端包名、版本和能力 allowlist，不引入冗长配置树。',
  },
  {
    icon: 'package',
    title: '完整发布链路',
    description: '开发、构建、测试、签名和打包都由同一套命令完成，减少手工步骤。',
  },
]

export const capabilityOverview = {
  label: '一份界面，两条原生路径',
  web: {
    label: 'Web 界面',
    detail: 'React / Vue / Nuxt',
  },
  bridge: {
    label: 'Kasumi API',
    detail: '请求 / 事件 / 权限',
  },
  platforms: [
    {
      label: 'Windows',
      detail: 'Win32 + WebView2',
    },
    {
      label: 'Android',
      detail: 'Kotlin + WebView',
    },
  ],
} satisfies CapabilityOverview

export const comparisonRows: ComparisonRow[] = [
  {
    criterion: '界面运行时',
    kasumi: '系统 WebView2 / Android WebView',
    tauri: '系统 WebView',
    electron: '随应用分发 Chromium',
  },
  {
    criterion: 'Windows',
    kasumi: '支持',
    tauri: '支持',
    electron: '支持',
  },
  {
    criterion: 'Android',
    kasumi: '支持',
    tauri: '支持',
    electron: '不提供官方移动端运行时',
  },
  {
    criterion: '宿主边界',
    kasumi: 'C++ / Kotlin 插件',
    tauri: 'Rust 命令与插件',
    electron: '主进程与 Node.js 模块',
  },
  {
    criterion: '能力扩展',
    kasumi: '协议契约 + 原生插件',
    tauri: '命令 + 插件生态',
    electron: 'npm 包 + IPC',
  },
  {
    criterion: '多 WebView',
    kasumi: '支持窗口级隔离',
    tauri: '支持多 WebView',
    electron: '支持多 WebContentsView',
  },
  {
    criterion: '配置方式',
    kasumi: '精简 JSON',
    tauri: 'JSON/TOML 与能力配置',
    electron: '主进程配置',
  },
]

export const footerLinks: FooterLink[] = [
  {
    label: '文档',
    href: 'https://blog.miomoe.cn/docs/kasumi',
  },
  {
    label: 'GitHub',
    href: 'https://github.com/YuiNijika/Kasumi',
  },
  {
    label: '插件',
    href: 'https://blog.miomoe.cn/docs/kasumi/plugins',
  },
]
