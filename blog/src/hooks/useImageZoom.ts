import { useCallback, useEffect, useState, type RefObject } from 'react'
import type { LightboxImage } from '../components/ImageLightbox'

/** Makes images inside a container open in the lightbox (click / Enter / Space). */
export function useImageZoom(
  containerRef: RefObject<HTMLElement | null>,
  contentKey: string,
) {
  const [image, setImage] = useState<LightboxImage | null>(null)

  const close = useCallback(() => {
    setImage(null)
  }, [])

  const openFrom = useCallback((img: HTMLImageElement) => {
    setImage({
      src: img.currentSrc || img.src,
      alt: img.alt ?? '',
    })
  }, [])

  useEffect(() => {
    const root = containerRef.current
    if (!root) {
      return
    }

    const enhance = (img: HTMLImageElement) => {
      if (img.dataset.zoomReady === '1') {
        return
      }
      img.dataset.zoomReady = '1'
      img.classList.add('img-zoomable')
      img.tabIndex = 0
      img.setAttribute('role', 'button')
      const label = img.alt.trim()
        ? `Enlarge image: ${img.alt.trim()}`
        : 'Enlarge image'
      img.setAttribute('aria-label', label)
      img.setAttribute('title', 'Click to enlarge')
    }

    root.querySelectorAll('img').forEach((node) => {
      if (node instanceof HTMLImageElement) {
        enhance(node)
      }
    })

    const onClick = (event: MouseEvent) => {
      const target = event.target
      if (!(target instanceof HTMLImageElement) || !root.contains(target)) {
        return
      }
      event.preventDefault()
      openFrom(target)
    }

    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key !== 'Enter' && event.key !== ' ') {
        return
      }
      const target = event.target
      if (!(target instanceof HTMLImageElement) || !root.contains(target)) {
        return
      }
      event.preventDefault()
      openFrom(target)
    }

    root.addEventListener('click', onClick)
    root.addEventListener('keydown', onKeyDown)
    return () => {
      root.removeEventListener('click', onClick)
      root.removeEventListener('keydown', onKeyDown)
    }
  }, [containerRef, contentKey, openFrom])

  return { image, close }
}
