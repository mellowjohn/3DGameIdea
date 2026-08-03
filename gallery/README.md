# Private Art Collaboration Gallery (Art Atlas)

Password-gated React **Art Atlas** for collaborators: overview landing, browsable catalog, comments with likes/dislikes.

**Not** part of the public [blog/](../blog/) (GitHub Pages). Live production URL (when deployed):

**https://wc-art-atlas.vercel.app**

Stack: React 19 + Vite 8 + react-router-dom, sharp (thumbs), Vercel Edge Middleware (shared password gate), Upstash Redis REST (durable comments).

---

## Local development

```bash
cd gallery
npm install
npm run sync    # catalog + media + branding chrome into public/
npm run dev     # http://localhost:5174
```

| Script | Purpose |
|---|---|
| `npm run sync` | Scan monorepo art/design roots → `public/catalog.json` + `public/media/` + branding |
| `npm run dev` | Vite + local comments API (`gallery/.data/comments.json`) |
| `npm run build` | Sync then `tsc` + Vite production build |
| `npm run preview` | Serve `dist/` only (no password gate, no serverless API) |

### Routes

| Path | Page |
|---|---|
| `/login` | Shared password form |
| `/` | Overview landing |
| `/browse` | Filterable catalog |
| Asset lightbox | Full image/font + comments / votes |

Local `vite` does **not** run Edge Middleware or `/api/login`. Password gate and production comments require Vercel (or `npx vercel dev` with env vars).

---

## Environment variables

Never commit real values. Use [`gallery/.env`](.env) locally (gitignored) and **Vercel → Project → Settings → Environment Variables** for Production / Preview / Development.

| Variable | Required | Purpose |
|---|---|---|
| `SITE_PASSWORD` | Yes (prod) | Shared collaborator password. Checked only server-side (middleware + `/api/login`). |
| `SESSION_SECRET` | No | Cookie HMAC secret; falls back to `SITE_PASSWORD` if empty. |
| `UPSTASH_REDIS_REST_URL` | Yes (prod comments) | HTTPS endpoint only (one URL). |
| `UPSTASH_REDIS_REST_TOKEN` | Yes (prod comments) | REST token for that database. |

### Password rotation

1. Update `SITE_PASSWORD` on Vercel (all environments you use).
2. Redeploy production.
3. Tell collaborators the new password. Existing session cookies stop working immediately (token is derived from the password).

### Upstash (durable comments)

Without Redis, serverless falls back to `/tmp` (writable but ephemeral and not shared across instances).

1. Create a Redis DB (free): [Upstash console](https://console.upstash.com) **or** claim an agent-provisioned DB via its claim URL.
2. Copy **REST URL** and **Token** into Vercel env (and local `.env` for `vercel dev`).
3. Redeploy so functions pick up the values.
4. After post, comments API responses include `"backend":"upstash"`.

**Pitfalls**

- `UPSTASH_REDIS_REST_URL` must be a **single** `https://….upstash.io` value. Two URLs smashed together break writes (`READONLY` / backup replica errors).
- Agent “start-redis” databases **expire in ~3 days** until claimed into a real Upstash account.

---

## Catalog sources

Synced on every `npm run build` / deploy build from monorepo paths (relative to repo root, not `gallery/` alone):

| Category | Roots (approx.) |
|---|---|
| Concepts | `context/art/concepts/` |
| Kits / palette | `context/art/tier*.png`, oak/palette root sheets |
| Reference | `context/art/reference/` |
| Branding chrome | `context/art/branding/`, title logos, editor app icons → also copied to `public/branding/` |
| Cartography (curated) | `context/art/cartography/` (**excluding** `world-map-tiles/`) + `context/story/official-world-map.png` |
| UI concepts | `context/design/{menu,hud,dialogue,quest}-assets/` (+ quest icons) |
| Fonts | `assets/ui/fonts/`, `assets/editor/fonts/` |
| Item icons | `samples/open-world-rpg/assets/ui/icons/` |

`public/media/`, `public/catalog.json`, and `public/branding/` are gitignored and regenerated at build time.

---

## Production architecture

```text
Browser
  → Edge Middleware (SITE_PASSWORD cookie)
  → static SPA (dist/) + /media + /catalog.json
  → /api/login | /api/logout     (Edge)
  → /api/comments | …/vote       (Node) → Upstash Redis REST
```

- Project: **`wc-art-atlas`** on team **hydrairius-projects**
- Production alias: **https://wc-art-atlas.vercel.app**
- **Root Directory** on the Vercel project: **`gallery`**
  - So `middleware.js`, `api/*`, and `npm run build` run inside `gallery/`
  - Parent monorepo folders (`context/`, `assets/`, …) must be **present in the upload** so `scripts/sync-catalog.mjs` can resolve `../context/art`, etc.

---

## Deployment (recommended CLI path)

Hobby teams cannot invite extra members; **GitHub auto-deploy can block** if the commit author email is not a seat on the Vercel team (`mellowjohnrossi@gmail.com` style blocks). Prefer CLI deploy from a **staging tree without `.git`**, or fix git `user.email` to the email already on the Vercel owner account.

### One-time project setup

1. Log in: `npx vercel login`
2. Create/link project (name must be **lowercase** slug, e.g. `wc-art-atlas` — not `Wrathful Conquest Asset Lib`).
3. Set **Root Directory** = `gallery` (Vercel dashboard or API `rootDirectory`).
4. Framework **Vite**, install `npm install`, build `npm run build`, output `dist`.
5. Set env vars above for Production (and Preview if used).

### Full-catalog production deploy

From the monorepo machine (PowerShell sketch used in practice):

1. **Stage** a deploy folder under `%TEMP%` (no `.git`):
   - `gallery/` (source; exclude `node_modules`, `dist`, `.data`, `public/media`)
   - `context/art` (exclude `world-map-tiles`, `__pycache__`)
   - `context/design` (optional: exclude `*.pen`)
   - `context/story/official-world-map.png`
   - `assets/ui/fonts`, `assets/editor`
   - `samples/open-world-rpg/assets/ui/icons`
   - `samples/open-world-rpg/assets/models` (runtime glTF + sibling textures)
   - Root [`.vercelignore`](../.vercelignore) must use **repo-root anchors** (`/src`, `/tools`, …) so `gallery/src` is not excluded
2. At staging root, set `VERCEL_ORG_ID` / `VERCEL_PROJECT_ID` (link via `.vercel/project.json`) matching `wc-art-atlas`.
3. Ensure project **Root Directory** remains `gallery`.
4. Deploy with runtime envs injected (so password + Redis are present even if project env lags):

```powershell
cd $env:TEMP\wc-art-atlas-full   # your staging path
$env:VERCEL_ORG_ID = "<team_id>"
$env:VERCEL_PROJECT_ID = "<project_id>"
npx vercel deploy --yes --prod `
  -e "SITE_PASSWORD=$SITE_PASSWORD" `
  -e "UPSTASH_REDIS_REST_URL=$UPSTASH_REDIS_REST_URL" `
  -e "UPSTASH_REDIS_REST_TOKEN=$UPSTASH_REDIS_REST_TOKEN" `
  --force
```

5. Confirm:
   - Login with correct password → 200 + `wc_gallery_session` cookie
   - Wrong password → 401
   - `catalog.json` (authed) → `itemCount` ~279+ and non-empty categories
   - `POST /api/comments` → `"backend":"upstash"` and 201

### Deploy without art (what went wrong once)

Deploying **only** the `gallery/` package (no monorepo parents) produces an **empty catalog** (`found 0 candidates` in build logs) while login/API still work. Always include art source trees or the catalog will be empty in production.

### Optional: GitHub Actions

[`.github/workflows/deploy-gallery.yml`](../.github/workflows/deploy-gallery.yml) expects secrets:

| Secret | Purpose |
|---|---|
| `VERCEL_TOKEN` | Deploy token |
| `VERCEL_ORG_ID` | Team id |
| `VERCEL_PROJECT_ID` | Project id |
| `SITE_PASSWORD` | Build/runtime password (also set on Vercel project) |

Git-triggered builds also need monorepo content reachable (full checkout + Root Directory `gallery`) and a non-blocked commit author. Prefer Actions **only after** git author / team email issues are resolved.

### Repo files that affect deploy

| Path | Role |
|---|---|
| [`gallery/vercel.json`](vercel.json) | Build/output + SPA rewrites |
| [`gallery/middleware.js`](middleware.js) | Password cookie gate |
| [`gallery/api/*`](api/) | Login, logout, comments, votes |
| [`gallery/lib/comment-store.mjs`](lib/comment-store.mjs) | Upstash or file/`/tmp` fallback |
| [`gallery/scripts/sync-catalog.mjs`](scripts/sync-catalog.mjs) | Catalog + thumbs + branding chrome |
| [`.vercelignore`](../.vercelignore) | Keep monorepo uploads lean when deploying from repo root |
| [`gallery/.vercelignore`](.vercelignore) | Ignore local media/node_modules when root is gallery-only |

---

## Auth behavior (production)

| Path | Unauthenticated |
|---|---|
| `/login`, `/api/login`, `/api/logout`, `/assets/*`, favicons | Public |
| `/`, `/browse`, HTML app routes | Redirect → `/login` |
| `/media/*`, `/catalog.json`, other `/api/*` | **401** JSON |

Session cookie: `wc_gallery_session` (HttpOnly, 30 days, HMAC over password).

---

## Comments and votes

- Lightbox → **Comments**: name + text; ▲ / ▼ votes (toggle off).
- Name and a stable `voterId` live in `localStorage`.
- Durable shared storage: **Upstash**. Local dev: `gallery/.data/comments.json`. Serverless without Redis: `/tmp` only (not durable).

---

## Filters

Search · Category · Act · Type/layer. Fonts use live type specimens; images use WebP thumbs in the grid and full originals in the lightbox (when full media was baked into the deploy).

---

## Security notes

- Shared password is collaborator-grade, not multi-tenant auth.
- Never commit `.env`, real passwords, Upstash tokens, `.data/`, or generated `public/media`.
- Anyone with the password can download media for 30 days while the session cookie lasts.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `SITE_PASSWORD not configured` | Deploy predates env, or env missing | Set env → **redeploy** |
| Empty gallery / 0 assets | Deploy lacked monorepo art paths | Full-catalog staging deploy (include `context/art`, etc.) |
| `ENOENT … mkdir …/gallery/.data` | File comments path on read-only `/var/task` | Upstash (preferred) or `/tmp` fallback in current code |
| `READONLY Writes are not allowed on backup replica` | Bad/corrupt Upstash URL or read-only replica | Single valid REST URL + write-capable token; claim/recreate DB |
| Vercel: git author email blocked | Hobby seat cannot add that Git email | Match `git user.email` to owner Vercel email, **or** CLI deploy without team invite |
| Project name 400 | Spaces/capitals in project name | Lowercase slug (`wc-art-atlas`) |
| Login works only locally | Hitting `vite` not Vercel | Use production URL or `vercel dev` |

---

## Quick checklist (ship a content update)

1. Art/design assets committed under catalog roots.
2. Local smoke: `cd gallery && npm run sync && npm run dev`.
3. Staging package with monorepo parents (no `.git` if author blocks).
4. Env ok: `SITE_PASSWORD` + Upstash URL/token (one URL each).
5. `vercel deploy --prod` with envs as above.
6. Open https://wc-art-atlas.vercel.app → login → Browse → open asset → comment.
