# CLI Contract — dx-extract

## `inspect`

```text
dx-extract inspect <source>
```

Purpose:
- establish source dimensions
- establish channels/depth
- provide initial source diagnostics

## `extract`

```text
dx-extract extract <source> --output <directory>
```

Purpose:
- execute the configured extraction pipeline
- persist intermediate/project artifacts
- emit editable SVG projection

Recognition options (mutually exclusive):

```text
--recognition <observations.json>
--vision-recognition
```

`--recognition` imports observations already produced by a human or an
external process, using the `eke-dx-wire-recognition-observation` contract.

`--vision-recognition` automates that hand-off: it sends the normalized page
and each detected text region to a vision-capable Claude model over the
Anthropic API and parses the response against the same contract. It requires
`EKE_DX_WIRE_ANTHROPIC_API_KEY` (or `ANTHROPIC_API_KEY`) to be set and fails
extraction with a configuration error if neither is present, rather than
silently falling back to no recognition.

Future options:

```text
--config <file>
--stage <name>
--page <number>
--resume
--force
--format <svg|json|oep>
--validate
--overlay
```

## Exit codes

```text
0  success
1  processing failure
2  command-line usage error
3  invalid configuration
4  unsupported source
5  validation failure
```

The CLI must never report successful extraction when a required stage failed.
