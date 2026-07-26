# Contributing to tezos-cyberdeck

Thanks for taking a look at this — genuinely glad you're interested.

The project's still early: the overall architecture and app scope are
worked out (you'll find the reasoning in `docs/build-notes/` and
`ROADMAP.md`), but very little code exists yet, which means there's a lot
of room to shape how this actually gets built. A few contributors are
likely to be working in parallel, some pairing with AI assistants, so this
doc is mostly here to help everyone build toward the same picture instead
of quietly arriving at three different versions of the same decision.

## Getting oriented

A quick read before diving in will save you some backtracking:
- `README.md` for the overall shape of the project
- `ROADMAP.md` for where things stand and what depends on what
- `docs/build-notes/` for the specific reasoning behind whatever
  app/subsystem you're interested in (e.g. `camera-app-build-notes.md` if
  you're thinking about the Camera app)
- `docs/architecture/` for the platform-shim and signer protocol specs,
  once those exist

If something in the build notes seems off to you, that's a great reason to
open an issue and talk it through — a lot of this was worked out in
conversation and is genuinely open to being revisited, we'd just rather
hash it out before code gets written around two different assumptions.

## Proposing a change

Standard flow: fork, branch, open a pull request. It helps a lot if the PR
description explains what changed and why, and how you tested it — future
you (and whoever reviews it) will thank you.

## A note on how apps get included

This project's going with a curated set of apps rather than an open app
store — not a knock on anyone's ideas, just a deliberate choice to keep
the device from getting bloated (more on the reasoning in `BOM.md`). In
practice that means:
- New apps are a good fit if they build on the shared Scanner forge/sign
  pipeline rather than a separate one-off — makes everything easier to
  reason about together.
- Anything touching the signer, key handling, or the forge/sign path gets
  a closer look before merging, simply because that's the part protecting
  people's funds — nothing personal, just the nature of that code.
- The project's settled on C as the core language, mostly so it matches
  the signer firmware and everyone's working in one language rather than
  several.

## A bit on tooling

Since a lot of this code will be parsing things from outside the device
(Michelson params, contract storage, marketplace responses), it's worth
building with `-fsanitize=address,undefined`, running `clang-tidy` or
`cppcheck`, and fuzzing any new parser (AFL or libFuzzer are both fine).
Good practice generally, and especially useful here.

## If you're pairing with an AI assistant

Totally welcome — a few of us expect to be doing this too. A couple of
things that make it easier for everyone:
- A quick note in the PR that an assistant helped is appreciated, just so
  reviewers know where to focus.
- It helps a lot to point your assistant at the relevant
  `docs/build-notes/` file before asking it to build something —
  otherwise it'll happily invent a plausible-looking API shape or pinout
  that doesn't match what someone else is building against.
- Same testing expectations apply either way — sanitizers/fuzzing aren't
  about whether a human or an assistant wrote the code, just about the
  code itself.
- Keeping PRs on the smaller side makes review easier for everyone, and
  makes it much easier to catch two people's assistants having converged
  on different solutions to the same problem.

## Where to talk things through

GitHub issues are the place for anything more than a small fix — happy to
add a Discord or point to Tezos Commons community calls here once those
are set up.

## Security issues

If you find something concerning, especially anything touching the
signer or signing flow, please don't post it as a public issue — reach
out privately instead (contact details coming soon).

## License

Code contributions fall under the project's MIT license (`LICENSE`);
hardware design contributions under CERN-OHL-P
(`LICENSE-HARDWARE.md`). Submitting a PR means you're fine with your
contribution being licensed that way.
