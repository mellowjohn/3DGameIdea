import { useEffect } from 'react'
import type { CatalogItem } from '../types'
import { CommentSection } from './CommentSection'
import { FontPreview } from './FontPreview'
import { ModelViewer } from './ModelViewer'

type Props = {
  item: CatalogItem
  onClose: () => void
}

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`
  return `${(n / (1024 * 1024)).toFixed(1)} MB`
}

export function Lightbox({ item, onClose }: Props) {
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose()
    }
    window.addEventListener('keydown', onKey)
    const prev = document.body.style.overflow
    document.body.style.overflow = 'hidden'
    return () => {
      window.removeEventListener('keydown', onKey)
      document.body.style.overflow = prev
    }
  }, [onClose])

  return (
    <div
      className="lightbox"
      role="dialog"
      aria-modal="true"
      aria-label={item.title}
      onClick={onClose}
    >
      <div className="lightbox-panel lightbox-panel-wide" onClick={(e) => e.stopPropagation()}>
        <div className="lightbox-header">
          <div>
            <h2>{item.title}</h2>
            <p className="path">{item.sourcePath}</p>
          </div>
          <button type="button" className="btn" onClick={onClose}>
            Close
          </button>
        </div>
        <div className="lightbox-scroll">
          <div className={`lightbox-body${item.kind === 'model' ? ' lightbox-body-model' : ''}`}>
            {item.kind === 'font' ? (
              <FontPreview src={item.mediaPath} familyId={item.id} size="detail" />
            ) : item.kind === 'model' ? (
              <ModelViewer
                src={item.mediaPath}
                alt={item.title}
                size="detail"
              />
            ) : (
              <img src={item.mediaPath} alt={item.title} />
            )}
          </div>
          <div className="lightbox-footer">
            <div className="tags">
              <span className="chip chip-accent">{item.category}</span>
              <span className="chip">{item.act}</span>
              <span className="chip">{item.layer}</span>
              <span className="chip">{formatBytes(item.bytes)}</span>
              {item.tags.map((t) => (
                <span key={t} className="chip">
                  {t}
                </span>
              ))}
            </div>
            <a className="btn" href={item.mediaPath} target="_blank" rel="noreferrer">
              Open original
            </a>
          </div>
          <CommentSection assetId={item.id} />
        </div>
      </div>
    </div>
  )
}
