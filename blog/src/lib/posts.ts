import { marked } from 'marked'

function rewriteContentUrls(html: string): string {
  const base = import.meta.env.BASE_URL
  return html
    .replace(/(src|href)="\/(?!\/)/g, `$1="${base}`)
    .replace(/\]\(\/(?!\/)/g, `](${base}`)
}

export type PostFrontmatter = {
  title: string
  date: string
  summary: string
  tags: string[]
  cover?: string
  draft?: boolean
}

export type Post = PostFrontmatter & {
  slug: string
  body: string
  html: string
}

function parseFrontmatter(raw: string): { data: Record<string, unknown>; content: string } {
  const match = raw.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n([\s\S]*)$/)
  if (!match) {
    return { data: {}, content: raw }
  }

  const data: Record<string, unknown> = {}
  const lines = match[1].split(/\r?\n/)
  let i = 0
  while (i < lines.length) {
    const line = lines[i]
    const kv = line.match(/^([A-Za-z0-9_-]+):\s*(.*)$/)
    if (!kv) {
      i += 1
      continue
    }
    const key = kv[1]
    const value = kv[2].trim()
    if (value === '') {
      const list: string[] = []
      i += 1
      while (i < lines.length && /^\s*-\s+/.test(lines[i])) {
        list.push(lines[i].replace(/^\s*-\s+/, '').trim())
        i += 1
      }
      data[key] = list
      continue
    }
    if (value === 'true' || value === 'false') {
      data[key] = value === 'true'
    } else if (
      (value.startsWith('"') && value.endsWith('"')) ||
      (value.startsWith("'") && value.endsWith("'"))
    ) {
      data[key] = value.slice(1, -1)
    } else {
      data[key] = value
    }
    i += 1
  }

  return { data, content: match[2].trim() }
}

function toPost(slug: string, raw: string): Post | null {
  const { data, content } = parseFrontmatter(raw)
  const title = typeof data.title === 'string' ? data.title : ''
  const date = typeof data.date === 'string' ? data.date : ''
  const summary = typeof data.summary === 'string' ? data.summary : ''
  if (!title || !date) {
    return null
  }

  const tags = Array.isArray(data.tags)
    ? data.tags.filter((t): t is string => typeof t === 'string')
    : []
  const cover = typeof data.cover === 'string' ? data.cover : undefined

  return {
    slug,
    title,
    date,
    summary,
    tags,
    cover,
    draft: Boolean(data.draft),
    body: content,
    html: '',
  }
}

const modules = import.meta.glob('../../content/posts/*.md', {
  query: '?raw',
  import: 'default',
  eager: true,
}) as Record<string, string>

let cached: Post[] | null = null

export async function loadPosts(): Promise<Post[]> {
  if (cached) {
    return cached
  }

  const posts: Post[] = []

  for (const [path, raw] of Object.entries(modules)) {
    const file = path.split('/').pop() ?? path
    const slug = file.replace(/\.md$/, '')
    const post = toPost(slug, raw)
    if (!post) {
      continue
    }
    if (import.meta.env.PROD && post.draft) {
      continue
    }
    const html = await marked.parse(post.body)
    post.html = rewriteContentUrls(html)
    posts.push(post)
  }

  posts.sort((a, b) => b.date.localeCompare(a.date) || a.title.localeCompare(b.title))
  cached = posts
  return posts
}

export async function getPost(slug: string): Promise<Post | undefined> {
  const posts = await loadPosts()
  return posts.find((p) => p.slug === slug)
}
