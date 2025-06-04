
# Updated Makefile

# Set compiler; you can change to clang++ if preferred.
CXX = clang++

# Uncomment the following line to use clang++ instead:
# CXX = clang++

# Define include directories
GLAD_INC    = glad/include
SYSTEM_INC  = /run/current-system/sw/include
GLM_INC     = /nix/store/8qfz11yw0hsgicp6i1gl2106bzg7bdmd-glm-1.0.1/include
GLFW_INC    = /nix/store/7v9lq96hibxf6zyl6nfhsl0y5g77zmcb-glfw-3.4/include
GLEW_INC		= /nix/store/l94yyg86v1dl04cvr426ddgz5jlv8rnl-glew-2.2.0-dev/include
ASSIMP = ./assimp/include
ASSIMP_LIB = ./assimp/build/bin

# Compiler flags
CXXFLAGS = -g -std=c++17 \
           -I$(GLAD_INC) \
           -I$(SYSTEM_INC) \
           -I$(GLM_INC) \
           -I$(GLFW_INC) \
			-I$(GLEW_INC) \
            -I$(ASSIMP) \
           -pthread \

# Linker flags
LDFLAGS = -L/run/current-system/sw/lib -L$(ASSIMP_LIB) -lglfw -lGLEW -ldl -lGL -lassimp 
PKG_GL_FLAGS = $(shell pkg-config --cflags --libs glu)
# Source files (update as needed)
SOURCES = shader.cpp   main.cpp glad/src/glad.c Model.cpp Mesh.cpp

# Output executable
TARGET = main

# Default target
all:
	$(CXX) $(CXXFLAGS) $(SOURCES) $(LDFLAGS) $(PKG_GL_FLAGS) -o $(TARGET)

# Clean up build files
clean:
	rm -f $(TARGET) *.o

run:
	LD_LIBRARY_PATH=$(ASSIMP_LIB) ./$(TARGET)
