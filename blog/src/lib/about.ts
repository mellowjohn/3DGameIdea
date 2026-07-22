import { marked } from 'marked'

export type AboutDoc = {
  title: string
  summary: string
  html: string
}

function parseFrontmatter(raw: string): { data: Record<string, string>; content: string } {
  const match = raw.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n([\s\S]*)$/)
  if (!match) {
    return { data: {}, content: raw }
  }
  const data: Record<string, string> = {}
  for (const line of match[1].split(/\r?\n/)) {
    const kv = line.match(/^([A-Za-z0-9_-]+):\s*(.*)$/)
    if (kv) {
      data[kv[1]] = kv[2].trim()
    }
  }
  return { data, content: match[2].trim() }
}

const aboutRaw = import.meta.glob('../../content/about.md', {
  query: '?raw',
  import: 'default',
  eager: true,
}) as Record<string, string>

let cached: AboutDoc | null = null

export async function loadAbout(): Promise<AboutDoc> {
  if (cached) {
    return cached
  }
  const raw = Object.values(aboutRaw)[0] ?? ''
  const { data, content } = parseFrontmatter(raw)
  const html = await marked.parse(content)
  const base = import.meta.env.BASE_URL
  const withBase = html.replace(/(src|href)="\/(?!\/)/g, `$1="${base}`)
  cached = {
    title: data.title || 'About',
    summary:
      data.summary ||
      'Who I am, what Wrathful Conquest is, and how this public build log works.',
    html: withBase,
  }
  return cached
}
