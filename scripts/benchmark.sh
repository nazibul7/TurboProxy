#!/bin/bash

URL="http://localhost:8000/"
VERSION=${VERSION:-"v1-single-threaded"}  # Change this for each version
OUTDIR="benchmarks/$VERSION"
mkdir -p "$OUTDIR"

echo "🔧 Benchmarking TurboProxy ($VERSION) at $URL"
echo "Results will be saved in $OUTDIR/"

# --------------------------
# wrk
# --------------------------
echo "🚀 Running wrk..."
wrk -t4 -c100 -d10s "$URL" > "$OUTDIR/wrk.txt" 2>&1
echo "✅ wrk done → $OUTDIR/wrk.txt"

# --------------------------
# ab
# --------------------------
echo "🚀 Running ab (ApacheBench)..."
ab -n 1000 -c 100 "$URL" > "$OUTDIR/ab.txt" 2>&1
echo "✅ ab done → $OUTDIR/ab.txt"

# --------------------------
# siege
# --------------------------
echo "🚀 Running siege..."
siege -c100 -t10S "$URL" > "$OUTDIR/siege.txt" 2>&1
echo "✅ siege done → $OUTDIR/siege.txt"

echo "✅ All benchmarks for $VERSION completed. 📊 Check $OUTDIR/ for results."
