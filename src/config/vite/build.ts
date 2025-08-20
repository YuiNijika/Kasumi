import { BuildOptions } from 'vite'
import { EntryFileGenerator } from '../entry-generator'

const entryGenerator = new EntryFileGenerator()

export const getBuildConfig = (): BuildOptions => ({
    outDir: 'assets/dist',
    assetsDir: '',
    rollupOptions: {
        input: {
            components: entryGenerator.getEntryFilePath()
        },
        output: {
            entryFileNames: 'components.js',
            assetFileNames: (assetInfo) => {
                if (assetInfo.name?.endsWith('.css')) {
                    return 'components.css'
                }
                return '[name].[ext]'
            },
            chunkFileNames: '[name].[hash].js',
            format: 'es'
        },
        preserveEntrySignatures: 'allow-extension'
    }
})