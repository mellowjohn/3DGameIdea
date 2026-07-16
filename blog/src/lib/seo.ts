import { assetUrl } from './assets'

const SITE_TITLE = 'Wrathful Conquest Devlog'
const DEFAULT_DESCRIPTION =
  'Build notes from a from-scratch C++20 / Direct3D 12 open-world RPG engine designed for human and AI collaboration.'
const DEFAULT_IMAGE = '/images/og-default.jpg'

function setMeta(attr: 'name' | 'property', key: string, content: string) {
  let el = document.head.querySelector(`meta[${attr}="${key}"]`)
  if (!el) {
    el = document.createElement('meta')
    el.setAttribute(attr, key)
    document.head.appendChild(el)
  }
  el.setAttribute('content', content)
}

export function setPageMeta(options: {
  title?: string
  description?: string
  path?: string
  type?: string
  image?: string
}) {
  const title = options.title ? `${options.title} · Wrathful Conquest` : SITE_TITLE
  const description = options.description ?? DEFAULT_DESCRIPTION
  document.title = title
  setMeta('name', 'description', description)
  setMeta('property', 'og:title', title)
  setMeta('property', 'og:description', description)
  setMeta('property', 'og:type', options.type ?? 'website')
  const imagePath = options.image ?? DEFAULT_IMAGE
  const imageUrl = new URL(assetUrl(imagePath), window.location.origin).toString()
  setMeta('property', 'og:image', imageUrl)
  if (options.path) {
    const url = new URL(options.path.replace(/^\//, ''), window.location.origin + import.meta.env.BASE_URL)
    setMeta('property', 'og:url', url.toString())
  }
}

export function absoluteUrl(path: string): string {
  const base = import.meta.env.BASE_URL
  const normalized = path.startsWith('/') ? path.slice(1) : path
  return new URL(normalized, window.location.origin + base).toString()
}
