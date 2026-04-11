#!/usr/bin/env python3
import os
import subprocess
import sys
import shutil

print("======================================")
print(" Starting Linux Test Suite")
print("======================================")

print("1. Checking dependencies...")
if not shutil.which("nvcc"):
    print("ERROR: nvcc (CUDA compiler) could not be found. Please ensure CUDA is installed.")
    sys.exit(1)
if not shutil.which("g++"):
    print("ERROR: g++ could not be found. Please ensure GCC is installed.")
    sys.exit(1)

print("2. Compiling the pipeline...")
subprocess.run(["make", "clean"], check=False)
try:
    subprocess.run(["make"], check=True)
except subprocess.CalledProcessError:
    print("ERROR: Compilation failed.")
    sys.exit(1)

print("3. Generating sample edge list (Triangle graph)...")
with open("sample_graph.txt", "w") as f:
    f.write("0 1\n1 2\n2 0\n0 3\n")

print("4. Running the pipeline...")
try:
    subprocess.run(["./kcore_pipeline", "sample_graph.txt"], check=True)
except subprocess.CalledProcessError:
    print("ERROR: Execution failed.")
    sys.exit(1)

print("======================================")
print(" Linux Test Suite Passed!")
print("======================================")
