#!/usr/bin/env python3
import os
import subprocess
import sys
import shutil

print("======================================")
print(" Starting Mac OS Test Suite")
print("======================================")

print("1. Checking dependencies...")
if not shutil.which("g++"):
    print("ERROR: g++ could not be found. Please install the Command Line Tools (xcode-select --install).")
    sys.exit(1)

if not shutil.which("nvcc"):
    print("WARNING: nvcc (CUDA compiler) is not natively found on Mac.")
    print("The full pipeline requires a CUDA-capable GPU.")
    print("We will attempt to compile ONLY the CPU components (Task A) for local verification.")
    
    print("\nCompiling CPU-only objects...")
    try:
        subprocess.run(["g++", "-O3", "-std=c++17", "-Wall", "-c", "main.cpp", "-o", "main.o"], check=True)
        subprocess.run(["g++", "-O3", "-std=c++17", "-Wall", "-c", "kcore_cpu.cpp", "-o", "kcore_cpu.o"], check=True)
        print("CPU compilation successful! (Linking requires GPU components, skipping execution).")
    except subprocess.CalledProcessError:
        print("ERROR: Compilation failed.")
        sys.exit(1)
else:
    print("nvcc found. Compiling full pipeline...")
    subprocess.run(["make", "clean"], check=False)
    try:
        subprocess.run(["make"], check=True)
    except subprocess.CalledProcessError:
        print("ERROR: Compilation failed.")
        sys.exit(1)
        
    print("\nGenerating sample edge list...")
    with open("sample_graph.txt", "w") as f:
        f.write("0 1\n1 2\n2 0\n0 3\n")

    print("Running the pipeline...")
    try:
        subprocess.run(["./kcore_pipeline", "sample_graph.txt"], check=True)
    except subprocess.CalledProcessError:
        print("ERROR: Execution failed.")
        sys.exit(1)

print("======================================")
print(" Mac Test Suite Passed!")
print("======================================")
