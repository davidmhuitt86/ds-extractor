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
