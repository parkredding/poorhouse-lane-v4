# CLAUDE.md

## Testing on Raspberry Pi

After SSH'ing into the Raspberry Pi, run this one-liner to fetch the newest branch, check it out, build, and run:

```
cd ~/poorhouse-lane-v4 && BRANCH=$(git ls-remote --heads origin 'claude/*' | sort -t/ -k3 -r | head -1 | awk '{print $2}' | sed 's|refs/heads/||') && git fetch origin "$BRANCH" && git checkout "$BRANCH" && git pull origin "$BRANCH" && mkdir -p build && cd build && cmake .. && make -j$(nproc) && ./dubsiren
```
