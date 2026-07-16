# Wrathful Conquest Devlog

React + Vite static site for engine build notes, deployed to GitHub Pages.

## Local development

```bash
cd blog
npm install
npm run dev
```

## About page

Edit [`content/about.md`](content/about.md). The About route loads that file. Follow `.cursor/rules/blog-writing-voice.mdc` for tone.

## Authoring a post

1. Add `content/posts/your-slug.md` with frontmatter:

```yaml
---
title: Your title
date: 2026-07-20
summary: One or two sentences for the archive and share previews.
cover: /images/your-cover.webp
tags:
  - engine
draft: false
---

Markdown body here.
```

Optional `cover` paths are files under `public/` (served from the site root). Add new art to `public/images/`.

2. Push to `master`. The deploy workflow rebuilds when `blog/**` changes.

Draft posts (`draft: true`) are omitted from production builds.

## Email updates (optional)

The site can collect emails through [Formspree](https://formspree.io/). You keep the list there (or export it) and send new-post notes yourself from your own email.

Production form id lives in [`blog/.env.production`](.env.production) (`VITE_FORMSPREE_ID`). Local overrides use `.env.local`.

Until that id is set, the UI shows that email updates are coming soon.

## Deploy / GitHub Pages

1. Push this repository to GitHub (repo name `3DGameIdea` matches `vite.config.ts` `base: '/3DGameIdea/'`).
2. Enable **Settings → Pages → Build and deployment → GitHub Actions**.
3. Run the **Deploy blog** workflow (auto on `blog/**` pushes to `master`, or `workflow_dispatch`).
4. Site URL: `https://<user>.github.io/3DGameIdea/`

### Custom domain or different repo name

Set `base: '/'` (or your path) in `vite.config.ts` and update the Pages custom domain settings.

## LinkedIn

Each post page has **LinkedIn** and **Copy link** controls. Share the article URL. Site-level Open Graph tags live in `index.html`; the SPA also updates title/description in the browser for readers.

Note: LinkedIn’s crawler does not run JavaScript, so link previews typically show the site-level OG tags from `index.html`, not per-post meta.
