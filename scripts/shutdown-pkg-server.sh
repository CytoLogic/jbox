#!/bin/bash
# Shutdown the jshell package registry server

PORT="${PORT:-3000}"

# Find the process listening on the specified port
# Try multiple methods for portability
PID=$(lsof -t -i :"$PORT" 2>/dev/null)

if [ -z "$PID" ]; then
    # Fallback: parse ss output
    PID=$(ss -tlnp 2>/dev/null | grep ":$PORT " | grep -oP 'pid=\K[0-9]+' | head -1)
fi

if [ -z "$PID" ]; then
    # Fallback: use fuser
    PID=$(fuser "$PORT"/tcp 2>/dev/null | awk '{print $1}')
fi

if [ -z "$PID" ]; then
    echo "No server running on port $PORT"
    exit 0
fi

echo "Stopping package registry server (PID: $PID)..."
kill "$PID" 2>/dev/null

# Wait briefly for graceful shutdown
sleep 1

# Check if still running
if kill -0 "$PID" 2>/dev/null; then
    echo "Server did not stop gracefully, forcing..."
    kill -9 "$PID" 2>/dev/null
fi

echo "Server stopped"
