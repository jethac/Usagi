# Audio Tool Reverse Engineering Track

This track defines the bounded reverse-engineering pass for the shipped
binary-only audio tools under `Tools/AudioTool`. The goal is to recover the
data contracts and generated-output behavior needed to replace the tools, not
to clone the old ATF/WinForms UI.

## Scope

Read-only inputs:

- `Tools/AudioTool/AudioTool.exe`
- `Tools/AudioTool/FSIDBuilder.exe`
- `Tools/AudioTool/AudioInterop.dll`
- `Tools/AudioTool/Vitei.FMODInterop.dll`
- `Tools/AudioTool/*.config`
- `Tools/AudioTool/*.dll`
- `Tools/AudioTool/README.ja.txt`
- `Engine/Audio/**`
- `Tools/build/game_common_rake.rb`
- `Tools/build/build_config.rb`

Primary outputs:

- Confirmed `FSIDBuilder.exe` CLI behavior.
- Managed assembly inventory for public and internal audio data types.
- Audio bank field map from legacy types to `Engine/Audio/AudioBank.proto`.
- CRC and enum-generation compatibility notes.
- WAV metadata and loop-region behavior notes.
- Replacement-tool requirements that are proven by binary behavior rather than
  inference.

Non-goals:

- Rebuilding `AudioTool.exe` from missing source.
- Reproducing the legacy docking UI.
- Reintroducing ATF as the replacement tool framework.
- Decompiling third-party libraries beyond metadata needed to identify public
  dependency boundaries.

## Work Packages

### 1. Binary Inventory

List every executable, managed assembly, native DLL, config file, data file,
and README in `Tools/AudioTool`. Capture:

- File name, size, timestamp, product/file version, and architecture.
- Managed target framework for .NET assemblies.
- Strong names and assembly references.
- Native imports for FMOD, MediaInfo, SharpDX, and platform APIs.
- Embedded resources that look like schemas, icons, defaults, or serialized
  templates.

Expected deliverable: a table in the RE report plus a dependency graph showing
which binaries are tool-runtime dependencies and which are editor-only.

### 2. Black-Box Smoke

Run `FSIDBuilder.exe` and `AudioTool.exe` in a controlled working directory.
Capture:

- `--help` and invalid-argument output.
- Exit codes for missing input, malformed YAML, unknown options, and valid
  synthetic banks.
- Current-directory assumptions.
- Config-file reads and missing-DLL behavior.
- Any log files or generated temp files.

`AudioTool.exe` smoke should stay minimal: process launch, startup failure
diagnostics, config reads, and file-open behavior if it can be reached without
manual UI work. The CLI builder is the high-value target.

### 3. Managed Metadata Inspection

Inspect managed assemblies with metadata-first tooling. Preferred order:

1. .NET reflection or `ildasm` for assembly references, type names, members,
   attributes, resources, and entry points.
2. ILSpy/dotPeek or equivalent if available under `B:\usagi_dev\tools`.
3. Focused IL reading only where metadata is insufficient, especially CRC,
   enum escaping, YAML normalization, and protobuf output.

Recover type names and member lists for:

- Audio bank/editor document models.
- Sound, filter, effect, room, category, and project settings models.
- YAML load/save adapters.
- FSID/protobuf/header writers.
- CRC helpers and identifier normalization.
- WAV/media metadata readers.
- FMOD preview or import adapters.

Expected deliverable: a type/member inventory with notes on which members map
directly to the open runtime schema.

### 4. Data Contract Mapping

Map recovered fields to the runtime and build contracts:

- `Engine/Audio/AudioBank.proto`
- `Engine/Audio/AudioComponents.proto`
- `Engine/Audio/AudioEvents.proto`
- `Engine/Audio/AudioEnums.proto`
- `Engine/Audio/SoundFile.*`
- `Engine/Audio/_xaudio2/WaveFile.*`
- `Tools/Source/PakFileGen/FileFactory.cpp`
- `Tools/build/game_common_rake.rb`

For each field, classify it as:

- Required by current runtime.
- Used only by tools/build.
- Preserved but ignored by open runtime.
- FMOD/private-platform suspect.
- Derived metadata that should not be hand-authored.

Expected deliverable: a compatibility table that can drive
`Usagi.ToolCore.Audio`.

### 5. Golden Output Capture

Create tiny synthetic audio bank fixtures and run the legacy builder to capture:

- Generated FSID proto output.
- Generated C++ header output, if still supported.
- CRC values for representative names.
- Enum escaping for spaces, punctuation, digits, lowercase names, and repeated
  names.
- Behavior for missing optional fields and default values.

Do not replace `FSIDBuilder.exe` until the new builder matches these fixtures
or differences are explicitly accepted.

Expected deliverable: fixtures and expected outputs under a focused test path,
or a documented blocker if the binary cannot run headlessly.

### 6. Replacement Spec Update

Fold proven findings back into `Documents/AudioToolReplacement.md`:

- Exact CRC algorithm and normalization.
- Exact FSID proto/header formatting.
- YAML shape and default-value behavior.
- WAV metadata requirements.
- FMOD dependency decision.
- CLI compatibility requirements for switching `game_common_rake.rb`.

Expected deliverable: an implementation-ready audio-tool spec with open
questions reduced to project-data gaps rather than binary-tool unknowns.

## Subagent Brief

Use a dedicated worker in its own worktree. The worker owns only:

- `Documents/AudioToolReverseEngineering.md`
- `Documents/AudioToolReplacement.md`
- `Tools/Tests/AudioToolReverseEngineering/**`, if fixtures are added
- local tooling under `B:\usagi_dev\tools`, if metadata tools are installed

The worker must treat `Tools/AudioTool/**` as read-only. It should not edit
runtime audio code unless a separate implementation task is opened.

Recommended first command sequence:

```powershell
Set-Location B:\usagi_dev\Usagi
Get-ChildItem .\Tools\AudioTool -Force
.\Tools\AudioTool\FSIDBuilder.exe --help
```

Then install any required inspection tools under `B:\usagi_dev\tools`, not
inside the repository.

## Acceptance Criteria

- The binary package inventory is complete.
- `FSIDBuilder.exe` CLI behavior is captured with command lines and exit codes.
- Managed audio model types are inventoried at the level needed to implement a
  replacement.
- Legacy fields are mapped to current `Engine/Audio` protobuf/runtime fields.
- At least one golden-output fixture exists, or the reason it cannot be created
  is documented.
- `Documents/AudioToolReplacement.md` is updated with any proven facts from the
  RE pass.
- The next implementation task for `Usagi.ToolCore.Audio` can proceed without
  guessing at builder output behavior.

