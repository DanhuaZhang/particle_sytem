//CSCI 5611 OpenGL Animation Tutorial 
//A 1D bouncing ball

//Running on Mac OSX
//  Download the SDL2 Framework from here: https://www.libsdl.org/download-2.0.php
//  Open the .dmg and move the file SDL2.Framework into the directory /Library/Frameworks/
//  Make sure you place this cpp file in the same directory with the "glad" folder and the "glm" folder
//  g++ Bounce.cpp glad/glad.c -framework OpenGL -framework SDL2; ./a.out

//Running on Windows
//  Download the SDL2 *Development Libararies* from here: https://www.libsdl.org/download-2.0.php
//  Place SDL2.dll, the 3 .lib files, and the include directory in locations known to MSVC
//  Add both Bounce.cpp and glad/glad.c to the project file
//  Compile and run

//Running on Ubuntu
//  sudo apt-get install libsdl2-2.0-0 libsdl2-dev
//  Make sure you place this cpp file in the same directory with the "glad" folder and the "glm" folder
//  g++ Bounce.cpp glad/glad.c -lGL -lSDL; ./a.out

#include "glad/glad.h"  //Include order can matter here
#ifndef _WIN32
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#else
#include <SDL.h>
#include <SDL_opengl.h>
#endif
#include <cstdio>
#include <cmath>
#include <stdlib.h>
#include <time.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define PARTICLE_NUM 1500
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/rotate_vector.hpp"

#include <fstream>
using namespace std;

bool saveOutput = false; //Make to true to save out your animation
int screen_width = 800;
int screen_height = 600;

//changed 02/03/2018
vector<glm::vec3>position;
vector<glm::vec3>velocity;
vector<glm::vec3>color;
vector<float>lifespan;
float radius = 0.02;
float floorPos = -1.2;

// Shader sources
const GLchar* vertexSource =
"#version 150 core\n"
"in vec3 position;"
//"in vec3 inColor;"
//the color will be changed in the OpenGL part
//change the number of particles
"uniform vec3 inColor;"
"in vec3 inNormal;"
"const vec3 inLightDir = normalize(vec3(0,2,2));"
"out vec3 Color;"
"out vec3 normal;"
"out vec3 lightDir;"
"uniform mat4 model;"
"uniform mat4 view;"
"uniform mat4 proj;"
"void main() {"
"   Color = inColor;"
"   gl_Position = proj * view * model * vec4(position, 1.0);"
"   vec4 norm4 = transpose(inverse(model)) * vec4(inNormal, 1.0);"
"   normal = normalize(norm4.xyz);"
"   lightDir = (view * vec4(inLightDir, 0)).xyz;"
"}";

const GLchar* fragmentSource =
"#version 150 core\n"
"in vec3 Color;"
"in vec3 normal;"
"in vec3 lightDir;"
"out vec4 outColor;"
"const float ambient = .2;"
"void main() {"
"   vec3 diffuseC = Color * max(dot(lightDir, normal), 0);"
"   vec3 ambC = Color * ambient;"
// the alpha channel of all pixels are 1.0, i.e. the opacity is 100%
"   outColor = vec4(diffuseC+ambC, 1.0);"
"}";

bool fullscreen = false;
void Win2PPM(int width, int height);
void computePhysics(int i, float dt);
bool IsInHemisphere(glm::vec3 point, glm::vec3 center, float radius);
bool IsUnderCone(glm::vec3 point, float radius, float height);
float randf();
bool DEBUG_ON = true;
GLuint InitShader(const char* vShaderFileName, const char* fShaderFileName);

//Index of where to model, view, and projection matricies are stored on the GPU
GLint uniModel, uniView, uniProj, uniColor;

float aspect; //aspect ratio (needs to be updated if the window is resized)

int main(int argc, char *argv[]) {
	SDL_Init(SDL_INIT_VIDEO);  //Initialize Graphics (for OpenGL)

	//Ask SDL to get a recent version of OpenGL (3.2 or greater)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);

	//Create a window (offsetx, offsety, width, height, flags)
	SDL_Window* window = SDL_CreateWindow("My OpenGL Program", 150, 50, screen_width, screen_height, SDL_WINDOW_OPENGL);
	aspect = screen_width / (float)screen_height; //aspect ratio (needs to be updated if the window is resized)

	//The above window cannot be resized which makes some code slightly easier.
	//Below show how to make a full screen window or allow resizing
	//SDL_Window* window = SDL_CreateWindow("My OpenGL Program", 0, 0, screen_width, screen_height, SDL_WINDOW_FULLSCREEN|SDL_WINDOW_OPENGL);
	//SDL_Window* window = SDL_CreateWindow("My OpenGL Program", 100, 100, screen_width, screen_height, SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL);
	//SDL_Window* window = SDL_CreateWindow("My OpenGL Program",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,0,0,SDL_WINDOW_FULLSCREEN_DESKTOP|SDL_WINDOW_OPENGL); //Boarderless window "fake" full screen

	//Create a context to draw in
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (gladLoadGLLoader(SDL_GL_GetProcAddress)) {
		printf("\nOpenGL loaded\n");
		printf("Vendor:   %s\n", glGetString(GL_VENDOR));
		printf("Renderer: %s\n", glGetString(GL_RENDERER));
		printf("Version:  %s\n\n", glGetString(GL_VERSION));
	}
	else {
		printf("ERROR: Failed to initialize OpenGL context.\n");
		return -1;
	}

	//Build a Vertex Array Object. This stores the VBO and attribute mappings in one object
	GLuint vao;
	glGenVertexArrays(1, &vao); //Create a VAO
	glBindVertexArray(vao); //Bind the above created VAO to the current context

	ifstream modelFile;
	modelFile.open("sphere.txt");
	int numLines = 0;
	modelFile >> numLines;
	float *vertices = new float[numLines];
	for (int i = 0; i < numLines; i++) {
		modelFile >> vertices[i];
	}
	printf("wall numLines: %d\n", numLines);
	int numVerts = numLines / 8;
	modelFile.close();

	//Allocate memory on the graphics card to store geometry (vertex buffer object)
	GLuint vbo;
	glGenBuffers(1, &vbo);  //Create 1 buffer called vbo
	glBindBuffer(GL_ARRAY_BUFFER, vbo); //Set the vbo as the active array buffer (Only one buffer can be active at a time)
	glBufferData(GL_ARRAY_BUFFER, numLines * sizeof(float), vertices, GL_STATIC_DRAW); //upload vertices to vbo
	//GL_STATIC_DRAW means we won't change the geometry, GL_DYNAMIC_DRAW = geometry changes infrequently
	//GL_STREAM_DRAW = geom. changes frequently.  This effects which types of GPU memory is used

	//Load the vertex Shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);

	//Let's double check the shader compiled 
	GLint status;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char buffer[512];
		glGetShaderInfoLog(vertexShader, 512, NULL, buffer);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
			"Compilation Error",
			"Failed to Compile: Check Consol Output.",
			NULL);
		printf("Vertex Shader Compile Failed. Info:\n\n%s\n", buffer);
	}

	//Load the fragment Shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);

	//Double check the shader compiled 
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);
	if (!status) {
		char buffer[512];
		glGetShaderInfoLog(fragmentShader, 512, NULL, buffer);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
			"Compilation Error",
			"Failed to Compile: Check Consol Output.",
			NULL);
		printf("Fragment Shader Compile Failed. Info:\n\n%s\n", buffer);
	}

	//Join the vertex and fragment shaders together into one program
	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glBindFragDataLocation(shaderProgram, 0, "outColor"); // set output
	glLinkProgram(shaderProgram); //run the linker

	glUseProgram(shaderProgram); //Set the active shader (only one can be used at a time)

	//Tell OpenGL how to set fragment shader input 
	GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
	glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
	//Attribute, vals/attrib., type, normalized?, stride, offset
	//Binds to VBO current GL_ARRAY_BUFFER 
	glEnableVertexAttribArray(posAttrib);

	/*GLint colAttrib = glGetAttribLocation(shaderProgram, "inColor");
	glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(colAttrib);*/

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	GLint normAttrib = glGetAttribLocation(shaderProgram, "inNormal");
	glVertexAttribPointer(normAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
	glEnableVertexAttribArray(normAttrib);

	glBindVertexArray(0); //Unbind the VAO

	//=================
	//|| load candle ||
	//=================

	//Build a Vertex Array Object. This stores the VBO and attribute mappings in one object
	GLuint vao1;
	glGenVertexArrays(1, &vao1); //Create a VAO
	glBindVertexArray(vao1); //Bind the above created VAO to the current context

	modelFile.open("candle.txt");
	numLines = 0;
	modelFile >> numLines;
	float *vertices_candle = new float[numLines];
	for (int i = 0; i < numLines; i++) {
		modelFile >> vertices_candle[i];
	}
	printf("wall numLines: %d\n", numLines);
	int numVerts_candle = numLines / 8;
	modelFile.close();

	//Allocate memory on the graphics card to store geometry (vertex buffer object)
	GLuint vbo1;
	glGenBuffers(1, &vbo1);  //Create 1 buffer called vbo
	glBindBuffer(GL_ARRAY_BUFFER, vbo1); //Set the vbo as the active array buffer (Only one buffer can be active at a time)
	glBufferData(GL_ARRAY_BUFFER, numLines * sizeof(float), vertices_candle, GL_STATIC_DRAW); //upload vertices_candle to vbo															   

	//Load the vertex Shader
	GLuint vertexShader1 = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader1, 1, &vertexSource, NULL);
	glCompileShader(vertexShader1);

	//Let's double check the shader compiled 
	GLint status1;
	glGetShaderiv(vertexShader1, GL_COMPILE_STATUS, &status1);
	if (!status) {
		char buffer[512];
		glGetShaderInfoLog(vertexShader, 512, NULL, buffer);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
			"Compilation Error",
			"Failed to Compile: Check Consol Output.",
			NULL);
		printf("Vertex Shader Compile Failed. Info:\n\n%s\n", buffer);
	}

	//Load the fragment Shader
	GLuint fragmentShader1 = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader1, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader1);

	//Double check the shader compiled 
	glGetShaderiv(fragmentShader1, GL_COMPILE_STATUS, &status1);
	if (!status) {
		char buffer[512];
		glGetShaderInfoLog(fragmentShader, 512, NULL, buffer);
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
			"Compilation Error",
			"Failed to Compile: Check Consol Output.",
			NULL);
		printf("Fragment Shader Compile Failed. Info:\n\n%s\n", buffer);
	}

	//Join the vertex and fragment shaders together into one program
	GLuint shaderProgram1 = glCreateProgram();
	glAttachShader(shaderProgram1, vertexShader1);
	glAttachShader(shaderProgram1, fragmentShader1);
	glBindFragDataLocation(shaderProgram1, 0, "outColor"); // set output
	glLinkProgram(shaderProgram1); //run the linker

	glUseProgram(shaderProgram1); //Set the active shader (only one can be used at a time)

								 //Tell OpenGL how to set fragment shader input 
	posAttrib = glGetAttribLocation(shaderProgram1, "position");
	glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), 0);
	//Attribute, vals/attrib., type, normalized?, stride, offset
	//Binds to VBO current GL_ARRAY_BUFFER 
	glEnableVertexAttribArray(posAttrib);

	/*GLint colAttrib = glGetAttribLocation(shaderProgram, "inColor");
	glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(colAttrib);*/

	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
	normAttrib = glGetAttribLocation(shaderProgram1, "inNormal");
	glVertexAttribPointer(normAttrib, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(5 * sizeof(float)));
	glEnableVertexAttribArray(normAttrib);

	glBindVertexArray(0); //Unbind the VAO


	//Where to model, view, and projection matricies are stored on the GPU
	uniModel = glGetUniformLocation(shaderProgram, "model");
	uniView = glGetUniformLocation(shaderProgram, "view");
	uniProj = glGetUniformLocation(shaderProgram, "proj");
	uniColor = glGetUniformLocation(shaderProgram, "inColor");

	glEnable(GL_DEPTH_TEST);
	//modified 02/04/2018
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//Event Loop (Loop forever processing each event as fast as possible)
	SDL_Event windowEvent;
	bool quit = false;

	srand(time(NULL));

	//particle system start here
	//generate the emitter shape
	//in this project, disk is used for the shape
	float shape_radius = 0.25f;

	float lastTime = SDL_GetTicks() / 1000.f;
	float dt = 0;
	glm::vec3 ini_position = glm::vec3(0.0f, 0.0f, 0.0f);
	float maxLifeSpan = 1.0f;

	//set attributes for flames on a candle
	//used a cone and a semisphere to simulate the shape of the flame
	//outside: red, sphere center is (0,0,r3)
	float r3 = shape_radius;
	glm::vec3 c3 = glm::vec3(0.0f, 0.0f, r3);
	float h3 = 1.0f;
	//core: white, sphere center is (0,0,r1)
	float r1 = 0.16f;
	glm::vec3 c1 = glm::vec3(0.0f, 0.0f, r1-r3+0.05);
	//median: yellow, sphere center is (0,0,r2)
	float r2 = 0.2f;
	glm::vec3 c2 = glm::vec3(0.0f, 0.0f, r2-r3);

	//set parameters for camera
	float movestep = 0.1;
	float anglestep = 0.1;
	glm::vec3 camera_position = glm::vec3(3.f, 0.f, 0.f);  //Cam Position
	glm::vec3 look_point = glm::vec3(0.0f, 0.0f, 0.0f);  //Look at point
	glm::vec3 up_vector = glm::vec3(0.0f, 0.0f, 1.0f); //Up
	glm::vec3 move_vector = glm::vec3(0.0f, 0.0f, 0.0f);

	while (!quit) {
		while (SDL_PollEvent(&windowEvent)) {
			if (windowEvent.type == SDL_QUIT) quit = true; //Exit event loop
		  //List of keycodes: https://wiki.libsdl.org/SDL_Keycode - You can catch many special keys
		  //Scancode referes to a keyboard position, keycode referes to the letter (e.g., EU keyboards)
			if (windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_ESCAPE)
				quit = true; ; //Exit event loop
			if (windowEvent.type == SDL_KEYUP && windowEvent.key.keysym.sym == SDLK_f) //If "f" is pressed
				fullscreen = !fullscreen;
			SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN : 0); //Set to full screen

			if ((windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_UP) || \
				(windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_w)) {
				//move up
				move_vector = glm::normalize(look_point - camera_position)*movestep;
				camera_position = camera_position + move_vector;
				look_point = look_point + move_vector;
			}
			if ((windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_DOWN) || \
				(windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_s)) {
				//move down
				move_vector = glm::normalize(look_point - camera_position)*movestep;
				camera_position = camera_position - move_vector;
				look_point = look_point - move_vector;
			}
			if ((windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_LEFT) || \
				(windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_a)) {
				//move left
				move_vector = glm::rotateZ(look_point - camera_position, anglestep);
				//move_vector = glm::normalize(move_vector);
				look_point = camera_position + move_vector;
			}
			if ((windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_RIGHT) || \
				(windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_d)) {
				//move right
				move_vector = glm::rotateZ(look_point - camera_position, -anglestep);
				look_point = camera_position + move_vector;
			}
			//if ((windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_LEFT) || \
			//	(windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_a)) {
			//	//move left
			//	move_vector = glm::normalize(look_point - camera_position);
			//	move_vector = glm::cross(up_vector, move_vector)*movestep;
			//	camera_position = camera_position + move_vector;
			//	look_point = look_point + move_vector;
			//}
			//if ((windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_RIGHT) || \
			//	(windowEvent.type == SDL_KEYDOWN && windowEvent.key.keysym.sym == SDLK_d)) {
			//	//move right
			//	move_vector = glm::normalize(look_point - camera_position);
			//	move_vector = glm::cross(up_vector, move_vector)*movestep;
			//	camera_position = camera_position - move_vector;
			//	look_point = look_point - move_vector;
			//}
			//if (windowEvent.type == SDL_MOUSEWHEEL && windowEvent.wheel.y == 1) {// scroll up
			//	//turn left
			//	move_vector = glm::rotateZ(look_point - camera_position, anglestep);
			//	look_point = camera_position + move_vector;
			//}
			//if (windowEvent.type == SDL_MOUSEWHEEL && windowEvent.wheel.y == -1) {// scroll up
			//	//turn right
			//	move_vector = glm::rotateZ(look_point - camera_position, anglestep);
			//	look_point = camera_position - move_vector;
			//}
		}

		// Clear the screen to default color
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (!saveOutput) dt = (SDL_GetTicks() / 1000.f) - lastTime;
		if (dt > .1) dt = .1; //Have some max dt
		lastTime = SDL_GetTicks() / 1000.f;
		if (saveOutput) dt += .07; //Fix framerate at 14 FPS

		glm::mat4 view = glm::lookAt(camera_position, look_point, up_vector);
		GLint uniView = glGetUniformLocation(shaderProgram, "view");
		glUniformMatrix4fv(uniView, 1, GL_FALSE, glm::value_ptr(view));

		glm::mat4 proj = glm::perspective(3.14f / 4, aspect, 1.0f, 10.0f); //FOV, aspect, near, far
		GLint uniProj = glGetUniformLocation(shaderProgram, "proj");
		glUniformMatrix4fv(uniProj, 1, GL_FALSE, glm::value_ptr(proj));

		//draw candle
		glm::mat4 candle = glm::translate(candle, glm::vec3(0.0f,0.0f,-1.0f));
		//candle = glm::scale(candle, glm::vec3(0.5));
		glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(candle));
		glBindVertexArray(vao1);
		glDrawArrays(GL_TRIANGLES, 0, numVerts_candle); //(Primitives, Which VBO, Number of vertices)
		glBindVertexArray(0);


		//Partical birthrate
		float numParticles = PARTICLE_NUM * dt;
		float fracPart = numParticles - int(numParticles);
		numParticles = int(numParticles);

		//modified 02/03/2018
		if (randf() < fracPart) {//randf() here creates random numbers from 0 to 1
			numParticles += 1;
		}

		//generate new particles
		for (int i = 0; i < numParticles; i++) {
			//choose random particle location
			float x = -shape_radius + randf() * shape_radius * 2;
			float y = -shape_radius + randf() * shape_radius * 2;
			position.push_back(glm::vec3(x, y, -sqrt(shape_radius *shape_radius -x*x-y*y)));
			//choose random particle velocity
			velocity.push_back(glm::vec3(0.0f, 0.0f, randf()));
			color.push_back(glm::vec3(1.0f, 0.0f, 0.0f));
			lifespan.push_back(maxLifeSpan);
		}

		for (int i = 0; i < position.size(); i++) {
			lifespan[i] -= dt;
			if (IsInHemisphere(position[i],c1,r1)) {
				color[i] = glm::vec3(1.0f, 1.0f, randf());
			}
			else if (IsInHemisphere(position[i]+radius, c2, r2)) {
				color[i] = glm::vec3(1.0f, randf(), 0.0f);
			}
			
			if(!IsUnderCone(position[i],r3,h3)){
				position.erase(position.begin() + i);
				velocity.erase(velocity.begin() + i);
				color.erase(color.begin() + i);
				lifespan.erase(lifespan.begin() + i);
				continue;
			}
			glm::vec3 inColor = color[i];
			glUniform3f(uniColor, inColor.r, inColor.g, inColor.b);

			glm::mat4 model(1.0f);
			if (lifespan[i] <= 0) {// draw the "alive" particles
				position.erase(position.begin() + i);
				velocity.erase(velocity.begin() + i);
				color.erase(color.begin() + i);
				lifespan.erase(lifespan.begin() + i);
			}
			model = glm::translate(model, position[i]);
			model = glm::scale(model, glm::vec3(radius));
			glUniformMatrix4fv(uniModel, 1, GL_FALSE, glm::value_ptr(model));
			
			computePhysics(i, dt);

			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLES, 0, numVerts); //(Primitives, Which VBO, Number of vertices)
		}


		
		if (saveOutput) Win2PPM(screen_width, screen_height);

		SDL_GL_SwapWindow(window); //Double buffering
	}

	//Clean Up
	glDeleteProgram(shaderProgram);
	glDeleteShader(fragmentShader);
	glDeleteShader(vertexShader);

	glDeleteBuffers(1, &vbo);

	glDeleteVertexArrays(1, &vao);

	SDL_GL_DeleteContext(context);
	SDL_Quit();
	return 0;
}

float randf() {
	return (float)(rand() % 1001) * 0.001f;
}

bool IsInHemisphere(glm::vec3 point, glm::vec3 center, float radius) {
	if (((point.x - center.x)*(point.x - center.x) + (point.y - center.y)*(point.y - center.y) + (point.z - center.z)*(point.z - center.z) < (radius*radius)) && (point.z < radius)) {
		return true;
	}
	else {
		return false;
	}
}

bool IsUnderCone(glm::vec3 point, float radius, float height) {
	if ((point.x*point.x + point.y*point.y) < ((radius*radius/height/height)*(point.z-height)*(point.z-height))\
		&& (point.z<height)) {
		return true;
	}
	else {
		return false;
	}
}

void computePhysics(int i, float dt) {
	//glm::vec3 acceleration = glm::vec3(0.0f,0.0f,-10.0f);//-9.8;
	//velocity[i] = velocity[i] + acceleration * dt;
	position[i] = position[i] + velocity[i] * dt;
	//printf("pos: (%.2f,%.2f,%.2f)\nvel: (%.2f,%.2f,%.2f)\n", position.x, position.y, position.z, velocity.x, velocity.y, velocity.z);
	if ((position[i].z + radius) > 3.0) {
		/*position[i].z = 3.0 + radius;
		velocity[i].z *= -.95;*/
		position.erase(position.begin() + i);
		velocity.erase(velocity.begin() + i);
		color.erase(color.begin() + i);
		lifespan.erase(lifespan.begin() + i);
	}
}

//// Create a GLSL program object from vertex and fragment shader files
//GLuint InitShader(const char* vShaderFileName, const char* fShaderFileName) {
//	GLuint vertex_shader, fragment_shader;
//	GLchar *vs_text, *fs_text;
//	GLuint program;
//
//	// check GLSL version
//	printf("GLSL version: %s\n\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
//
//	// Create shader handlers
//	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
//	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
//
//	// Read source code from shader files
//	vs_text = readShaderSource(vShaderFileName);
//	fs_text = readShaderSource(fShaderFileName);
//
//	// error check
//	if (vs_text == NULL) {
//		printf("Failed to read from vertex shader file %s\n", vShaderFileName);
//		exit(1);
//	}
//	else if (DEBUG_ON) {
//		printf("Vertex Shader:\n=====================\n");
//		printf("%s\n", vs_text);
//		printf("=====================\n\n");
//	}
//	if (fs_text == NULL) {
//		printf("Failed to read from fragent shader file %s\n", fShaderFileName);
//		exit(1);
//	}
//	else if (DEBUG_ON) {
//		printf("\nFragment Shader:\n=====================\n");
//		printf("%s\n", fs_text);
//		printf("=====================\n\n");
//	}
//
//	// Load Vertex Shader
//	const char *vv = vs_text;
//	glShaderSource(vertex_shader, 1, &vv, NULL);  //Read source
//	glCompileShader(vertex_shader); // Compile shaders
//
//									// Check for errors
//	GLint  compiled;
//	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compiled);
//	if (!compiled) {
//		printf("Vertex shader failed to compile:\n");
//		if (DEBUG_ON) {
//			GLint logMaxSize, logLength;
//			glGetShaderiv(vertex_shader, GL_INFO_LOG_LENGTH, &logMaxSize);
//			printf("printing error message of %d bytes\n", logMaxSize);
//			char* logMsg = new char[logMaxSize];
//			glGetShaderInfoLog(vertex_shader, logMaxSize, &logLength, logMsg);
//			printf("%d bytes retrieved\n", logLength);
//			printf("error message: %s\n", logMsg);
//			delete[] logMsg;
//		}
//		exit(1);
//	}
//
//	// Load Fragment Shader
//	const char *ff = fs_text;
//	glShaderSource(fragment_shader, 1, &ff, NULL);
//	glCompileShader(fragment_shader);
//	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compiled);
//
//	//Check for Errors
//	if (!compiled) {
//		printf("Fragment shader failed to compile\n");
//		if (DEBUG_ON) {
//			GLint logMaxSize, logLength;
//			glGetShaderiv(fragment_shader, GL_INFO_LOG_LENGTH, &logMaxSize);
//			printf("printing error message of %d bytes\n", logMaxSize);
//			char* logMsg = new char[logMaxSize];
//			glGetShaderInfoLog(fragment_shader, logMaxSize, &logLength, logMsg);
//			printf("%d bytes retrieved\n", logLength);
//			printf("error message: %s\n", logMsg);
//			delete[] logMsg;
//		}
//		exit(1);
//	}
//
//	// Create the program
//	program = glCreateProgram();
//
//	// Attach shaders to program
//	glAttachShader(program, vertex_shader);
//	glAttachShader(program, fragment_shader);
//
//	// Link and set program to use
//	glLinkProgram(program);
//
//	return program;
//}

void Win2PPM(int width, int height) {
	char outdir[10] = "out/"; //Must be defined!
	int i, j;
	FILE* fptr;
	static int counter = 0;
	char fname[32];
	unsigned char *image;

	/* Allocate our buffer for the image */
	image = (unsigned char *)malloc(3 * width*height * sizeof(char));
	if (image == NULL) {
		fprintf(stderr, "ERROR: Failed to allocate memory for image\n");
	}

	/* Open the file */
	sprintf(fname, "%simage_%04d.ppm", outdir, counter);
	if ((fptr = fopen(fname, "w")) == NULL) {
		fprintf(stderr, "ERROR: Failed to open file for window capture\n");
	}

	/* Copy the image into our buffer */
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, image);

	/* Write the PPM file */
	fprintf(fptr, "P6\n%d %d\n255\n", width, height);
	for (j = height - 1; j >= 0; j--) {
		for (i = 0; i < width; i++) {
			fputc(image[3 * j*width + 3 * i + 0], fptr);
			fputc(image[3 * j*width + 3 * i + 1], fptr);
			fputc(image[3 * j*width + 3 * i + 2], fptr);
		}
	}

	free(image);
	fclose(fptr);
	counter++;
}