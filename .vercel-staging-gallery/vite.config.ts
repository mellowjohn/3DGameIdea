import { defineConfig, type Plugin } from 'vite'
import react from '@vitejs/plugin-react'
// @ts-expect-error plain ESM plugin, no typings
import { commentsDevApiPlugin } from './scripts/comments-dev-plugin.mjs'

export default defineConfig({
  base: '/',
  plugins: [react(), commentsDevApiPlugin() as Plugin],
  server: {
    port: 5174,
  },
})
