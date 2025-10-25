# Work Scripts Directory

This directory contains **temporary utility scripts** used during development.

## Rules

1. **Auto-cleanup:** Scripts are automatically deleted after use
2. **Git-ignored:** All files here (except this README) are ignored by git
3. **Temporary only:** Don't commit scripts here - they're tools, not project files

## Usage

```bash
# Create a temporary script
cat > work_scripts/my_task.sh << 'EOF'
#!/bin/bash
# Do something useful
EOF

# Run it
bash work_scripts/my_task.sh

# Auto-delete after use (Claude does this)
rm work_scripts/my_task.sh
```

## Purpose

- Quick one-off automation
- Build helpers
- Data processing scripts
- Testing utilities

**Note:** If a script becomes permanent, move it to appropriate location (e.g., `scripts/`, `tools/`)
