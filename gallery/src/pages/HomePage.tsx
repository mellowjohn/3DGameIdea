import { useEffect, useMemo, useState } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { BrandMark, BrandTitleLogo } from '../components/BrandMark'
import type { Catalog, CatalogItem, Category } from '../types'

const CATEGORY_COPY: { id: Category; title: string; blurb: string }[] = [
  { id: 'concept', title: 'Concepts', blurb: 'LD perspectives, scene sets, and character art by act.' },
  { id: 'kit', title: 'Kits', blurb: 'Modular prop and environment sheets for Blockbench production.' },
  { id: 'model', title: 'Models', blurb: 'Runtime glTF props and characters with orbit review.' },
  { id: 'ui', title: 'UI chrome', blurb: 'Menu, HUD, dialogue, and quest visual targets.' },
  { id: 'cartography', title: 'Cartography', blurb: 'Heraldry, frames, icons, and world map plates.' },
  { id: 'font', title: 'Type', blurb: 'Shipped UI and map faces with live specimens.' },
  { id: 'reference', title: 'Reference', blurb: 'Player turnarounds and production orthos.' },
]

export function HomePage() {
  const [catalog, setCatalog] = useState<Catalog | null>(null)
  const [error, setError] = useState<string | null>(null)
  const navigate = useNavigate()

  useEffect(() => {
    let cancelled = false
    fetch('/catalog.json')
      .then(async (res) => {
        if (res.status === 401) {
          navigate('/login', { replace: true })
          return null
        }
        if (!res.ok) throw new Error(`Catalog failed (${res.status}). Run npm run sync first.`)
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

  const counts = useMemo(() => {
    const map = new Map<string, number>()
    if (!catalog) return map
    for (const item of catalog.items) {
      map.set(item.category, (map.get(item.category) || 0) + 1)
    }
    return map
  }, [catalog])

  const featured = useMemo(() => pickFeatured(catalog?.items ?? []), [catalog])

  async function onLogout() {
    try {
      await fetch('/api/logout', { method: 'POST' })
    } catch {
      // ignore
    }
    navigate('/login', { replace: true })
  }

  if (error) {
    return (
      <div className="error-state">
        <p>{error}</p>
      </div>
    )
  }

  return (
    <div className="home">
      <header className="home-top">
        <div className="home-top-brand">
          <BrandMark size="sm" />
          <span className="home-top-label">Art Atlas</span>
        </div>
        <nav className="home-nav">
          <Link to="/browse">Browse</Link>
          <button type="button" className="btn btn-ghost" onClick={() => void onLogout()}>
            Log out
          </button>
        </nav>
      </header>

      <section className="home-hero">
        <div className="home-hero-glow" aria-hidden />
        <div className="home-hero-grid" aria-hidden />
        <BrandMark size="lg" to={null} className="home-hero-emblem" />
        <BrandTitleLogo className="home-title-logo" />
        <p className="home-tool-badge">Art Atlas</p>
        <p className="home-kicker">Private collaboration tool</p>
        <p className="home-lead">
          One shelf for every concept plate, kit sheet, runtime model, UI target, typeface, and map
          asset the team is building toward.
        </p>
        <div className="home-cta-row">
          <Link to="/browse" className="btn btn-primary home-cta">
            Enter the library
          </Link>
          <a href="#how" className="btn home-cta-secondary">
            How feedback works
          </a>
        </div>
        {catalog && (
          <p className="home-sync-meta">
            {catalog.itemCount} assets · last sync{' '}
            {new Date(catalog.generatedAt).toLocaleString()}
          </p>
        )}
      </section>

      {featured.length > 0 && (
        <section className="home-spotlight" aria-label="Featured art">
          <div className="home-spotlight-rail">
            {featured.map((item, i) => (
              <Link
                key={item.id}
                to="/browse"
                className={`home-spotlight-tile tile-${i % 3}`}
                style={
                  item.thumbPath || item.mediaPath
                    ? {
                        backgroundImage: `linear-gradient(180deg, transparent 40%, rgba(10,8,6,0.92)), url(${item.thumbPath || item.mediaPath})`,
                      }
                    : undefined
                }
              >
                <span className="home-spotlight-label">{item.category}</span>
                <span className="home-spotlight-title">{item.title}</span>
              </Link>
            ))}
          </div>
        </section>
      )}

      <section className="home-section" id="collections">
        <h2>What lives here</h2>
        <p className="home-section-lead">
          Filter by act, layer, and type once you open the library. Everything below is regenerated
          from the repo on each deploy.
        </p>
        <div className="home-cat-grid">
          {CATEGORY_COPY.map((cat) => (
            <Link key={cat.id} to={`/browse?category=${cat.id}`} className="home-cat-card">
              <div className="home-cat-count">{counts.get(cat.id) ?? '—'}</div>
              <h3>{cat.title}</h3>
              <p>{cat.blurb}</p>
            </Link>
          ))}
        </div>
      </section>

      <section className="home-section home-section-split" id="how">
        <div>
          <h2>Leave a mark</h2>
          <p className="home-section-lead">
            Open any asset for the full plate, then write feedback in the comments. Like or dislike
            a note to surface what the room agrees with.
          </p>
          <ul className="home-steps">
            <li>
              <span>01</span> Browse and filter the full catalog
            </li>
            <li>
              <span>02</span> Open a piece in the lightbox
            </li>
            <li>
              <span>03</span> Comment with your name · vote with ▲ / ▼
            </li>
          </ul>
        </div>
        <div className="home-callout">
          <BrandMark size="md" to={null} className="home-callout-mark" />
          <p className="home-callout-kicker">Shared password</p>
          <p>
            Collaborators share one gate password. Comments stick for the whole team when Redis is
            configured on the deploy. Local dev keeps notes on disk under <code>.data/</code>.
          </p>
          <Link to="/browse" className="btn btn-primary">
            Start reviewing
          </Link>
        </div>
      </section>

      <footer className="home-footer">
        <span className="home-footer-brand">
          <img src="/branding/icon.png" alt="" width={22} height={22} />
          Wrathful Conquest · Art Atlas
        </span>
        <span>Internal · not public marketing</span>
      </footer>
    </div>
  )
}

function pickFeatured(items: CatalogItem[]): CatalogItem[] {
  const prefer = items.filter(
    (i) =>
      i.kind === 'image' &&
      (i.category === 'concept' || i.category === 'kit' || i.category === 'ui') &&
      (i.thumbPath || i.mediaPath),
  )
  const pool = prefer.length >= 6 ? prefer : items.filter((i) => i.kind === 'image')
  return [...pool]
    .sort((a, b) => a.id.localeCompare(b.id))
    .filter((_, idx) => idx % Math.max(1, Math.floor(pool.length / 8)) === 0)
    .slice(0, 8)
}
