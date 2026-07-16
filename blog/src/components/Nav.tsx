import { NavLink } from 'react-router-dom'

export function Nav() {
  return (
    <header className="site-header">
      <div className="site-header__inner">
        <NavLink to="/" className="brand">
          <span className="brand__mark">WC</span>
          <span className="brand__text">Wrathful Conquest</span>
        </NavLink>
        <nav className="nav" aria-label="Primary">
          <NavLink to="/posts" className={({ isActive }) => (isActive ? 'nav__link is-active' : 'nav__link')}>
            Posts
          </NavLink>
          <NavLink to="/about" className={({ isActive }) => (isActive ? 'nav__link is-active' : 'nav__link')}>
            About
          </NavLink>
        </nav>
      </div>
    </header>
  )
}
