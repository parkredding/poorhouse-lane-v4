# CLAUDE.md

## Testing on Raspberry Pi

After SSH'ing into the Raspberry Pi, run this one-liner to pull, build, and run the current branch:

```
cd ~/poorhouse-lane-v4 && git pull origin claude/add-claude-md-4291S && mkdir -p build && cd build && cmake .. && make -j2 && ./dubsiren
```
