#include <iostream>
#include <fstream>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
#include <GLFW/glfw3native.h>

#include "glm/glm.hpp"
#define __CL_ENABLE_EXCEPTIONS
#define CL_VERSION_1_2
#include "CL/cl.h"
#include "CL/cl.hpp"
#include "CL/cl_gl.h"

using namespace std;
GLXContext gGLContext;

int main() {

	if (!glfwInit()) {
		return 1;
	}

	const int wWidth = 640;
	const int wHeight = 480;

	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window =
		glfwCreateWindow( wWidth, wHeight, "Title", NULL, NULL);

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

	vector<cl::Device> devices;
	vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	cl::Context cl_context;
	cl::Device cl_gl_device;
	try {
		// Link OpenCL with OpenGL
		gGLContext = glXGetCurrentContext();
		cl_context_properties cl_properties[] = { 
			CL_GL_CONTEXT_KHR, (cl_context_properties)gGLContext,
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
				<< "Failed to query proc address for clGetGLContextInfoKHR"
				<< endl;
		}


		size_t devicesSize;
		auto status = clGetGLContextInfoKHR(cl_properties,
				CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, 0, NULL, &devicesSize);
		if (status != CL_SUCCESS) {
			cerr << status << endl;
			throw exception();
		}
		
		cl_device_id c_cl_gl_device;
		status = clGetGLContextInfoKHR(cl_properties,
				CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR, sizeof(cl_device_id),
				&c_cl_gl_device, NULL);
        if(status != CL_SUCCESS) {
			cerr << status << endl;
			throw exception();
		}
		cl_gl_device = cl::Device(c_cl_gl_device);

		// Create context
		vector<cl::Device> devices;
		devices.push_back(cl_gl_device);
		//platforms[0].getDevices(CL_DEVICE_TYPE_GPU, &devices);
		cl_context = cl::Context(devices, cl_properties);
	} catch (cl::Error error) {
		cout << error.err() << endl;
		throw error;
	}
	cout << "OK" << endl;
	return 0;
}
