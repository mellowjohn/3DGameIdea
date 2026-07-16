import { useState } from 'react'
import { absoluteUrl } from '../lib/seo'

type Props = {
  path: string
  title: string
}

export function ShareLinks({ path, title }: Props) {
  const [copied, setCopied] = useState(false)
  const url = absoluteUrl(path)
  const linkedIn = `https://www.linkedin.com/sharing/share-offsite/?url=${encodeURIComponent(url)}`

  async function copyLink() {
    try {
      await navigator.clipboard.writeText(url)
      setCopied(true)
      window.setTimeout(() => setCopied(false), 2000)
    } catch {
      setCopied(false)
    }
  }

  return (
    <div className="share-links">
      <span className="share-links__label">Share</span>
      <a className="share-links__btn" href={linkedIn} target="_blank" rel="noreferrer">
        LinkedIn
      </a>
      <button type="button" className="share-links__btn" onClick={copyLink} aria-label={`Copy link to ${title}`}>
        {copied ? 'Copied' : 'Copy link'}
      </button>
    </div>
  )
}
