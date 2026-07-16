/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly BASE_URL: string
  readonly VITE_FORMSPREE_ID?: string
}

interface ImportMeta {
  readonly env: ImportMetaEnv
}
