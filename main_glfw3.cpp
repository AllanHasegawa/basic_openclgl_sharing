#include <cstdio>
#include <vector>
#include <algorithm>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <fstream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#include <GLFW/glfw3native.h>

#include "glm/glm.hpp"
#define __CL_ENABLE_EXCEPTIONS
#include "CL/cl.hpp"

using namespace std;

auto DREAM_FRAME_TIME = std::chrono::microseconds(16666);
auto BAD_FRAME_TIME = std::chrono::milliseconds(1666);

// synching vars;
mutex m;
condition_variable cv;
bool ready = false;
bool quit = false;

const int wWidth = 640;
const int wHeight = 480;

void error_callback(int error, const char* description) {
	fputs(description, stderr);
}

static void key_callback(GLFWwindow* window, int key, int scancode,
		int action, int mods) {
	cout << "Key: " << key << " [" << action << "]" << endl;
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		quit = true;
		glfwSetWindowShouldClose(window, GL_TRUE);
	}
}

void reshape(GLFWwindow* window, int width, int height) {
	std::cout << "Viewport: " << width << "," << height << std::endl;
	glViewport(0, 0, width, height);
}

void manager(cl::CommandQueue& queue, cl::Kernel& gl_kernel,
		vector<cl::Memory>& cl_gl_objs) {
	float x = 0;
	while (!quit) {
		queue.enqueueAcquireGLObjects(&cl_gl_objs);

		x += 0.01f;
		if (x > 1.f) x = 0;
		gl_kernel.setArg(1, x);

		// Execute Kernel
		cl::NDRange global(wWidth, wHeight);
		cl::NDRange local(2, 2);
		try {
			queue.enqueueNDRangeKernel(gl_kernel, cl::NullRange,
					global, local);
		} catch (cl::Error error) {
			cerr << error.err() << endl;
		}

		queue.enqueueReleaseGLObjects(&cl_gl_objs);

		queue.finish();
		std::this_thread::sleep_for(DREAM_FRAME_TIME);
		{
			lock_guard<mutex> lk(m);
			ready = true;
			cv.notify_one();
		}
	}
}

/*
 * Method copied from:
 * http://www.arcsynthesis.org/gltut/Basics/Tut01%20Making%20Shaders.html
 * */
GLuint CreateShader(GLenum eShaderType, const std::string &strShaderFile) {
	GLuint shader = glCreateShader(eShaderType);
	const char *strFileData = strShaderFile.c_str();
	glShaderSource(shader, 1, &strFileData, NULL);

	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint infoLogLength;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

		GLchar *strInfoLog = new GLchar[infoLogLength + 1];
		glGetShaderInfoLog(shader, infoLogLength, NULL, strInfoLog);

		const char *strShaderType = NULL;
		switch(eShaderType)
		{
			case GL_VERTEX_SHADER: strShaderType = "vertex"; break;
								   //case GL_GEOMETRY_SHADER: strShaderType = "geometry"; break;
			case GL_FRAGMENT_SHADER: strShaderType = "fragment"; break;
		}

		fprintf(stderr, "Compile failure in %s shader:\n%s\n", strShaderType, strInfoLog);
		delete[] strInfoLog;
	}

	return shader;
}

/*
 * Method copied from:
 * http://www.arcsynthesis.org/gltut/Basics/Tut01%20Making%20Shaders.html
 * */
GLuint CreateProgram(const std::vector<GLuint> &shaderList) {
	GLuint program = glCreateProgram();

	for(size_t iLoop = 0; iLoop < shaderList.size(); iLoop++)
		glAttachShader(program, shaderList[iLoop]);

	glLinkProgram(program);

	GLint status;
	glGetProgramiv (program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		GLint infoLogLength;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

		GLchar *strInfoLog = new GLchar[infoLogLength + 1];
		glGetProgramInfoLog(program, infoLogLength, NULL, strInfoLog);
		fprintf(stderr, "Linker failure: %s\n", strInfoLog);
		delete[] strInfoLog;
	}

	for(size_t iLoop = 0; iLoop < shaderList.size(); iLoop++)
		glDetachShader(program, shaderList[iLoop]);

	return program;
}

string strVertexShader = R".(
#version 330

in vec4 position;
in vec2 inTexCoord;

out vec2 texCoord;

void main()
{
	texCoord = inTexCoord;
	gl_Position = position;
}
).";

string strFragmentShader = R".(
#version 330

uniform sampler2D tex;
out vec4 outColor;

in vec2 texCoord;

void main()
{
	//outColor = vec4(1.0, 0.0, 0.0, 1.0);
	outColor = texture(tex, texCoord);
}
).";

GLuint theProgram;

/*
 * Method copied from:
 * http://www.arcsynthesis.org/gltut/Basics/Tut01%20Making%20Shaders.html
 * */
void InitializeProgram()
{
	std::vector<GLuint> shaderList;

	shaderList.push_back(CreateShader(GL_VERTEX_SHADER, strVertexShader));
	shaderList.push_back(CreateShader(GL_FRAGMENT_SHADER, strFragmentShader));

	theProgram = CreateProgram(shaderList);

	std::for_each(shaderList.begin(), shaderList.end(), glDeleteShader);
}

int main() {

	vector<cl::Device> devices;
	vector<cl::Platform> platforms;
	vector<cl::Memory> cl_gl_objs;

	cl::Context cl_context;
	cl::Kernel gl_kernel;
	cl::Program cl_program;
	cl::CommandQueue queue;

	try {
		cl::Platform::get(&platforms);
		cout << "N Platforms: " << platforms.size() << endl;
		std::string t;

		cout << string(32, '-') << endl;
		for (cl::Platform p : platforms) {
			p.getInfo(CL_PLATFORM_NAME, &t);
			cout << "Platform Name: " << t << endl;

			p.getInfo(CL_PLATFORM_VERSION, &t);
			cout << "Platform Version: " << t << endl;

			vector<cl::Device> devices;

			p.getDevices(CL_DEVICE_TYPE_ALL, &devices);
			cout << "Platform N Devices: " << devices.size()
				<< endl;

			for (cl::Device d : devices) {
				d.getInfo(CL_DEVICE_NAME, &t);
				cout << "Device Name: "
					<< t << endl;
				cl_device_type dt;
				d.getInfo(CL_DEVICE_TYPE, &dt);
				cout << "Device Type: "
					<< dt << endl;
				d.getInfo(CL_DRIVER_VERSION, &t);
				cout << "Device Driver: "
					<< t << endl;
				cl_uint dcu;
				d.getInfo(CL_DEVICE_MAX_COMPUTE_UNITS, &dcu);
				cout << "Device MCU: " << dcu << endl;
				d.getInfo(CL_DEVICE_EXTENSIONS, &t);
				cout << "Device Extensions: "
					<< t << endl;
			}
			cout << string(32,'-') << endl;
		}


	} catch (cl::Error error) {
		cout << error.what() << "(" <<
			error.err() << ")" << endl;
	}

	if (!glfwInit()) {
		return 1;
	}

	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow( wWidth, wHeight,
			"Title", NULL, NULL);

	if (!window) {
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);

	const GLubyte* renderer = glGetString(GL_RENDERER);
	const GLubyte* version = glGetString(GL_VERSION);
	printf ("Renderer: %s\n", renderer);
	printf ("OpenGL version supported %s\n", version);
	glFinish();


	try {
		// Link OpenCL with OpenGL
		cl_context_properties cl_properties[] = { 
			CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
			CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(), 
			CL_CONTEXT_PLATFORM,
			(cl_context_properties)(platforms[0])(), 
			0};

		clGetGLContextInfoKHR_fn clGetGLContextInfoKHR =
			(clGetGLContextInfoKHR_fn)
			clGetExtensionFunctionAddressForPlatform(
					platforms[0](),"clGetGLContextInfoKHR");
		if (!clGetGLContextInfoKHR) {
			std::cerr
				<< "clGetGLContextInfoKHR"
				<< endl;
			throw runtime_error{"clGetGLContextInfoKHR"};
		}


		size_t devicesSize;
		auto status = clGetGLContextInfoKHR(cl_properties,
				CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, 0, NULL, &devicesSize);
		if (status != CL_SUCCESS) {
			cerr << status << endl;
			throw runtime_error{"clGetGLContextInfoKHR"};
		}

		cl_device_id c_cl_gl_device;
		status = clGetGLContextInfoKHR(cl_properties,
				CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id),
				&c_cl_gl_device, NULL);
		if(status != CL_SUCCESS) {
			cerr << status << endl;
			throw runtime_error{"clGetGLContextInfoKHR"};
		}
		cl::Device cl_gl_device{c_cl_gl_device};

		// Create context
		devices.clear();
		devices.push_back(move(cl_gl_device));

		cout << string(32, '-') << endl;
		cout << "Interop OpenGL/OpenCL Devices" << endl;
		for (cl::Device d : devices) {
			string t;
			d.getInfo(CL_DEVICE_NAME, &t);
			cout << "Device Name: "
				<< t << endl;
			cl_device_type dt;
			d.getInfo(CL_DEVICE_TYPE, &dt);
			cout << "Device Type: "
				<< dt << endl;
			d.getInfo(CL_DRIVER_VERSION, &t);
			cout << "Device Driver: "
				<< t << endl;
			cl_uint dcu;
			d.getInfo(CL_DEVICE_MAX_COMPUTE_UNITS, &dcu);
			cout << "Device MCU: " << dcu << endl;
			d.getInfo(CL_DEVICE_EXTENSIONS, &t);
			cout << "Device Extensions: "
				<< t << endl;
		}
		cout << string(32, '-') << endl;

		//platforms[0].getDevices(CL_DEVICE_TYPE_GPU, &devices);
		cl_context = cl::Context(devices, cl_properties);

		// Create a command Queue for the first device
		queue = cl::CommandQueue(cl_context, devices[0]);

		ifstream sourceFile("gl_kernel.cl");
		string sourceCode(istreambuf_iterator<char>(sourceFile),
				(istreambuf_iterator<char>()));
		// Creating the sources
		cl::Program::Sources source(1, make_pair(sourceCode.c_str(),
					sourceCode.length()+1));
		// Make program from sources
		cl_program = cl::Program(cl_context, source);

		// Compile sources
		cl_program.build(devices);

		// Make kernel
		gl_kernel = cl::Kernel(cl_program, "glk");
	} catch (cl::Error error) {
		cout << error.what() << error.err() << endl;
		throw error;
	}
	// Initialize GLEW
	if (glewInit() != GLEW_OK) {
		cout << "Failed to initialize GLEW" << endl;
		return 1;
	}

	glfwSetErrorCallback(error_callback);
	glfwSetKeyCallback(window, key_callback);

	glfwSetFramebufferSizeCallback(window, reshape);

	double lastTime = glfwGetTime();
	double currentTime = glfwGetTime();
	int frames = 0;

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	reshape(window, width, height);

	/*
	 * 2 ---- 4
	 * |\     |
	 * | \    |
	 * |  \   |
	 * |   \  |
	 * |    \ |
	 * 1 ---- 3
	 */
	const GLfloat vertexPositions[] = {
		// vertex position, texture coords
		// x, y, z, w, u, v
		-1.f, -1.f, 0.0f, 1.0f, 0.f, 0.f,
		-1.f, 1.f, 0.0f, 1.0f, 0.f, 1.f,
		1.f, -1.f, 0.0f, 1.0f, 1.f, 0.f,
		1.f, 1.f, 0.0f, 1.0f, 1.f, 1.f
	};

	GLuint positionBufferObject;
	glGenBuffers(1, &positionBufferObject);

	glBindBuffer(GL_ARRAY_BUFFER, positionBufferObject);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexPositions),
			vertexPositions, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	InitializeProgram();

	glUseProgram(theProgram);
	glBindBuffer(GL_ARRAY_BUFFER, positionBufferObject);
	GLint posAttrib = glGetAttribLocation(theProgram, "position");
	GLint texAttrib = glGetAttribLocation(theProgram, "inTexCoord");

	glEnableVertexAttribArray(posAttrib);
	glEnableVertexAttribArray(texAttrib);

	glVertexAttribPointer(posAttrib, 4, GL_FLOAT, GL_FALSE,
			6*sizeof(GLfloat), 0);
	glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE,
			6*sizeof(GLfloat), (void*)(4*sizeof(GLfloat)));

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER); 
	GLfloat red_color[] = { 1.0f, 0.5f, 0.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, red_color);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	array<GLfloat,640*480*4> pixels;
	for (int i = 0; i < pixels.size(); ++i) {
		int x = (i/4) % wWidth;
		//int y = (i/4) / wWidth;
		if (i % 4 == 0) {
			if (x < wWidth) {
				pixels[i] = 255;
			} else {
				pixels[i] = 0;
			}
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
			GL_FLOAT, pixels.data());
	glActiveTexture(GL_TEXTURE0);
	glFinish();

	try {
		cl_gl_objs.push_back(cl::ImageGL{cl_context, CL_MEM_WRITE_ONLY,
				GL_TEXTURE_2D, 0, tex});
	} catch(cl::Error error) {
		cout << error.what()  << error.err() << endl;
		throw error;
	}
	gl_kernel.setArg(0, cl_gl_objs[0]);

	// Start second thread
	thread mgr(manager, std::ref(queue), ref(gl_kernel),
			ref(cl_gl_objs));

	while (!glfwWindowShouldClose(window)) {
		unique_lock<mutex> lk(m);
		while (!ready) {
			auto until = chrono::high_resolution_clock::now()
				+ chrono::milliseconds(5);
			if (cv.wait_until(lk, until) ==
					std::cv_status::timeout) {
				glfwPollEvents();
				if (quit) {
					break;
				}
			}
		}

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glfwSwapBuffers(window);
		glfwPollEvents();

		ready = false;
		lk.unlock();

		++frames;
		currentTime = glfwGetTime();
		if (currentTime - lastTime >= 3.0) {
			lastTime = currentTime;
			string title;
			title = "oglcl - FPS: " + to_string(frames/3.0);
			glfwSetWindowTitle(window, title.c_str());
			frames = 0;
		}
	}

	quit = true;
	mgr.join();

	queue.finish();

	// I """"HAVE TO"""" release OpenCL resources
	// """"BEFORE"""" OpenGL resources T_T
	//  --- don't judge -_-
	cl_gl_objs.clear();
	queue = cl::CommandQueue{};
	cl_program = cl::Program{};
	gl_kernel = cl::Kernel{};
	cl_context = cl::Context{};

	glFinish();
	glfwDestroyWindow(window);
	glfwTerminate();


	cout << "Finish" << endl;
	return 0;
}
