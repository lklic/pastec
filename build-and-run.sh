#!/bin/bash
# Script to build and run Pastec with the optimized Docker setup

# Enable BuildKit for better caching
export DOCKER_BUILDKIT=1

# Build the Docker image with progress output
echo "Building Pastec Docker image..."
DOCKER_BUILDKIT=1 COMPOSE_DOCKER_CLI_BUILD=1 docker-compose build --progress=plain

# Show the layers that were created
echo "Docker image layers:"
docker history pastec_pastec

# Run the container
echo "Starting Pastec container..."
docker-compose up -d

# Show logs
echo "Container logs:"
docker-compose logs -f
