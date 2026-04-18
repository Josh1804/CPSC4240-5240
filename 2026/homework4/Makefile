NVCC = nvcc
CXX = g++
CXX_FLAGS = -O3 -std=c++17 -Wall
NVCC_FLAGS = -O3 -std=c++17 -arch=native

TARGET = kcore_pipeline

all: $(TARGET)

$(TARGET): main.o kcore_cpu.o kcore_gpu.o
	$(NVCC) $(NVCC_FLAGS) -o $@ $^

main.o: main.cpp kcore_pipeline.cuh
	$(CXX) $(CXX_FLAGS) -c $< -o $@

kcore_cpu.o: kcore_cpu.cpp kcore_pipeline.cuh
	$(CXX) $(CXX_FLAGS) -c $< -o $@

kcore_gpu.o: kcore_gpu.cu kcore_pipeline.cuh
	$(NVCC) $(NVCC_FLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)
