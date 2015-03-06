#include <string>
#include <fstream>
#include <iostream>
#include <blib/json.h>
#include <string.h>

#pragma pack(push)
#pragma pack(1)
struct Header
{
	char header[3];
	float version;

	char nameJap[20];
	char commentJap[256];
};

struct Vertex
{
	float	posx,
			posy,
			posz,

			normalx,
			normaly,
			normalz,

			texcoordx,
			texcoordy;
	unsigned short	boneId0,
					boneId1;
	char	weight,
			edgeFlag;
};


struct Material
{
	float diffuseR,
		diffuseG,
		diffuseB,
		alpha,
		shinyness,
		specularR,
		specularG,
		specularB,
		ambientR,
		ambientG,
		ambientB;
	char toonNumber,
		edgeFlag;
	unsigned long vertexCount;
	char texture[20];

};

#pragma pack(pop)



blib::json::Value convertPmd(std::string filename)
{
	std::ifstream file(filename.c_str(), std::ios_base::binary | std::ios_base::in);
	if (!file.is_open())
	{
		printf("Could not open file\n ");
		return blib::json::Value();
	}
	Header header;
	file.read((char*)&header, sizeof(Header));
	
	unsigned long vertexCount;
	file.read((char*)&vertexCount, 4);
	printf("%i vertices found\n", vertexCount);
	Vertex* vertices = new Vertex[vertexCount];
	file.read((char*)vertices, vertexCount * sizeof(Vertex));

	unsigned long indexCount;
	file.read((char*)&indexCount, 4);
	printf("%i indices found (should be divisable by 3)\n", indexCount);
	
	unsigned short* indices = new unsigned short[indexCount];
	file.read((char*)indices, indexCount * sizeof(unsigned short));

	unsigned long materialCount;
	file.read((char*)&materialCount, 4);
	printf("%i materials\n", materialCount);

	Material* materials = new Material[materialCount];
	memset(materials, 0, materialCount * sizeof(Material));
	file.read((char*)materials, materialCount * sizeof(Material));


	file.close();


	blib::json::Value model(blib::json::Type::objectValue);
	model["name"] = "converted from " + filename;
	model["version"] = 1;
	model["format"].push_back("position");
	model["format"].push_back(3);
	model["format"].push_back("texcoord");
	model["format"].push_back(2);
	model["format"].push_back("normal");
	model["format"].push_back(3);

	for (size_t i = 0; i < vertexCount; i++)
	{
		model["vertices"].push_back(vertices[i].posx);
		model["vertices"].push_back(vertices[i].posy);
		model["vertices"].push_back(vertices[i].posz);

		model["vertices"].push_back(vertices[i].texcoordx);
		model["vertices"].push_back(vertices[i].texcoordy);

		model["vertices"].push_back(vertices[i].normalx);
		model["vertices"].push_back(vertices[i].normaly);
		model["vertices"].push_back(vertices[i].normalz);
	}


	int index = 0;

	for (size_t i = 0; i < materialCount; i++)
	{
		blib::json::Value mesh;
		mesh["material"]["ambient"].push_back(materials[i].ambientR);
		mesh["material"]["ambient"].push_back(materials[i].ambientG);
		mesh["material"]["ambient"].push_back(materials[i].ambientB);
		
		mesh["material"]["diffuse"].push_back(materials[i].diffuseR);
		mesh["material"]["diffuse"].push_back(materials[i].diffuseG);
		mesh["material"]["diffuse"].push_back(materials[i].diffuseB);

		mesh["material"]["shinyness"] = materials[i].shinyness;

		mesh["material"]["specular"].push_back(materials[i].specularR);
		mesh["material"]["specular"].push_back(materials[i].specularG);
		mesh["material"]["specular"].push_back(materials[i].specularB);

		mesh["material"]["alpha"] = materials[i].alpha;

		if (strstr(materials[i].texture, "*") == NULL)
			mesh["material"]["texture"] = materials[i].texture;
		else
			mesh["material"]["texture"] = std::string(materials[i].texture, strstr(materials[i].texture, "*") - materials[i].texture);

		for (size_t ii = index; ii < index + materials[i].vertexCount; ii++)
			mesh["faces"].push_back(indices[ii]);

		index += materials[i].vertexCount;
		model["meshes"].push_back(mesh);
	}


	delete[] vertices;
	delete[] indices;


	return model;
}