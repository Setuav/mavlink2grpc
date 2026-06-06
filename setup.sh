#!/bin/bash
# This script sets up the environment for the project
# Tested on Ubuntu 22.04

set -e

echo "Setting up the environment..."

# Install system dependencies
echo "-- Installing system dependencies..."
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc

# Ensure git submodules are initialized and updated
echo "-- Initializing and updating submodules..."
git submodule update --init --recursive

echo "-- Installing Python dependencies for the generator..."
pip3 install -r generator/requirements.txt

echo "Environment setup complete."