# CLAUDE.md

## Fresh Install (one-liner)

Run on a fresh Raspberry Pi to install everything. Prompts for autostart and kiosk mode:

```
curl -fsSL https://raw.githubusercontent.com/parkredding/poorhouse-lane-v4/main/setup.sh | sudo bash
```

## Testing on Raspberry Pi

After SSH'ing into the Raspberry Pi, run this one-liner to stop any running instance, fetch the newest branch, check it out, build, and run:

```
sudo systemctl stop dubsiren.service 2>/dev/null; cd ~/dubsiren && git fetch origin && BRANCH=$(git for-each-ref --sort=-committerdate 'refs/remotes/origin/claude/*' --format='%(refname:lstrip=3)' | head -1) && git checkout "$BRANCH" && git pull && mkdir -p build && cd build && cmake .. && make -j$(nproc) && ./dubsiren
```
