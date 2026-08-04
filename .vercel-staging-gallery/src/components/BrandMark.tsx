import { Link } from 'react-router-dom'
import { BRAND } from '../branding'

type Size = 'sm' | 'md' | 'lg'

type Props = {
  size?: Size
  to?: string | null
  className?: string
}

const PX: Record<Size, number> = { sm: 36, md: 48, lg: 72 }

/** Circular Wrathful Conquest emblem. */
export function BrandMark({ size = 'md', to = '/', className = '' }: Props) {
  const px = PX[size]
  const img = (
    <img
      src={BRAND.icon}
      alt="Wrathful Conquest"
      width={px}
      height={px}
      className={`brand-mark ${className}`.trim()}
      decoding="async"
    />
  )
  if (to === null) return img
  return (
    <Link to={to} className="brand-mark-link" aria-label="Wrathful Conquest home">
      {img}
    </Link>
  )
}

type TitleProps = {
  className?: string
  alt?: string
}

/** Full title lockup for hero / login. */
export function BrandTitleLogo({ className = '', alt = 'Wrathful Conquest' }: TitleProps) {
  return (
    <img
      src={BRAND.titleLogo}
      alt={alt}
      className={`brand-title-logo ${className}`.trim()}
      decoding="async"
    />
  )
}
