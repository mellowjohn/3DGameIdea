# Private Art Collaboration Gallery (Art Atlas)

Password-gated React **Art Atlas** for collaborators: overview landing, browsable catalog, comments with likes/dislikes.

**Not** part of the public blog. Deployed on **Vercel Hobby** with free shared-password middleware (native Vercel Password Protection is paid).

## Local development

```bash
cd gallery
npm install
npm run sync    # copies media + thumbs into public/ (gitignored)
npm run dev     # http://localhost:5174
```

| Script | Purpose |
|---|---|
| `npm run sync` | Scan repo sources → `public/catalog.json` + `public/media/` |
| `npm run dev` | Vite + local comments API (`/.data/comments.json`) |
| `npm run build` | Sync then typecheck + Vite production build |
| `npm run preview` | Preview production build locally |

### Routes

| Path | Page |
|---|---|
| `/login` | Shared password gate |
| `/` | Overview landing (Art Atlas) |
| `/browse` | Filterable catalog |
| Asset lightbox | Full image/font + comments |

Local Vite has no password gate. Comments work via the Vite plugin. Test full auth with `npx vercel dev`.

## Password setup

1. Copy [`.env.example`](.env.example); never commit real secrets.
2. Vercel → **Settings → Environment Variables**:
   - **`SITE_PASSWORD`** (required)
   - **`SESSION_SECRET`** (optional)
   - **`UPSTASH_REDIS_REST_URL`** + **`UPSTASH_REDIS_REST_TOKEN`** for shared production comments (free [Upstash](https://upstash.com) Redis)

Without Upstash on Vercel, comments use the file store on ephemeral disks and will not stay shared. Use Upstash for real team feedback.

### Rotate the password

Set a new `SITE_PASSWORD` → redeploy. Old session cookies stop matching immediately.

## Comments and votes

- Open any asset → **Comments** in the lightbox.
- Name is remembered in `localStorage`; each browser has a stable `voterId` (one vote per comment).
- ▲ like / ▼ dislike (press again to clear).
- APIs: `GET/POST /api/comments`, `POST /api/comments/vote`.

## Catalog sources

Concepts, kits, reference, branding, curated cartography, UI concepts, fonts, item icons. See earlier plan / `scripts/sync-catalog.mjs` for include roots. Tile pyramids and meshes are excluded.

Media under `public/media/` is gitignored and regenerated on build.

## Deploy to Vercel

1. Import monorepo; **Root Directory** = `gallery`.
2. Build `npm run build`, output `dist`.
3. Env: `SITE_PASSWORD` + Upstash keys.
4. Optional GitHub Action: [`.github/workflows/deploy-gallery.yml`](../.github/workflows/deploy-gallery.yml).

## Security

Shared password is for collaborators, not multi-tenant security. Do not commit `.env`, passwords, `.data/`, or `public/media`.

## Stack

React 19 + Vite 8 + react-router-dom, sharp thumbs, Vercel Edge Middleware, Upstash Redis (optional) for comments.
