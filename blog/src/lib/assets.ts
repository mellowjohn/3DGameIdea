/** Resolve a public asset path against the Vite base (e.g. /3DGameIdea/). */
export function assetUrl(path: string): string {
  const normalized = path.startsWith('/') ? path.slice(1) : path
  const base = import.meta.env.BASE_URL
  return `${base}${normalized}`
}
