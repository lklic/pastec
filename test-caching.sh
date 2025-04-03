#!/bin/bash
# Script to demonstrate Docker layer caching with the new build system

# Enable BuildKit for better caching
export DOCKER_BUILDKIT=1

# Clean any previous builds to ensure a fresh test
echo "=== Cleaning previous builds ==="
docker system prune -f

# First build - should build everything
echo "=== First Build (Full Build) ==="
time docker-compose build

# Show the layers that were created
echo "=== Docker image layers after first build ==="
docker history pastec_pastec

# Make a small change to a Pastec source file
echo "=== Making a small change to Pastec source code ==="
echo "// Test comment to trigger rebuild - $(date)" >> src/main.cpp

# Second build - should only rebuild the Pastec application, not dependencies
echo "=== Second Build (Should reuse dependency cache) ==="
time docker-compose build

# Show the layers that were created/reused
echo "=== Docker image layers after second build ==="
docker history pastec_pastec

# Restore the source file
echo "=== Restoring source file ==="
git checkout -- src/main.cpp

echo "=== Test Complete ==="
echo "The second build should be significantly faster than the first build"
echo "because it reused the cached dependencies layer."
echo ""
echo "You should see in the docker history output that the deps-builder stage"
echo "was reused (marked as 'cached') in the second build."
