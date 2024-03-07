#include <iostream>
#include <cmath>
#include <math.h>
#include <vector>
#include <windows.h>

// GLEW
#define GLEW_STATIC
#include <GL/glew.h>

// GLFW
#include <GLFW/glfw3.h>

//GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//shader header
#include "shader.h"
#include "compute_shader.h"

//CALLBACK FUNCTIONS
void error_callback(int, const char*);
void window_close_callback(GLFWwindow*);
void key_callback(GLFWwindow*, int, int, int, int);


//window size
const unsigned int window_width = 1920;
const unsigned int window_height = 1080;

//cell size
const unsigned int cell_size = 1;
//the value here is each cells size in number of pixels. cells are always square so a cell_size of 3 will result in every cell being 3 x 3 pixels.

//run states
bool paused = true;		//flag used to pause
bool swap_mode_1 = false;
bool swap_mode_2 = false;
//swap modes can be used to change the rulestring of our current board during runtime


//display settings
const unsigned int DISPLAY_MODE = 1;
//options for display mode: 
//0: normal rendering for life-like automata
//1: rendering for life-like automata with an added maximum age constraint (uses separate shader for simplicity)


//define some vertices and indices which will be used to display fully rendered textures to our window
float window_vertices[] = {
	1.0f,  1.0f, 0.0f,		1.0f, 1.0f,   // top right
	1.0f, -1.0f, 0.0f,		1.0f, 0.0f,   // bottom right
	-1.0f, -1.0f, 0.0f,		0.0f, 0.0f,   // bottom left
	-1.0f,  1.0f, 0.0f,		0.0f, 1.0f    // top left 
};
unsigned int window_indices[] = {
		0, 1, 3, // first triangle
		1, 2, 3  // second triangle
};





int main() {
	//---------------------------------------------------------------------------------------------------
	//GLFW AND GLEW INITILIZATION. THIS INCLUDES SETTING UP OUR WINDOW AND LINKING CALLBACK FUNCTIONS.
	//---------------------------------------------------------------------------------------------------
	//link our error callback function
	glfwSetErrorCallback(error_callback);

	//initialize glfw
	if (!glfwInit()) {
		std::cout << "ERROR: Could not initialize glfw!\n";
	}

	//create the window
	GLFWwindow* window = glfwCreateWindow(window_width, window_height, "Window", glfwGetPrimaryMonitor(), NULL);
	//GLFWwindow* window = glfwCreateWindow(window_width, window_height, "Window", NULL, NULL);

	//link the window callback functions
	glfwSetWindowCloseCallback(window, window_close_callback);
	glfwSetKeyCallback(window, key_callback);

	//set our window as current
	glfwMakeContextCurrent(window);

	//initiaize GLEW (must be done after we have set a current context)
	if (glewInit() != GLEW_OK) {
		std::cout << "ERROR: Could not initialize glew!\n";
	}

	//set up the GL viewport
	int w, h;
	glfwGetFramebufferSize(window, &w, &h);
	glViewport(0, 0, w, h);

	//set our update interval (argument is number of frames per update)
	glfwSwapInterval(0);


	//---------------------------------------------------------------------------------------------------
	//SHADER PROGRAM CREATION
	//---------------------------------------------------------------------------------------------------
	//texture_shader is used when displaying rendered textures to the window
	Shader texture_shader("texture_vert_shader.vs", "texture_frag_shader.fs");

	//compute shader used in texture calculations when display mode is set to 0
	ComputeShader cell_shader("cell_solver.computes");

	//compute shader used in texture calculations when display mode is set to 1
	ComputeShader cell_shader_age("cell_solver_age.computes");



	//---------------------------------------------------------------------------------------------------
	//VBO AND VAO SETUP
	//---------------------------------------------------------------------------------------------------
	//generate vbos and store their ids
	unsigned int VBO_texture;
	glGenBuffers(1, &VBO_texture);

	//generate vaos and store their ids
	unsigned int VAO_texture;
	glGenVertexArrays(1, &VAO_texture);

	//generate ebos and store their ids
	unsigned int EBO_texture;
	glGenBuffers(1, &EBO_texture);

	//bind the texture vao and assign the some basic data to the texture vbo and ebo. This data is always the same and is just used for drawing a computed texture to the window
	glBindVertexArray(VAO_texture);
	glBindBuffer(GL_ARRAY_BUFFER, VBO_texture);
	glBufferData(GL_ARRAY_BUFFER, sizeof(window_vertices), window_vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_texture);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(window_indices), window_indices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);




	//---------------------------------------------------------------------------------------------------
	//SSBO AND TEXTURE SETUP
	//---------------------------------------------------------------------------------------------------

	//We use a couple of SSBOs for the purpose of sending data to and between our different compute shaders. 
	//We also initialize a texture which our final image can be saved to after gpu computation is done.

	//points represents a 2-d grid of pixels where reflected points land. It is an itermediate step in our computation for our final output image.

	//cells_buff_1 and cells_buff_2 are where we store our two boards. 
	//each time we update our texture, we use one board to draw it, and then write our next board state into the other buffer
	//after each update we swap our buffers and repeat
	GLuint cells_buff_1;
	glGenBuffers(1, &cells_buff_1);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, cells_buff_1);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * (window_height / cell_size) * (window_width / cell_size), nullptr, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, cells_buff_1);

	GLuint cells_buff_2;
	glGenBuffers(1, &cells_buff_2);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, cells_buff_2);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * (window_height / cell_size) * (window_width / cell_size), nullptr, GL_DYNAMIC_COPY);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cells_buff_2);

	//output_texture is where our final image is constructed before being displayed to the window. It is used to get the final output of our compute shaders.
	GLuint output_texture;
	glGenTextures(1, &output_texture);
	glBindTexture(GL_TEXTURE_2D, output_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	float borderColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, output_texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, window_width, window_height, 0, GL_RGBA, GL_FLOAT, NULL);
	glBindImageTexture(0, output_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);





	//UNIFORM SETTING
	//-----------------------------------------------------------------------------------------------------------------------------------------------------------
	glProgramUniform1ui(cell_shader.programID, glGetUniformLocation(cell_shader.programID, "window_width"), window_width);
	glProgramUniform1ui(cell_shader.programID, glGetUniformLocation(cell_shader.programID, "window_height"), window_height);
	glProgramUniform1ui(cell_shader.programID, glGetUniformLocation(cell_shader.programID, "cell_size"), cell_size);

	glProgramUniform1ui(cell_shader_age.programID, glGetUniformLocation(cell_shader_age.programID, "window_width"), window_width);
	glProgramUniform1ui(cell_shader_age.programID, glGetUniformLocation(cell_shader_age.programID, "window_height"), window_height);
	glProgramUniform1ui(cell_shader_age.programID, glGetUniformLocation(cell_shader_age.programID, "cell_size"), cell_size);
	//-----------------------------------------------------------------------------------------------------------------------------------------------------------

	//bellow are some nice pre-written rulestrings that can be used to generate nice images

	int gen_density = 5;

	//conway
	//GLint rule_survive[9] = { 0, 0, 1, 1, 0, 0, 0, 0, 0 };
	//GLint rule_birth[9] = { 0, 0, 0, 1, 0, 0, 0, 0, 0 };

	//2x2
	//GLint rule_survive[9] = { 0, 1, 1, 0, 0, 1, 0, 0, 0 };
	//GLint rule_birth[9] = { 0, 0, 0, 1, 0, 0, 1, 0, 0 };
	//gen_density = 8;

	//34 life
	//GLint rule_survive[9] = { 0, 0, 0, 1, 1, 0, 0, 0, 0 };
	//GLint rule_birth[9] = { 0, 0, 0, 1, 1, 0, 0, 0, 0 };
	//gen_density = 12;

	//ameoba
	//GLint rule_survive[9] = { 0, 1, 0, 1, 0, 1, 0, 0, 1 };
	//GLint rule_birth[9] = { 0, 0, 0, 1, 0, 1, 0, 1, 0 };
	//gen_density = 6;

	//assimilation
	//GLint rule_survive[9] = { 0, 0, 0, 0, 1, 1, 1, 1, 0 };
	//GLint rule_birth[9] = { 0, 0, 0, 1, 1, 1, 0, 0, 0 };
	//gen_density = 6;

	//coagulations
	//GLint rule_survive[9] = { 0, 0, 1, 1, 0, 1, 1, 1, 1 };
	//GLint rule_birth[9] = { 0, 0, 0, 1, 0, 0, 0, 1, 1 };
	//gen_density = 10;

	//coral
	GLint rule_survive[9] = { 0, 0, 0, 0, 1, 1, 1, 1, 1 };
	GLint rule_birth[9] = { 0, 0, 0, 1, 0, 0, 0, 0, 0 };
	gen_density = 5;

	//day and night
	//GLint rule_survive[9] = { 0, 0, 0, 1, 1, 0, 1, 1, 1 };
	//GLint rule_birth[9] = { 0, 0, 0, 1, 0, 0, 1, 1, 1 };
	//gen_density = 2;

	//flakes
	//GLint rule_survive[9] = { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
	//GLint rule_birth[9] = { 0, 0, 0, 1, 0, 0, 0, 0, 0 };
	//gen_density = 100;
	
	//gnarl
	//GLint rule_survive[9] = { 0, 1, 0, 0, 0, 0, 0, 0, 0 };
	//GLint rule_birth[9] = { 0, 1, 0, 0, 0, 0, 0, 0, 0 };
	//gen_density = 100000;

	//walled cities
	//GLint rule_survive[9] = { 0, 0, 1, 1, 1, 1, 0, 0, 0 };
	//GLint rule_birth[9] = { 0, 0, 0, 0, 1, 1, 1, 1, 1 };
	//gen_density = 6;

	//GLint rule_survive[9] = { 1, 0, 0, 0, 0, 0, 0, 0, 0 };
	//GLint rule_birth[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	//gen_density = 700;
	 
	//GLint rule_survive[9] = { 0, 1, 1, 1, 1, 1, 1, 1, 0 };
	//GLint rule_birth[9] = { 0, 1, 1, 1, 1, 1, 1, 1, 0 };
	//gen_density = 700;

	//star trek
	//GLint rule_survive[9] = { 1, 0, 1, 0, 1, 0, 0, 0, 1 };
	//GLint rule_birth[9] = { 0, 0, 0, 1, 0, 0, 0, 0, 0 };

	//GLint rule_survive[9] = { 0, 0, 1, 1, 1, 1, 0, 0, 1 };
	//GLint rule_birth[9] = { 0, 0, 1, 0, 1, 0, 0, 0, 0 };


	//now we send our chosen rulestring to the shader programs
	glProgramUniform1iv(cell_shader.programID, glGetUniformLocation(cell_shader.programID, "rule_survive"), 9, rule_survive);
	glProgramUniform1iv(cell_shader.programID, glGetUniformLocation(cell_shader.programID, "rule_birth"), 9, rule_birth);

	glProgramUniform1iv(cell_shader_age.programID, glGetUniformLocation(cell_shader_age.programID, "rule_survive"), 9, rule_survive);
	glProgramUniform1iv(cell_shader_age.programID, glGetUniformLocation(cell_shader_age.programID, "rule_birth"), 9, rule_birth);


	//used to store cursor position
	double xpos, ypos;

	//zero out the point SSBO
	GLuint zero = 0;
	GLuint one = 1;
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, cells_buff_1);
	glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED, GL_UNSIGNED_INT, &zero);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cells_buff_2);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, cells_buff_1);


	//RANDOM CODE THAT WAS USED TO GENERATE INTERESTING STARTING BOARDS
	//-----------------------------------------------------------------------------------------------------------------------------------------------------------
	// 
	//for (int i = 0; i < (window_height / cell_size) * (window_width / cell_size); i++) {
		//if (rand() % gen_density > 0) {
		//if ((int)(2*(rand() / (RAND_MAX + 1.0))) > 0) {
		//if(i % 16 > 0){
			//if (i / 540 < 1) {
			//	glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * i, sizeof(unsigned int), &zero);
			//}
			//else {
			//	glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * i, sizeof(unsigned int), &one);
			//}

			//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * i, sizeof(unsigned int), &zero);
		//}
		//else {
			//if (i / 540 < 1) {
			//	glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * i, sizeof(unsigned int), &one);
			//}
			//else {
			//	glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * i, sizeof(unsigned int), &zero);
			//}

			//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * i, sizeof(unsigned int), &one);
		//}

		//if (i % window_width < 960) {
		//	if (i % 16 > 0) {
		//		glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * i, sizeof(unsigned int), &zero);
		//	}
		//	else {
		//		glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * i, sizeof(unsigned int), &one);
		//	}
		//}
		//else {
		//	if (i % 16 < 15) {
		//		glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * i, sizeof(unsigned int), &zero);
		//	}
		//	else {
		//		glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * i, sizeof(unsigned int), &one);
		//	}
		//}
	//}
	
	//int dist;
	//for (int i = 0; i < (window_width / cell_size); i++) {
		//for (int j = 0; j < (window_height / cell_size); j++) {
			//dist = sqrt(pow((int)(i - ((window_width / cell_size) / 2)), 2) + pow((int)(j - ((window_height / cell_size) / 2)), 2));
			//if ((dist < 400) && (dist > 397)) {
				//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * (i + (j * (window_width / cell_size))), sizeof(unsigned int), &one);
			//}
			//else if ((dist < 300) && (dist > 297)) {
				//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * (i + (j * (window_width / cell_size))), sizeof(unsigned int), &one);
			//}
			//else if ((dist < 200) && (dist > 197)) {
				//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * (i + (j * (window_width / cell_size))), sizeof(unsigned int), &one);
			//}
			//else if ((dist < 100) && (dist > 97)) {
				//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * (i + (j * (window_width / cell_size))), sizeof(unsigned int), &one);
			//}
			//else if ((dist < 20) && (dist > 0)) {
				//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * (i + (j * (window_width / cell_size))), sizeof(unsigned int), &one);
			//}

			//else if ((dist < 200) && (dist > 196)) {
			//	if ((((i - ((window_width / cell_size) / 2)) > 50) && ((i - ((window_width / cell_size) / 2)) < -50)) && (((j - ((window_height / cell_size) / 2)) > 50) && ((j - ((window_height / cell_size) / 2)) < -50))) {
			//		glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * (i + (j * (window_width / cell_size))), sizeof(unsigned int), &one);
			//	}
			//}
		//}
	//}


	//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * ((int)((window_width / 2) / cell_size) + ((int)((window_height / 2) / cell_size) * (window_width / cell_size))), sizeof(unsigned int), &one);
	//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * 96, sizeof(unsigned int), &one);

	//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * ((int)((window_width / 4) / cell_size) + ((int)((window_height / 2) / cell_size) * (window_width / cell_size))), sizeof(unsigned int), &one);
	//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * ((int)(((window_width / 4) * 3) / cell_size) + ((int)((window_height / 2) / cell_size) * (window_width / cell_size))), sizeof(unsigned int), &one);


	//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * ((int)((window_width / 4) / cell_size) + ((int)((window_height / 4) / cell_size) * (window_width / cell_size))), sizeof(unsigned int), &one);
	//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * ((int)(((window_width / 4) * 3) / cell_size) + ((int)((window_height / 4) / cell_size) * (window_width / cell_size))), sizeof(unsigned int), &one);
	//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * ((int)((window_width / 4) / cell_size) + ((int)(((window_height / 4) * 3) / cell_size) * (window_width / cell_size))), sizeof(unsigned int), &one);
	//glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * ((int)(((window_width / 4) * 3) / cell_size) + ((int)(((window_height / 4) * 3) / cell_size) * (window_width / cell_size))), sizeof(unsigned int), &one);


	//-----------------------------------------------------------------------------------------------------------------------------------------------------------



	bool evenFrame = false;
	unsigned int frameNum = 0;

	int cursor_width = 15; //size of square drawn and erased when clicking during runtime

	int tempy = 0;
	//---------------------------------------------------------------------------------------------------
	//MAIN PROGRAM LOOP
	//---------------------------------------------------------------------------------------------------
	while (!glfwWindowShouldClose(window)) {
		//curTime = GetTickCount64();
		//fps++;
		//if (((int)((curTime - startTime) / 1000)) > biggestDiff) {
		//	biggestDiff = (int)((curTime - startTime) / 1000);
		//	std::cout << fps << std::endl;
		//	fps = 0;
		//}
		
		
		if (frameNum % 2 < 1) {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cells_buff_2);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, cells_buff_1);
			evenFrame = false;
		}
		else {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cells_buff_1);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, cells_buff_2);
			evenFrame = true;
		}
		frameNum++;
		if (paused) {
			frameNum--;
		}

		//decide which shader program to use based on DISPLAY_MODE, and dispatch our compute shaders
		if (DISPLAY_MODE == 0) {
			cell_shader.use();
			glDispatchCompute(window_width / cell_size, window_height / cell_size, 1);
		}
		else if (DISPLAY_MODE == 1) {
			cell_shader_age.use();
			glDispatchCompute(window_width / cell_size, window_height / cell_size, 1);
		}

		glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
		




		//display the computed image to the window
		texture_shader.use();
		glBindTexture(GL_TEXTURE_2D, output_texture);
		glBindVertexArray(VAO_texture);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);


		//handles mouse input, allowing user to draw and erase cells on the board
		if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
			glfwGetCursorPos(window, &xpos, &ypos);
			for (int i = 0 - ((int)(cursor_width / 2)); i < (int)((cursor_width + 1) / 2); i++) {
				for (int j = 0 - ((int)(cursor_width / 2)); j < (int)((cursor_width + 1) / 2); j++) {
					glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int)* ((int)((xpos + i) / cell_size) + ((int)((ypos + j) / cell_size) * (window_width / cell_size))), sizeof(unsigned int), &one);
				}
			}
		}
		else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
			glfwGetCursorPos(window, &xpos, &ypos);
			for (int i = 0 - ((int)(cursor_width / 2)); i < (int)((cursor_width + 1) / 2); i++) {
				for (int j = 0 - ((int)(cursor_width / 2)); j < (int)((cursor_width + 1) / 2); j++) {
					glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * ((int)((xpos + i) / cell_size) + ((int)((ypos + j) / cell_size) * (window_width / cell_size))), sizeof(unsigned int), &zero);
				}
			}
		}


		//allows the user to change rulestrings during runtime
		if (swap_mode_1) {
			GLint rule_survive_swap_1[9] = { 0, 0, 0, 0, 1, 1, 1, 1, 0 };
			GLint rule_birth_swap_1[9] = { 0, 0, 0, 1, 1, 1, 0, 0, 0 };

			glProgramUniform1iv(cell_shader.programID, glGetUniformLocation(cell_shader.programID, "rule_survive"), 9, rule_survive_swap_1);
			glProgramUniform1iv(cell_shader.programID, glGetUniformLocation(cell_shader.programID, "rule_birth"), 9, rule_birth_swap_1);

			glProgramUniform1iv(cell_shader_age.programID, glGetUniformLocation(cell_shader_age.programID, "rule_survive"), 9, rule_survive_swap_1);
			glProgramUniform1iv(cell_shader_age.programID, glGetUniformLocation(cell_shader_age.programID, "rule_birth"), 9, rule_birth_swap_1);
			swap_mode_1 = false;
		}
		if (swap_mode_2) {
			GLint rule_survive_swap_2[9] = { 0, 0, 1, 1, 0, 1, 1, 1, 1 };
			GLint rule_birth_swap_2[9] = { 0, 0, 0, 1, 0, 0, 0, 1, 1 };

			glProgramUniform1iv(cell_shader.programID, glGetUniformLocation(cell_shader.programID, "rule_survive"), 9, rule_survive_swap_2);
			glProgramUniform1iv(cell_shader.programID, glGetUniformLocation(cell_shader.programID, "rule_birth"), 9, rule_birth_swap_2);

			glProgramUniform1iv(cell_shader_age.programID, glGetUniformLocation(cell_shader_age.programID, "rule_survive"), 9, rule_survive_swap_2);
			glProgramUniform1iv(cell_shader_age.programID, glGetUniformLocation(cell_shader_age.programID, "rule_birth"), 9, rule_birth_swap_2);
			swap_mode_2 = false;
		}

		//check for events which occured since the last update
		glfwPollEvents();
		//swap our frame buffers
		glfwSwapBuffers(window);
	}



	//termintate glfw and exit
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}






//basic error logging
void error_callback(int error, const char* description)
{
	fprintf(stderr, "Error: %s\n", description);
}

//callback to handle window closing
void window_close_callback(GLFWwindow* window) {
	std::cout << "Window will now close.\n";
}

//callback to handle key presses
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		window_close_callback(window);
	}
	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
		paused = !paused;
	}
	if (key == GLFW_KEY_R && action == GLFW_PRESS) {
		swap_mode_1 = true;
	}
	if (key == GLFW_KEY_T && action == GLFW_PRESS) {
		swap_mode_2 = true;
	}
}

