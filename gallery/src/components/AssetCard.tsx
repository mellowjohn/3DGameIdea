import type { CatalogItem } from '../types'
import { FontPreview } from './FontPreview'
import { ModelViewer } from './ModelViewer'

type Props = {
  item: CatalogItem
  onSelect: (item: CatalogItem) => void
}

export function AssetCard({ item, onSelect }: Props) {
  return (
    <button type="button" className="asset-card" onClick={() => onSelect(item)}>
      <div className="asset-card-media">
        {item.kind === 'font' ? (
          <div className="asset-card-font">
            <FontPreview src={item.mediaPath} familyId={item.id} size="card" />
          </div>
        ) : item.kind === 'model' ? (
          <div className="asset-card-model">
            <ModelViewer src={item.mediaPath} alt={item.title} size="card" />
            <span className="asset-card-model-badge">3D</span>
          </div>
        ) : (
          <img
            src={item.thumbPath || item.mediaPath}
            alt={item.title}
            loading="lazy"
          />
        )}
      </div>
      <div className="asset-card-body">
        <h3>{item.title}</h3>
        <div className="asset-card-meta">
          <span className="chip chip-accent">{item.category}</span>
          {item.act !== 'none' && <span className="chip">{item.act}</span>}
          {item.layer !== 'other' && <span className="chip">{item.layer}</span>}
        </div>
      </div>
    </button>
  )
}

type GridProps = {
  items: CatalogItem[]
  onSelect: (item: CatalogItem) => void
}

export function AssetGrid({ items, onSelect }: GridProps) {
  if (items.length === 0) {
    return <div className="empty-state">No assets match these filters.</div>
  }

  return (
    <div className="asset-grid">
      {items.map((item) => (
        <AssetCard key={item.id} item={item} onSelect={onSelect} />
      ))}
    </div>
  )
}
