# System Scheduler

`SystemScheduler` is the first scheduling seam for ECS signal dispatch.

The current implementation is immediate and single-threaded. It records a small
task count for trace/debug use, then invokes the same `SignalRunner` callback on
the same stack frame. This preserves:

- priority-sorted runner order
- stack-backed `Signal` and event payload lifetimes
- existing entity traversal order inside `Signal`
- existing profiling timer coverage around each runner

`SystemCoordinator::Trigger` and `TriggerFromRoot` are the only call sites routed
through the scheduler initially. That keeps frame signals, queued events, and
physics/raycast callbacks on the same behavior-preserving path.

Future worker execution should keep a drain-before-return contract or copy task
payloads into scheduler-owned storage before deferring work.
