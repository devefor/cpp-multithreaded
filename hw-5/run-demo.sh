#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

SHM_PATH="${1:-/mpsc_demo}"
SHM_SIZE="${2:-65536}"
MESSAGE_TYPE="${3:-text}"

echo "[INFO] project dir: $PROJECT_DIR"
echo "[INFO] build dir:   $BUILD_DIR"
echo "[INFO] shm path:    $SHM_PATH"
echo "[INFO] shm size:    $SHM_SIZE"
echo "[INFO] msg type:    $MESSAGE_TYPE"

# echo "[INFO] cleaning old shared memory... (if has)"
# "$BUILD_DIR/mpsc_cleanup" "$SHM_PATH" 2>/dev/null || true

"$BUILD_DIR/mpsc_producer_first" "$SHM_PATH" "$SHM_SIZE" &
PRODUCER1_PID=$!

"$BUILD_DIR/mpsc_consumer" "$SHM_PATH" "$SHM_SIZE" "$MESSAGE_TYPE" &
CONSUMER_PID=$!

"$BUILD_DIR/mpsc_producer_second" "$SHM_PATH" "$SHM_SIZE" &
PRODUCER2_PID=$!

wait "$PRODUCER1_PID"
wait "$PRODUCER2_PID"

sleep 0.1
"$BUILD_DIR/mpsc_producer_stop" "$SHM_PATH" "$SHM_SIZE"

wait "$CONSUMER_PID"

# sleep 0.5
# "$BUILD_DIR/mpsc_cleanup" "$SHM_PATH" || true