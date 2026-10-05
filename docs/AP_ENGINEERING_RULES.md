# EKE-DX-WIRE AP Engineering Rules

Status: NORMATIVE
Scope: All AP, AP-DIAG, AP-WIRE, AP-FIX, and corrective engineering changes.

## 1. AP definition

An AP is a controlled engineering change, not merely a code-writing task.

An AP is complete only when:
1. the intended change is implemented;
2. the repository builds from a clean Release configuration;
3. the complete Release CTest suite passes;
4. AP-specific validation passes;
5. expected artifacts and metrics are verified;
6. the final committed state is reproducible.

A failed acceptance item means the AP is incomplete.

## 2. Baseline rule

Before changing code, record:
- current commit SHA;
- current build/test status;
- relevant extraction metrics;
- relevant diagnostic/artifact state;
- the exact AP objective and acceptance criteria.

If the baseline is already broken, stop and record the pre-existing failure before making the AP change.

## 3. Repository inspection rule

Before implementation, inspect the current repository state relevant to the AP.

Do not generate a patch from memory of an earlier repository state.

At minimum inspect:
- declarations and definitions being changed;
- all relevant call sites;
- current tests;
- target/CMake definitions;
- current AP or diagnostic artifacts;
- current branch and working-tree state.

## 4. Bounded-change rule

One AP has one primary objective.

Do not combine unrelated:
- production changes;
- diagnostics;
- CMake/build changes;
- refactors;
- test rewrites.

If a supporting change is required, document why it is required for the AP.

## 5. Interface-first rule

Before changing a function, type, constructor, parameter list, return type, or public structure:
- inspect the declaration;
- inspect the definition;
- locate every call site;
- update all affected interfaces atomically;
- compile the affected target immediately.

No assumed or stale signatures.

## 6. Include/self-containment rule

Source files must explicitly include the headers required for the symbols they use.

Do not rely on accidental transitive includes, precompiled headers, or build-order side effects.

New code must compile from a clean build tree.

## 7. No speculative implementation rule

Do not introduce symbols, APIs, members, fields, files, or helper functions whose existence has not been verified against the current repository.

If an interface is uncertain, inspect it before writing the change.

## 8. Test preservation rule

Tests are production safeguards.

Do not delete, substantially replace, disable, weaken, or rewrite existing tests merely to make an AP pass.

Any intentional reduction in coverage requires explicit justification and replacement coverage.

## 9. Diagnostic separation rule

Diagnostic/replay tools are instrumentation.

They must not become accidental production dependencies.

Production behavior must not be changed solely to make a diagnostic executable easier to implement.

## 10. CMake rule

Every CMake change must have an identified dependency:
- target;
- source;
- include path;
- library;
- test;
- generated artifact; or
- build requirement.

New executables must build independently.

## 11. Clean-build rule

Incremental compilation is never sufficient for AP acceptance.

The mandatory acceptance build is:

    cmake -S . -B build
    cmake --build build --config Release --clean-first --parallel 1
    ctest --test-dir build -C Release --output-on-failure

Use:

    .\tools\dx-ap-gate.ps1

for committed-state acceptance.

## 12. Pre-commit validation rule

Before committing an AP, validate the working tree with:

    .\tools\dx-ap-preflight.ps1

The preflight performs a clean Release build and complete Release test suite against the proposed working-tree changes.

The commit is then created only after preflight passes.

After committing, run:

    .\tools\dx-ap-gate.ps1

This proves the committed state is identical in behavior to the accepted working state.

## 13. Warning rule

New compiler warnings attributable to an AP are defects unless explicitly reviewed and documented.

Unused variables, unreachable code, missing declarations, narrowing conversions, and similar warnings must not be ignored simply because compilation succeeds.

## 14. Validation hierarchy

Every AP must pass all applicable levels:

1. compile;
2. complete Release CTest;
3. AP-specific test;
4. AP-specific diagnostic/replay;
5. production extraction/replay;
6. artifact inspection;
7. metric comparison.

A lower level passing does not substitute for a required higher level.

## 15. Artifact rule

Successful process exit is not proof of correct output.

For generated artifacts verify:
- file exists;
- expected schema/content exists;
- provenance is correct;
- artifact corresponds to the tested source;
- no stale artifact was accidentally reused.

## 16. Metric regression rule

Compare relevant production metrics against the established baseline.

Unexpected changes must be explained.

Do not accept a metric change merely because tests pass.

## 17. Diff-review rule

Before commit, inspect the complete diff.

Verify:
- every changed file is necessary;
- every changed line supports the AP objective;
- no unrelated edits exist;
- no tests disappeared;
- no interfaces changed unintentionally;
- no generated files were accidentally committed.

## 18. No stacked failures rule

Do not begin a new AP on top of an unresolved AP failure.

If an AP fails, the next action is diagnosis and correction of that failure, not another feature/fix layered on top.

## 19. Rollback rule

If an AP introduces an unexplained build/test regression:
1. stop;
2. identify the first failing change;
3. preserve the diagnostic evidence;
4. revert to the last known-good checkpoint when practical;
5. correct the root cause;
6. rerun acceptance.

Do not accumulate speculative fixes on top of an unexplained failure.

## 20. Known-good checkpoint rule

Every accepted AP establishes a checkpoint:

    commit
      -> clean Release build
      -> complete CTest
      -> AP-specific validation
      -> artifact verification
      -> metric verification

That checkpoint is the next AP's baseline.

## 21. Stop-the-line rule

Any of the following stops the AP:
- compiler error;
- linker error;
- new unexplained warning;
- test failure;
- diagnostic failure;
- artifact mismatch;
- unexpected metric change;
- dirty working tree caused by validation;
- unexplained CMake behavior.

The AP remains incomplete until the condition is resolved.

## 22. AI implementation rule

ChatGPT, Claude, or any other agent must inspect the current repository before proposing implementation changes.

AI-generated code must be treated as untrusted until it passes the same engineering acceptance process as human-written code.

No agent may assume that an earlier conversation, prior commit, or remembered interface still matches HEAD.

## 23. Commit rule

An AP commit is a validated engineering checkpoint.

Required sequence:

    inspect baseline
    -> implement bounded change
    -> preflight
    -> review diff
    -> commit
    -> AP gate
    -> record checkpoint

Do not declare an AP complete before the final gate passes.

## 24. Required AP record

Each AP should record:
- AP identifier;
- objective;
- baseline commit;
- files changed;
- interfaces changed;
- tests added/modified;
- validation commands;
- build result;
- CTest result;
- diagnostic result;
- artifact result;
- metric result;
- final commit;
- final gate result.

## 25. Authority

When an AP conflicts with convenience, schedule, or an earlier implementation assumption, these rules take precedence.

The objective is not to maximize the number of APs completed. The objective is to produce a sequence of small, reproducible, known-good engineering checkpoints.
