import { useEffect, useRef, useState } from 'react'

type Props = {
  src: string
  poster?: string | null
  alt: string
  size?: 'card' | 'detail'
}

let modelViewerLoad: Promise<void> | null = null

function ensureModelViewer(): Promise<void> {
  if (typeof customElements !== 'undefined' && customElements.get('model-viewer')) {
    return Promise.resolve()
  }
  if (!modelViewerLoad) {
    modelViewerLoad = import('@google/model-viewer').then(() => undefined)
  }
  return modelViewerLoad
}

export function ModelViewer({ src, poster, alt, size = 'detail' }: Props) {
  const hostRef = useRef<HTMLDivElement>(null)
  const viewerRef = useRef<HTMLElement | null>(null)
  const [inView, setInView] = useState(size === 'detail')
  const [ready, setReady] = useState(
    () => typeof customElements !== 'undefined' && Boolean(customElements.get('model-viewer')),
  )
  const [modelLoaded, setModelLoaded] = useState(false)

  useEffect(() => {
    if (size === 'detail') return
    const el = hostRef.current
    if (!el || typeof IntersectionObserver === 'undefined') {
      setInView(true)
      return
    }
    const io = new IntersectionObserver(
      (entries) => {
        if (entries.some((e) => e.isIntersecting)) {
          setInView(true)
          io.disconnect()
        }
      },
      { rootMargin: '120px 0px', threshold: 0.05 },
    )
    io.observe(el)
    return () => io.disconnect()
  }, [size])

  useEffect(() => {
    if (!inView) return
    let cancelled = false
    void ensureModelViewer().then(() => {
      if (!cancelled) setReady(true)
    })
    return () => {
      cancelled = true
    }
  }, [inView])

  useEffect(() => {
    setModelLoaded(false)
  }, [src])

  useEffect(() => {
    const el = viewerRef.current
    if (!el) return
    const onLoad = () => setModelLoaded(true)
    el.addEventListener('load', onLoad)
    return () => el.removeEventListener('load', onLoad)
  }, [ready, inView, src])

  const className =
    size === 'card' ? 'model-viewer model-viewer-card' : 'model-viewer model-viewer-detail'

  // Cards never use atlas posters — that reads as a texture sheet, not the mesh.
  const posterSrc = size === 'detail' ? poster || undefined : undefined

  return (
    <div
      ref={hostRef}
      className={`model-viewer-host model-viewer-host-${size}${modelLoaded ? ' is-loaded' : ''}`}
    >
      {(!ready || !inView || !modelLoaded) && (
        <div className="model-viewer-fallback" aria-hidden={modelLoaded}>
          <div className="model-viewer-skel" />
          <p>{size === 'card' ? 'Mesh' : 'Loading 3D viewer…'}</p>
        </div>
      )}
      {ready && inView ? (
        <model-viewer
          ref={viewerRef as never}
          className={className}
          src={src}
          poster={posterSrc}
          alt={alt}
          camera-controls={size === 'detail' ? true : undefined}
          touch-action={size === 'detail' ? 'pan-y' : 'none'}
          auto-rotate
          shadow-intensity="0.55"
          exposure="1.1"
          environment-image="neutral"
          interaction-prompt={size === 'detail' ? 'auto' : 'none'}
          reveal="auto"
          loading="lazy"
        />
      ) : null}
    </div>
  )
}
