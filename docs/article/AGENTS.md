# AGENTS.md

## Scope
- This file applies to everything under `docs/article/`.
- Follow the repository root `AGENTS.md` as well. This file adds article-local
  structure and sync rules.

## Purpose of this folder
- This folder contains publishable article material and article-specific
  visuals.
- The main current topic is the leaderless consensus article set.

## File structure
- `README.md`: index for the article folder.
- `leaderless-consensus.md`: canonical Markdown article.
- `leaderless-consensus.html`: single-file blog-post HTML version of the same
  article. This is intended for platforms that allow only one HTML file.
- `leaderless-consensus-short.md`: short professional summary derived from the
  main article.
- `leaderless-consensus-trace.html`: standalone set-based visualization source
  for `Sore`, `Calm`, `Flat`, and `Most`.
- `rush-prefix-visualizer.html`: standalone `Rush` visualization source.

## Sync rules
- `leaderless-consensus.md` and `leaderless-consensus.html` must stay aligned in
  structure, claims, terminology, and conclusions.
- If the main article changes materially in one format, update the other format
  in the same pass.
- `leaderless-consensus.html` must remain self-contained. Do not rely on
  external local files for visuals or article content.
- The visual walkthrough sections embedded in
  `leaderless-consensus.html` must stay aligned with the standalone sources:
  - `leaderless-consensus-trace.html`
  - `rush-prefix-visualizer.html`
- If a standalone visualization changes in a way that affects the embedded blog
  version, propagate the same change into `leaderless-consensus.html`.
- `leaderless-consensus.md` does not need to embed the full interactive HTML,
  but it must describe the same walkthroughs and outcomes at a content level.
- `leaderless-consensus-short.md` is a derivative summary. Keep it consistent
  with the main article, but do not force identical wording.
- `README.md` must be updated when files are added, removed, or renamed.

## Content rules
- Treat the Markdown article as the source for article text and structure unless
  the task is specifically about blog-only HTML formatting.
- Treat the HTML article as the source for final single-file publishability.
- Keep references appropriate for a blog post. Avoid GitHub-specific phrasing in
  the article body unless explicitly needed.
- Keep checked-in links portable and repo-relative where links are used in
  Markdown within the repository.

## Editing guidance
- Prefer small synchronized edits over letting the Markdown and HTML drift.
- When adding a new article-only section, decide explicitly whether it belongs:
  - in both `leaderless-consensus.md` and `leaderless-consensus.html`, or
  - only in the HTML for presentation reasons
- If the change is HTML-only presentation, keep the Markdown article semantically
  in sync by updating the corresponding explanatory section.
