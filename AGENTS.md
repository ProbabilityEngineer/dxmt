AI must not be used to generate code for contributions to this project.

Private derivative exception: AI-generated code is permitted on the local
`private/elite-metalfx4` branch for the owner's private derivative only. This
exception does not apply to upstream contributions or other branches.
AI-generated changes must remain separate from upstream contribution work.

This work is a private derivative. Do not submit it upstream.

# Scope, verification, and cost boundaries

Treat the requested outcome as the scope boundary. Complete the work without
asking permission for each ordinary implementation or verification step.

## Included by default

When I authorize implementation, you may:
- Inspect relevant code, documentation, configuration, and local evidence.
- Make the changes necessary for the requested outcome.
- Add or update focused regression tests for the changed behavior.
- Run relevant local tests, type checks, linters, and incremental builds
  using existing local dependencies and toolchains.
- Reproduce a reported failure and verify that the change addresses it.

These permissions do not authorize unrelated refactoring, cleanup, new
features, or broad test-suite expansion.

## Keep verification proportional

Use the smallest set of checks that directly establishes the requested
behavior and guards likely regressions. Prefer existing test infrastructure.

A focused regression test and a relevant build/check are normally enough.
Do not recursively add tests for test helpers, validators for validators,
or additional planning and review layers merely to increase confidence.

If a check exposes a defect in the requested change, fix it and rerun the
relevant check. If it exposes an unrelated problem, report it rather than
expanding scope.

Stop when the requested work is complete and the relevant checks pass.
If checks cannot run, report the specific blocker and remaining uncertainty;
do not invent substitute validation layers or claim success.

## Ask before expanding

Obtain approval before:
- Paid compute, remote CI, or other externally metered operations.
- Downloading or installing dependencies, or changing system/toolchain settings.
- Installing or deploying artifacts into applications, games, or live environments.
- Committing, pushing, publication, or destructive operations.
- Substantial unrelated work or unusually expensive local validation.

Before an operation that will trigger remote CI or publication, disclose the
expected downstream actions and obtain approval covering them. Approval for
the initiating operation alone does not authorize undisclosed downstream work.

Ordinary local tests and incremental builds are preauthorized; they do not
require a separate cost-approval exchange.

If project rules require verification substantially beyond these boundaries,
explain the conflict once and ask a concrete question. Do not repeatedly ask
for permission already granted.

## Reporting

Briefly report what changed, which checks ran and their results, and any
remaining limitations. Distinguish verified behavior from hypotheses.
Do not create extra plans, reports, manifests, or evidence archives unless
requested or necessary for the task.

This restriction concerns extra process documentation, not artifacts required
to implement or operate the requested behavior, such as lossless spill files,
generated assets, or normal build and test output.
