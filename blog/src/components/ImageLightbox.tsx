import { useEffect, useId, useRef } from 'react'

export type LightboxImage = {
  src: string
  alt: string
}

type Props = {
  image: LightboxImage | null
  onClose: () => void
}

export function ImageLightbox({ image, onClose }: Props) {
  const closeRef = useRef<HTMLButtonElement>(null)
  const previouslyFocused = useRef<HTMLElement | null>(null)
  const titleId = useId()

  useEffect(() => {
    if (!image) {
      return
    }

    previouslyFocused.current =
      document.activeElement instanceof HTMLElement ? document.activeElement : null
    const previousOverflow = document.body.style.overflow
    document.body.style.overflow = 'hidden'

    const frame = window.requestAnimationFrame(() => {
      closeRef.current?.focus()
    })

    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        event.preventDefault()
        onClose()
        return
      }
      if (event.key !== 'Tab') {
        return
      }
      // Single focusable control in the dialog: keep focus on Close.
      event.preventDefault()
      closeRef.current?.focus()
    }

    document.addEventListener('keydown', onKeyDown)
    return () => {
      window.cancelAnimationFrame(frame)
      document.removeEventListener('keydown', onKeyDown)
      document.body.style.overflow = previousOverflow
      previouslyFocused.current?.focus()
    }
  }, [image, onClose])

  if (!image) {
    return null
  }

  const label = image.alt.trim() || 'Enlarged image'

  return (
    <div
      className="lightbox"
      role="dialog"
      aria-modal="true"
      aria-labelledby={titleId}
      onClick={onClose}
    >
      <div className="lightbox__panel" onClick={(event) => event.stopPropagation()}>
        <div className="lightbox__toolbar">
          <p id={titleId} className="lightbox__title">
            {label}
          </p>
          <button
            ref={closeRef}
            type="button"
            className="lightbox__close"
            onClick={onClose}
          >
            Close
          </button>
        </div>
        <img className="lightbox__image" src={image.src} alt={image.alt} />
        <p className="lightbox__hint muted">Press Escape or Close to return to the article.</p>
      </div>
    </div>
  )
}
