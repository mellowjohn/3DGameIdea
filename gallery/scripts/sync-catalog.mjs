/**
 * Scan repo art/design sources → public/catalog.json + public/media/ + thumbs.
 * Run from gallery/: node scripts/sync-catalog.mjs
 */
import { createHash } from 'node:crypto'
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  readdirSync,
  readFileSync,
  rmSync,
  statSync,
  writeFileSync,
} from 'node:fs'
import { basename, dirname, extname, join, relative, sep } from 'node:path'
import { fileURLToPath } from 'node:url'
import sharp from 'sharp'

const __dirname = fileURLToPath(new URL('.', import.meta.url))
const GALLERY_ROOT = join(__dirname, '..')
const REPO_ROOT = join(GALLERY_ROOT, '..')
const PUBLIC_DIR = join(GALLERY_ROOT, 'public')
const MEDIA_DIR = join(PUBLIC_DIR, 'media')
const THUMBS_DIR = join(MEDIA_DIR, 'thumbs')
const BRAND_DIR = join(PUBLIC_DIR, 'branding')
const CATALOG_PATH = join(PUBLIC_DIR, 'catalog.json')

/** Site chrome assets (always mirrored into public/branding). */
const BRAND_ASSETS = [
  {
    from: 'context/art/branding/wrathful-conquest-icon.png',
    to: 'icon.png',
  },
  {
    from: 'context/art/concepts/wrathful-conquest-title-logo.png',
    to: 'title-logo.png',
  },
  {
    from: 'context/art/concepts/wrathful-conquest-title-logo.opaque.png',
    to: 'title-logo-opaque.png',
  },
  {
    from: 'assets/editor/branding/app-icon-256.png',
    to: 'app-icon.png',
  },
]

const IMAGE_EXT = new Set(['.png', '.jpg', '.jpeg', '.webp', '.gif', '.svg'])
const FONT_EXT = new Set(['.ttf', '.otf', '.woff', '.woff2'])
const THUMB_MAX = 480

/** @typedef {'concept'|'kit'|'reference'|'ui'|'cartography'|'font'|'icon'|'branding'|'model'} Category */
/** @typedef {'act0'|'act1'|'act2'|'act3'|'kits'|'none'} Act */
/** @typedef {'ld'|'scene'|'char'|'kit'|'legacy'|'menu'|'hud'|'dialogue'|'quest'|'font'|'map'|'icon'|'mesh'|'other'} Layer */

/**
 * @typedef {object} CatalogItem
 * @property {string} id
 * @property {string} title
 * @property {string} filename
 * @property {string} sourcePath
 * @property {string} mediaPath
 * @property {string|null} thumbPath
 * @property {Category} category
 * @property {Act} act
 * @property {Layer} layer
 * @property {string[]} tags
 * @property {string} kind  image | font | model
 * @property {number} bytes
 * @property {string|null} notes
 */

/** Runtime glTF stubs / clip bags — not useful for orbit review. */
const MODEL_EXCLUDE = new Set(['player_clips.gltf', 'dead-tree.gltf'])

/** @type {{ id: string, roots: string[], files?: string[], category: Category }[]} */
const INCLUDE = [
  {
    id: 'concepts',
    roots: ['context/art/concepts'],
    category: 'concept',
  },
  {
    id: 'kits',
    roots: ['context/art'],
    category: 'kit',
  },
  {
    id: 'reference',
    roots: ['context/art/reference'],
    category: 'reference',
  },
  {
    id: 'branding',
    roots: ['context/art/branding', 'assets/editor/branding'],
    category: 'branding',
  },
  {
    id: 'cartography',
    roots: ['context/art/cartography'],
    category: 'cartography',
  },
  {
    id: 'story-map',
    roots: ['context/story'],
    category: 'cartography',
  },
  {
    id: 'ui-menu',
    roots: ['context/design/menu-assets'],
    category: 'ui',
  },
  {
    id: 'ui-hud',
    roots: ['context/design/hud-assets'],
    category: 'ui',
  },
  {
    id: 'ui-dialogue',
    roots: ['context/design/dialogue-assets'],
    category: 'ui',
  },
  {
    id: 'ui-quest',
    roots: ['context/design/quest-assets', 'context/design/quest-icons'],
    category: 'ui',
  },
  {
    id: 'fonts',
    roots: ['assets/ui/fonts', 'assets/editor/fonts'],
    category: 'font',
  },
  {
    id: 'icons',
    roots: ['samples/open-world-rpg/assets/ui/icons'],
    category: 'icon',
  },
  {
    id: 'models',
    roots: ['samples/open-world-rpg/assets/models'],
    category: 'model',
  },
]

const EXCLUDE_DIR_PARTS = [
  'world-map-tiles',
  '__pycache__',
  'node_modules',
  'ai-source',
]

const KIT_ROOT_PATTERNS = [
  /^tier\d+-.*\.png$/i,
  /^act0-.*kit-concept\.png$/i,
  /^oak-.*\.png$/i,
  /^theme-palette-swatches\.png$/i,
  /^engine-app-icon\.png$/i,
]

function norm(p) {
  return p.split(sep).join('/')
}

function shouldExclude(relPath) {
  const n = norm(relPath).toLowerCase()
  return EXCLUDE_DIR_PARTS.some((part) => n.includes(`/${part}/`) || n.startsWith(`${part}/`))
}

/**
 * @param {string} dir
 * @param {(full: string, rel: string) => void} visitor
 * @param {string} [base]
 */
function walk(dir, visitor, base = dir) {
  if (!existsSync(dir)) return
  for (const name of readdirSync(dir)) {
    const full = join(dir, name)
    let st
    try {
      st = statSync(full)
    } catch {
      continue
    }
    const rel = relative(base, full)
    if (st.isDirectory()) {
      if (shouldExclude(rel + '/')) continue
      walk(full, visitor, base)
    } else if (st.isFile()) {
      if (shouldExclude(rel)) continue
      visitor(full, rel)
    }
  }
}

function titleFromFilename(name) {
  const stem = name.replace(/\.[^.]+$/, '')
  return stem
    .replace(/[-_]+/g, ' ')
    .replace(/\b\w/g, (c) => c.toUpperCase())
}

/**
 * @param {string} filename
 * @param {string} relFromRepo
 * @param {Category} defaultCategory
 * @returns {{ category: Category, act: Act, layer: Layer, tags: string[] }}
 */
function classify(filename, relFromRepo, defaultCategory) {
  const lower = filename.toLowerCase()
  const pathLower = norm(relFromRepo).toLowerCase()
  /** @type {string[]} */
  const tags = []
  /** @type {Category} */
  let category = defaultCategory
  /** @type {Act} */
  let act = 'none'
  /** @type {Layer} */
  let layer = 'other'

  if (pathLower.includes('/menu-assets/')) {
    category = 'ui'
    layer = 'menu'
    tags.push('ui', 'menu')
  } else if (pathLower.includes('/hud-assets/')) {
    category = 'ui'
    layer = 'hud'
    tags.push('ui', 'hud')
  } else if (pathLower.includes('/dialogue-assets/')) {
    category = 'ui'
    layer = 'dialogue'
    tags.push('ui', 'dialogue')
  } else if (pathLower.includes('/quest-assets/') || pathLower.includes('/quest-icons/')) {
    category = 'ui'
    layer = 'quest'
    tags.push('ui', 'quest')
  }

  if (pathLower.includes('/cartography/') || lower.includes('world-map') || lower.includes('heraldry')) {
    category = 'cartography'
    layer = 'map'
    tags.push('cartography')
    if (lower.includes('heraldry')) tags.push('heraldry')
    if (lower.includes('icon-') || pathLower.includes('/icon')) tags.push('map-icon')
    if (pathLower.includes('/frame')) tags.push('frame')
    if (pathLower.includes('/stroke')) tags.push('stroke')
    if (pathLower.includes('/fog')) tags.push('fog')
    if (pathLower.includes('world-map-layers')) tags.push('map-layer')
  }

  if (pathLower.includes('/fonts/') || FONT_EXT.has(extname(lower))) {
    category = 'font'
    layer = 'font'
    tags.push('font')
  }

  if (pathLower.includes('/icons/') || pathLower.includes('/icons/items/')) {
    category = 'icon'
    layer = 'icon'
    tags.push('icon', 'item')
  }

  if (pathLower.includes('/branding/') || lower.includes('icon') && pathLower.includes('engine-app')) {
    if (category === 'kit' || defaultCategory === 'branding') {
      category = 'branding'
      tags.push('branding')
    }
  }

  if (pathLower.includes('/reference/')) {
    category = 'reference'
    tags.push('reference')
    if (lower.includes('turnaround')) tags.push('turnaround')
    if (lower.includes('flame')) tags.push('vfx')
  }

  if (KIT_ROOT_PATTERNS.some((re) => re.test(filename)) && pathLower.startsWith('context/art/')) {
    // only root-level kit files (not nested dirs already handled)
    if (!pathLower.includes('/concepts/') && !pathLower.includes('/reference/')) {
      category = 'kit'
      layer = 'kit'
      act = 'kits'
      tags.push('kit')
      const tier = lower.match(/tier(\d+)/)
      if (tier) tags.push(`tier${tier[1]}`)
      if (lower.includes('oak')) tags.push('foliage', 'oak')
      if (lower.includes('palette')) tags.push('palette')
    }
  }

  const actMatch = lower.match(/\bact([0-3])\b/)
  if (actMatch) {
    act = /** @type {Act} */ (`act${actMatch[1]}`)
    tags.push(act)
  }

  if (lower.includes('-ld-') || lower.includes('_ld_') || lower.startsWith('ld-')) {
    layer = 'ld'
    tags.push('ld')
    if (category === 'kit') category = 'concept'
  } else if (lower.includes('sceneset') || lower.includes('-scene-')) {
    layer = 'scene'
    tags.push('sceneset', 'scene')
    if (category === 'kit') category = 'concept'
  } else if (lower.includes('-char-') || lower.includes('character')) {
    layer = 'char'
    tags.push('char')
    if (category === 'kit' || category === 'concept') {
      category = 'concept'
    }
  } else if (lower.includes('kit-concept') || lower.includes('-kit-')) {
    layer = 'kit'
    category = 'kit'
    act = act === 'none' ? 'kits' : act
    tags.push('kit')
  }

  // Legacy beat / menu stills under concepts
  if (category === 'concept' && layer === 'other') {
    if (
      lower.includes('prologue') ||
      lower.includes('main-menu') ||
      lower.includes('glass') ||
      lower.includes('title-logo') ||
      /act0-a0-\d+/.test(lower) ||
      lower.includes('character-creation') ||
      lower.includes('difficulty')
    ) {
      layer = 'legacy'
      tags.push('legacy')
    } else if (lower.includes('starter') || lower.includes('item') || lower.includes('bandage') || lower.includes('sword') || lower.includes('bow')) {
      tags.push('item', 'starter')
    }
  }

  if (defaultCategory === 'concept' && category !== 'branding') {
    category = category === 'kit' && !lower.includes('kit') ? 'concept' : category
    if (pathLower.includes('/concepts/')) {
      if (category === 'kit' && !lower.includes('kit')) category = 'concept'
    }
  }

  // Imperium / faction tags from names
  if (lower.includes('imperium')) tags.push('imperium')
  if (lower.includes('tessera')) tags.push('tessera')
  if (lower.includes('luceran')) tags.push('luceran')
  if (lower.includes('creotar')) tags.push('creotar')
  if (lower.includes('arkand')) tags.push('arkand')

  if (defaultCategory === 'model' || pathLower.includes('/assets/models/')) {
    category = 'model'
    layer = lower.includes('player') ? 'char' : 'mesh'
    tags.push('model', 'gltf', layer === 'char' ? 'character' : 'prop')
    if (lower.includes('sword') || lower.includes('bow') || lower.includes('arrow')) {
      tags.push('weapon')
    }
    if (
      lower.includes('oak') ||
      lower.includes('tree') ||
      lower.includes('bush') ||
      lower.includes('stump') ||
      lower.includes('dead_log')
    ) {
      tags.push('foliage')
    }
  }

  const uniqueTags = [...new Set(tags)]
  return { category, act, layer, tags: uniqueTags }
}

/**
 * External texture / bin URIs referenced by a glTF (skip data: URIs).
 * @param {string} gltfAbs
 * @returns {string[]} absolute paths that exist beside the glTF
 */
function gltfExternalDeps(gltfAbs) {
  /** @type {string[]} */
  const deps = []
  try {
    const data = JSON.parse(readFileSync(gltfAbs, 'utf8'))
    const dir = dirname(gltfAbs)
    /** @type {string[]} */
    const uris = []
    for (const img of data.images || []) {
      if (typeof img?.uri === 'string') uris.push(img.uri)
    }
    for (const buf of data.buffers || []) {
      if (typeof buf?.uri === 'string') uris.push(buf.uri)
    }
    for (const uri of uris) {
      if (!uri || uri.startsWith('data:')) continue
      // Reject path escape
      const cleaned = uri.replace(/\\/g, '/').split('/').filter((p) => p && p !== '..').join('/')
      const abs = join(dir, cleaned)
      if (existsSync(abs)) deps.push(abs)
    }
  } catch (err) {
    console.warn(`  glTF parse failed for ${gltfAbs}: ${err.message}`)
  }
  return deps
}

/**
 * @param {string} absPath
 * @param {string} relRepo
 * @returns {string}
 */
function mediaKey(relRepo) {
  return norm(relRepo)
}

async function ensureThumb(srcAbs, thumbAbs) {
  mkdirSync(dirname(thumbAbs), { recursive: true })
  const ext = extname(srcAbs).toLowerCase()
  if (ext === '.svg') {
    copyFileSync(srcAbs, thumbAbs.replace(/\.[^.]+$/, '.svg'))
    return thumbAbs.replace(/\.[^.]+$/, '.svg')
  }
  try {
    await sharp(srcAbs)
      .rotate()
      .resize({ width: THUMB_MAX, height: THUMB_MAX, fit: 'inside', withoutEnlargement: true })
      .webp({ quality: 78 })
      .toFile(thumbAbs)
    return thumbAbs
  } catch (err) {
    console.warn(`  thumb failed for ${srcAbs}: ${err.message}`)
    return null
  }
}

function collectCandidates() {
  /** @type {{ abs: string, rel: string, category: Category }[]} */
  const found = []
  const seen = new Set()

  for (const rule of INCLUDE) {
    for (const rootRel of rule.roots) {
      const rootAbs = join(REPO_ROOT, rootRel)
      if (!existsSync(rootAbs)) {
        console.warn(`skip missing root: ${rootRel}`)
        continue
      }

      const st = statSync(rootAbs)
      if (st.isFile()) {
        const rel = norm(rootRel)
        if (seen.has(rel)) continue
        seen.add(rel)
        found.push({ abs: rootAbs, rel, category: rule.category })
        continue
      }

      // Special: kits only root-level matching files under context/art
      if (rule.id === 'kits') {
        for (const name of readdirSync(rootAbs)) {
          if (!KIT_ROOT_PATTERNS.some((re) => re.test(name))) continue
          const abs = join(rootAbs, name)
          if (!statSync(abs).isFile()) continue
          const rel = norm(join(rootRel, name))
          if (seen.has(rel)) continue
          seen.add(rel)
          found.push({ abs, rel, category: rule.category })
        }
        continue
      }

      // story-map: only official world map
      if (rule.id === 'story-map') {
        const name = 'official-world-map.png'
        const abs = join(rootAbs, name)
        if (existsSync(abs)) {
          const rel = norm(join(rootRel, name))
          if (!seen.has(rel)) {
            seen.add(rel)
            found.push({ abs, rel, category: rule.category })
          }
        }
        continue
      }

      // models: curated root-level runtime glTF (skip clip/stub files)
      if (rule.id === 'models') {
        for (const name of readdirSync(rootAbs)) {
          if (!name.toLowerCase().endsWith('.gltf')) continue
          if (MODEL_EXCLUDE.has(name.toLowerCase()) || MODEL_EXCLUDE.has(name)) continue
          const abs = join(rootAbs, name)
          if (!statSync(abs).isFile()) continue
          const rel = norm(join(rootRel, name))
          if (seen.has(rel)) continue
          seen.add(rel)
          found.push({ abs, rel, category: rule.category })
        }
        continue
      }

      walk(rootAbs, (full, relFromRoot) => {
        const ext = extname(full).toLowerCase()
        const isImg = IMAGE_EXT.has(ext)
        const isFont = FONT_EXT.has(ext)
        if (!isImg && !isFont) return
        if (rule.category === 'font' && !isFont && !isImg) return
        if (rule.category !== 'font' && isFont) return

        // fonts rule: accept fonts; optional skip non-font images under font dirs
        if (rule.id === 'fonts' && !isFont) return

        const rel = norm(join(rootRel, relFromRoot))
        if (seen.has(rel)) return
        if (shouldExclude(rel)) return
        seen.add(rel)
        found.push({ abs: full, rel, category: rule.category })
      })
    }
  }

  return found
}

async function syncBrandingChrome() {
  mkdirSync(BRAND_DIR, { recursive: true })
  let n = 0
  for (const asset of BRAND_ASSETS) {
    const src = join(REPO_ROOT, asset.from)
    if (!existsSync(src)) {
      console.warn(`  brand missing: ${asset.from}`)
      continue
    }
    copyFileSync(src, join(BRAND_DIR, asset.to))
    n++
  }

  const iconSrc = join(BRAND_DIR, 'icon.png')
  if (existsSync(iconSrc)) {
    await sharp(iconSrc).resize(64, 64, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } }).png().toFile(join(PUBLIC_DIR, 'favicon.png'))
    await sharp(iconSrc).resize(180, 180, { fit: 'contain', background: { r: 0, g: 0, b: 0, alpha: 0 } }).png().toFile(join(BRAND_DIR, 'apple-touch-icon.png'))
    n += 2
  }
  console.log(`  branding chrome: ${n} files → public/branding/`)
}

async function main() {
  console.log('sync-catalog: scanning…')
  await syncBrandingChrome()
  const candidates = collectCandidates()
  console.log(`  found ${candidates.length} candidates`)

  // Clean and rebuild media (keeps public/ small in git)
  if (existsSync(MEDIA_DIR)) {
    for (let attempt = 0; attempt < 5; attempt++) {
      try {
        rmSync(MEDIA_DIR, { recursive: true, force: true })
        break
      } catch (err) {
        if (attempt === 4) throw err
        console.warn(`  media cleanup retry ${attempt + 1}: ${err.message}`)
        await new Promise((resolve) => setTimeout(resolve, 250 * (attempt + 1)))
      }
    }
  }
  mkdirSync(THUMBS_DIR, { recursive: true })

  /** @type {CatalogItem[]} */
  const items = []

  for (const c of candidates) {
    const ext = extname(c.abs).toLowerCase()
    const filename = basename(c.abs)
    const kind = FONT_EXT.has(ext) ? 'font' : ext === '.gltf' ? 'model' : 'image'
    const { category, act, layer, tags } = classify(filename, c.rel, c.category)
    const key = mediaKey(c.rel)
    const destAbs = join(MEDIA_DIR, key)
    mkdirSync(dirname(destAbs), { recursive: true })
    copyFileSync(c.abs, destAbs)

    /** @type {string|null} */
    let thumbSource = kind === 'image' ? c.abs : null

    if (kind === 'model') {
      const deps = gltfExternalDeps(c.abs)
      const mediaDir = dirname(destAbs)
      for (const depAbs of deps) {
        const depName = basename(depAbs)
        copyFileSync(depAbs, join(mediaDir, depName))
      }
      // Prefer sibling atlas / first external image as a browse poster.
      const siblingPng = join(dirname(c.abs), `${basename(c.abs, ext)}.png`)
      if (existsSync(siblingPng)) {
        thumbSource = siblingPng
      } else {
        const imgDep = deps.find((d) => IMAGE_EXT.has(extname(d).toLowerCase()))
        if (imgDep) thumbSource = imgDep
      }
    }

    let thumbPath = null
    if (thumbSource) {
      const hash = createHash('sha1').update(key).digest('hex').slice(0, 12)
      const thumbRelFile = `${hash}.webp`
      const thumbAbs = join(THUMBS_DIR, thumbRelFile)
      const made = await ensureThumb(thumbSource, thumbAbs)
      if (made) {
        thumbPath = `/media/thumbs/${thumbRelFile}`
      }
    }

    const id = createHash('sha1').update(key).digest('hex').slice(0, 16)
    const bytes = statSync(c.abs).size

    items.push({
      id,
      title: titleFromFilename(filename),
      filename,
      sourcePath: c.rel,
      mediaPath: `/media/${key}`,
      thumbPath,
      category,
      act,
      layer,
      tags,
      kind,
      bytes,
      notes: null,
    })
  }

  items.sort((a, b) => {
    if (a.category !== b.category) return a.category.localeCompare(b.category)
    if (a.act !== b.act) return a.act.localeCompare(b.act)
    return a.title.localeCompare(b.title)
  })

  const catalog = {
    generatedAt: new Date().toISOString(),
    itemCount: items.length,
    items,
  }

  writeFileSync(CATALOG_PATH, JSON.stringify(catalog, null, 2))
  console.log(`sync-catalog: wrote ${items.length} items → public/catalog.json`)
  console.log(`  media root: ${MEDIA_DIR}`)
}

main().catch((err) => {
  console.error(err)
  process.exit(1)
})
