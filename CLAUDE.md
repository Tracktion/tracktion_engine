# Tracktion Engine - Claude Code Assistant Configuration

## Git Workflow

### Branches
- Never commit to `develop` - it is protected. All changes go on a `bugfix/...` or `feature/...` branch (snake_case names, e.g. `bugfix/looping_midi_note_off_map`)
- **CI shortcut**: including `quick` anywhere in the branch name (convention: a `_quick` suffix, e.g. `feature/new_fade_curves_quick`) makes GitHub CI skip all 12 Debug matrix jobs (they no-op in seconds), roughly halving CI time. Release, ASan and static-analysis jobs still run. Use it for iterating on a branch, but do a final push on a normally-named branch (or rename before merging) so full Debug coverage runs before the PR into `develop`
- Avoid the word `quick` in normally-named branches - the CI check is a substring match and would silently skip Debug CI

### Commit Messages
- Use clear, descriptive messages
- Prefix with component (e.g. "MIDI: Fixed note-off handling", "Audio: Improved latency")
