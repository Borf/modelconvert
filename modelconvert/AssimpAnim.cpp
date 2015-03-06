#include <string>
#include <functional>

#include <blib/json.h>
#include <blib/util/FileSystem.h>
#include <blib/util/Log.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

using blib::util::Log;

blib::json::Value materialToJson(const aiMaterial* material);
blib::json::Value matrixAsJson(const aiMatrix4x4& matrix);

static int vertexSize = 0;



void import(blib::json::Value &data, const aiScene* scene, aiNode* node)
{
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
			data["vertices"].push_back(mesh->mVertices[i].x);
			data["vertices"].push_back(mesh->mVertices[i].y);
			data["vertices"].push_back(mesh->mVertices[i].z);

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
				normal = glm::vec4(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z, 0.0f);
			else
				normal = glm::vec4(vertexNormals[i], 0);

			data["vertices"].push_back(normal.x);
			data["vertices"].push_back(normal.y);
			data["vertices"].push_back(normal.z);

			//boneIDs
			data["vertices"].push_back(-1);
			data["vertices"].push_back(-1);
			data["vertices"].push_back(-1);
			data["vertices"].push_back(-1);

			//weights
			data["vertices"].push_back(0.0f);
			data["vertices"].push_back(0.0f);
			data["vertices"].push_back(0.0f);
			data["vertices"].push_back(0.0f);
		}


		const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		blib::json::Value meshData;
		meshData["material"] = materialToJson(material);

		//add faces
		for (unsigned int ii = 0; ii < mesh->mNumFaces; ii++)
		{
			const struct aiFace* face = &mesh->mFaces[ii];
			if (face->mNumIndices != 3)
				continue;
			assert(face->mNumIndices == 3);

			for (int iii = 0; iii < 3; iii++)
				meshData["faces"].push_back(vertexStart + (int)face->mIndices[iii]);
		}
		// add bone IDs and weights to the global (non-mesh) vertices
		if (mesh->HasBones())
		{
			for (unsigned int ii = 0; ii < mesh->mNumBones; ii++)
			{
				const aiBone* bone = mesh->mBones[ii];
				for (unsigned int iii = 0; iii < bone->mNumWeights; iii++)
				{
					const aiVertexWeight& weight = bone->mWeights[iii];
					int index = (vertexStart + weight.mVertexId);
					for (unsigned int iiii = 0; iiii < 4; iiii++)
					{
						if (data["vertices"][vertexSize * index + 8 + iiii].asInt() == -1)
						{
							data["vertices"][vertexSize * index + 8 + iiii] = (int)ii;
							data["vertices"][vertexSize * index + 8 + iiii + 4] = weight.mWeight;
						}
						break;
					}
				}
			}
		}


		if (meshData.isMember("faces"))
			data["meshes"].push_back(meshData);
	}


	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		import(data, scene, node->mChildren[i]);
	}



}


blib::json::Value buildSkeleton(const aiNode* node)
{
	blib::json::Value skeleton;
	skeleton["name"] = node->mName.C_Str();
	skeleton["matrix"] = matrixAsJson(node->mTransformation);
	for (unsigned int i = 0; i < node->mNumChildren; i++)
		skeleton["children"].push_back(buildSkeleton(node->mChildren[i]));
	return skeleton;
}


blib::json::Value convertAssimpAnim(const std::string &filename)
{
	blib::json::Value modelData;
	blib::json::Value skeletonData;

	Assimp::Importer importer;
	char* data;
	int len = blib::util::FileSystem::getData(filename, data);
	if (len == 0)
	{
		printf("Error opening file %s\n", filename.c_str());
		return blib::json::Value::null;
	}

	const aiScene* scene = importer.ReadFileFromMemory(data, len, aiProcessPreset_TargetRealtime_Quality | aiProcess_OptimizeMeshes | aiProcess_RemoveRedundantMaterials | aiProcess_OptimizeGraph);
	if (!scene)
	{
		printf("Errors? : %s\n", importer.GetErrorString());
		return blib::json::Value::null;
	}

	modelData["name"] = "Converted from " + filename;
	modelData["version"] = 1;
	modelData["format"].push_back("position");
	modelData["format"].push_back(3);
	modelData["format"].push_back("texcoord");
	modelData["format"].push_back(2);
	modelData["format"].push_back("normal");
	modelData["format"].push_back(3);
	modelData["format"].push_back("boneIDs");
	modelData["format"].push_back(4);
	modelData["format"].push_back("weights");
	modelData["format"].push_back(4);

	vertexSize = 0;
	for (size_t i = 1; i < modelData["format"].size(); i += 2)
		vertexSize += modelData["format"][i].asInt();

	modelData["vertices"] = blib::json::Value(blib::json::Type::arrayValue);
	modelData["meshes"] = blib::json::Value(blib::json::Type::arrayValue);
	import(modelData, scene, scene->mRootNode);
	for (size_t i = 0; i < modelData["vertices"].size(); i += vertexSize)
	{
		for (int ii = 0; ii < 4; ii++)
			if (modelData["vertices"][i + 8 + ii].asInt() == -1)
				modelData["vertices"][i + 8 + ii] = 0;
	}


	{
		std::ofstream out(filename + ".mesh.json");
		modelData.prettyPrint(out, blib::json::readJson(R"V0G0N(	
	{
		"wrap" : 1,
		"format" :
		{
			"wrap" : 2
		},
		"meshes" :
		{
			"wrap" : 1,
			"elements" :
			{
				"faces" : 
				{
					"wrap" : 3
				},
				"material" :
				{
					"wrap" : 2
				},
				"sort" : [ "material", "faces" ]
			}
		},
		"vertices" :
		{		
			"wrap" : 16
		},
		"sort" : [ "name", "version", "format", "vertices", "meshes" ]
	})V0G0N"));
		out.close();
	}

	//build up json tree
	skeletonData = buildSkeleton(scene->mRootNode);
	//fill in the nodes
	for (unsigned int i = 0; i < scene->mNumMeshes; i++)
	{
		if (scene->mMeshes[i]->HasBones())
		{
			for (unsigned int ii = 0; ii < scene->mMeshes[i]->mNumBones; ii++)
			{
				const aiBone* bone = scene->mMeshes[i]->mBones[ii];
				std::string name = bone->mName.C_Str();

				std::function<void(blib::json::Value&)> func;
				func = [&func, &name, &ii, &bone](blib::json::Value& b){
					if (b["name"].asString() == name)
					{
						b["id"] = (int)ii;
						b["offset"] = matrixAsJson(bone->mOffsetMatrix);
					}
					else if (b.isMember("children"))
					{
						for (size_t i = 0; i < b["children"].size(); i++)
							func(b["children"][i]);
					}
				};
				func(skeletonData);
			}
		}
	}


	{
		std::ofstream out(filename + ".skel.json");
		skeletonData.prettyPrint(out, blib::json::readJson(R"V0G0N(	
	{
		"recursive" : true,
		"wrap" : 1,
		"matrix" :
		{
			"wrap" : 4,
			"seperator" : "		",
			"elements" : { "wrap" : 4 }
		},
		"offset" :
		{
			"wrap" : 4,
			"seperator" : "		",
			"elements" : { "wrap" : 4 }
		},
		"sort" : [ "name", "matrix", "offset", "id", "children" ]
	})V0G0N"));
		out.close();
	}


	for (unsigned int i = 0; i < scene->mNumAnimations; i++)
	{
		blib::json::Value animationData;
		const aiAnimation* animation = scene->mAnimations[i];
		float tps = (float)animation->mTicksPerSecond;
		if (tps == 0)
			tps = 25;

		animationData["name"] = animation->mName.C_Str();
		animationData["length"] = (float)(animation->mDuration / tps);
		
		for (unsigned int ii = 0; ii < animation->mNumChannels; ii++)
		{
			const aiNodeAnim* stream = animation->mChannels[ii];
			blib::json::Value streamData;
			streamData["node"] = stream->mNodeName.C_Str();
			for (size_t iii = 0; iii < stream->mNumPositionKeys; iii++)
			{
				blib::json::Value data;
				data["time"] = (float)(stream->mPositionKeys[iii].mTime / tps);
				data["pos"].push_back(stream->mPositionKeys[iii].mValue.x);
				data["pos"].push_back(stream->mPositionKeys[iii].mValue.y);
				data["pos"].push_back(stream->mPositionKeys[iii].mValue.z);
				streamData["positions"].push_back(data);
			}
			for (size_t iii = 0; iii < stream->mNumScalingKeys; iii++)
			{
				blib::json::Value data;
				data["time"] = (float)(stream->mScalingKeys[iii].mTime / tps);
				data["scale"].push_back(stream->mScalingKeys[iii].mValue.x);
				data["scale"].push_back(stream->mScalingKeys[iii].mValue.y);
				data["scale"].push_back(stream->mScalingKeys[iii].mValue.z);
				streamData["scales"].push_back(data);
			}
			for (size_t iii = 0; iii < stream->mNumRotationKeys; iii++)
			{
				blib::json::Value data;
				data["time"] = (float)(stream->mRotationKeys[iii].mTime / tps);
				data["rot"].push_back(stream->mRotationKeys[iii].mValue.x);
				data["rot"].push_back(stream->mRotationKeys[iii].mValue.y);
				data["rot"].push_back(stream->mRotationKeys[iii].mValue.z);
				data["rot"].push_back(stream->mRotationKeys[iii].mValue.w);
				streamData["rotations"].push_back(data);
			}


			animationData["streams"].push_back(streamData);
		}



		std::ofstream out(filename + "."+animationData["name"].asString()+".anim.json");
		animationData.prettyPrint(out, blib::json::readJson(R"V0G0N(	
	{
		"wrap" : 1,
		"streams" :
		{	
			"wrap" : 1,
			"elements" :
			{		
				"wrap" : 1,
				"positions" :
				{	
					"wrap" : 1,
					"elements" :
					{
						"wrap" : 1,
						"pos" : { "wrap" : 3 }
					}
				},
				"scales" :
				{	
					"wrap" : 1,
					"elements" :
					{
						"wrap" : 1,
						"scale" : { "wrap" : 3 }
					}
				},
				"rotations" :
				{	
					"wrap" : 1,
					"elements" :
					{
						"wrap" : 1,
						"rot" : { "wrap" : 4 }
					}
				}
			}
		}
	})V0G0N"));
		out.close();

	}






	return blib::json::Value::null;
}



blib::json::Value materialToJson(const aiMaterial* material)
{

	aiColor4D diffuse;
	aiColor4D specular;
	aiColor4D ambient;
	aiColor4D transparency;
	aiString texPath;

	blib::json::Value ret;

	if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == aiReturn_SUCCESS)
	{
		ret["diffuse"].push_back(diffuse.r);
		ret["diffuse"].push_back(diffuse.g);
		ret["diffuse"].push_back(diffuse.b);
	}
	else
	{
		ret["diffuse"].push_back(0.5f);
		ret["diffuse"].push_back(0.5f);
		ret["diffuse"].push_back(0.5f);
	}


	if (aiGetMaterialColor(material, AI_MATKEY_COLOR_AMBIENT, &specular) == aiReturn_SUCCESS)
	{
		ret["ambient"].push_back(ambient.r);
		ret["ambient"].push_back(ambient.g);
		ret["ambient"].push_back(ambient.b);
	}
	else
	{
		ret["ambient"].push_back(0.5f);
		ret["ambient"].push_back(0.5f);
		ret["ambient"].push_back(0.5f);
	}

	if (aiGetMaterialColor(material, AI_MATKEY_COLOR_SPECULAR, &specular) == aiReturn_SUCCESS)
	{
		ret["specular"].push_back(specular.r);
		ret["specular"].push_back(specular.g);
		ret["specular"].push_back(specular.b);
	}
	else
	{
		ret["specular"].push_back(1.0f);
		ret["specular"].push_back(1.0f);
		ret["specular"].push_back(1.0f);
	}

	if (aiGetMaterialColor(material, AI_MATKEY_COLOR_TRANSPARENT, &transparency) == aiReturn_SUCCESS)
		ret["alpha"] = transparency.a;
	else
		ret["alpha"] = 1;

	if (aiGetMaterialColor(material, AI_MATKEY_SHININESS, &transparency) == aiReturn_SUCCESS)
		ret["shinyness"] = transparency.a;
	else
		ret["shinyness"] = 0;



	return ret;

}