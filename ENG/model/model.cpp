#include "model.h"

Model::Model()
{
	
}

// loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector
void Model::loadModel(std::string path)
{
	// read file via ASSIMP
	Assimp::Importer import;
	const aiScene *scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
	// check for errors
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
		return;
	}
	// retrieve the directory path of the filepath
	directory = path.substr(0, path.find_last_of('/'));
	// process ASSIMP's root node recursively
	processNode(scene->mRootNode, scene);

	normalizeModel();
}

// processes a node in a recursive fashion. Processes each individual mesh located at the node
// and repeats this process on its children nodes (if any)
void Model::processNode(aiNode* node, const aiScene* scene)
{
	// process each mesh located at the current node
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		// the node object only contains indices to index the actual objects in the scene
		// the scene contains all the data, node is just to keep stuff organized (like relations between nodes)
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		meshes.push_back(processMesh(mesh, scene));
	}
	// after we've processed all of the meshes (if any) we then recursively process each of the children nodes
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene)
{
	// data to fill
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<Texture> textures;
	Material uMaterial;

	// walk through each of the mesh's vertices
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;
		// we declare a placeholder vector since assimp uses its own vector class
		// that doesn't directly convert to glm's vec3 class so we transfer the data to
		// this placeholder glm::vec3 first.
		glm::vec3 vector;
		// positions
		vector.x = mesh->mVertices[i].x;
		vector.y = mesh->mVertices[i].y;
		vector.z = mesh->mVertices[i].z;
		vertex.Position = vector;
		// normals
		vector.x = mesh->mNormals[i].x;
		vector.y = mesh->mNormals[i].y;
		vector.z = mesh->mNormals[i].z;
		vertex.Normal = vector;
		// texture coordinates
		if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
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
	// now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		// retrieve all the indices of the face and store them in the indices vector
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j]);
		}
	}
	// process materials
	if (mesh->mMaterialIndex >= 0)
	{
		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		// we assume a convention for sampler names in the shaders. Each diffuse texture should be named
		// as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER.
		// same applies to other textures as the following list summarizes:
		// diffuse: texture_diffuseN
		// specular: texture_specularN
		// normal: texture_normalN

		// 1. diffuse maps
		std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
		textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
		// 2. specular maps
		std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
		textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
		// 3. normal maps
		std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
		textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
		// 4. height maps
		std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
		textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

		// 5. untextured materials
		if (textures.size() == 0)
			uMaterial = loadMaterial(material);
	}

	// return a mesh object created from the extracted mesh data
	if (textures.size() > 0)
		return Mesh(vertices, indices, textures);
	else
		return Mesh(vertices, indices, uMaterial);
}

// checks all material textures of a given type and loads the textures if they're not loaded yet.
// the required info is returned as a Texture struct.
std::vector<Texture> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName)
{
	std::vector<Texture> textures;
	for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
	{
		aiString str;
		mat->GetTexture(type, i, &str);
		// check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
		bool skip = false;
		for (unsigned int j = 0; j < textures_loaded.size(); j++)
		{
			if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
			{
				textures.push_back(textures_loaded[j]);
				skip = true; // a texture with the same filepath has already been loaded, continue to next one (optimization)
				break;
			}
		}
		if (!skip)
		{	// if texture hasn't been loaded already, load it
			Texture texture;
			texture.id = TextureFromFile(str.C_Str(), directory);
			texture.type = typeName;
			texture.path = str.C_Str();
			textures.push_back(texture);
			textures_loaded.push_back(texture); // store it as texture loaded for entire model, to ensure we won't unnecessary load duplicate textures
		}
	}
	return textures;
}

Material Model::loadMaterial(aiMaterial* mat)
{
	Material material;
	material.untextured = true;

	//*
	aiColor3D color(0.0f, 0.0f, 0.0f);
	float shininess;

	mat->Get(AI_MATKEY_COLOR_AMBIENT, color);
	material.ambient = glm::vec3(color.r, color.b, color.g);

	mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
	material.diffuse = glm::vec3(color.r, color.b, color.g);

	mat->Get(AI_MATKEY_COLOR_SPECULAR, color);
	material.specular = glm::vec3(color.r, color.b, color.g);

	mat->Get(AI_MATKEY_SHININESS, shininess);
	material.shininess = shininess;
	//*/

	/*/
	material.ambient = glm::vec3(0.0215f, 0.1745f, 0.0215f);
	material.diffuse = glm::vec3(0.07568f, 0.61424f, 0.07568f);
	material.specular = glm::vec3(0.633f, 0.727811f, 0.633f);
	material.shininess = 32.0f;
	//*/

	/*
	std::cout << "Material:" << std::endl;
	std::cout << "\tAmbient: [x:" << material.ambient.x << ", y:" << material.ambient.y << ", z:" << material.ambient.z << "]" << std::endl;
	std::cout << "\tDiffuse: [x:" << material.diffuse.x << ", y:" << material.diffuse.y << ", z:" << material.diffuse.z << "]" << std::endl;
	std::cout << "\tSpecular: [x:" << material.specular.x << ", y:" << material.specular.y << ", z:" << material.specular.z << "]" << std::endl;
	std::cout << "\tShininess: " << material.shininess << std::endl;
	//*/


	return material;
}

void Model::normalizeModel()
{
	glm::vec3 min(9999.0f);
	glm::vec3 max(-9999.0f);

	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		for (unsigned int j = 0; j < meshes[i].vertices.size(); j++)
		{
			glm::vec3 pos = meshes[i].vertices[j].Position;
			if (pos.x > max.x)	{ max.x = pos.x; }
			if (pos.y > max.y)	{ max.y = pos.y; }
			if (pos.z > max.z)	{ max.z = pos.z; }
			if (pos.x < min.x)	{ min.x = pos.x; }
			if (pos.y < min.y)	{ min.y = pos.y; }
			if (pos.z < min.z)	{ min.z = pos.z; }
		}
	}

	glm::vec3 center;
	center.x = min.x + (max.x - min.x) / 2.0f;
	center.y = min.y + (max.y - min.y) / 2.0f;
	center.z = min.z + (max.z - min.z) / 2.0f;
	//std::cout << "Center: [x:" << center.x << ", y:" << center.y << ", z:" << center.z << "]" << std::endl;

	for (unsigned int i = 0; i < meshes.size(); i++)
	{
		Mesh mesh;
		for (unsigned int j = 0; j < meshes[i].vertices.size(); j++)
		{
			Vertex vertex = meshes[i].vertices[j];
			vertex.Position -= center;
			mesh.vertices.push_back(vertex);
			mesh.indices = meshes[i].indices;
			mesh.textures = meshes[i].textures;
			mesh.material = meshes[i].material;
		}
		mesh.setupMesh();
		meshes[i] = mesh;
	}
}