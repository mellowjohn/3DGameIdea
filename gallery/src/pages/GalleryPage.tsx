import { useCallback, useEffect, useMemo, useState } from 'react'
import { Link, useNavigate, useSearchParams } from 'react-router-dom'
import { AssetGrid } from '../components/AssetCard'
import { BrandMark } from '../components/BrandMark'
import { FilterBar, matchesFilters } from '../components/FilterBar'
import { Lightbox } from '../components/Lightbox'
import type { Catalog, CatalogItem, Category, GalleryFilters } from '../types'

export function GalleryPage() {
  const [catalog, setCatalog] = useState<Catalog | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [selected, setSelected] = useState<CatalogItem | null>(null)
  const [params] = useSearchParams()
  const initialCategory = (params.get('category') as Category | null) || 'all'
  const [filters, setFilters] = useState<GalleryFilters>({
    search: '',
    category: isCategory(initialCategory) ? initialCategory : 'all',
    act: 'all',
    layer: 'all',
  })
  const navigate = useNavigate()

  useEffect(() => {
    let cancelled = false
    fetch('/catalog.json')
      .then(async (res) => {
        if (res.status === 401) {
          navigate('/login', { replace: true })
          return null
        }
        if (!res.ok) {
          throw new Error(`Catalog failed (${res.status}). Run npm run sync first.`)
        }
        return (await res.json()) as Catalog
      })
      .then((data) => {
        if (!cancelled && data) setCatalog(data)
      })
      .catch((err: Error) => {
        if (!cancelled) setError(err.message)
      })
    return () => {
      cancelled = true
    }
  }, [navigate])

  const shown = useMemo(() => {
    if (!catalog) return []
    return catalog.items.filter((item) => matchesFilters(item, filters))
  }, [catalog, filters])

  const onLogout = useCallback(async () => {
    try {
      await fetch('/api/logout', { method: 'POST' })
    } catch {
      // ignore
    }
    navigate('/login', { replace: true })
  }, [navigate])

  if (error) {
    return (
      <div className="error-state">
        <p>{error}</p>
        <p>
          From <code>gallery/</code>, run <code>npm run sync</code> then <code>npm run dev</code>.
        </p>
      </div>
    )
  }

  if (!catalog) {
    return <div className="loading">Loading catalog…</div>
  }

  const generated = new Date(catalog.generatedAt).toLocaleString()

  return (
    <>
      <header className="app-header">
        <div className="app-header-brand">
          <BrandMark size="sm" />
          <div>
            <h1>
              <Link to="/" className="header-home-link">
                Art Atlas
              </Link>
              <span className="header-sep">·</span> Browse
            </h1>
            <div className="meta">
              {catalog.itemCount} assets · synced {generated}
            </div>
          </div>
        </div>
        <div className="actions">
          <Link to="/" className="btn btn-ghost">
            Overview
          </Link>
          <button type="button" className="btn btn-ghost" onClick={() => void onLogout()}>
            Log out
          </button>
        </div>
      </header>
      <div className="gallery-layout">
        <FilterBar
          filters={filters}
          onChange={setFilters}
          counts={{ total: catalog.itemCount, shown: shown.length }}
        />
        <main className="gallery-main">
          <AssetGrid items={shown} onSelect={setSelected} />
        </main>
      </div>
      {selected && <Lightbox item={selected} onClose={() => setSelected(null)} />}
    </>
  )
}

function isCategory(v: string): v is Category | 'all' {
  return (
    v === 'all' ||
    v === 'concept' ||
    v === 'kit' ||
    v === 'reference' ||
    v === 'ui' ||
    v === 'cartography' ||
    v === 'font' ||
    v === 'icon' ||
    v === 'branding' ||
    v === 'model'
  )
}
