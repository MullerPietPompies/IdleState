#include "Mesh.h"
#include <iostream>

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures) {
    this->vertices = vertices;
    this->indices = indices;
    this->textures = textures;

    setupMesh();
}

void Mesh::Draw(unsigned int shaderProgram) {
    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    
    for(unsigned int i = 0; i < textures.size(); i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        
        std::string number;
        std::string name = textures[i].type;

        if(name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if(name == "texture_specular")
            number = std::to_string(specularNr++);
        else 
            continue; // Skip unknown texture types

        std::string uniformName = "material." + name + number;
        std::cout << "Binding texture " << i << " to uniform: " << uniformName << std::endl;
        
        // Check if the texture ID is valid
        if (textures[i].id == 0) {
            std::cerr << "Warning: Texture ID is 0 for " << textures[i].path << ". Using default texture." << std::endl;
            continue;
        }
        
        // Set the shader sampler to the current texture unit
        GLint location = glGetUniformLocation(shaderProgram, uniformName.c_str());
        if (location == -1) {
            std::cerr << "Warning: Uniform " << uniformName << " not found in shader." << std::endl;
        } else {
            glUniform1i(location, i);
        }
        
        // Bind the texture
        glBindTexture(GL_TEXTURE_2D, textures[i].id);

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "OpenGL error when binding texture: " << err << std::endl;
        }
    }
    
    // If no diffuse texture was bound, make sure the uniform is set to 0
    if (diffuseNr == 1) {
        std::cout << "No diffuse texture found, setting default" << std::endl;
        GLint location = glGetUniformLocation(shaderProgram, "material.texture_diffuse1");

        if (location != -1) {
            glUniform1i(location, 0);
        }
    }
    
    // Draw mesh
    glBindVertexArray(VAO);
    
    // Check for errors before drawing
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error before drawing: " << err << std::endl;
    }
    
    // Draw the mesh
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    
    // Check for errors after drawing
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after drawing: " << err << std::endl;
    }
    
    glBindVertexArray(0);
    
    // Reset to default texture
    glActiveTexture(GL_TEXTURE0);
}

void Mesh::setupMesh() {
    // Create buffers/arrays
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    
    // Load data into vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    // Load data into element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

    // Set the vertex attribute pointers
    // Vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    
    // Vertex normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    
    // Vertex texture coords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    // Unbind VAO (it's always a good thing to unbind any buffer/array to prevent strange bugs)
    glBindVertexArray(0);
    
    // Check for OpenGL errors during setup
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error during mesh setup: " << err << std::endl;
    }
}