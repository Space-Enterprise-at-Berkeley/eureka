# orch
Orchestration software for dGSE.

## Development
Virtual environment and package management requires [uv](https://docs.astral.sh/uv/).

```bash
# Install dependencies (creates .venv)
uv sync

# Run project
uv run python -m orch

# Add a dependency
uv add <package>          # runtime
uv add --dev <package>    # dev-only
```
