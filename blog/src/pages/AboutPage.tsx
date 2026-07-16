import { useEffect, useState } from 'react'
import { SubscribeForm } from '../components/SubscribeForm'
import { assetUrl } from '../lib/assets'
import { loadAbout, type AboutDoc } from '../lib/about'
import { setPageMeta } from '../lib/seo'

export function AboutPage() {
  const [doc, setDoc] = useState<AboutDoc | null>(null)

  useEffect(() => {
    void loadAbout().then((about) => {
      setDoc(about)
      setPageMeta({
        title: about.title,
        description: about.summary,
        path: '/about',
        image: '/images/og-default.jpg',
      })
    })
  }, [])

  return (
    <main className="page page--read">
      <div className="page-banner">
        <img src={assetUrl('/images/og-default.webp')} alt="" />
      </div>
      <header className="page-header">
        <h1>{doc?.title ?? 'About'}</h1>
        {doc && <p>{doc.summary}</p>}
      </header>
      {doc ? (
        <div className="prose prose--about" dangerouslySetInnerHTML={{ __html: doc.html }} />
      ) : (
        <p className="muted">Loading…</p>
      )}
      <section className="section--subscribe">
        <SubscribeForm />
      </section>
    </main>
  )
}
