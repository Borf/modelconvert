#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <direct.h>
#include <blib/Util.h>
#include <blib/json.h>
#include <blib/util/FileSystem.h>

blib::json::Value convertPmd(std::string filename);
blib::json::Value convertAssimp(std::string filename);
blib::json::Value convertAssimpAnim(const std::string &filename);

#pragma comment(lib, "blib.lib")


int main(int argc, char* argv[])
{
	blib::util::FileSystem::registerHandler(new blib::util::PhysicalFileSystemHandler());
	printf("ModelConverter...\n");
	if (argc == 1)
	{
		printf("Please add a model filename as 2nd parameter\n");
		getchar();
		return -1;
	}

	char buf[1024];
	_getcwd(buf, 1024);
	printf("Current working dir: %s\n", buf);


	std::string filename = argv[1];
	std::replace(filename.begin(), filename.end(), '/', '\\');
	std::string extension = filename.substr(filename.rfind("."));
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

	printf("Extension found: %s\n", extension.c_str());


	blib::json::Value data;

	if (extension == ".pmd")
		data = convertPmd(filename);
	if (extension == ".dae")
		data = convertAssimp(filename);
	if (extension == ".obj")
		data = convertAssimp(filename);
	if (extension == ".3ds")
		data = convertAssimp(filename);
	if (extension == ".fbx")
		data = convertAssimp(filename);


	std::string outfile = filename + ".json";
	if (argc > 2)
		outfile = argv[2];

	std::string format = R"V0G0N(	
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
				"bones" :
				{
					"wrap" : 1,
					"elements" :
					{
						"wrap" : 1,
						"matrix" :
						{
							"wrap" : 4,
							"seperator" : "		",
							"elements" :
							{
								"wrap" : 4
							}
						}

					}
				}
			}
		},
		"vertices" :
		{
			"wrap" : 8,
			"seperator" : "	"
		}
	})V0G0N";
	blib::json::Value wrapConfig = blib::json::readJson(format);




	if (outfile == "-")
		std::cout << data;
	else
	{
		//std::ofstream(outfile) << data;
		std::ofstream out(outfile);
		data.prettyPrint(out, wrapConfig);
	}

	return 0;
}