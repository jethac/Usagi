# System Scheduler

`SystemScheduler` is the first scheduling seam for ECS signal dispatch.

The current implementation is immediate and single-threaded. It records a small
task count for trace/debug use, wraps work in `TaskRunner::Task`, then invokes
the same `SignalRunner` callback on the same stack frame. This preserves:

- priority-sorted runner order
- stack-backed `Signal` and event payload lifetimes
- existing entity traversal order inside `Signal`
- existing profiling timer coverage around each runner

`SystemCoordinator::Trigger` and `TriggerFromRoot` are the only call sites routed
through the scheduler initially. That keeps frame signals, queued events, and
physics/raycast callbacks on the same behavior-preserving path.

Root signals are split into branch callbacks before execution reaches
`TaskRunner`. The scheduler still drains each branch immediately in sibling
order, so branch visibility is available without changing traversal behavior.

`TaskRunner` currently has zero workers and drains tasks inline. Future worker
execution should keep that drain-before-return contract or copy task payloads
into scheduler-owned storage before deferring work.
