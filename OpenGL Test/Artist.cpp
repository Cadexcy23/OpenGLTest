#include <iostream>
#include <fstream>
#include <sstream>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <GL/glew.h>
#include <freeglut.h>
#include <SDL_opengl.h>
#include <time.h>
#include "Artist.h"
#include "FPSCounter.h"
#include <map>
//TEMP
#include "Controller.h"



//Screen constants    
int Artist::SCREEN_WIDTH = 1920;
int Artist::SCREEN_HEIGHT = 1080;
float Artist::SCREEN_SCALE = 1;
bool Artist::FULLSCREEN = true;
bool Artist::drawMenus = true;
bool Artist::displayFPS = false;

//Global stuff holding info for rendering
Artist::mat4x4 model, view, projection;
GLint modelUniform;
GLuint shaderProgram;
GLuint vao;
std::vector<GLuint> vbos;//maybe dont need master lists of IDs for these
std::vector<GLuint> eabs;
std::vector<GLuint> textures;
std::vector<Artist::object> objectMasterList;

//keep track of time MAYBE MOVE TO CONTROLLER
Uint64 NOW = 0;
Uint64 LAST = 0;
double deltaTime = 0;


//The window we'll be rendering to
SDL_Window* gWindow = NULL;
//OpenGL context handle
SDL_GLContext GLContext;
//The window renderer
SDL_Renderer* gRenderer = NULL;





//Declare textures in a header -> define them in that headers respective file (maybe here) -> load em in the load funcion in the artist file
//how to define a texture
//SDL_Texture* Class::gTestTexture = NULL;
//how to define a animation
//std::vector<SDL_Texture*> Class::gTestAnimation;

//fonts
Artist::fontTextureSet Artist::largeFont;
Artist::fontTextureSet Artist::smallFont;




//Update screen
void Artist::updateScreen()//NEEDS REMOVE/REWORK
{
	SDL_RenderPresent(gRenderer);
}

//Clear screen
void Artist::clearScreen()//NEEDS REMOVE/REWORK
{
	SDL_RenderClear(gRenderer);
}

void read_shader_src(const char* fname, std::vector<char>& buffer)
{
	std::ifstream in;
	in.open(fname, std::ios::binary);

	if (in.is_open()) {
		// Get the number of bytes stored in this file
		in.seekg(0, std::ios::end);
		size_t length = (size_t)in.tellg();

		// Go to start of the file
		in.seekg(0, std::ios::beg);

		// Read the content of the file in a buffer
		buffer.resize(length + 1);
		in.read(&buffer[0], length);
		in.close();
		// Add a valid C - string end
		buffer[length] = '\0';
	}
	else {
		std::cerr << "Unable to open " << fname << " I'm out!" << std::endl;
		exit(-1);
	}
}

// Compile a shader
GLuint load_and_compile_shader(const char* fname, GLenum shaderType) {
	// Load a shader from an external file
	std::vector<char> buffer;
	read_shader_src(fname, buffer);
	const char* src = &buffer[0];

	// Compile the shader
	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);
	// Check the result of the compilation
	GLint test;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &test);
	if (!test) {
		std::cerr << "Shader compilation failed with this message:" << std::endl;
		std::vector<char> compilation_log(512);
		glGetShaderInfoLog(shader, compilation_log.size(), NULL, &compilation_log[0]);
		std::cerr << &compilation_log[0] << std::endl;
		exit(-1);

	}
	return shader;
}

GLuint create_program(const char* path_vert_shader, const char* path_frag_shader) {
	// Load and compile the vertex and fragment shaders
	GLuint vertexShader = load_and_compile_shader(path_vert_shader, GL_VERTEX_SHADER);
	GLuint fragmentShader = load_and_compile_shader(path_frag_shader, GL_FRAGMENT_SHADER);

	// Attach the above shader to a program
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);

	// Flag the shaders for deletion
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	// Link and use the program
	glLinkProgram(shaderProgram);
	glUseProgram(shaderProgram);

	return shaderProgram;
}

SDL_Texture* Artist::loadTexture(std::string path)
{
	//The final texture
	SDL_Texture* newTexture = NULL;

	//Load image at specified path
	SDL_Surface* loadedSurface = IMG_Load(path.c_str());
	if (loadedSurface == NULL)
	{
		printf("Unable to load image %s! SDL_image Error: %s\n", path.c_str(), IMG_GetError());
	}
	else
	{
		//Create texture from surface pixels
		newTexture = SDL_CreateTextureFromSurface(gRenderer, loadedSurface);
		if (newTexture == NULL)
		{
			printf("Unable to create texture from %s! SDL Error: %s\n", path.c_str(), SDL_GetError());
		}

		//Get rid of old loaded surface
		SDL_FreeSurface(loadedSurface);
	}

	return newTexture;
}

Artist::object constructObject(std::vector<GLfloat> vertexPositions, std::vector <GLfloat> textureCoords, std::vector <GLuint> indices, std::vector<GLfloat> colors, std::vector<std::string> textureList, Artist::point scale, Artist::point tran,  Artist::point rot, bool draw, bool transparency)
{
	Artist::object tempObject;


	//save the amount of vertecies
	tempObject.vertexCount = vertexPositions.size();

	//get the ID of the vbo we generate and add it to the list of VBOs
	GLuint VBOID;
	glGenBuffers(1, &VBOID);
	vbos.push_back(VBOID);
	//save the ID we got to the object
	tempObject.VBOID = VBOID;

	// Allocate space and upload the data from CPU to GPU    maybe doesnt need to be done? or maybe just neesd to be done when we manipulate the contents of them?
	glBindBuffer(GL_ARRAY_BUFFER, VBOID);
	glBufferData(GL_ARRAY_BUFFER, vertexPositions.size() * sizeof(GLfloat) + colors.size() * sizeof(GLfloat) + textureCoords.size() * sizeof(GLfloat), &vertexPositions[0], GL_STATIC_DRAW);
	// Transfer the vertex texture positions
	glBufferSubData(GL_ARRAY_BUFFER, vertexPositions.size() * sizeof(GLfloat), textureCoords.size() * sizeof(GLfloat), &textureCoords[0]);
	// Transfer the vertex colors
	glBufferSubData(GL_ARRAY_BUFFER, vertexPositions.size() * sizeof(GLfloat) + textureCoords.size() * sizeof(GLfloat), colors.size() * sizeof(GLfloat), &colors[0]);

	//get the ID of the eab we generate and add it to the list of EABs
	GLuint EABID;
	glGenBuffers(1, &EABID);
	eabs.push_back(EABID);
	//save the ID we got to the object
	tempObject.EABID = EABID;

	// Transfer the data from indices to eab
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EABID);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);

	//create a vector to hold the IDs we generate, resize it, generate them, add IDs to main texture list
	int textureCount = textureList.size();
	std::vector<GLuint> textureIDs;
	textureIDs.resize(textureCount);
	glGenTextures(textureCount, &textureIDs[0]);
	for (int i = 0; i < textureIDs.size(); i++)
	{
		textures.push_back(textureIDs[i]);
	}
	//save the list of IDs to the object
	tempObject.textures = textureIDs;
	//for every texture we are going to load
	for (int i = 0; i < textureCount; i++)
	{
		//std::string location = textureList[i];
		//bind the texture we are working with
		glBindTexture(GL_TEXTURE_2D, textureIDs[i]);
		//std::string stringNumber = location + std::to_string(i + 1) + ".png";
		//std::string stringNumber = location + ".png";
		//load PNG as a surface
		SDL_Surface* texture = IMG_Load(textureList[i].c_str());
		//pass the pixel data to the GPU
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->w, texture->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture->pixels);

		//set wrap modes
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		//set filtering modes
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	//save the scale tran and rot to the object
	tempObject.scale = { scale.x , scale.y , scale.z };
	tempObject.tran = { tran.x , tran.y , tran.z };
	tempObject.rot = { rot.x , rot.y , rot.z };
	//test textures for any transparency
	tempObject.transparency = transparency;
	//set it so we draw object
	tempObject.draw = draw;


	return tempObject;
}

Artist::object loadObjectFromFile(std::string fileLocation)
{
	std::vector<GLfloat> vertexPositions;
	std::vector <GLfloat> textureCoords;
	std::vector <GLuint> indices;
	std::vector<GLfloat> colors; // prolly just default to 1s same size as points
	std::vector<std::string> textureList; // add a way to get thru obj
	Artist::point scale = { 1.0f , 1.0f , 1.0f };//probably just set to normal
	Artist::point tran = { 0.0f , 0.0f , 0.0f }; //probably just set to normal
	Artist::point rot = { 0.0f , 0.0f , 0.0f };	 //probably just set to normal
	bool draw = true; //set true and can be changed later in runtime
	bool transparency = false; //make a check for transparency

	Artist::object tempObject;
	std::ifstream myFile;
	myFile.open("Resource/" + fileLocation + ".obj");
	if (myFile.is_open())
	{ 
		bool readingFile = true;
		bool checkingFace = true;
		std::string dataS;
		while (readingFile)
		{
			//get next line
			getline(myFile, dataS);
			//float for getting numbers from strings
			float dataF = 0;
			//create a string stream
			std::istringstream iss(dataS);
			//get the first chunk of the string
			std::string subS;
			iss >> subS;
			// list used for holding indices points
			std::vector<Artist::point> pointList;
			//get int value of chars from ASCII conversion
			int value = 0;
			for (int i = 0; i < subS.size(); i++)
			{
				value += int(subS[i]);
			}
			//decide what we need to do based on first chunk
			switch (value)
			{
			case 0:
				readingFile = false;
				break;
			case 35: //#
				break;
			case 102: //f
				//add indices
				//get the 4 points
				pointList.resize(4);
				for (int i = 0; i < pointList.size(); i++)
				{
					//break it down to the 3 dimentions
					iss >> subS;
					pointList[i].x = std::stoi(subS);
					//clear first number
					checkingFace = true;
					while (checkingFace)
					{
						if (subS[0] != '/')
						{
							subS.erase(0, 1);
						}
						else
						{
							checkingFace = false;
						}
					}
					subS.erase(0, 1);
					if (subS[0] != '/')
					{
						pointList[i].y = std::stoi(subS);
						//clear second number
						checkingFace = true;
						while (checkingFace)
						{
							if (subS[0] != '/')
							{
								subS.erase(0, 1);
							}
							else
							{
								checkingFace = false;
							}
						}
						subS.erase(0, 1);
					}
					else
					{
						pointList[i].y = 0;
						subS.erase(0, 1);
					}
				
					pointList[i].z = std::stoi(subS);
				}
				//ignore all but the first one for now
				//add tris to indice list   0 1 2   2 3 0
				indices.push_back(pointList[0].x - 1);
				indices.push_back(pointList[3].x - 1);
				indices.push_back(pointList[2].x - 1);

				indices.push_back(pointList[2].x - 1);
				indices.push_back(pointList[1].x - 1);
				indices.push_back(pointList[0].x - 1);

				break;
			case 111: //o
				break;
			case 115: //s
				break;
			case 118: //v
				//add verts to list
				iss >> subS;
				vertexPositions.push_back(std::stof(subS));
				iss >> subS;
				vertexPositions.push_back(std::stof(subS));
				iss >> subS;
				vertexPositions.push_back(std::stof(subS));
				break;
			case 228: //vn
				break;
			case 234: //vt
				//add texture cords to list
				iss >> subS;
				textureCoords.push_back(std::stof(subS));
				iss >> subS;
				textureCoords.push_back(std::stof(subS));
				break;
			case 644: //mtllib
				break;
			case 666: //usemtl
				break;
			}
		}
	}
	else
	{
		printf("Unable to open File\n");
	}

	// set colors
	int colorSize = vertexPositions.size() + vertexPositions.size() / 3;
	for (int i = 0; i < colorSize; i++)
	{
		colors.push_back(1.0);
	}

	//TEMP
	textureList = { "Resource/mag.png" };
	tran.z += 3;

	tempObject = constructObject(vertexPositions, textureCoords, indices, colors, textureList, scale, tran, rot, draw, transparency);

	return tempObject;
}

void TEMPloadObjects() //needs to be made into a funcion we can pass data into to make objects
{
	//create temp object to store data in
	//Artist::object tempObject;
	//this shouldnt really be here
	std::vector<GLfloat> verticesPosition = {
		-0.5, 0.5, -0.5,
		0.5, 0.5, -0.5,
		0.5, -0.5, -0.5,
		-0.5, -0.5, -0.5,

		-0.5, 0.5, 0.5,
		0.5, 0.5, 0.5,
		0.5, -0.5, 0.5,
		-0.5, -0.5, 0.5,

		//remake some verts so we can not have bad textures
		-0.5, 0.5, 0.5,
		-0.5, -0.5, 0.5,
		0.5, 0.5, 0.5,
		0.5, -0.5, 0.5,
	};
	//save the amount of vertecies
	//tempObject.vertexCount = verticesPosition.size();

	std::vector<GLfloat> verticesPositionB = {
		-0.25, 0.25, -0.25,
		0.25, 0.25, -0.25,
		0.25, -0.25, -0.25,
		-0.25, -0.25, -0.25,

		-0.5, 0.5, 0.5,
		0.5, 0.5, 0.5,
		0.5, -0.5, 0.5,
		-0.5, -0.5, 0.5,

		//remake some verts so we can not have bad textures
		-0.5, 0.5, 0.5,
		-0.5, -0.5, 0.5,
		0.5, 0.5, 0.5,
		0.5, -0.5, 0.5,
	};

	std::vector <GLfloat> textureCoord = {
		0.0, 0.0,
		1.0, 0.0,
		1.0, 1.0,
		0.0, 1.0,

		0.0, 1.0,
		1.0, 1.0,
		1.0, 0.0,
		0.0, 0.0,

		1.0, 0.0,
		1.0, 1.0,
		0.0, 0.0,
		0.0, 1.0,
	};

	std::vector <GLuint> indices = {
		3, 2, 1,
		1, 0, 3,

		4, 0, 5,
		5, 0, 1,

		4, 5, 6,
		6, 7, 4,

		6, 2, 7,
		7, 2, 3,

		0, 8, 9,
		9, 3, 0,

		2, 11, 10,
		10, 1, 2
	};

	std::vector<GLfloat> colors = {
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
		1.0, 1.0, 1.0, 1.0,
	};

	std::vector<std::string> textureList = {
		"Resource/dirt.png"
	};

	Artist::point scale = { 1.0f , 1.0f , 1.0f };
	Artist::point tran = { 0.0f , 0.0f , 1.0f };
	Artist::point rot = { 0.0f , 0.0f , 0.0f };

	objectMasterList.push_back(constructObject(verticesPosition, textureCoord, indices, colors, textureList, scale, tran, rot, true, false));
	//objectMasterList.push_back(constructObject(verticesPosition, textureCoord, indices, colors, textureList, scale, tran, rot, true, false));
	//objectMasterList.push_back(constructObject(verticesPosition, textureCoord, indices, colors, textureList, scale, tran, rot, true, false));
	//objectMasterList.push_back(constructObject(verticesPosition, textureCoord, indices, colors, textureList, scale, tran, rot, true, false));
	//
	//srand(clock());
	//
	//objectMasterList[0].tran.x = float(rand() % 200) / 100 - 1;
	//objectMasterList[0].tran.y = float(rand() % 200) / 100 - 1;
	//objectMasterList[0].tran.z = float(rand() % 200) / 100 + 1;
	//objectMasterList[1].tran.x = float(rand() % 200) / 100 - 1;
	//objectMasterList[1].tran.y = float(rand() % 200) / 100 - 1;
	//objectMasterList[1].tran.z = float(rand() % 200) / 100 + 1;
	//objectMasterList[2].tran.x = float(rand() % 200) / 100 - 1;
	//objectMasterList[2].tran.y = float(rand() % 200) / 100 - 1;
	//objectMasterList[2].tran.z = float(rand() % 200) / 100 + 1;
	//objectMasterList[3].tran.x = float(rand() % 200) / 100 - 1;
	//objectMasterList[3].tran.y = float(rand() % 200) / 100 - 1;
	//objectMasterList[3].tran.z = float(rand() % 200) / 100 + 1;

	//objectMasterList.push_back(loadObjectFromFile("cube"));
}

void initialize(GLuint &vao)
{
	// Use a Vertex Array Object
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	
	// Initialize the model, view and projection matrices
	for (int i = 0; i < 4; i++)
	{
		model.m[i][i] = 1;
		view.m[i][i] = 1;
		projection.m[i][i] = 1;
	}

	//TEMP
	view.m[3][0] = 0;//change X
	view.m[3][1] = 0;//change Y
	view.m[3][2] = 0;//change Z

	//projection matrix
	//frustum
	float angleOfView = 0.75f, aspectRatio = float(Artist::SCREEN_WIDTH) / float(Artist::SCREEN_HEIGHT), zNear = -1, zFar = 1;
	projection.m[0][0] = 1.0f / tanf(angleOfView);
	projection.m[1][1] = aspectRatio / tanf(angleOfView);
	projection.m[2][2] = (zFar + zNear) / (zFar - zNear);
	projection.m[3][2] = 1;
	projection.m[2][3] = -2.0f * zFar * zNear / (zFar - zNear);
	//ortho
	//float left = -16.0f / 9.0f, right = 16.0f / 9.0f, bottem = -1.0f, top = 1.0f, nearf = -1.0f, farf = 1.0f;
	//projection.m[0][0] = 2 / (right - left);
	//projection.m[1][1] = 2 / (top - bottem);
	//projection.m[2][2] = 2 / (farf - nearf);
	//projection.m[0][3] = -(right + left) / (right - left);
	//projection.m[1][3] = -(top + bottem) / (top - bottem);
	//projection.m[2][3] = -(farf + nearf) / (farf - nearf);
	
	//get a handle on the shader program
	shaderProgram = create_program("Shaders/vert.shader", "Shaders/frag.shader");
	
	// Get the location of the attributes that enters in the vertex shader
	GLint positionAttribute = glGetAttribLocation(shaderProgram, "position");
	// Specify how the data for position can be accessed
	//glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, 0, 0);
	// Enable the attribute
	glEnableVertexAttribArray(positionAttribute);
	
	// Color attribute TRY TO DO WITHOUT SOME OF THIS MIGHT ONLY BE NEEDED WHEN WE GO TO DRAW BUT MAYBE DOING ONCE IS WORTH
	GLint colorAttribute = glGetAttribLocation(shaderProgram, "color");
	//glVertexAttribPointer(colorAttribute, 4, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(verticesPosition.size() * sizeof(GLfloat)));//might be set to wrong offset
	//glVertexAttribPointer(colorAttribute, 4, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(36 * sizeof(GLfloat)));//might be set to wrong offset
	glEnableVertexAttribArray(colorAttribute);

	// Texture attribute
	GLint textureCoordAttribute = glGetAttribLocation(shaderProgram, "textureCoord");
	//glVertexAttribPointer(textureCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(verticesPosition.size() * sizeof(GLfloat)));//might be set to wrong offset
	//glVertexAttribPointer(textureCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(36 * sizeof(GLfloat)));//might be set to wrong offset
	glEnableVertexAttribArray(textureCoordAttribute);

	// Transfer the transformation matrices to the shader program
	modelUniform = glGetUniformLocation(shaderProgram, "Model");
	glUniformMatrix4fv(modelUniform, 1, GL_FALSE, &model.m[0][0]);
	
	GLint viewUniform = glGetUniformLocation(shaderProgram, "View");
	glUniformMatrix4fv(viewUniform, 1, GL_FALSE, &view.m[0][0]);
	
	GLint projectionUniform = glGetUniformLocation(shaderProgram, "Projection");
	glUniformMatrix4fv(projectionUniform, 1, GL_FALSE, &projection.m[0][0]);

	//Enable point size manipulation
	glEnable(GL_PROGRAM_POINT_SIZE);

	//Enable blending
	glEnable(GL_BLEND);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
	glBlendEquation(GL_FUNC_ADD);

	//Enable depth testing
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_GREATER);
	//disables depth writing, use for when we go to draw the transparent objects
	//glDepthMask(GL_FALSE);
	

	//Enable backface Culling (dissable when drawing transparent objects)
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glFrontFace(GL_CW);

	//Change polly mode
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	//Set clear color
	glClearColor(1.0, 0.0, 1.0, 1.0);

	//TEMP 
	TEMPloadObjects();

	//loadObjectFromFile("cube");
}



bool Artist::init()
{

	//Initialization flag
	bool success = true;

	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
		printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
		success = false;
	}
	else
	{
		//turn on double buffering
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);//may need to be 16 or 32
		

		//Set texture filtering to linear. NOT!!
		if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"))
		{
			printf("Warning: Linear texture filtering not enabled!");
		}

		//Create window
		gWindow = SDL_CreateWindow("Are we using OpenGL yet?", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Artist::SCREEN_WIDTH, Artist::SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
		Artist artist;
		SDL_Surface* icon = IMG_Load("Resource/icon.png");
		SDL_SetWindowIcon(gWindow, icon);
		SDL_FreeSurface(icon);
		if (gWindow == NULL)
		{
			printf("Window could not be created! SDL Error: %s\n", SDL_GetError());
			success = false;
		}
		else
		{
			
			//create opengl context and sttach to window
			GLContext = SDL_GL_CreateContext(gWindow);

			// Initialize GLEW
			glewExperimental = GL_TRUE;
			if (glewInit() != GLEW_OK) {
				std::cerr << "Failed to initialize GLEW! I'm out!" << std::endl;
				exit(-1);
			}

			//basicaly vsync maybe turn off?
			SDL_GL_SetSwapInterval(0);

			

			//Create renderer for window MAYBE DONT NEED THIS ANYMORE ONCE OPENGL IS SET UP
			gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
			if (gRenderer == NULL)
			{
				printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
				success = false;
			}
			else
			{
				//Initialize renderer color
				SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 0xFF);
				SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
				//Initialize PNG loading
				int imgFlags = IMG_INIT_PNG;
				if (!(IMG_Init(imgFlags) & imgFlags))
				{
					printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
					success = false;
				}

				//init the data to be rendered
				initialize(vao);

				//Initialize SDL_mixer
				if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
				{
					printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
					success = false;
				}
			}
		}
	}

	return success;
}

void loadFontTextures(Artist::fontTextureSet* fontToLoad, std::string fontName)
{
	Artist artist;
	fontToLoad->LowerA = artist.loadTexture("Resource/fonts/" + fontName + "/a.png");
	fontToLoad->LowerB = artist.loadTexture("Resource/fonts/" + fontName + "/b.png");
	fontToLoad->LowerC = artist.loadTexture("Resource/fonts/" + fontName + "/c.png");
	fontToLoad->LowerD = artist.loadTexture("Resource/fonts/" + fontName + "/d.png");
	fontToLoad->LowerE = artist.loadTexture("Resource/fonts/" + fontName + "/e.png");
	fontToLoad->LowerF = artist.loadTexture("Resource/fonts/" + fontName + "/f.png");
	fontToLoad->LowerG = artist.loadTexture("Resource/fonts/" + fontName + "/g.png");
	fontToLoad->LowerH = artist.loadTexture("Resource/fonts/" + fontName + "/h.png");
	fontToLoad->LowerI = artist.loadTexture("Resource/fonts/" + fontName + "/i.png");
	fontToLoad->LowerJ = artist.loadTexture("Resource/fonts/" + fontName + "/j.png");
	fontToLoad->LowerK = artist.loadTexture("Resource/fonts/" + fontName + "/k.png");
	fontToLoad->LowerL = artist.loadTexture("Resource/fonts/" + fontName + "/l.png");
	fontToLoad->LowerM = artist.loadTexture("Resource/fonts/" + fontName + "/m.png");
	fontToLoad->LowerN = artist.loadTexture("Resource/fonts/" + fontName + "/n.png");
	fontToLoad->LowerO = artist.loadTexture("Resource/fonts/" + fontName + "/o.png");
	fontToLoad->LowerP = artist.loadTexture("Resource/fonts/" + fontName + "/p.png");
	fontToLoad->LowerQ = artist.loadTexture("Resource/fonts/" + fontName + "/q.png");
	fontToLoad->LowerR = artist.loadTexture("Resource/fonts/" + fontName + "/r.png");
	fontToLoad->LowerS = artist.loadTexture("Resource/fonts/" + fontName + "/s.png");
	fontToLoad->LowerT = artist.loadTexture("Resource/fonts/" + fontName + "/t.png");
	fontToLoad->LowerU = artist.loadTexture("Resource/fonts/" + fontName + "/u.png");
	fontToLoad->LowerV = artist.loadTexture("Resource/fonts/" + fontName + "/v.png");
	fontToLoad->LowerW = artist.loadTexture("Resource/fonts/" + fontName + "/w.png");
	fontToLoad->LowerX = artist.loadTexture("Resource/fonts/" + fontName + "/x.png");
	fontToLoad->LowerY = artist.loadTexture("Resource/fonts/" + fontName + "/y.png");
	fontToLoad->LowerZ = artist.loadTexture("Resource/fonts/" + fontName + "/z.png");
	fontToLoad->CapA = artist.loadTexture("Resource/fonts/" + fontName + "/capA.png");
	fontToLoad->CapB = artist.loadTexture("Resource/fonts/" + fontName + "/capB.png");
	fontToLoad->CapC = artist.loadTexture("Resource/fonts/" + fontName + "/capC.png");
	fontToLoad->CapD = artist.loadTexture("Resource/fonts/" + fontName + "/capD.png");
	fontToLoad->CapE = artist.loadTexture("Resource/fonts/" + fontName + "/capE.png");
	fontToLoad->CapF = artist.loadTexture("Resource/fonts/" + fontName + "/capF.png");
	fontToLoad->CapG = artist.loadTexture("Resource/fonts/" + fontName + "/capG.png");
	fontToLoad->CapH = artist.loadTexture("Resource/fonts/" + fontName + "/capH.png");
	fontToLoad->CapI = artist.loadTexture("Resource/fonts/" + fontName + "/capI.png");
	fontToLoad->CapJ = artist.loadTexture("Resource/fonts/" + fontName + "/capJ.png");
	fontToLoad->CapK = artist.loadTexture("Resource/fonts/" + fontName + "/capK.png");
	fontToLoad->CapL = artist.loadTexture("Resource/fonts/" + fontName + "/capL.png");
	fontToLoad->CapM = artist.loadTexture("Resource/fonts/" + fontName + "/capM.png");
	fontToLoad->CapN = artist.loadTexture("Resource/fonts/" + fontName + "/capN.png");
	fontToLoad->CapO = artist.loadTexture("Resource/fonts/" + fontName + "/capO.png");
	fontToLoad->CapP = artist.loadTexture("Resource/fonts/" + fontName + "/capP.png");
	fontToLoad->CapQ = artist.loadTexture("Resource/fonts/" + fontName + "/capQ.png");
	fontToLoad->CapR = artist.loadTexture("Resource/fonts/" + fontName + "/capR.png");
	fontToLoad->CapS = artist.loadTexture("Resource/fonts/" + fontName + "/capS.png");
	fontToLoad->CapT = artist.loadTexture("Resource/fonts/" + fontName + "/capT.png");
	fontToLoad->CapU = artist.loadTexture("Resource/fonts/" + fontName + "/capU.png");
	fontToLoad->CapV = artist.loadTexture("Resource/fonts/" + fontName + "/capV.png");
	fontToLoad->CapW = artist.loadTexture("Resource/fonts/" + fontName + "/capW.png");
	fontToLoad->CapX = artist.loadTexture("Resource/fonts/" + fontName + "/capX.png");
	fontToLoad->CapY = artist.loadTexture("Resource/fonts/" + fontName + "/capY.png");
	fontToLoad->CapZ = artist.loadTexture("Resource/fonts/" + fontName + "/capZ.png");
	fontToLoad->num1 = artist.loadTexture("Resource/fonts/" + fontName + "/1.png");
	fontToLoad->num2 = artist.loadTexture("Resource/fonts/" + fontName + "/2.png");
	fontToLoad->num3 = artist.loadTexture("Resource/fonts/" + fontName + "/3.png");
	fontToLoad->num4 = artist.loadTexture("Resource/fonts/" + fontName + "/4.png");
	fontToLoad->num5 = artist.loadTexture("Resource/fonts/" + fontName + "/5.png");
	fontToLoad->num6 = artist.loadTexture("Resource/fonts/" + fontName + "/6.png");
	fontToLoad->num7 = artist.loadTexture("Resource/fonts/" + fontName + "/7.png");
	fontToLoad->num8 = artist.loadTexture("Resource/fonts/" + fontName + "/8.png");
	fontToLoad->num9 = artist.loadTexture("Resource/fonts/" + fontName + "/9.png");
	fontToLoad->num0 = artist.loadTexture("Resource/fonts/" + fontName + "/0.png");
	fontToLoad->Pound = artist.loadTexture("Resource/fonts/" + fontName + "/#.png");
	fontToLoad->$ = artist.loadTexture("Resource/fonts/" + fontName + "/$.png");
	fontToLoad->Percent = artist.loadTexture("Resource/fonts/" + fontName + "/%.png");
	fontToLoad->Ampersand = artist.loadTexture("Resource/fonts/" + fontName + "/&.png");
	fontToLoad->LeftParentheses = artist.loadTexture("Resource/fonts/" + fontName + "/(.png");
	fontToLoad->RightParentheses = artist.loadTexture("Resource/fonts/" + fontName + "/).png");
	fontToLoad->Comma = artist.loadTexture("Resource/fonts/" + fontName + "/,.png");
	fontToLoad->Apostrophe = artist.loadTexture("Resource/fonts/" + fontName + "/'.png");
	fontToLoad->Minus = artist.loadTexture("Resource/fonts/" + fontName + "/-.png");
	fontToLoad->SemiColon = artist.loadTexture("Resource/fonts/" + fontName + "/;.png");
	fontToLoad->Colon = artist.loadTexture("Resource/fonts/" + fontName + "/colon.png");
	fontToLoad->At = artist.loadTexture("Resource/fonts/" + fontName + "/@.png");
	fontToLoad->LeftBracket = artist.loadTexture("Resource/fonts/" + fontName + "/[.png");
	fontToLoad->RightBracket = artist.loadTexture("Resource/fonts/" + fontName + "/].png");
	fontToLoad->Caret = artist.loadTexture("Resource/fonts/" + fontName + "/^.png");
	fontToLoad->UnderScore = artist.loadTexture("Resource/fonts/" + fontName + "/_.png");
	fontToLoad->Tilde = artist.loadTexture("Resource/fonts/" + fontName + "/~.png");
	fontToLoad->Acute = artist.loadTexture("Resource/fonts/" + fontName + "/`.png");
	fontToLoad->LeftBrace = artist.loadTexture("Resource/fonts/" + fontName + "/{.png");
	fontToLoad->RightBrace = artist.loadTexture("Resource/fonts/" + fontName + "/}.png");
	fontToLoad->Plus = artist.loadTexture("Resource/fonts/" + fontName + "/+.png");
	fontToLoad->Equals = artist.loadTexture("Resource/fonts/" + fontName + "/=.png");
	fontToLoad->Asterisk = artist.loadTexture("Resource/fonts/" + fontName + "/asterisk.png");
	fontToLoad->BackSlash = artist.loadTexture("Resource/fonts/" + fontName + "/backSlash.png");
	fontToLoad->ForwardSlash = artist.loadTexture("Resource/fonts/" + fontName + "/forwardSlash.png");
	fontToLoad->ExclamationMark = artist.loadTexture("Resource/fonts/" + fontName + "/exclamationMark.png");
	fontToLoad->QuestionMark = artist.loadTexture("Resource/fonts/" + fontName + "/questionMark.png");
	fontToLoad->GreaterThan = artist.loadTexture("Resource/fonts/" + fontName + "/greaterThan.png");
	fontToLoad->LessThan = artist.loadTexture("Resource/fonts/" + fontName + "/lessThan.png");
	fontToLoad->Period = artist.loadTexture("Resource/fonts/" + fontName + "/period.png");
	fontToLoad->QuotationMark = artist.loadTexture("Resource/fonts/" + fontName + "/quotationMark.png");
	fontToLoad->VerticalBar = artist.loadTexture("Resource/fonts/" + fontName + "/verticalBar.png");
}

std::vector<SDL_Texture*> loadAnimationData(std::string textureFolderLocation, std::string textureSetName, int textureAmount)
{
	Artist artist;

	std::vector<SDL_Texture*> tempTexSet;
	for (int i = 1; i <= textureAmount; i++)
	{
		tempTexSet.push_back(artist.loadTexture("Resource/" + textureFolderLocation + "/" + textureSetName + "/" + textureSetName + std::to_string(i) + ".png"));
	}
	return tempTexSet;
}

bool Artist::loadMedia()
{
	Artist artist;
	//Loading success flag
	bool success = true;


	//Declare textures in a header -> define them in that headers respective file -> load em here
	//how to load a texture
	//Class::gTestTexture = loadTexture("Resource/testTexture.png");
	//how to load a animation
	//Class::gTestAnimation = loadAnimationData("testFiles (Parent folder path)", "testAnimation(Name of animation folder and images)", 2(number of images);

	//seperate by class for good organization
	//write load functions here \/

	
	
	//fonts
	loadFontTextures(&Artist::largeFont, "large");
	loadFontTextures(&Artist::smallFont, "small");


	return success;
}

void Artist::close()
{
	//Free loaded image
	//SDL_DestroyTexture(gBackground);
	//gBackground = NULL;






	//Destroy window	
	SDL_DestroyRenderer(gRenderer);
	SDL_DestroyWindow(gWindow);
	gWindow = NULL;
	gRenderer = NULL;

	//Quit SDL subsystems
	IMG_Quit();
	SDL_Quit();
}

void Artist::changeRenderColor(int r, int g, int b, int alpha = 255)
{
	SDL_SetRenderDrawColor(gRenderer, r, g, b, alpha);
}

void Artist::drawLineFromPoints(int sX, int sY, int eX, int eY)
{
	SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 0xFF);
	SDL_RenderDrawLine(gRenderer, sX, sY, eX, eY);
}

void Artist::drawRectangle(int x, int y, int w, int h)
{
	SDL_Rect tempRect;
	tempRect.x = x;
	tempRect.y = y;
	tempRect.w = w;
	tempRect.h = h;
	SDL_RenderFillRect(gRenderer, &tempRect);
}

void Artist::drawImage(SDL_Texture* tex, int x, int y, int w, int h, double r, SDL_Point* center, SDL_RendererFlip flip, bool scaled)
{
	SDL_Rect img;
	img.x = x * Artist::SCREEN_SCALE;
	img.y = y * Artist::SCREEN_SCALE;
	int drawW, drawH;
	if (w < 1)
	{
		SDL_QueryTexture(tex, NULL, NULL, &drawW, NULL);
	}
	else
	{
		drawW = w;
	}
	if (h < 1)
	{
		SDL_QueryTexture(tex, NULL, NULL, NULL, &drawH);
	}
	else
	{
		drawH = h;
	}
	if (scaled)
		img.w = drawW * Artist::SCREEN_SCALE;
	else
		img.w = drawW;
	if (scaled)
		img.h = drawH * Artist::SCREEN_SCALE;
	else
		img.h = drawH;
	SDL_RenderCopyEx(gRenderer, tex, NULL, &img, r, center, flip);
}

void Artist::drawAnimation(std::vector < SDL_Texture*> texSet, int x, int y, int frameTime)
{
	int frameToDraw = clock() / frameTime % texSet.size();
	//printf("FrameTime: %i, DisplayedFrame: %i \n", clock(), frameToDraw);
	drawImage(texSet[frameToDraw], x, y);
}

void Artist::drawLetters(std::string string, int x, int y, Artist::fontTextureSet font)
{
	std::string letterS;
	SDL_Texture* letterT = NULL;
	int count = 0;
	int offset = 0;
	//std::cout << string << std::endl;
	while (count < string.length())
	{
		letterS = string[count];
		if (letterS == "a")
		{
			letterT = font.LowerA;
		}
		else if (letterS == "b")
		{
			letterT = font.LowerB;
		}
		else if (letterS == "c")
		{
			letterT = font.LowerC;
		}
		else if (letterS == "d")
		{
			letterT = font.LowerD;
		}
		else if (letterS == "e")
		{
			letterT = font.LowerE;
		}
		else if (letterS == "f")
		{
			letterT = font.LowerF;
		}
		else if (letterS == "g")
		{
			letterT = font.LowerG;
		}
		else if (letterS == "h")
		{
			letterT = font.LowerH;
		}
		else if (letterS == "i")
		{
			letterT = font.LowerI;
		}
		else if (letterS == "j")
		{
			letterT = font.LowerJ;
		}
		else if (letterS == "k")
		{
			letterT = font.LowerK;
		}
		else if (letterS == "l")
		{
			letterT = font.LowerL;
		}
		else if (letterS == "m")
		{
			letterT = font.LowerM;
		}
		else if (letterS == "n")
		{
			letterT = font.LowerN;
		}
		else if (letterS == "o")
		{
			letterT = font.LowerO;
		}
		else if (letterS == "p")
		{
			letterT = font.LowerP;
		}
		else if (letterS == "q")
		{
			letterT = font.LowerQ;
		}
		else if (letterS == "r")
		{
			letterT = font.LowerR;
		}
		else if (letterS == "s")
		{
			letterT = font.LowerS;
		}
		else if (letterS == "t")
		{
			letterT = font.LowerT;
		}
		else if (letterS == "u")
		{
			letterT = font.LowerU;
		}
		else if (letterS == "v")
		{
			letterT = font.LowerV;
		}
		else if (letterS == "w")
		{
			letterT = font.LowerW;
		}
		else if (letterS == "x")
		{
			letterT = font.LowerX;
		}
		else if (letterS == "y")
		{
			letterT = font.LowerY;
		}
		else if (letterS == "z")
		{
			letterT = font.LowerZ;
		}
		else if (letterS == "A")
		{
			letterT = font.CapA;
		}
		else if (letterS == "B")
		{
			letterT = font.CapB;
		}
		else if (letterS == "C")
		{
			letterT = font.CapC;
		}
		else if (letterS == "D")
		{
			letterT = font.CapD;
		}
		else if (letterS == "E")
		{
			letterT = font.CapE;
		}
		else if (letterS == "F")
		{
			letterT = font.CapF;
		}
		else if (letterS == "G")
		{
			letterT = font.CapG;
		}
		else if (letterS == "H")
		{
			letterT = font.CapH;
		}
		else if (letterS == "I")
		{
			letterT = font.CapI;
		}
		else if (letterS == "J")
		{
			letterT = font.CapJ;
		}
		else if (letterS == "K")
		{
			letterT = font.CapK;
		}
		else if (letterS == "L")
		{
			letterT = font.CapL;
		}
		else if (letterS == "M")
		{
			letterT = font.CapM;
		}
		else if (letterS == "N")
		{
			letterT = font.CapN;
		}
		else if (letterS == "O")
		{
			letterT = font.CapO;
		}
		else if (letterS == "P")
		{
			letterT = font.CapP;
		}
		else if (letterS == "Q")
		{
			letterT = font.CapQ;
		}
		else if (letterS == "R")
		{
			letterT = font.CapR;
		}
		else if (letterS == "S")
		{
			letterT = font.CapS;
		}
		else if (letterS == "T")
		{
			letterT = font.CapT;
		}
		else if (letterS == "U")
		{
			letterT = font.CapU;
		}
		else if (letterS == "V")
		{
			letterT = font.CapV;
		}
		else if (letterS == "W")
		{
			letterT = font.CapW;
		}
		else if (letterS == "X")
		{
			letterT = font.CapX;
		}
		else if (letterS == "Y")
		{
			letterT = font.CapY;
		}
		else if (letterS == "Z")
		{
			letterT = font.CapZ;
		}
		else if (letterS == " ")
		{
			letterT = NULL;
		}
		else if (letterS == "1")
		{
			letterT = font.num1;
		}
		else if (letterS == "2")
		{
			letterT = font.num2;
		}
		else if (letterS == "3")
		{
			letterT = font.num3;
		}
		else if (letterS == "4")
		{
			letterT = font.num4;
		}
		else if (letterS == "5")
		{
			letterT = font.num5;
		}
		else if (letterS == "6")
		{
			letterT = font.num6;
		}
		else if (letterS == "7")
		{
			letterT = font.num7;
		}
		else if (letterS == "8")
		{
			letterT = font.num8;
		}
		else if (letterS == "9")
		{
			letterT = font.num9;
		}
		else if (letterS == "0")
		{
			letterT = font.num0;
		}
		else if (letterS == "`")
		{
			letterT = font.Acute;
		}
		else if (letterS == "~")
		{
			letterT = font.Tilde;
		}
		else if (letterS == "!")
		{
			letterT = font.ExclamationMark;
		}
		else if (letterS == "?")
		{
			letterT = font.QuestionMark;
		}
		else if (letterS == "#")
		{
			letterT = font.Pound;
		}
		else if (letterS == "$")
		{
			letterT = font.$;
		}
		else if (letterS == "%")
		{
			letterT = font.Percent;
		}
		else if (letterS == "^")
		{
			letterT = font.Caret;
		}
		else if (letterS == "&")
		{
			letterT = font.Ampersand;
		}
		else if (letterS == "*")
		{
			letterT = font.Asterisk;
		}
		else if (letterS == "(")
		{
			letterT = font.LeftParentheses;
		}
		else if (letterS == ")")
		{
			letterT = font.RightParentheses;
		}
		else if (letterS == "-")
		{
			letterT = font.Minus;
		}
		else if (letterS == "_")
		{
			letterT = font.UnderScore;
		}
		else if (letterS == "=")
		{
			letterT = font.Equals;
		}
		else if (letterS == "+")
		{
			letterT = font.Plus;
		}
		else if (letterS == "[")
		{
			letterT = font.LeftBracket;
		}
		else if (letterS == "]")
		{
			letterT = font.RightBracket;
		}
		else if (letterS == "{")
		{
			letterT = font.LeftBrace;
		}
		else if (letterS == "}")
		{
			letterT = font.RightBrace;
		}
		else if (letterS == "\\")
		{
			letterT = font.BackSlash;
		}
		else if (letterS == "|")
		{
			letterT = font.VerticalBar;
		}
		else if (letterS == ";")
		{
			letterT = font.SemiColon;
		}
		else if (letterS == ":")
		{
			letterT = font.Colon;
		}
		else if (letterS == "'")
		{
			letterT = font.Apostrophe;
		}
		else if (letterS == "\"")
		{
			letterT = font.QuotationMark;
		}
		else if (letterS == ",")
		{
			letterT = font.Comma;
		}
		else if (letterS == "<")
		{
			letterT = font.LessThan;
		}
		else if (letterS == ".")
		{
			letterT = font.Period;
		}
		else if (letterS == ">")
		{
			letterT = font.GreaterThan;
		}
		else if (letterS == "/")
		{
			letterT = font.ForwardSlash;
		}
		else if (letterS == "@")
		{
			letterT = font.At;
		}
		else
		{
			letterT = font.QuestionMark;
		}
		int w, h;
		//Make it sized based on width but adding the count and TO the count each time
		SDL_QueryTexture(letterT, NULL, NULL, &w, &h);
		drawImage(letterT, offset + x, y, w, h);
		offset += w;
		//std::cout << count << std::endl;
		//std::cout << letterS << std::endl;
		count++;
	}


}









Artist::mat4x4 matMult(Artist::mat4x4 matA, Artist::mat4x4 matB)
{
	Artist::mat4x4 returnMat;

	for (int r = 0; r < 4; r++)
	{
		for (int c = 0; c < 4; c++)
		{
			returnMat.m[r][c] = matA.m[r][0] * matB.m[0][c] + matA.m[r][1] * matB.m[1][c] + matA.m[r][2] * matB.m[2][c] + matA.m[r][3] * matB.m[3][c];
		}
	}

	return returnMat;
}

Artist::mat4x4 modelManip(float scaleX, float scaleY, float scaleZ, float tranX, float tranY, float tranZ, float rotX, float rotY, float rotZ)
{
	//individual matricies used for multiplication
	Artist::mat4x4 scaleMat;
	Artist::mat4x4 transMat;
	Artist::mat4x4 rotXMat;
	Artist::mat4x4 rotYMat;
	Artist::mat4x4 rotZMat;
	//make identity maxtrix
	for (int i = 0; i < 4; i++)
	{
		scaleMat.m[i][i] = 1;
		transMat.m[i][i] = 1;
		rotXMat.m[i][i] = 1;
		rotYMat.m[i][i] = 1;
		rotZMat.m[i][i] = 1;
	}

	//scaling
	scaleMat.m[0][0] = scaleX;
	scaleMat.m[1][1] = scaleY;
	scaleMat.m[2][2] = scaleZ;
	//translation
	transMat.m[3][0] = tranX;
	transMat.m[3][1] = tranY;
	transMat.m[3][2] = tranZ;
	//rotation x
	rotXMat.m[1][1] = cosf(rotX);
	rotXMat.m[2][1] = -sinf(rotX);
	rotXMat.m[1][2] = sinf(rotX);
	rotXMat.m[2][2] = cosf(rotX);
	//y
	rotYMat.m[0][0] = cosf(rotY);
	rotYMat.m[2][0] = sinf(rotY);
	rotYMat.m[0][2] = -sinf(rotY);
	rotYMat.m[2][2] = cosf(rotY);
	//z
	rotZMat.m[0][0] = cosf(rotZ);
	rotZMat.m[1][0] = -sinf(rotZ);
	rotZMat.m[0][1] = sinf(rotZ);
	rotZMat.m[1][1] = cosf(rotZ);

	//combine scale, all rotation, and translation as the model
	return matMult(matMult(scaleMat, matMult(rotZMat, matMult(rotXMat, rotYMat))), transMat);
}

std::vector<Artist::object> sortByDepth(std::vector<Artist::object> objectlList)//maybe make this a pointer to it
{
	//sort objects by depth
	std::vector<Artist::object> objectListSorted;
	for (int i = 0; i < objectlList.size(); i++)
	{
		bool spotFound = false;
		if (objectListSorted.size() == 0)
		{
			objectListSorted.push_back(objectlList[i]);
		}
		else
		{
			for (int j = 0; j < objectListSorted.size(); j++)
			{
				//calculate depth
				float depthA = sqrt(pow(view.m[3][0] - objectlList[i].tran.x, 2) + pow(view.m[3][1] - objectlList[i].tran.y, 2) + pow(view.m[3][2] - objectlList[i].tran.z, 2));
				float depthB = sqrt(pow(view.m[3][0] - objectListSorted[j].tran.x, 2) + pow(view.m[3][1] - objectListSorted[j].tran.y, 2) + pow(view.m[3][2] - objectListSorted[j].tran.z, 2));
				if (depthA > depthB)
				{
					objectListSorted.insert(objectListSorted.begin() + j, objectlList[i]);
					j = objectListSorted.size() + 1;
					spotFound = true;
				}
			}
			if (!spotFound)
			{
				objectListSorted.push_back(objectlList[i]);
			}
		}
	}
	return objectListSorted;
}

void renderObjects()
{
	float deltaTimeScaler = deltaTime * 0.006f;//REMOVE ONCE ITS IN CONTROLLER AND JUST USE THAT ONE
	if (deltaTime > 33.3333333333f)
	{
		deltaTimeScaler = 0.0333333333f;
	}

	//TEMP
	objectMasterList[0].rot.x += Controller::spin.x * deltaTimeScaler;
	objectMasterList[0].rot.y += Controller::spin.y * deltaTimeScaler;
	objectMasterList[0].rot.z += Controller::spin.z * deltaTimeScaler;
	//objectMasterList[0].rot.x += -0.03f * deltaTimeScaler;
	//objectMasterList[0].rot.y += -0.02f * deltaTimeScaler;
	//objectMasterList[0].rot.z += -0.01f * deltaTimeScaler;
	//objectMasterList[1].rot.x += -0.3f * deltaTimeScaler;
	//objectMasterList[1].rot.y += -0.2f * deltaTimeScaler;
	//objectMasterList[1].rot.z += -0.1f * deltaTimeScaler;

	//get the ID of the different attributes MAYBE MOVE
	GLint viewUniform = glGetUniformLocation(shaderProgram, "View");
	GLint positionAttribute = glGetAttribLocation(shaderProgram, "position");
	GLint textureCoordAttribute = glGetAttribLocation(shaderProgram, "textureCoord");
	GLint colorAttribute = glGetAttribLocation(shaderProgram, "color");

	//object lists to be used for filtering out transparent objects the sorting them by distance
	std::vector<Artist::object> objectListTrans;
	std::vector<Artist::object> objectListSorted;

	//update our view
	glUniformMatrix4fv(viewUniform, 1, GL_FALSE, &view.m[0][0]);

	//draw all objects filtering out any transparent ones into a new list
	for (int i = 0; i < objectMasterList.size(); i++)
	{
		
		if (!objectMasterList[i].transparency)
		{
			//create model
			Artist::mat4x4 model = modelManip(objectMasterList[i].scale.x, objectMasterList[i].scale.y, objectMasterList[i].scale.z, objectMasterList[i].tran.x, objectMasterList[i].tran.y, objectMasterList[i].tran.z, objectMasterList[i].rot.x, objectMasterList[i].rot.y, objectMasterList[i].rot.z);

			//bind all the different buffers/textures
			glBindTexture(GL_TEXTURE_2D, objectMasterList[i].textures[0]);//add way to pick texture
			glBindBuffer(GL_ARRAY_BUFFER, objectMasterList[i].VBOID);
			//ADD EABs might need add bottem part aswell
			//ADD COLOR
			//add more things we can change





			//push all the changes we made
			//pass new vertex pos to the GPU
			glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, 0, 0);
			// Texture attribute
			glVertexAttribPointer(textureCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(objectMasterList[i].vertexCount * sizeof(GLfloat)));
			// Color attribute
			glVertexAttribPointer(colorAttribute, 4, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(objectMasterList[i].vertexCount * sizeof(GLfloat) + (objectMasterList[i].vertexCount - (objectMasterList[i].vertexCount / 3)) * sizeof(GLfloat)));
			//pass new model matrix to the GPU
			glUniformMatrix4fv(modelUniform, 1, GL_FALSE, &model.m[0][0]);
			//actualy draw the shits
			glDrawElements(GL_TRIANGLES, objectMasterList[i].vertexCount, GL_UNSIGNED_INT, 0);
		}
		else
		{
			objectListTrans.push_back(objectMasterList[i]);
		}
	}

	//sort the transparent objects into farthest first
	objectListSorted = sortByDepth(objectListTrans);

	//change the peramiters of rendering to suit transparent objects
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);

	//draw all the transparent objects
	for (int i = 0; i < objectListSorted.size(); i++)
	{
		//create model
		Artist::mat4x4 model = modelManip(objectListSorted[i].scale.x, objectListSorted[i].scale.y, objectListSorted[i].scale.z, objectListSorted[i].tran.x, objectListSorted[i].tran.y, objectListSorted[i].tran.z, objectListSorted[i].rot.x, objectListSorted[i].rot.y, objectListSorted[i].rot.z);

		//bind all the different buffers/textures
		glBindTexture(GL_TEXTURE_2D, objectListSorted[i].textures[0]);//add way to pick texture
		glBindBuffer(GL_ARRAY_BUFFER, objectListSorted[i].VBOID);
		//add more things we can change





		//push all the changes we made
		//update our view
		glUniformMatrix4fv(viewUniform, 1, GL_FALSE, &view.m[0][0]);//move somewhere where we only do it once per frame
		//pass new vertex pos to the GPU
		glVertexAttribPointer(positionAttribute, 3, GL_FLOAT, GL_FALSE, 0, 0);
		// Texture attribute
		glVertexAttribPointer(textureCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(objectListSorted[i].vertexCount * sizeof(GLfloat)));
		// Color attribute
		glVertexAttribPointer(colorAttribute, 4, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(objectListSorted[i].vertexCount * sizeof(GLfloat) + (objectListSorted[i].vertexCount - (objectListSorted[i].vertexCount / 3)) * sizeof(GLfloat)));
		//pass new model matrix to the GPU
		glUniformMatrix4fv(modelUniform, 1, GL_FALSE, &model.m[0][0]);
		//actualy draw the shits
		glDrawElements(GL_TRIANGLES, objectListSorted[i].vertexCount, GL_UNSIGNED_INT, 0);
	}

	//change the peramiters of rendering to suit opaque objects
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

void Artist::draw()
{
	Artist artist;
	FPSCounter fpscounter;
	

	//TEMP
	
	//clear buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearDepth(0.0f);

	//calculate fps
	float fps = 1000.0f / deltaTime;
	//get time elapsed
	LAST = NOW;
	NOW = SDL_GetPerformanceCounter();

	//get a scaler so movment is based on time not framerate
	deltaTime = (double)((NOW - LAST) * 1000 / (double)SDL_GetPerformanceFrequency()); //MAKE GLOBAL OR SOMETTHING SO I CAN BE USED ALL OVER
	float deltaTimeScaler = deltaTime * 0.006f;
	if (deltaTime > 33.3333333333f)
	{
		deltaTimeScaler = 0.0333333333f;
	}
	

	
	//render all the objects on the master list
	renderObjects();
	
	
	

	/* Swap our back buffer to the front */
	SDL_GL_SwapWindow(gWindow);
	
	

	
	

	
	//FPS counter code PROLLY REMOVE
	if (Artist::displayFPS)
	{
		fpscounter.updateFPS();

		artist.drawLetters(std::to_string(fpscounter.getFPS()), 0, 1, Artist::smallFont);
	}
}
