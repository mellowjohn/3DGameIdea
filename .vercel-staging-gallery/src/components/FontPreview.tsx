import { useEffect, useState } from 'react'

const loaded = new Map<string, string>()

type Props = {
  src: string
  familyId: string
  size?: 'card' | 'detail'
}

export function FontPreview({ src, familyId, size = 'card' }: Props) {
  const family = `gallery-font-${familyId}`
  const [ready, setReady] = useState(loaded.has(src))

  useEffect(() => {
    if (loaded.has(src)) {
      setReady(true)
      return
    }
    const face = new FontFace(family, `url(${src})`)
    let cancelled = false
    face
      .load()
      .then((loadedFace) => {
        document.fonts.add(loadedFace)
        loaded.set(src, family)
        if (!cancelled) setReady(true)
      })
      .catch(() => {
        if (!cancelled) setReady(false)
      })
    return () => {
      cancelled = true
    }
  }, [src, family])

  const style = ready ? { fontFamily: `"${family}", serif` } : undefined

  if (size === 'detail') {
    return (
      <div className="lightbox-font" style={style}>
        <div className="big">Wrathful Conquest</div>
        <div className="medium">ABCDEFGHIJKLMNOPQRSTUVWXYZ</div>
        <div className="medium">abcdefghijklmnopqrstuvwxyz 0123456789</div>
        <p className="sample">
          The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs.
        </p>
        {!ready && <p className="medium">Loading typeface…</p>}
      </div>
    )
  }

  return (
    <>
      <span className="specimen" style={style}>
        Aa
      </span>
      <span className="specimen-sub" style={style}>
        {ready ? 'Type sample' : 'Font'}
      </span>
    </>
  )
}
