# CLAUDE.md

**Model:** Opus 5.5, extra-high effort.

How Claude Code works in this repository. [README.md](README.md) describes the project; this file has the rules and the workflow.

## Git

- **Every new feature is built on a `feature/` branch.** The engineer picks the name; there's no naming standard yet (no ticket numbers, for example).
  - Claude doesn't create or name branches. Before implementing, it checks the current branch (`git branch --show-current`). On `main`, it stops and asks for a feature branch.
- **Commits, merges, tags and pushes are the engineer's.** Claude never stages, commits, amends, merges, tags or pushes. Read-only commands (`status`, `diff`, `log`) are fine.
  - If a file needs a special staging command (e.g. `git add --chmod=+x`), Claude gives the command.
- **Commit messages.** At the end of each phase, Claude hands over:
  - the **title**, 80 characters at most, in its own code block;
  - the **description**, in a separate code block: **succinct**, with the core idea in one sentence, then short bullet points. No hard line wraps, but no long lines either: being allowed long lines is no reason to write them;
  - no `Co-Authored-By` line.

## Documents

**Every document in `claude/`, and this file, states which model wrote it, and at what effort**, on a `**Model:**` line right under its title. That includes the research, spec, plan, pre-implementation review, implementation report and session summaries. READMEs and other docs don't have the line.
- Example: `**Model:** Opus 5.5, extra-high effort.`
- When a different model or effort changes the document later, the line adds it: `**Model:** Opus 5.5, extra-high effort; updated 2026-12-01 with Fable 5.2, high effort.`
- A document with parts written by different models names each part. For example, the pre-implementation review names the agent's model, and the model that checked its findings.

## Workflow

Every piece of work, whether a new version, a feature, or a fix from a ticket, goes through seven steps in order:

**research → spec → plan → pre-implementation review → implementation → validation → review**

Its documents go in `claude/<date>_<n>-<topic>-<version>/`, e.g. `claude/2026-11-24_1-windows-1.1.0/`.

### Models

| Step | Model and effort |
|---|---|
| Research, spec, plan | **Fable, high**, preferred. If the session isn't on Fable, Claude asks the engineer to switch to it, when it's available to them. Otherwise **Opus, extra high: mandatory**. |
| Pre-implementation review | An agent on **Sonnet, extra high** if Opus wrote the research, spec and plan; on **Opus, extra high** if Fable did. |
| Implementation and validation | **Opus, extra high: mandatory.** |
| Review | Agents on **Fable, high**; on **Sonnet, extra high** if Fable isn't available. |

- **Mandatory:** before the step starts, Claude checks its own configuration. It refuses to start the step in any other configuration, unless the engineer explicitly says to override.
- Claude knows its model from its environment, and its effort from `modelSettings` in Claude Code's settings (`~/.claude/settings.json`). If it can't tell, it asks the engineer.
- Agents are spawned with the Agent tool's `model` (`"fable"`, `"opus"`, `"sonnet"`) and `effort` (`"high"`, `"xhigh"`).
- The pre-implementation review picks its model from the `**Model:**` line of the research, spec and plan (see Documents).

### 1. Research: `research.md`

The problem, and what was found about it:
- probes and throwaway prototypes, kept outside the repo;
- what was verified on the real machine;
- the options weighed, each with why it was chosen or rejected.

**Open questions come first**, each marked:
- **Blocker:** answered before the spec is written;
- **Minor blocker:** asked when the phase that needs it comes up, with options and a recommendation.

Once answered, they become the **Decisions** table, and all the documents are reconciled with them, so nothing contradicts a decision. A decision taken later, during a phase, is added to the table with the phase noted.

### 2. Spec: `spec.md`

What the work does (its behavior) and its design: modules, functions, files and build. It also lists what's out of scope. Claude's own calls are marked "(my call)", for the engineer's review.

### 3. Plan: `plan.md`

How the work gets built:
- **Phases**, each producing **exactly one commit**. Each phase lists its changes, Claude's checks, and the steps that need the engineer's hands, such as hardware or Windows settings.
- A **release checklist**, when the work ends in a release.

**Phase 0** exists only for real setup, such as a repository or a toolchain. Writing the research, spec and plan is never a phase: the engineer reviews and commits those documents first.

### 4. Pre-implementation review: `pre-implementation-review.md`

It starts automatically when the engineer asks to implement the plan. It runs before any code is changed and before Phase 1, and Claude waits for its report.

1. **Claude spawns one agent**, on the model the table above gives. The agent checks the claims of the research, spec and plan against the code:
   - names of files, functions and types;
   - how the current code behaves;
   - what the design says it will change;
   - whether the phases can be built as written.
2. **The agent writes the report** in the documents' folder, with its own model and effort on the `**Model:**` line. Each finding has a severity: **Critical**, **Major**, **Minor** or **Nit**.
3. **When the agent is done**, Claude adds to the end of the report the tokens the agent consumed, and the model and effort it ran on, from the agent's completion notice.
4. **Claude double-checks every finding** against the code, and adds its own model and effort to the `**Model:**` line, as the checker:
   - It doesn't hold → marked **Invalid** in the report, with why, and ignored.
   - It holds and is **Critical or Major** → it **blocks implementation**. Claude stops and asks the engineer to review and adjust the research, spec and plan.
   - It holds and is **Minor or Nit** → handled during implementation, and recorded in the implementation report.
5. **If nothing blocks**, implementation starts with Phase 1.

### 5. Implementation: `implementation-report.md`

**Always on Opus, extra high** (mandatory). One phase at a time, on the feature branch. The build has zero warnings and the tests pass.

**Deviations** from the spec or plan go in `implementation-report.md`, in the documents' folder. They come either from what a phase finds, or from the Minor and Nit findings of the pre-implementation review. The report is updated in each phase and committed with it.

The engineer reviews as a Java/TypeScript developer, so the code stays plain, and each phase report explains new C++ ideas in Java/TS terms ("C++ notes").

### 6. Validation

Claude checks the phase against two references:
- **The plan and the spec:** everything the phase lists is done and matches the design, and nothing else slipped in. Deviations are in the implementation report.
- **The problem:** the change actually solves the issue described in `research.md`, or in the ticket the work comes from. This is checked on the real app, not only by reading the code.
  - Steps that need the engineer's hands are asked for with exact instructions, and Claude checks the result in the app's log.

At the last phase, the whole work is validated end to end against the research or ticket.

### 7. Review

A code review of the phase's diff, by **five agents in parallel**, one per aspect:

| Aspect | Looks for |
|---|---|
| **Security** | Anything an attacker or malware could use: paths, command lines, DLL loading, registry, privileges, input from the device or the system. |
| **Risk** | What can fail at runtime: crashes, leaks, races, error paths, edge cases, and dependence on undocumented behavior. |
| **Regression risk** | Existing behavior the change could break, especially paths that no test or check covers. |
| **Pragmatism** | Over-engineering, abstractions nobody needs, work beyond the plan, simpler ways to get the same result. |
| **Best practices** | Code style and quality: the codebase's conventions, readability for a Java/TS reader, naming, comments, and sound use of the language and the platform. |

- The agents run on the models in the table above.
- Each agent gets the diff, the spec and plan, and its aspect.
- Claude verifies each finding against the code and fixes the confirmed ones that are in scope. The phase report lists every finding as fixed, or not fixed and why.

**Then Claude stops and reports:**
- what changed, and any deviations;
- the checks, with their output;
- the validation;
- the review findings;
- the engineer's steps;
- the commit message.

The next phase starts only after the engineer's go.
