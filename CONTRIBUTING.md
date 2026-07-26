# Contributing to tezos-cyberdeck

Thanks for considering contributing. This project is early — architecture
and app scope are decided (see `docs/build-notes/` and `ROADMAP.md`), but
almost no code exists yet. That makes this document more important than
usual: several people, some working with AI coding assistants, are likely
to be building in parallel, and the fastest way for that to go wrong is
everyone quietly inventing their own version of a decision that's already
been made.

## Before writing any code

Read, in this order:
1. `README.md` — project shape and directory structure
2. `ROADMAP.md` — what milestone you're actually contributing toward, and
   what depends on what
3. `docs/build-notes/` — the specific decisions already made for the
   app/subsystem you're touching (e.g. `camera-app-build-notes.md` before
   touching the Camera app)
4. `docs/architecture/` — platform-shim interface and signer protocol
   spec, once those exist

**If a build-notes doc already answers your question, build against that
answer.** If you think it's wrong, open an issue and discuss it before
writing code against a different assumption — a PR that silently
contradicts an existing decision creates real integration pain for
whoever's touching that code next.

## How to propose a change

Fork, branch, open a pull request. A good PR description says what
changed, why, and how it was tested — not just a diff with no context.

## What actually gets merged

This project deliberately uses a **curated repo, not an open app store**
(see `BOM.md`). That means:
- New apps should follow the app manifest spec (see `docs/architecture/`
  once published) and use the shared Scanner forge/sign pipeline rather
  than inventing a parallel one.
- Anything touching the signer, key handling, or the forge/sign path gets
  extra scrutiny and needs a maintainer sign-off regardless of how
  confident the PR looks — this is a hardware wallet, not a hobby toy, and
  that boundary doesn't get relaxed for convenience.
- Core language is **C**, matching the signer firmware — one language
  across the whole project, not per-contributor preference.

## Tooling expectations

Given C's memory-safety gap matters most exactly where this project parses
untrusted external data (Michelson params, contract storage, marketplace
API responses, incoming operation requests):
- Build with `-fsanitize=address,undefined` in dev/CI
- Run `clang-tidy` / `cppcheck` as a matter of course
- Fuzz any new parser touching untrusted input (AFL or libFuzzer)

This applies regardless of how the code was written — see below.

## Contributing with AI coding assistants

Several contributors are expected to use AI assistants for parts of this
build. That's fine, with a few rules to keep parallel work coherent:

- **Disclose it in the PR.** A short note ("drafted with an assistant,
  reviewed and tested by me") is enough — this isn't about gatekeeping,
  it's about calibrating review attention.
- **Point your assistant at the actual project docs, not its own
  assumptions.** Feed it the relevant `docs/build-notes/` file and
  `docs/architecture/` spec before asking it to implement something —
  otherwise you'll get a plausible-looking implementation that quietly
  invents its own API shape, pinout, or protocol detail that conflicts
  with what another contributor is building against.
- **No exemption from the tooling bar above.** Sanitizers, static
  analysis, and fuzzing apply the same whether a human or an assistant
  wrote the code — "the assistant wrote it" is not a substitute for
  actually testing it.
- **Signer/crypto/forge-sign code gets human review no matter what wrote
  it**, given what's at stake if it's wrong.
- **Keep PRs scoped small.** Easier for a human reviewer to actually
  verify, and easier to catch two contributors' assistants having
  independently reinvented the same thing two different ways.

## Where to discuss before writing code

Open a GitHub issue for anything bigger than a small fix or bug. (Community
chat/call details — Tezos Commons community calls, project Discord if one
exists — to be added here once set up.)

## Reporting a security issue

Do not open a public GitHub issue for a security vulnerability, especially
anything touching the signer, key handling, or signing flow. (Private
disclosure contact to be added here once established.)

## License

Code contributions are under the project's MIT license (`LICENSE`).
Hardware design contributions are under CERN-OHL-P (`LICENSE-HARDWARE.md`).
By submitting a PR, you agree your contribution is made under these terms.
