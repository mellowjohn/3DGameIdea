import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { copyFileSync, existsSync } from 'node:fs'
import { resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const rootDir = fileURLToPath(new URL('.', import.meta.url))

/** After build, duplicate index.html as 404.html so GH Pages SPA routes resolve. */
function spaFallbackPlugin() {
  return {
    name: 'spa-github-pages-fallback',
    closeBundle() {
      const dist = resolve(rootDir, 'dist')
      const index = resolve(dist, 'index.html')
      if (existsSync(index)) {
        copyFileSync(index, resolve(dist, '404.html'))
      }
    },
  }
}

// Change base to '/' if you use a custom domain or user/org root site.
export default defineConfig({
  base: '/3DGameIdea/',
  plugins: [react(), spaFallbackPlugin()],
})
