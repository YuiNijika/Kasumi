import { Alias } from 'vite'
import { resolve } from 'path'

export function getAliasConfig(): Alias[] {
    return [
        {
            find: '@',
            replacement: resolve(__dirname, '../../src')
        }
    ]
}