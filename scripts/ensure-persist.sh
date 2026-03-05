#!/usr/bin/env bash
# ensure-persist.sh — Ensure persistent storage is available for dubsiren
#
# Called as ExecStartPre= in the dubsiren systemd service.  Tries multiple
# strategies to make INSTALL_DIR/data a durable, writable mount.  Exits 0
# even on total failure so the siren still starts (the C++ code handles
# volatile storage gracefully with warnings).
#
# Strategies (tried in order):
#   1. Data dir is already a mount point (systemd unit or previous run)
#   2. Mount root device rw at /mnt/persist, bind-mount data from there
#   3. Find the overlay lower-layer mount and bind-mount data from it
#   4. All failed — exit 0 anyway so dubsiren starts with volatile storage

set -u

INSTALL_DIR="${1:?Usage: ensure-persist.sh <install-dir>}"
DATA_DIR="${INSTALL_DIR}/data"
PERSIST_MNT="/mnt/persist"
LOG_TAG="ensure-persist"

log()  { echo "[${LOG_TAG}] $*"; }
warn() { echo "[${LOG_TAG}] WARNING: $*" >&2; }

# --- Helper: is $1 a mount point? -------------------------------------------
is_mount_point() {
    local dev parent_dev
    dev=$(stat -c '%d' "$1" 2>/dev/null) || return 1
    parent_dev=$(stat -c '%d' "$1/.." 2>/dev/null) || return 1
    [[ "$dev" != "$parent_dev" ]]
}

# --- Helper: is root an overlay filesystem? ----------------------------------
is_overlay_root() {
    awk '$2 == "/" && $3 == "overlay" { found=1 } END { exit !found }' /proc/mounts
}

# --- Strategy 1: Already mounted? -------------------------------------------
if is_mount_point "${DATA_DIR}"; then
    log "Data dir already mounted at ${DATA_DIR} — done."
    exit 0
fi

# If root is not overlay, no special handling needed — data dir is on rw disk
if ! is_overlay_root; then
    log "Not on overlay FS — data dir is on normal rw filesystem."
    exit 0
fi

log "Overlay FS detected — attempting to set up persistent storage..."

# --- Strategy 2: Mount root device rw at /mnt/persist -----------------------
# Find the root block device (the real one underneath the overlay)
ROOT_DEV=""
ROOT_FSTYPE=""

# Look for the ro mount of the root device that the overlay uses as lower layer
while IFS=' ' read -r dev mp fstype _rest; do
    # raspi-config overlay typically mounts root ro at /overlay/lower or similar
    if [[ "$fstype" != "overlay" && "$fstype" != "tmpfs" && "$fstype" != "devtmpfs" && \
          "$fstype" != "proc" && "$fstype" != "sysfs" && "$fstype" != "devpts" && \
          "$fstype" != "cgroup" && "$fstype" != "cgroup2" && "$fstype" != "fuse"* && \
          "$dev" == /dev/* ]]; then
        # Prefer the mount that looks like the root partition
        if [[ "$dev" == *mmcblk* || "$dev" == *nvme* || "$dev" == *sd* ]]; then
            ROOT_DEV="$dev"
            ROOT_FSTYPE="$fstype"
            # If this is the overlay lower layer, that's our best match
            if [[ "$mp" == */lower* || "$mp" == */ro* ]]; then
                break
            fi
        fi
    fi
done < /proc/mounts

if [[ -n "$ROOT_DEV" ]]; then
    log "Found root device: ${ROOT_DEV} (${ROOT_FSTYPE})"

    # Try to mount it rw at /mnt/persist (may already be mounted by systemd unit)
    if is_mount_point "${PERSIST_MNT}"; then
        log "Persist mount already active at ${PERSIST_MNT}."
    else
        mkdir -p "${PERSIST_MNT}"
        for attempt in 1 2 3; do
            if mount -t "${ROOT_FSTYPE}" -o rw,noatime "${ROOT_DEV}" "${PERSIST_MNT}" 2>/dev/null; then
                log "Mounted ${ROOT_DEV} rw at ${PERSIST_MNT} (attempt ${attempt})."
                break
            fi
            warn "Mount attempt ${attempt}/3 failed — retrying..."
            sleep 1
        done
    fi

    # Now try to bind-mount the data directory
    if is_mount_point "${PERSIST_MNT}" && [[ -d "${PERSIST_MNT}${DATA_DIR}" ]]; then
        mkdir -p "${DATA_DIR}"
        if mount --bind "${PERSIST_MNT}${DATA_DIR}" "${DATA_DIR}"; then
            log "Bind-mounted ${PERSIST_MNT}${DATA_DIR} → ${DATA_DIR} — persistent storage ready."
            exit 0
        fi
        warn "Bind mount failed."
    elif is_mount_point "${PERSIST_MNT}"; then
        # Data dir doesn't exist on persist mount yet — create it
        mkdir -p "${PERSIST_MNT}${DATA_DIR}/mp3s" "${PERSIST_MNT}${DATA_DIR}/presets"
        mkdir -p "${DATA_DIR}"
        if mount --bind "${PERSIST_MNT}${DATA_DIR}" "${DATA_DIR}"; then
            log "Created and bind-mounted ${PERSIST_MNT}${DATA_DIR} → ${DATA_DIR} — persistent storage ready."
            exit 0
        fi
        warn "Bind mount failed after creating directories."
    fi
fi

# --- Strategy 3: Find overlay lower-layer mount and bind from it ------------
log "Strategy 2 failed — looking for overlay lower layer..."

LOWER_PATH=""
# Check common raspi-config overlay lower-layer paths
for candidate in /overlay/lower /media/root-ro /mnt/lower; do
    if [[ -d "${candidate}${DATA_DIR}" ]]; then
        LOWER_PATH="${candidate}"
        break
    fi
done

# Also parse overlay mount options for lowerdir=
if [[ -z "$LOWER_PATH" ]]; then
    LOWER_PATH=$(awk '$2 == "/" && $3 == "overlay" {
        n = split($4, opts, ",")
        for (i = 1; i <= n; i++) {
            if (opts[i] ~ /^lowerdir=/) {
                sub(/^lowerdir=/, "", opts[i])
                # lowerdir can have multiple colon-separated paths; take first
                split(opts[i], parts, ":")
                print parts[1]
                exit
            }
        }
    }' /proc/mounts)
fi

if [[ -n "$LOWER_PATH" && -d "${LOWER_PATH}${DATA_DIR}" ]]; then
    log "Found overlay lower layer at ${LOWER_PATH}"

    # The lower layer is typically ro.  Try remounting it rw first.
    mount -o remount,rw "${LOWER_PATH}" 2>/dev/null || true

    mkdir -p "${DATA_DIR}"
    if mount --bind "${LOWER_PATH}${DATA_DIR}" "${DATA_DIR}"; then
        # Check if the bind mount is actually writable
        if touch "${DATA_DIR}/.persist-test" 2>/dev/null; then
            rm -f "${DATA_DIR}/.persist-test"
            log "Bind-mounted from overlay lower layer — persistent storage ready."
            exit 0
        else
            warn "Lower-layer bind mount is read-only — presets won't persist."
            umount "${DATA_DIR}" 2>/dev/null || true
        fi
    fi
fi

# --- All strategies failed ---------------------------------------------------
warn "Could not set up persistent storage."
warn "Dubsiren will start with volatile storage — presets will be lost on power cycle."
warn "Check: systemctl status mnt-persist.mount"
exit 0
