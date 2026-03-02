# CLAUDE.md

## Testing on Raspberry Pi

After SSH'ing into the Raspberry Pi, run this one-liner to pull, build, and run the current branch:

```
cd ~/poorhouse-lane-v4 && git pull origin $(git branch --show-current) && mkdir -p build && cd build && cmake .. && make -j$(nproc) && ./dubsiren
```
