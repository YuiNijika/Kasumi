import { ServerOptions } from 'vite'

export const getServerConfig = (): ServerOptions => ({
    port: 3000,
    strictPort: true,
    host: true,
    origin: 'http://localhost:3000',
    
    // 添加 CORS 支持
    cors: true,
    
    hmr: {
        overlay: true
    },
    
    watch: {
        usePolling: true,
        interval: 1000
    }
})