import { Link } from 'react-router-dom'
import { BrandMark } from '../components/BrandMark'

const pieces = [
  {
    src: '/storyboards/act0-cine-calrenoth-pan-storyboard.png',
    title: 'Six-beat storyboard',
    label: 'Sequence overview',
    description:
      'High wide (Landfall title) → siege lines → approach road → front gate → wheelbarrow settle (no Arkand reveal) → player control.',
  },
  {
    src: '/storyboards/act0-cine-pan-camera-path-diagram.png',
    title: 'Camera-path plan',
    label: 'Movement proposal',
    description:
      'Continuous-feeling descending sweep with concealed blends allowed; focus stays on the approach corridor and front gate.',
  },
  {
    src: '/storyboards/act0-cine-pan-frame-a-peninsula-wide.png',
    title: 'Opening hero frame',
    label: 'Landfall scale',
    description:
      'The western peninsula, Calrenoth under fire, and the landward reinforcement road — where the Landfall title appears.',
  },
  {
    src: '/storyboards/act0-cine-pan-frame-b-wheelbarrow-settle.png',
    title: 'Handoff hero frame',
    label: 'Arkand setup',
    description:
      'The pan settles on the overturned wheelbarrow only; the player discovers Arkand after gaining control.',
  },
]

const shotBeats = [
  ['0–5s+', 'High wide', 'Establish the western peninsula, ocean, smoke, and Calrenoth’s silhouette. Show the Landfall title here.'],
  ['Mid', 'Siege lines', 'Descend over Imperium camps, engines, and fire arcs striking the fortress.'],
  ['Mid', 'Road drift', 'Follow the landlocked approach through conifers, wreckage, ash, and barricades.'],
  ['Mid–late', 'Front gate', 'Frame the gatehouse and approach walls — keep moat / rear drawbridge out of the required read.'],
  ['Late', 'Settle', 'Find the wheelbarrow staging and stop short of any armored-hand / Arkand reveal.'],
  ['End', 'Player control', 'Hand off control; player discovers Arkand under the barrow and the rescue begins.'],
]

const decisions = [
  {
    title: 'How visible is Arkand?',
    answer: 'Stop short — the player discovers Arkand after gaining control (no armored-hand reveal in the final cine frame).',
  },
  {
    title: 'One move or hidden cuts?',
    answer: 'Concealed blends allowed — continuous-feeling, not a literal uninterrupted single move.',
  },
  {
    title: 'What geography must read?',
    answer: 'Stay focused on the approach road and front gate; moat / rear drawbridge are not required in this opening.',
  },
  {
    title: 'How long should it run?',
    answer: 'At least 30 seconds, skippable; fine-tune timing during camera authoring.',
  },
  {
    title: 'When does “Landfall” appear?',
    answer: 'During the aerial wide shot (not only on settle or as a post-control quest banner).',
  },
  {
    title: 'What carries the audio?',
    answer:
      'Backing track plus ambient fighting and impacts (horns / fire / wind as fits). No new narration after Frangitur; no military callout required.',
  },
]

export function StoryboardsPage() {
  return (
    <div className="storyboard-page">
      <header className="storyboard-header">
        <Link to="/" className="storyboard-brand">
          <BrandMark size="sm" to={null} />
          <span>Art Atlas</span>
        </Link>
        <nav>
          <Link to="/">Overview</Link>
          <Link to="/browse">Browse</Link>
        </nav>
      </header>

      <main>
        <section className="storyboard-hero">
          <p className="storyboard-kicker">Act 0 · A0-02 → A0-03 · Locked (D-P0-17e)</p>
          <h1>Landfall: Calrenoth establishing pan</h1>
          <p>
            A WoW-style continuous-feeling introduction after character creation. The camera
            establishes the siege corridor, follows the approach road to the front gate, and hands
            control to the player at the wheelbarrow — Arkand is discovered in play.
          </p>
          <div className="storyboard-status">
            <span>Direction locked: long-take pan</span>
            <span>Quest title locked: Landfall</span>
            <span>Camera locks: D-P0-17e</span>
            <span>≥30s · skippable</span>
          </div>
        </section>

        <section className="storyboard-section" aria-labelledby="pieces-title">
          <div className="storyboard-section-head">
            <div>
              <p className="storyboard-eyebrow">Visual review</p>
              <h2 id="pieces-title">Storyboard pieces</h2>
            </div>
            <p>Click any piece to open the full-resolution image.</p>
          </div>
          <div className="storyboard-piece-grid">
            {pieces.map((piece) => (
              <article className="storyboard-piece" key={piece.src}>
                <a href={piece.src} target="_blank" rel="noreferrer">
                  <img src={piece.src} alt={piece.title} />
                </a>
                <div>
                  <p className="storyboard-piece-label">{piece.label}</p>
                  <h3>{piece.title}</h3>
                  <p>{piece.description}</p>
                </div>
              </article>
            ))}
          </div>
        </section>

        <section className="storyboard-section" aria-labelledby="sequence-title">
          <div className="storyboard-section-head">
            <div>
              <p className="storyboard-eyebrow">Timing proposal</p>
              <h2 id="sequence-title">Continuous-feeling move (≥30s)</h2>
            </div>
            <p>Skippable; concealed blends OK; backing track + siege ambients; no new dialogue.</p>
          </div>
          <ol className="storyboard-timeline">
            {shotBeats.map(([time, title, copy], index) => (
              <li key={title}>
                <span className="storyboard-step">{String(index + 1).padStart(2, '0')}</span>
                <span className="storyboard-time">{time}</span>
                <div>
                  <h3>{title}</h3>
                  <p>{copy}</p>
                </div>
              </li>
            ))}
          </ol>
        </section>

        <section className="storyboard-section storyboard-questions" aria-labelledby="decisions-title">
          <div className="storyboard-section-head">
            <div>
              <p className="storyboard-eyebrow">Camera locks</p>
              <h2 id="decisions-title">D-P0-17e answers</h2>
            </div>
            <p>Owner-locked 2026-08-05 — ready for camera authoring.</p>
          </div>
          <div className="storyboard-question-grid">
            {decisions.map((decision, index) => (
              <article key={decision.title}>
                <span>{String(index + 1).padStart(2, '0')}</span>
                <h3>{decision.title}</h3>
                <p>{decision.answer}</p>
              </article>
            ))}
          </div>
        </section>
      </main>

      <footer className="storyboard-footer">
        <span>Locked camera brief · still open to timing polish within D-P0-17e</span>
        <Link to="/browse?category=concept">View all Act 0 concepts</Link>
      </footer>
    </div>
  )
}
