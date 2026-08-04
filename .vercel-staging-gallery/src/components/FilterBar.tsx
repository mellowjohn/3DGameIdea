import type { CatalogItem, GalleryFilters } from '../types'

type Props = {
  filters: GalleryFilters
  onChange: (next: GalleryFilters) => void
  counts: { total: number; shown: number }
}

const CATEGORIES: { value: GalleryFilters['category']; label: string }[] = [
  { value: 'all', label: 'All categories' },
  { value: 'concept', label: 'Concept' },
  { value: 'kit', label: 'Kit' },
  { value: 'reference', label: 'Reference' },
  { value: 'ui', label: 'UI' },
  { value: 'cartography', label: 'Cartography' },
  { value: 'font', label: 'Font' },
  { value: 'icon', label: 'Icon' },
  { value: 'branding', label: 'Branding' },
]

const ACTS: { value: GalleryFilters['act']; label: string }[] = [
  { value: 'all', label: 'All acts' },
  { value: 'act0', label: 'Act 0' },
  { value: 'act1', label: 'Act 1' },
  { value: 'act2', label: 'Act 2' },
  { value: 'act3', label: 'Act 3' },
  { value: 'kits', label: 'Kits' },
  { value: 'none', label: 'Unassigned' },
]

const LAYERS: { value: GalleryFilters['layer']; label: string }[] = [
  { value: 'all', label: 'All types' },
  { value: 'ld', label: 'LD perspective' },
  { value: 'scene', label: 'Scene set' },
  { value: 'char', label: 'Character' },
  { value: 'kit', label: 'Kit sheet' },
  { value: 'legacy', label: 'Legacy / menu' },
  { value: 'menu', label: 'Menu UI' },
  { value: 'hud', label: 'HUD UI' },
  { value: 'dialogue', label: 'Dialogue UI' },
  { value: 'quest', label: 'Quest UI' },
  { value: 'map', label: 'Map / cartography' },
  { value: 'font', label: 'Font' },
  { value: 'icon', label: 'Icon' },
  { value: 'other', label: 'Other' },
]

export function FilterBar({ filters, onChange, counts }: Props) {
  return (
    <aside className="filter-bar">
      <div className="filter-group">
        <label htmlFor="filter-search">Search</label>
        <input
          id="filter-search"
          type="search"
          placeholder="Title, file, tag…"
          value={filters.search}
          onChange={(e) => onChange({ ...filters, search: e.target.value })}
        />
      </div>
      <div className="filter-group">
        <label htmlFor="filter-category">Category</label>
        <select
          id="filter-category"
          value={filters.category}
          onChange={(e) =>
            onChange({ ...filters, category: e.target.value as GalleryFilters['category'] })
          }
        >
          {CATEGORIES.map((c) => (
            <option key={c.value} value={c.value}>
              {c.label}
            </option>
          ))}
        </select>
      </div>
      <div className="filter-group">
        <label htmlFor="filter-act">Act</label>
        <select
          id="filter-act"
          value={filters.act}
          onChange={(e) => onChange({ ...filters, act: e.target.value as GalleryFilters['act'] })}
        >
          {ACTS.map((c) => (
            <option key={c.value} value={c.value}>
              {c.label}
            </option>
          ))}
        </select>
      </div>
      <div className="filter-group">
        <label htmlFor="filter-layer">Type / layer</label>
        <select
          id="filter-layer"
          value={filters.layer}
          onChange={(e) =>
            onChange({ ...filters, layer: e.target.value as GalleryFilters['layer'] })
          }
        >
          {LAYERS.map((c) => (
            <option key={c.value} value={c.value}>
              {c.label}
            </option>
          ))}
        </select>
      </div>
      <p className="gallery-status" style={{ margin: 0 }}>
        Showing {counts.shown} of {counts.total}
      </p>
      <button
        type="button"
        className="btn btn-ghost"
        onClick={() =>
          onChange({ search: '', category: 'all', act: 'all', layer: 'all' })
        }
      >
        Clear filters
      </button>
    </aside>
  )
}

export function matchesFilters(item: CatalogItem, filters: GalleryFilters): boolean {
  if (filters.category !== 'all' && item.category !== filters.category) return false
  if (filters.act !== 'all' && item.act !== filters.act) return false
  if (filters.layer !== 'all' && item.layer !== filters.layer) return false

  const q = filters.search.trim().toLowerCase()
  if (!q) return true

  const hay = [
    item.title,
    item.filename,
    item.sourcePath,
    item.category,
    item.act,
    item.layer,
    ...item.tags,
  ]
    .join(' ')
    .toLowerCase()

  return hay.includes(q)
}
