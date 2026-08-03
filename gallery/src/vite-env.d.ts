/// <reference types="vite/client" />

import type { DetailedHTMLProps, HTMLAttributes, Ref } from 'react'

type ModelViewerAttributes = DetailedHTMLProps<HTMLAttributes<HTMLElement>, HTMLElement> & {
  src?: string
  poster?: string
  alt?: string
  'camera-controls'?: boolean
  'touch-action'?: string
  'auto-rotate'?: boolean
  'shadow-intensity'?: string | number
  exposure?: string | number
  'environment-image'?: string
  'interaction-prompt'?: 'auto' | 'when-focused' | 'none'
  reveal?: 'auto' | 'interaction' | 'manual'
  loading?: 'auto' | 'lazy' | 'eager'
  ar?: boolean
  class?: string
  className?: string
  ref?: Ref<HTMLElement>
}

declare module 'react' {
  namespace JSX {
    interface IntrinsicElements {
      'model-viewer': ModelViewerAttributes
    }
  }
}

declare global {
  namespace JSX {
    interface IntrinsicElements {
      'model-viewer': ModelViewerAttributes
    }
  }
}

export {}
