#include <string>
#include <iostream>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <blib/json.h>
#include <blib/util/Log.h>
#include <blib/util/FileSystem.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

blib::json::Value convertAssimpAnim(const std::string &filename);


#pragma comment(lib, "../externals/assimp/assimp.lib")

using blib::util::Log;


int vertexSize = 0;


blib::json::Value matrixAsJson(const aiMatrix4x4& matrix)
{
	blib::json::Value ret;
	
	for (int i = 0; i < 4; i++)
	{
		blib::json::Value row;
		for (int ii = 0; ii < 4; ii++)
			row.push_back(matrix[ii][i]);
		ret.push_back(row);
	}
	

	return ret;
}


void import(blib::json::Value &data, const aiScene* scene, aiNode* node, glm::mat4 matrix)
{

	glm::mat4 transformation = glm::make_mat4((float*)&node->mTransformation);
	
	matrix *= glm::transpose(transformation);

	//matrix = glm::mat4();


	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		const struct aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		std::vector<glm::vec3> faceNormals;
		std::vector<glm::vec3> vertexNormals;
		if (!mesh->HasNormals())
		{
			Log::out << "Mesh does not have normals...calculating" << Log::newline;

			for (unsigned int ii = 0; ii < mesh->mNumFaces; ii++)
			{
				const struct aiFace* face = &mesh->mFaces[ii];
				if (face->mNumIndices > 2)
				{
					glm::vec3 v1(mesh->mVertices[face->mIndices[0]].x, mesh->mVertices[face->mIndices[0]].y, mesh->mVertices[face->mIndices[0]].z);
					glm::vec3 v2(mesh->mVertices[face->mIndices[1]].x, mesh->mVertices[face->mIndices[1]].y, mesh->mVertices[face->mIndices[1]].z);
					glm::vec3 v3(mesh->mVertices[face->mIndices[2]].x, mesh->mVertices[face->mIndices[2]].y, mesh->mVertices[face->mIndices[2]].z);
					v1 = glm::vec3(glm::vec4(v1, 1) * matrix);
					v2 = glm::vec3(glm::vec4(v2, 1) * matrix);
					v3 = glm::vec3(glm::vec4(v3, 1) * matrix);

					glm::vec3 normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
					faceNormals.push_back(normal);
				}
				else
					faceNormals.push_back(glm::vec3(0, 0, 0));
			}


			for (unsigned int i = 0; i < mesh->mNumVertices; i++)
			{
				glm::vec3 normal(0, 0, 0);
				for (unsigned int ii = 0; ii < mesh->mNumFaces; ii++)
				{
					const struct aiFace* face = &mesh->mFaces[ii];
					for (unsigned int iii = 0; iii < face->mNumIndices; iii++)
					{
						if (face->mIndices[iii] == i)
							normal += faceNormals[ii];
					}
				}
				if (glm::length(normal) > 0.1)
					normal = glm::normalize(normal);
				vertexNormals.push_back(normal);
			}
		}


		int vertexStart = data["vertices"].size() / 8;
		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			glm::vec4 vertex(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
			vertex = vertex * matrix;

			data["vertices"].push_back(vertex.x);
			data["vertices"].push_back(vertex.y);
			data["vertices"].push_back(vertex.z);

			if (mesh->HasTextureCoords(0))
			{
				data["vertices"].push_back(mesh->mTextureCoords[0][i].x);
				data["vertices"].push_back(1 - mesh->mTextureCoords[0][i].y);
			}
			else
			{
				data["vertices"].push_back(0);
				data["vertices"].push_back(0);
			}
			glm::vec4 normal;
			if (mesh->HasNormals())
			{
				normal = glm::vec4(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z, 0.0f);
				normal = normal * matrix; // TODO: should this be matrix, or should this be a normalmatrix?
			}
			else
			{
				normal = glm::vec4(vertexNormals[i], 0) * matrix; // TODO: matrix
			}

			data["vertices"].push_back(normal.x);
			data["vertices"].push_back(normal.y);
			data["vertices"].push_back(normal.z);
			
	/*		//boneIDs
			data["vertices"].push_back(-1);
			data["vertices"].push_back(-1);
			data["vertices"].push_back(-1);
			data["vertices"].push_back(-1);

			//weights
			data["vertices"].push_back(0.0f);
			data["vertices"].push_back(0.0f);
			data["vertices"].push_back(0.0f);
			data["vertices"].push_back(0.0f);*/
		}


		const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

		aiColor4D diffuse;
		aiColor4D specular;
		aiColor4D ambient;
		aiColor4D transparency;
		aiString texPath;

		blib::json::Value meshData;

		if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == aiReturn_SUCCESS)
		{
			meshData["material"]["diffuse"].push_back(diffuse.r);
			meshData["material"]["diffuse"].push_back(diffuse.g);
			meshData["material"]["diffuse"].push_back(diffuse.b);
		}
		else
		{
			meshData["material"]["diffuse"].push_back(0.5f);
			meshData["material"]["diffuse"].push_back(0.5f);
			meshData["material"]["diffuse"].push_back(0.5f);
		}


		if (aiGetMaterialColor(material, AI_MATKEY_COLOR_AMBIENT, &specular) == aiReturn_SUCCESS)
		{
			meshData["material"]["ambient"].push_back(ambient.r);
			meshData["material"]["ambient"].push_back(ambient.g);
			meshData["material"]["ambient"].push_back(ambient.b);
		}
		else
		{
			meshData["material"]["ambient"].push_back(0.5f);
			meshData["material"]["ambient"].push_back(0.5f);
			meshData["material"]["ambient"].push_back(0.5f);
		}

		if (aiGetMaterialColor(material, AI_MATKEY_COLOR_SPECULAR, &specular) == aiReturn_SUCCESS)
		{
			meshData["material"]["specular"].push_back(specular.r);
			meshData["material"]["specular"].push_back(specular.g);
			meshData["material"]["specular"].push_back(specular.b);
		}
		else
		{
			meshData["material"]["specular"].push_back(1.0f);
			meshData["material"]["specular"].push_back(1.0f);
			meshData["material"]["specular"].push_back(1.0f);
		}

		if (aiGetMaterialColor(material, AI_MATKEY_COLOR_TRANSPARENT, &transparency) == aiReturn_SUCCESS)
			meshData["material"]["alpha"] = transparency.a;
		else
			meshData["material"]["alpha"] = 1;

		if (aiGetMaterialColor(material, AI_MATKEY_SHININESS, &transparency) == aiReturn_SUCCESS)
			meshData["material"]["shinyness"] = transparency.a;
		else
			meshData["material"]["shinyness"] = 0;


		

		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == aiReturn_SUCCESS)
			meshData["material"]["texture"] = texPath.C_Str();
		else
			meshData["material"]["texture"] = "../textures/whitepixel.png";

		for (unsigned int ii = 0; ii < mesh->mNumFaces; ii++)
		{
			const struct aiFace* face = &mesh->mFaces[ii];
			if (face->mNumIndices != 3)
				continue;
			assert(face->mNumIndices == 3);

			for (int iii = 0; iii < 3; iii++)
				meshData["faces"].push_back(vertexStart + (int)face->mIndices[iii]);
		}



		if (mesh->HasBones())
		{
			std::function<void(aiNode* node, blib::json::Value& data)> writeNode;
			writeNode = [&writeNode, &mesh, &data](aiNode* node, blib::json::Value& d)
			{
				d["name"] = node->mName.C_Str();
				d["matrix"] = matrixAsJson(node->mTransformation);
				
				const aiBone* bone = NULL;
				for (unsigned int ii = 0; ii < mesh->mNumBones; ii++)
				{
					if (mesh->mBones[ii]->mName == node->mName)
					{
						d["offset"] = matrixAsJson(mesh->mBones[ii]->mOffsetMatrix);
						d["boneid"] = (int)ii;


						for (size_t iii = 0; iii < mesh->mBones[ii]->mNumWeights; iii++)
						{
							const aiVertexWeight& w = mesh->mBones[ii]->mWeights[iii];

							for (int iiii = 0; iiii < 4; iiii++)
							{
								if (data["vertices"][w.mVertexId * vertexSize + 8 + iiii].asInt() == -1)
								{
									data["vertices"][w.mVertexId * vertexSize + 8] = (int)ii;
									data["vertices"][w.mVertexId * vertexSize + 8 + 4] = w.mWeight;
									break;
								}
							}
						}

					}
				}

				
				for (unsigned int ii = 0; ii < node->mNumChildren; ii++)
				{
					blib::json::Value value;
					writeNode(node->mChildren[ii], value);
					d["children"].push_back(value);
				}
			};
			writeNode(scene->mRootNode, meshData["bones"]);
		}

		if (meshData.isMember("faces"))
			data["meshes"].push_back(meshData);
	}


	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		import(data, scene, node->mChildren[i], matrix);
	}



}



blib::json::Value convertAssimp(std::string filename)
{
	blib::util::FileSystem::registerHandler(new blib::util::PhysicalFileSystemHandler(""));
	Assimp::Importer importer;
	
	char* data;
	int len = blib::util::FileSystem::getData(filename, data);

	if (len == 0)
	{
		printf("Error opening file %s\n", filename.c_str());
		return blib::json::Value();
	}

	const aiScene* scene = importer.ReadFileFromMemory(data, len, aiProcessPreset_TargetRealtime_Quality | aiProcess_OptimizeMeshes | aiProcess_RemoveRedundantMaterials | aiProcess_OptimizeGraph, filename.substr(filename.rfind(".")).c_str());
	if (!scene)
	{
		printf("Errors? : %s\n", importer.GetErrorString());
		return blib::json::Value();
	}

	if (scene->HasAnimations())
	{
		Log::out << "Found animation!" << Log::newline;
		return convertAssimpAnim(filename);
	}




	blib::json::Value jsonData(blib::json::Type::objectValue);

	jsonData["name"] = "Converted from " + filename;
	jsonData["version"] = 1;
	jsonData["format"].push_back("position");
	jsonData["format"].push_back(3);
	jsonData["format"].push_back("texcoord");
	jsonData["format"].push_back(2);
	jsonData["format"].push_back("normal");
	jsonData["format"].push_back(3);
	jsonData["format"].push_back("boneIDs");
	jsonData["format"].push_back(4);
	jsonData["format"].push_back("weights");
	jsonData["format"].push_back(4);

	vertexSize = 0;
	for (size_t i = 1; i < jsonData["format"].size(); i += 2)
		vertexSize += jsonData["format"][i].asInt();

	jsonData["vertices"] = blib::json::Value(blib::json::Type::arrayValue);
	jsonData["meshes"] = blib::json::Value(blib::json::Type::arrayValue);



	import(jsonData, scene, scene->mRootNode, glm::rotate(glm::rotate(glm::mat4(), 180.0f, glm::vec3(1,0,0)), 180.0f, glm::vec3(0,0,1)));

	if (scene->HasAnimations())
	{
		for (unsigned int i = 0; i < scene->mNumAnimations; i++)
		{
			const aiAnimation* animation = scene->mAnimations[i];
			Log::out << "Animation found: " << animation->mName.C_Str() << Log::newline;

			blib::json::Value animData;
			animData["name"] = animation->mName.C_Str();

			float tps = (float)animation->mTicksPerSecond;
			if (tps == 0)
				tps = 25;

			animData["length"] = (float)(animation->mDuration / tps);


			for (unsigned int ii = 0; ii < animation->mNumChannels; ii++)
			{
				const aiNodeAnim* anim = animation->mChannels[ii];
				Log::out << "Found animation for node " << anim->mNodeName.C_Str() << Log::newline;

				blib::json::Value channel;
				channel["name"] = anim->mNodeName.C_Str();
				channel["positions"];

				for (unsigned int iii = 0; iii < anim->mNumPositionKeys; iii++)
				{
					blib::json::Value frame;
					frame["pos"].push_back(anim->mPositionKeys[iii].mValue.x);
					frame["pos"].push_back(anim->mPositionKeys[iii].mValue.y);
					frame["pos"].push_back(anim->mPositionKeys[iii].mValue.z);
					frame["time"] = (float)anim->mPositionKeys[iii].mTime / tps;
					channel["positions"].push_back(frame);
				}
				for (unsigned int iii = 0; iii < anim->mNumScalingKeys; iii++)
				{
					blib::json::Value frame;
					frame["scale"].push_back(anim->mScalingKeys[iii].mValue.x);
					frame["scale"].push_back(anim->mScalingKeys[iii].mValue.y);
					frame["scale"].push_back(anim->mScalingKeys[iii].mValue.z);
					frame["time"] = (float)anim->mScalingKeys[iii].mTime / tps;
					channel["scales"].push_back(frame);
				}
				for (unsigned int iii = 0; iii < anim->mNumRotationKeys; iii++)
				{
					blib::json::Value frame;
					frame["rot"].push_back(anim->mRotationKeys[iii].mValue.x);
					frame["rot"].push_back(anim->mRotationKeys[iii].mValue.y);
					frame["rot"].push_back(anim->mRotationKeys[iii].mValue.z);
					frame["rot"].push_back(anim->mRotationKeys[iii].mValue.w);
					frame["time"] = (float)anim->mRotationKeys[iii].mTime / tps;
					channel["rotations"].push_back(frame);
				}
				animData["channels"].push_back(channel);
			}

			jsonData["animations"].push_back(animData);
		}
	}


	return jsonData;
}
