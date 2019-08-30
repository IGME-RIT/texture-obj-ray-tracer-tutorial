/*
Title: Basic Ray Tracer
File Name: Main.cpp
Copyright � 2015, 2019
Original authors: Brockton Roth, Niko Procopi
Written under the supervision of David I. Schwartz, Ph.D., and
supported by a professional development seed grant from the B. Thomas
Golisano College of Computing & Information Sciences
(https://www.rit.edu/gccis) at the Rochester Institute of Technology.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or (at
your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <windows.h>

#include "GL/glew.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "FreeImage.h"

#define MAX_LIGHTS 2
#define MAX_MESHES 3
#define MAX_TRIANGLES_PER_MESH 1500 //1268 in car
#define NUM_TRIANGLES_IN_SCENE 1282 // 2 + 12 + 1268

struct triangle {
	glm::vec4 pos[3];
	glm::vec4 uv[3];
	glm::vec4 normal[3];
	glm::vec4 color;
};

struct Mesh
{
	int numTriangles;
	int junk1;
	int junk2;
	int junk3;
	triangle triangles[MAX_TRIANGLES_PER_MESH];
};

struct light {
	glm::vec4 pos;
	glm::vec4 color;
	float radius;
	float brightness;
	float junk1;
	float junk2;
};

GLuint compToFrag;
int compToFragSize = sizeof(triangle) * NUM_TRIANGLES_IN_SCENE;

GLuint lightToFrag;
int lightToFragSize = sizeof(light) * MAX_LIGHTS;

GLuint matrixBuffer;
int matrixBufferSize = sizeof(glm::mat4x4) * MAX_MESHES;

GLuint triangleBuffer;
int triangleBufferSize = sizeof(Mesh) * MAX_MESHES;

// This is your reference to your shader program.
// This will be assigned with glCreateProgram().
// This program will run on your GPU.
GLuint draw_program;
GLuint transform_program;

// These are your references to your actual compiled shaders
GLuint vertex_shader;
GLuint fragment_shader;
GLuint compute_shader;

// These are your uniform variables.
GLuint eye_loc;		// Specifies where cameraPos is in the GLSL shader
// The rays are the four corner rays of the camera view. See: https://camo.githubusercontent.com/21a84a8b21d6a4bc98b9992e8eaeb7d7acb1185d/687474703a2f2f63646e2e6c776a676c2e6f72672f7475746f7269616c732f3134313230385f676c736c5f636f6d707574652f726179696e746572706f6c6174696f6e2e706e67
GLuint ray00;
GLuint ray01;
GLuint ray10;
GLuint ray11;

// texture information
GLuint tex_loc;
GLuint m_texture;

// A variable used to describe the position of the camera.
glm::vec3 cameraPos;

// A reference to our window.
GLFWwindow* window;

// Variables you will need to calculate FPS.
int tempFrame = 0;
int totalFrame = 0;
double dtime = 0.0;
double timebase = 0.0;
double totalTime = 0.0;
int fps = 0;

int width = 1920;
int height = 1080;
int videoFPS = 60;
int videoSeconds = 13;
int maxFrames = videoFPS * videoSeconds;

// This function takes in variables that define the perspective view of the camera, then outputs the four corner rays of the camera's view.
// It takes in a vec3 eye, which is the position of the camera.
// It also takes vec3 center, the position the camera's view is centered on.
// Then it will takes a vec3 up which is a vector that defines the upward direction. (So if you point it down, the camera view will be upside down.)
// Then it takes a float defining the verticle field of view angle. It also takes a float defining the ratio of the screen (in this case, 800/600 pixels).
// The last four parameters are actually just variables for this function to output data into. They should be pointers to pre-defined vec4 variables.
// For a visual reference, see this image: https://camo.githubusercontent.com/21a84a8b21d6a4bc98b9992e8eaeb7d7acb1185d/687474703a2f2f63646e2e6c776a676c2e6f72672f7475746f7269616c732f3134313230385f676c736c5f636f6d707574652f726179696e746572706f6c6174696f6e2e706e67
void calcCameraRays(glm::vec3 eye, glm::vec3 center, glm::vec3 up, float fov, float ratio)
{
	// Grab a ray from the camera position toward where the camera is to be centered on.
	glm::vec3 centerRay = center - eye;

	// w: Vector from center toward eye
	// u: Vector pointing directly right relative to the camera.
	// v: Vector pointing directly upward relative to the camera.

	// Create a w vector which is the opposite of that ray.
	glm::vec3 w = -centerRay;

	// Get the rightward (relative to camera) pointing vector by crossing up with w.
	glm::vec3 u = glm::cross(up, w);

	// Get the upward (relative to camera) pointing vector by crossing the rightward vector with your w vector.
	glm::vec3 v = glm::cross(w, u);

	// We create these two helper variables, as when we rotate the ray about it's relative Y axis (v), we will then need to rotate it about it's relative X axis (u).
	// This means that u has to be rotated by v too, otherwise the rotation will not be accurate. When the ray is rotated about v, so then are it's relative axes.
	glm::vec4 uRotateLeft = glm::vec4(u, 1.0f) * glm::rotate(glm::mat4(), glm::radians(-fov * ratio / 2.0f), v);
	glm::vec4 uRotateRight = glm::vec4(u, 1.0f) * glm::rotate(glm::mat4(), glm::radians(fov * ratio / 2.0f), v);

	// Now we simply take the ray and rotate it in each direction to create our four corner rays.
	glm::vec4 r00 = glm::vec4(centerRay, 1.0f) * glm::rotate(glm::mat4(), glm::radians(-fov * ratio / 2.0f), v) * glm::rotate(glm::mat4(), glm::radians(fov / 2.0f), glm::vec3(uRotateLeft));
	glm::vec4 r01 = glm::vec4(centerRay, 1.0f) * glm::rotate(glm::mat4(), glm::radians(-fov * ratio / 2.0f), v) * glm::rotate(glm::mat4(), glm::radians(-fov / 2.0f), glm::vec3(uRotateLeft));
	glm::vec4 r10 = glm::vec4(centerRay, 1.0f) * glm::rotate(glm::mat4(), glm::radians(fov * ratio / 2.0f), v) * glm::rotate(glm::mat4(), glm::radians(fov / 2.0f), glm::vec3(uRotateRight));
	glm::vec4 r11 = glm::vec4(centerRay, 1.0f) * glm::rotate(glm::mat4(), glm::radians(fov * ratio / 2.0f), v) * glm::rotate(glm::mat4(), glm::radians(-fov / 2.0f), glm::vec3(uRotateRight));

	// Now set the uniform variables in the shader to match our camera variables (cameraPos = eye, then four corner rays)
	glUniform3f(eye_loc, eye.x, eye.y, eye.z);
	glUniform3f(ray00, r00.x, r00.y, r00.z);
	glUniform3f(ray01, r01.x, r01.y, r01.z);
	glUniform3f(ray10, r10.x, r10.y, r10.z);
	glUniform3f(ray11, r11.x, r11.y, r11.z);
}

// This function runs every frame
void renderScene()
{
	// Used for FPS
	dtime = glfwGetTime();
	totalTime = dtime;

	// Every second, basically.
	if (dtime - timebase > 1)
	{
		// Calculate the FPS and set the window title to display it.
		fps = tempFrame / (int)(dtime - timebase);
		timebase = dtime;
		tempFrame = 0;

		std::string s = 
			"FPS: " + std::to_string(fps) + 
			" Frame: " + std::to_string(totalFrame) + 
			" / " + std::to_string(maxFrames);

		glfwSetWindowTitle(window, s.c_str());
	}

	// set camera position
	cameraPos = glm::vec3(
		0.0f,
		8.0f,
		8.0f
	);

	// There are two different ways of animating. We can 
	// animate with respect to the time elapsed in the program, or we can
	// animate with respect to the time elapsed in the video. 
	
	// If we want to test our animations, without waiting for the full 
	// video to render: lower the resolution, disable reflections, then we 
	// can render a real-time animation with totalTimeElapsedInProgram. This 
	// works, even if the computer renders less than 60 frames per second

	// After we have finished testing our animations, we can enable all of
	// our quality settings (reflections, resolution, etc), and then change
	// the elapsed time to totalTimeElapsedInVideo, and then all animations
	// should look correct in the final video.

	// Sometimes while prototyping, the real-time render will have a longer
	// duration than the actual rendered video, keep that in mind while testing

	float totalTimeElapsedInVideo = (float)totalFrame / videoFPS;
	float totalTimeElapsedInProgram = (float)totalTime;

	// choose which one you want here
	float time = totalTimeElapsedInVideo;

	//=================================================================

	// start using transform program
	glUseProgram(transform_program);

	glm::mat4x4 test[MAX_MESHES];
	
	// scale the floor
	test[0] = glm::mat4();
	test[0] = glm::translate(test[0],glm::vec3(0, -0.5, 0));
	test[0] = glm::scale(test[0], glm::vec3((sin(time) + 6.0f)) / 3.0f);

	// move and rotate the cube
	test[1] = glm::mat4();
	test[1] = glm::translate(test[1], glm::vec3(5 * cos(time), 1.5, 5 * sin(time)));
	test[1] = glm::rotate(test[1], -time, glm::vec3(0, 1, 0));
	test[1] = glm::scale(test[1], glm::vec3((2 + sin(time - 3.14159f))));

	test[2] = glm::mat4();
	test[2] = glm::rotate(test[2], time / 2, glm::vec3(0, 1, 0));

	glBindBuffer(GL_UNIFORM_BUFFER, matrixBuffer);
	glBufferData(GL_UNIFORM_BUFFER, matrixBufferSize, test, GL_DYNAMIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, compToFrag);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangleBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, matrixBuffer);
	glDispatchCompute(NUM_TRIANGLES_IN_SCENE, 1, 1);

	//=================================================================

	// start using draw program
	glUseProgram(draw_program);

	light lights[MAX_LIGHTS];

	// white light
	lights[0].color = glm::vec4(1.0, 1.0, 1.0, 0.0);
	lights[0].radius = 7;
	lights[0].brightness = 1;

	lights[0].pos = glm::vec4(
		2 * sin(time),
		4,
		2 * cos(time),
		0
	);

	// red light
	lights[1].color = glm::vec4(1.0, 0.0, 0.0, 0.0);
	lights[1].radius = 4;
	lights[1].brightness = 6;

	lights[1].pos = glm::vec4(
		4 * cos(time),
		1,
		4,
		0
	);

	glBindBuffer(GL_UNIFORM_BUFFER, lightToFrag); // 'lights' is a pointer
	glBufferData(GL_UNIFORM_BUFFER, lightToFragSize, lights, GL_DYNAMIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, compToFrag);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, lightToFrag);

	// Call the function we created to calculate the corner rays.
	// We use the camera position, the focus position, and the up direction (just like glm::lookAt)
	// We use Field of View, and aspect ratio (just like glm::perspective)
	calcCameraRays(cameraPos, glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 45.0f, (float)width / height);

	// Activate Texture
	glActiveTexture(GL_TEXTURE0 + m_texture);

	glBindTexture(GL_TEXTURE_2D, m_texture);

	// Assign uniform to texture
	glUniform1i(tex_loc, m_texture);

	// Draw an image on the screen
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// help us keep track of FPS
	tempFrame++;
	totalFrame++;
}

// This method reads the text from a file.
// Realistically, we wouldn't want plain text shaders hardcoded in, we'd rather read them in from a separate file so that the shader code is separated.
std::string readShader(std::string fileName)
{
	std::string shaderCode;
	std::string line;

	// We choose ifstream and std::ios::in because we are opening the file for input into our program.
	// If we were writing to the file, we would use ofstream and std::ios::out.
	std::ifstream file(fileName, std::ios::binary);

	// This checks to make sure that we didn't encounter any errors when getting the file.
	if (!file.good())
	{
		std::cout << "Can't read file: " << fileName.data() << std::endl;

		// Return so we don't error out.
		return "";
	}

	// Get size of file
	file.seekg(0, std::ios::end);					
	shaderCode.resize((unsigned int)file.tellg());	
	file.seekg(0, std::ios::beg);					

	// Dump the file into our array
	file.read(&shaderCode[0], shaderCode.size());

	// close the file
	file.close();

	return shaderCode;
}

// This method will consolidate some of the shader code we've written to return a GLuint to the compiled shader.
// It only requires the shader source code and the shader type.
GLuint createShader(std::string sourceCode, GLenum shaderType)
{
	// glCreateShader, creates a shader given a type (such as GL_VERTEX_SHADER) and returns a GLuint reference to that shader.
	GLuint shader = glCreateShader(shaderType);
	const char *shader_code_ptr = sourceCode.c_str(); // We establish a pointer to our shader code string
	const int shader_code_size = sourceCode.size();   // And we get the size of that string.

	// glShaderSource replaces the source code in a shader object
	// It takes the reference to the shader (a GLuint), a count of the number of elements in the string array (in case you're passing in multiple strings), a pointer to the string array
	// that contains your source code, and a size variable determining the length of the array.
	glShaderSource(shader, 1, &shader_code_ptr, &shader_code_size);
	glCompileShader(shader); // This just compiles the shader, given the source code.

	GLint isCompiled = 0;

	// Check the compile status to see if the shader compiled correctly.
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);

	if (isCompiled == GL_FALSE)
	{
		char infolog[1024];
		glGetShaderInfoLog(shader, 1024, NULL, infolog);

		// Print the compile error.
		std::cout << "The shader failed to compile with the error:" << std::endl << infolog << std::endl;

		// Provide the infolog in whatever manor you deem best.
		// Exit with failure.
		glDeleteShader(shader); // Don't leak the shader.

		// NOTE: I almost always put a break point here, so that instead of the program continuing with a deleted/failed shader, it stops and gives me a chance to look at what may
		// have gone wrong. You can check the console output to see what the error was, and usually that will point you in the right direction.
	}

	return shader;
}

void loadOBJ(Mesh* m)
{
	// Part 1
	// Initialize variables and pointers

	const char* path = "../Assets/GreenCar14.3Dobj";
	FILE *f = fopen(path, "r");
	//delete path;

	float x[3];
	unsigned short y[9];

	std::vector<float>pos;
	std::vector<float>uvs;
	std::vector<float>norms;
	std::vector<unsigned short>faces;
	char line[100];


	// Part 2
	// Fill vectors for pos, uvs, norms, and faces

	while (fgets(line, sizeof(line), f))
	{
		if (sscanf(line, "v %f %f %f", &x[0], &x[1], &x[2]) == 3)
			for (int i = 0; i < 3; i++)
				pos.push_back(x[i]);

		if (sscanf(line, "vt %f %f", &x[0], &x[1]) == 2)
			for (int i = 0; i < 2; i++)
				uvs.push_back(x[i]);

		if (sscanf(line, "vn %f %f %f", &x[0], &x[1], &x[2]) == 3)
			for (int i = 0; i < 3; i++)
				norms.push_back(x[i]);

		if (sscanf(line, "f %hu/%hu/%hu %hu/%hu/%hu %hu/%hu/%hu", &y[0], &y[1], &y[2], &y[3], &y[4], &y[5], &y[6], &y[7], &y[8]) == 9)
			for (int i = 0; i < 9; i++)
				faces.push_back(y[i] - 1);
	}


	// Part 3
	// Initialize more variables and pointers

	int numVerts = 3 * (int)faces.size() / 9;

	m->numTriangles = numVerts / 3;

	// Part 4
	// Build final Vertex Buffer

	// for every triangle
	for (int i = 0; i < (int)numVerts / 3; i++)
	{
		// for every point
		for (int j = 0; j < 3; j++)
		{
			// X, Y, and Z
			for (int k = 0; k < 3; k++)
			{
				int coordIndex = 3 * faces[9 * i + 3 * j + 0] + k;
				m->triangles[i].pos[j][k] = pos[coordIndex];
			}

			for (int k = 0; k < 2; k++)
			{
				int uvIndex = 2 * faces[9 * i + 3 * j + 1] + k;
				m->triangles[i].uv[j][k] = uvs[uvIndex];
			}

			for (int k = 0; k < 3; k++)
			{
				int normalIndex = 3 * faces[9 * i + 3 * j + 2] + k;
				m->triangles[i].normal[j][k] = norms[normalIndex];
			}
		}

		m->triangles[i].color = glm::vec4(0.2, 1.0, 0.2, 1.0);
	}

	fclose(f);

}

// Initialization code
void init()
{
	glewExperimental = GL_TRUE;
	// Initializes the glew library
	glewInit();

	// Read in the shader code from a file.
	std::string vertShader = readShader("../Assets/VertexShader.glsl");
	std::string fragShader = readShader("../Assets/FragmentShader.glsl");
	std::string compShader = readShader("../Assets/Compute.glsl");

	// createShader consolidates all of the shader compilation code
	vertex_shader = createShader(vertShader, GL_VERTEX_SHADER);
	fragment_shader = createShader(fragShader, GL_FRAGMENT_SHADER);
	compute_shader = createShader(compShader, GL_COMPUTE_SHADER);

	// A shader is a program that runs on your GPU instead of your CPU. In this sense, OpenGL refers to your groups of shaders as "programs".
	// Using glCreateProgram creates a shader program and returns a GLuint reference to it.
	draw_program = glCreateProgram();
	glAttachShader(draw_program, vertex_shader);		// This attaches our vertex shader to our program.
	glAttachShader(draw_program, fragment_shader);	// This attaches our fragment shader to our program.
	glLinkProgram(draw_program);					// Link the program
	// End of shader and program creation

	// Tell our code to use the program
	glUseProgram(draw_program);

	// This gets us a reference to the uniform variables in the vertex shader, which are called by the same name here as in the shader.
	// We're using these variables to define the camera. The eye is the camera position, and teh rays are the four corner rays of what the camera sees.
	// Only 2 parameters required: A reference to the shader program and the name of the uniform variable within the shader code.
	eye_loc = glGetUniformLocation(draw_program, "eye");
	ray00 = glGetUniformLocation(draw_program, "ray00");
	ray01 = glGetUniformLocation(draw_program, "ray01");
	ray10 = glGetUniformLocation(draw_program, "ray10");
	ray11 = glGetUniformLocation(draw_program, "ray11");
	tex_loc = glGetUniformLocation(draw_program, "textureTest");

	glEnable(GL_TEXTURE_2D);

	// Load Texture ========================================
	
	//char* file = (char*)"../Assets/CarColor.png";
	char* file = (char*)"../Assets/texture.jpg";

	// Load the file.
	FIBITMAP* bitmap = FreeImage_Load(FreeImage_GetFileType(file), file);
	// Convert the file to 32 bits so we can use it.
	FIBITMAP* bitmap32 = FreeImage_ConvertTo32Bits(bitmap);

	// Create an OpenGL texture.
	glGenTextures(1, &m_texture);

	// Bind our texture.
	glBindTexture(GL_TEXTURE_2D, m_texture);

	// Fill our openGL side texture object.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FreeImage_GetWidth(bitmap32), FreeImage_GetHeight(bitmap32),
		0, GL_BGRA, GL_UNSIGNED_BYTE, static_cast<void*>(FreeImage_GetBits(bitmap32)));

	// Set texture sampling parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Unbind the texture.
	glBindTexture(GL_TEXTURE_2D, 0);

	// We can unload the images now that the texture data has been buffered with opengl
	FreeImage_Unload(bitmap);
	FreeImage_Unload(bitmap32);

	// =====================================================

	transform_program = glCreateProgram();
	glAttachShader(transform_program, compute_shader);
	glLinkProgram(transform_program);					// Link the program
	// End of shader and program creation

	// Make a buffer for our particle data.
	glGenBuffers(1, &compToFrag);
	glBindBuffer(GL_UNIFORM_BUFFER, compToFrag);
	glBufferData(GL_UNIFORM_BUFFER, compToFragSize, nullptr, GL_STATIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glGenBuffers(1, &matrixBuffer);
	glBindBuffer(GL_UNIFORM_BUFFER, matrixBuffer);
	glBufferData(GL_UNIFORM_BUFFER, matrixBufferSize, nullptr, GL_DYNAMIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	Mesh meshes[MAX_MESHES];

	meshes[0].numTriangles = 2;
	meshes[0].triangles[0].pos[0] = glm::vec4(-5.0, 0.0, 5.0, 1.0); 
	meshes[0].triangles[0].pos[1] = glm::vec4(-5.0, 0.0, -5.0, 1.0);
	meshes[0].triangles[0].pos[2] = glm::vec4(5.0, 0.0, -5.0, 1.0);
	meshes[0].triangles[0].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[0].triangles[0].uv[1] = glm::vec4(0, 0, 1, 1);
	meshes[0].triangles[0].uv[2] = glm::vec4(1, 0, 1, 1);
	meshes[0].triangles[0].normal[0] = glm::vec4(0.0, 1.0, 0.0, 1.0); 
	meshes[0].triangles[0].color = glm::vec4(1.0, 1.0, 1.0, 1.0);

	meshes[0].triangles[1].pos[0] = glm::vec4(-5.0, 0.0, 5.0, 1.0);
	meshes[0].triangles[1].pos[1] = glm::vec4(5.0, 0.0, -5.0, 1.0);
	meshes[0].triangles[1].pos[2] = glm::vec4(5.0, 0.0, 5.0, 1.0);
	meshes[0].triangles[1].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[0].triangles[1].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[0].triangles[1].uv[2] = glm::vec4(1, 1, 1, 1);
	meshes[0].triangles[1].normal[0] = glm::vec4(0.0, 1.0, 0.0, 1.0);
	meshes[0].triangles[1].color = glm::vec4(1.0, 1.0, 1.0, 1.0);

	// Mesh 0 is a plane
	// It should have one normal per triangle
	// dulicate the first normal we give it
	for (int i = 0; i < meshes[0].numTriangles; i++)
	{
		meshes[0].triangles[i].normal[1] = meshes[0].triangles[i].normal[0];
		meshes[0].triangles[i].normal[2] = meshes[0].triangles[i].normal[0];
	}

	meshes[1].numTriangles = 12;
	meshes[1].triangles[0].pos[0] = glm::vec4(-0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[0].pos[1] = glm::vec4(0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[0].pos[2] = glm::vec4(-0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[0].uv[0] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[0].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[0].uv[2] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[0].normal[0] = glm::vec4(0.0, 0.0, -1.0, 1.0);
	meshes[1].triangles[0].color = glm::vec4(1.0, 0.5, 0.1, 1.0);
		   
	meshes[1].triangles[1].pos[0] = glm::vec4(0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[1].pos[1] = glm::vec4(0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[1].pos[2] = glm::vec4(-0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[1].uv[0] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[1].uv[1] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[1].uv[2] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[1].normal[0] = glm::vec4(0.0, 0.0, -1.0, 1.0);
	meshes[1].triangles[1].color = glm::vec4(1.0, 0.5, 0.1, 1.0);
		   
	meshes[1].triangles[2].pos[0] = glm::vec4(-0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[2].pos[1] = glm::vec4(-0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[2].pos[2] = glm::vec4(0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[2].uv[0] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[2].uv[1] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[2].uv[2] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[2].normal[0] = glm::vec4(0.0, 0.0, 1.0, 1.0);
	meshes[1].triangles[2].color = glm::vec4(1.0, 0.5, 0.1, 1.0);
		   
	meshes[1].triangles[3].pos[0] = glm::vec4(-0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[3].pos[1] = glm::vec4(0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[3].pos[2] = glm::vec4(0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[3].uv[0] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[3].uv[1] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[3].uv[2] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[3].normal[0] = glm::vec4(0.0, 0.0, 1.0, 1.0);
	meshes[1].triangles[3].color = glm::vec4(1.0, 0.5, 0.1, 1.0);
		   
	meshes[1].triangles[4].pos[0] = glm::vec4(0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[4].pos[1] = glm::vec4(0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[4].pos[2] = glm::vec4(0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[4].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[4].uv[1] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[4].uv[2] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[4].normal[0] = glm::vec4(1.0, 0.0, 0.0, 1.0);
	meshes[1].triangles[4].color = glm::vec4(1.0, 0.5, 0.1, 1.0);
		   
	meshes[1].triangles[5].pos[0] = glm::vec4(0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[5].pos[1] = glm::vec4(0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[5].pos[2] = glm::vec4(0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[5].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[5].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[5].uv[2] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[5].normal[0] = glm::vec4(1.0, 0.0, 0.0, 1.0);
	meshes[1].triangles[5].color = glm::vec4(1.0, 0.5, 0.1, 1.0);

	meshes[1].triangles[6].pos[0] = glm::vec4(-0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[6].pos[1] = glm::vec4(-0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[6].pos[2] = glm::vec4(-0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[6].uv[0] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[6].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[6].uv[2] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[6].normal[0] = glm::vec4(-1.0, 0.0, 0.0, 1.0);
	meshes[1].triangles[6].color = glm::vec4(1.0, 0.5, 0.1, 1.0);

	meshes[1].triangles[7].pos[0] = glm::vec4(-0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[7].pos[1] = glm::vec4(-0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[7].pos[2] = glm::vec4(-0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[7].uv[0] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[7].uv[1] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[7].uv[2] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[7].normal[0] = glm::vec4(-1.0, 0.0, 0.0, 1.0);
	meshes[1].triangles[7].color = glm::vec4(1.0, 0.5, 0.1, 1.0);

	meshes[1].triangles[8].pos[0] = glm::vec4(-0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[8].pos[1] = glm::vec4(-0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[8].pos[2] = glm::vec4(0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[8].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[8].uv[1] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[8].uv[2] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[8].normal[0] = glm::vec4(0.0, 1.0, 0.0, 1.0);
	meshes[1].triangles[8].color = glm::vec4(1.0, 0.5, 0.1, 1.0);

	meshes[1].triangles[9].pos[0] = glm::vec4(-0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[9].pos[1] = glm::vec4(0.5, 0.5, -0.5, 1.0);
	meshes[1].triangles[9].pos[2] = glm::vec4(0.5, 0.5, 0.5, 1.0);
	meshes[1].triangles[9].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[9].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[9].uv[2] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[9].normal[0] = glm::vec4(0.0, 1.0, 0.0, 1.0);
	meshes[1].triangles[9].color = glm::vec4(1.0, 0.5, 0.1, 1.0);

	meshes[1].triangles[10].pos[0] = glm::vec4(-0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[10].pos[1] = glm::vec4(-0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[10].pos[2] = glm::vec4(0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[10].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[10].uv[1] = glm::vec4(0, 0, 1, 1);
	meshes[1].triangles[10].uv[2] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[10].normal[0] = glm::vec4(0.0, -1.0, 0.0, 1.0);
	meshes[1].triangles[10].color = glm::vec4(1.0, 0.5, 0.1, 1.0);

	meshes[1].triangles[11].pos[0] = glm::vec4(-0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[11].pos[1] = glm::vec4(0.5, -0.5, -0.5, 1.0);
	meshes[1].triangles[11].pos[2] = glm::vec4(0.5, -0.5, 0.5, 1.0);
	meshes[1].triangles[11].uv[0] = glm::vec4(0, 1, 1, 1);
	meshes[1].triangles[11].uv[1] = glm::vec4(1, 0, 1, 1);
	meshes[1].triangles[11].uv[2] = glm::vec4(1, 1, 1, 1);
	meshes[1].triangles[11].normal[0] = glm::vec4(0.0, -1.0, 0.0, 1.0);
	meshes[1].triangles[11].color = glm::vec4(1.0, 0.5, 0.1, 1.0);

	// Mesh 1 is a cube
	// It should have one normal per triangle
	// dulicate the first normal we give it
	for (int i = 0; i < meshes[1].numTriangles; i++)
	{
		meshes[1].triangles[i].normal[1] = meshes[1].triangles[i].normal[0];
		meshes[1].triangles[i].normal[2] = meshes[1].triangles[i].normal[0];
	}

	// Mesh 2 is a car
	// It will have one normal per vertex
	loadOBJ(&meshes[2]);

	glGenBuffers(1, &triangleBuffer);
	glBindBuffer(GL_UNIFORM_BUFFER, triangleBuffer);
	glBufferData(GL_UNIFORM_BUFFER, triangleBufferSize, meshes, GL_STATIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	glGenBuffers(1, &lightToFrag);
	glBindBuffer(GL_UNIFORM_BUFFER, lightToFrag);
	glBufferData(GL_UNIFORM_BUFFER, lightToFrag, nullptr, GL_DYNAMIC_DRAW); // static because CPU won't touch it
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void window_size_callback(GLFWwindow* window, int w, int h)
{
	width = w;
	height = h;
	glViewport(0, 0, width, height);
}

int main(int argc, char **argv)
{
	// Initializes the GLFW library
	glfwInit();

	// Creates a window given (width, height, title, monitorPtr, windowPtr).
	// Don't worry about the last two, as they have to do with controlling which monitor to display on and having a reference to other windows. Leaving them as nullptr is fine.
	window = glfwCreateWindow(width, height, "", nullptr, nullptr);

	// This allows us to resize the window when we want to
	glfwSetWindowSizeCallback(window, window_size_callback);

	// Makes the OpenGL context current for the created window.
	glfwMakeContextCurrent(window);

	// Sets the number of screen updates to wait before swapping the buffers.
	glfwSwapInterval(1);

	// Initializes most things needed before the main loop
	init();

	// Make the BYTE array, factor of 3 because it's RGB.
	// This will hold each screenshot
	unsigned char* pixels = new unsigned char[3 * width * height];

	// Create a place for fileName
	char* fileName = (char*)malloc(100);

	// This creates the folder, only if it does
	// not already exist, called "exportedFrames"
	CreateDirectoryA("exportedFrames", NULL);

	// continue rendering until the desired
	// number of frames are hit

	while (totalFrame != maxFrames)
	{
		// Call the render function.
		renderScene();

		// Swaps the back buffer to the front buffer
		// Remember, you're rendering to the back buffer, then once rendering is complete, you're moving the back buffer to the front so it can be displayed.
		glfwSwapBuffers(window);

		// Checks to see if any events are pending and then processes them.
		glfwPollEvents();

		// get the image that was rendered
		// We use BGR format, because BMP images use BGR
		glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, pixels);

		// make the name of the current file
		sprintf(fileName, "exportedFrames/%d.png", totalFrame);

		// Convert to FreeImage format & save to file
		FIBITMAP* image = FreeImage_ConvertFromRawBits(pixels, width, height, 3 * width, 24, 0xFF0000, 0x00FF00, 0x0000FF, false);
		FreeImage_Save(FIF_PNG, image, fileName, 0);
		FreeImage_Unload(image);
	}

	// After the program is over, cleanup your data!
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	glDeleteProgram(draw_program);
	delete[] pixels;

	// Frees up GLFW memory
	glfwTerminate();
	
	// make space for a command
	char* command = (char*)malloc(1000);

	// build the command with proper FPS
	sprintf(command, "ffmpeg -r %d -i exportedFrames/%%d.png -q 0 test.avi", videoFPS);

	// give the command to build the video
	system(command);

	return 0;
}
