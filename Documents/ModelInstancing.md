# Model Instancing

## Roadmap 08-03 smoke coverage

`Tools/Tests/ModelInstancing/Run.ps1` is a narrow smoke test for the existing model instance conversion path. It creates a temporary Usagi data root with a minimal entity YAML that references `PBRSample/PBRSample.vmdf`, converts a tiny `.lvl` through `Tools/python/lvl2vhir/lvl2vhir.py`, and verifies the generated instance-set YAML contract:

- `Instance: true`
- `Format: transform`
- `Length: 2`
- `ModelName: PBRSample/PBRSample.vmdf`
- one node per source entity placement, including translated and scaled transform data
- no obsolete `.vmdc` model extension
- `Tools/ruby/maya2pb.rb` can convert the generated instance-set YAML to a non-empty binary output

This does not touch runtime renderer code.
