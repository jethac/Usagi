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

`TaskRunner` currently has zero configured workers, so tasks drain inline. Its
worker path is still synchronous: when workers are enabled, the caller also helps
execute tasks and does not return until all tasks finish. Future scheduler work
can opt into workers without changing the payload lifetime contract.

Use `SystemCoordinator::ConfigureSystemScheduler` to set the worker count and
maximum task count. The default configuration remains zero workers.
