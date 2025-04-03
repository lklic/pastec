# Pastec Build System

This document explains the build system for Pastec, which uses Nix and Docker to create optimized builds with efficient caching.

## Overview

The build system is designed to optimize Docker layer caching by separating the build process into two main parts:

1. Building dependencies (which change infrequently)
2. Building the Pastec application (which changes more frequently)

This separation allows Docker to reuse the cached dependencies layer when only the Pastec code changes, significantly reducing build times for iterative development.

## Key Components

### 1. Dependencies Flake (`deps-flake.nix`)

This flake is used in the Docker build process to build all the optimized dependencies that Pastec requires:

- OpenCV
- libmicrohttpd
- curl
- jsoncpp
- mimalloc
- Build tools (cmake, clang, lld)

These dependencies are built with optimization flags and stored in a separate Docker layer.

### 2. Main Flake (`flake.nix`)

The main flake contains the full build configuration for Pastec:

- For local builds, it defines and builds all dependencies directly
- In the Docker build, it uses the pre-built dependencies from the previous stage
- Builds the Pastec application code

### 3. Multi-stage Dockerfile

The Dockerfile uses a three-stage build process:

1. **Dependencies Stage**: 
   - Copies `deps-flake.nix` to a temporary directory and renames it to `flake.nix`
   - Builds only the dependencies
   - Stores the dependencies in the Nix store

2. **Application Stage**: 
   - Copies the pre-built dependencies from the previous stage
   - Builds Pastec using the main flake
   - Reuses the dependencies from the previous stage

3. **Final Stage**: 
   - Creates a minimal runtime image with just the necessary files
   - Includes only the built application and its runtime dependencies

## Benefits

This approach provides several benefits:

1. **Faster Iterative Builds**: When only the Pastec code changes, Docker reuses the cached dependencies layer, significantly reducing build time.

2. **Optimized Dependencies**: All dependencies are built with optimization flags for maximum performance.

3. **Minimal Final Image**: The final image contains only the necessary files, keeping the image size small.

## How Docker Caching Works

Docker's layer caching works based on the Dockerfile instructions:

1. If the instructions for a layer haven't changed, Docker reuses the cached layer.
2. If a layer changes, that layer and all subsequent layers are rebuilt.

By separating dependencies and application code into different stages, we ensure that changes to the application code don't invalidate the dependencies cache.

## Example Build Process

When you run `docker build`:

1. If `deps-flake.nix` hasn't changed, Docker reuses the cached dependencies layer.
2. If only the Pastec code has changed, Docker rebuilds only the application and final stages.
3. If `deps-flake.nix` has changed, Docker rebuilds all stages.

This results in much faster builds during normal development, where dependencies rarely change but application code changes frequently.
