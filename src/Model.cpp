#include "Model.h"
#include <iostream>

#include "stb_image.h"

Model::Model(std::string const &path, bool gamma) : gammaCorrection(gamma) {
    try {
        loadModel(path);
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during model loading: " << e.what() << std::endl;
        throw;
    }
    catch (...) {
        std::cerr << "Unknown exception during model loading!" << std::endl;
        throw;
    }
}

void Model::Draw(unsigned int shaderProgram) {
    for(unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].Draw(shaderProgram);
}

void Model::loadModel(std::string const &path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
    
    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
    {
        std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
        return;
    }
    
    directory = path.substr(0, path.find_last_of('/'));

    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode *node, const aiScene *scene) {
    //process each mesh located at the current node
    for(unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }
    
    // process each child node
    for(unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    // process vertices
    for(unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;
        glm::vec3 vector;
        
        // positions
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;
        
        // normals
        if (mesh->HasNormals())
        {
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
        }
        
        // texture coordinates
        if(mesh->mTextureCoords[0])
        {
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[0][i].x; 
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.TexCoords = vec;
        }
        else
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);

        vertices.push_back(vertex);
    }
    
    // process indices
    for(unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for(unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);        
    }
    
    // process materials
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];    

    // diffuse maps
    std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
    textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    
    // specular maps
    std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
    textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    
    // normal maps
    std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
    textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
    
    // height maps
    std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
    textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());
    
    return Mesh(vertices, indices, textures);
}

std::vector<Texture> Model::loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName) {
    std::vector<Texture> textures;

    for(unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        std::cout << "  Found texture: " << str.C_Str() << std::endl;
        
        bool skip = false;
        for(unsigned int j = 0; j < textures_loaded.size(); j++) {
            if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0) {
                textures.push_back(textures_loaded[j]);
                skip = true;
                break;
            }
        }
        if(!skip) {
            try {
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);
                std::cout << "Loaded texture ID: " << texture.id << " for path: " << texture.path << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "ERROR: Failed to load texture " << str.C_Str() << ": " << e.what() << std::endl;
            }
            catch (...) {
                std::cerr << "ERROR: Unknown exception while loading texture " << str.C_Str() << std::endl;
            }
        }
    }

    return textures;
}


unsigned int TextureFromFile(const char *path, const std::string &directory, bool gamma)
{
    std::string filename = std::string(path);
    
    // Check if path is absolute
    bool isAbsolute = (filename.find(':') != std::string::npos) || (filename[0] == '/');
    
    if (isAbsolute) {
        // Absolute path detected - extract just the filename and use it relatively
        size_t lastSlash = filename.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            std::string justFilename = filename.substr(lastSlash + 1);
            filename = directory + "/textures/" + justFilename;
            std::cout << "    Absolute path detected, converted to relative: " << filename << std::endl;
        } else {
            filename = directory + '/' + filename;
        }
    } else {
        filename = directory + '/' + filename;
    }

    std::cout << "    Attempting to load texture: " << filename << std::endl;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    
    // If primary path fails, try alternate paths
    if (!data) {
        std::string altPath = directory + "/textures/" + std::string(path);
        std::cout << "    Primary path failed, trying: " << altPath << std::endl;
        data = stbi_load(altPath.c_str(), &width, &height, &nrComponents, 0);
        
        if (!data) {
            size_t lastSlash = std::string(path).find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                std::string justFilename = std::string(path).substr(lastSlash + 1);
                altPath = directory + "/textures/" + justFilename;
                std::cout << "    Alternate path failed, trying: " << altPath << std::endl;
                data = stbi_load(altPath.c_str(), &width, &height, &nrComponents, 0);
            }
        }
    }
    
    if (data)
    {
        std::cout << "    ✓ Texture loaded successfully: " << width << "x" << height << " with " << nrComponents << " components" << std::endl;
        
        GLenum format;
        GLenum internalFormat;
        
        if (nrComponents == 1) {
            format = GL_RED;
            internalFormat = GL_RED;
        }
        else if (nrComponents == 3) {
            format = GL_RGB;
            internalFormat = gamma ? GL_SRGB : GL_RGB;
        }
        else if (nrComponents == 4) {
            format = GL_RGBA;
            internalFormat = gamma ? GL_SRGB_ALPHA : GL_RGBA;
        }
        else {
            std::cerr << "    Unsupported texture format with " << nrComponents << " components" << std::endl;
            stbi_image_free(data);
            glDeleteTextures(1, &textureID);
            return createDefaultTexture(); 
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Set texture wrapping and filtering parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // For single-component (grayscale) textures, set swizzle mask to replicate red to RGB
        if (nrComponents == 1) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ONE);
            std::cout << "    Applied grayscale swizzle mask for single-component texture" << std::endl;
        }

        stbi_image_free(data);
        
        // Check for OpenGL errors
        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            std::cerr << "    OpenGL error while creating texture: " << error << std::endl;
        }
        
        return textureID;
    }
    else
    {
        std::cerr << "    ✗ Texture failed to load at path: " << filename << std::endl;
        std::cerr << "    STB Error: " << stbi_failure_reason() << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        
        return createDefaultTexture();  // Create default texture
    }
}

// Add this helper function to create a default texture
unsigned int createDefaultTexture() {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    
    unsigned char checkerboard[] = {
        200, 200, 200, 255,   50, 50, 50, 255,
        50, 50, 50, 255,   200, 200, 200, 255
    };
    
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, checkerboard);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    std::cout << "    Created default checkerboard texture as fallback" << std::endl;
    return textureID;
}
