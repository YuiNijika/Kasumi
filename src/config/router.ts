import { App } from 'vue'
import { createRouter, createWebHistory, RouteRecordRaw } from 'vue-router'

export function setupRouter(app: App) {
    const routes: RouteRecordRaw[] = [
        {
            path: '/',
            name: 'Home',
            component: () => import('../views/index.vue')
        }
    ]

    const router = createRouter({
        history: createWebHistory(),
        routes
    })

    app.use(router)
}