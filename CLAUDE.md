# CLAUDE.md

## Fresh Install (one-liner)

Run on a fresh Raspberry Pi to install everything. Prompts for autostart and kiosk mode:

```
curl -fsSL https://raw.githubusercontent.com/parkredding/poorhouse-lane-v4/main/setup.sh | sudo bash
```

## Testing on Raspberry Pi

After SSH'ing into the Raspberry Pi, run this one-liner to fetch the newest branch, check it out, build, and run:

```
cd ~/dubsiren && BRANCH=$(git ls-remote --heads origin 'claude/*' | sort -t/ -k3 -r | head -1 | awk '{print $2}' | sed 's|refs/heads/||') && git fetch origin && git checkout "$BRANCH" && git pull && mkdir -p build && cd build && cmake .. && make -j$(nproc) && ./dubsiren
```
