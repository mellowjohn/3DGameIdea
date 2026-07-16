# Engine Interview Question Bank

Use only the sections relevant to the current milestone. Ask one blocking question at a time.

## Product

- What game or technical demo must the first usable milestone create?
- Is this a reusable general engine or an engine optimized for one game?
- Who is the first user: programmer, technical artist, designer, or a mixed team?
- What does “works well with AI tools” mean: inspectable text assets, agent APIs, runtime AI, editor automation, or all of these?

## Platform and Toolchain

- Which desktop operating systems must development and shipped games support?
- Which language and compiler constraints are non-negotiable?
- Which graphics backends are required now, and which may be added later?
- Are third-party libraries permitted, and what does “from scratch” exclude?

## Runtime Architecture

- Which scene/world model is preferred: ECS, scene graph, or hybrid?
- Must gameplay support hot reload or scripting? If so, which language?
- What determinism, threading, and simulation-rate guarantees are required?
- Which asset formats and import pipeline are required for the first milestone?

## Editor and AI Integration

- Is a graphical editor required for the MVP, or are CLI and text workflows sufficient?
- Which engine operations must agents invoke programmatically?
- Which project state must remain human-readable and diffable?
- What permissions and confirmation boundaries apply to autonomous agents?

## Quality and Delivery

- What performance target, representative scene, hardware, and resolution define success?
- What test levels and supported hardware form the release gate?
- What licensing and redistribution constraints apply?
- What is explicitly outside the first milestone?
