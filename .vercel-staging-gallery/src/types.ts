export type Category =
  | 'concept'
  | 'kit'
  | 'reference'
  | 'ui'
  | 'cartography'
  | 'font'
  | 'icon'
  | 'branding'

export type Act = 'act0' | 'act1' | 'act2' | 'act3' | 'kits' | 'none'

export type Layer =
  | 'ld'
  | 'scene'
  | 'char'
  | 'kit'
  | 'legacy'
  | 'menu'
  | 'hud'
  | 'dialogue'
  | 'quest'
  | 'font'
  | 'map'
  | 'icon'
  | 'other'

export type CatalogItem = {
  id: string
  title: string
  filename: string
  sourcePath: string
  mediaPath: string
  thumbPath: string | null
  category: Category
  act: Act
  layer: Layer
  tags: string[]
  kind: 'image' | 'font'
  bytes: number
  notes: string | null
}

export type Catalog = {
  generatedAt: string
  itemCount: number
  items: CatalogItem[]
}

export type GalleryFilters = {
  search: string
  category: Category | 'all'
  act: Act | 'all'
  layer: Layer | 'all'
}
