// CIS565 CUDA Raytracer: A parallel raytracer for Patrick Cozzi's CIS565: GPU Computing at the University of Pennsylvania
// Written by Yining Karl Li, Copyright (c) 2012 University of Pennsylvania
// This file includes code from:
//       Rob Farber for CUDA-GL interop, from CUDA Supercomputing For The Masses: http://www.drdobbs.com/architecture-and-design/cuda-supercomputing-for-the-masses-part/222600097
//       Varun Sampath and Patrick Cozzi for GLSL Loading, from CIS565 Spring 2012 HW5 at the University of Pennsylvania: http://cis565-spring-2012.github.com/
//       Yining Karl Li's TAKUA Render, a massively parallel pathtracing renderer: http://www.yiningkarlli.com

#include "main.h"

//-------------------------------
//-------------MAIN--------------
//-------------------------------

int main(int argc, char** argv){

  #ifdef __APPLE__
	  // Needed in OSX to force use of OpenGL3.2 
	  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
	  glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 2);
	  glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	  glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  #endif

  // Set up pathtracer stuff
  bool loadedScene = false;
  finishedRender = false;

  targetFrame = 0;
  singleFrameMode = false;


  // Load scene file
  for(int i=1; i<argc; i++){
    string header; string data;
    istringstream liness(argv[i]);
    getline(liness, header, '='); getline(liness, data, '=');
    if(strcmp(header.c_str(), "scene")==0){
      renderScene = new scene(data);
      loadedScene = true;
    }else if(strcmp(header.c_str(), "frame")==0){
      targetFrame = atoi(data.c_str());
      singleFrameMode = true;
    }
	else if(strcmp(header.c_str(), "mblur")==0){
      mblur = atoi(data.c_str());
    }
	else if(strcmp(header.c_str(), "dof")==0){
      dof = atoi(data.c_str());
    }
	else if(strcmp(header.c_str(), "textureMode")==0){
      textureMode = atoi(data.c_str());
    }
  }

  if(!loadedScene){
    cout << "Error: scene file needed!" << endl;
    return 0;
  }

  // Set up camera stuff from loaded pathtracer settings
  iterations = 0;
  renderCam = &renderScene->renderCam;
  width = renderCam->resolution[0];
  height = renderCam->resolution[1];

  if(targetFrame>=renderCam->frames){
    cout << "Warning: Specified target frame is out of range, defaulting to frame 0." << endl;
    targetFrame = 0;
  }

  // Launch CUDA/GL

  #ifdef __APPLE__
	init();
  #else
	init(argc, argv);
  #endif

  initCuda();

  initVAO();
  initTextures();

  GLuint passthroughProgram;
  passthroughProgram = initShader("shaders/passthroughVS.glsl", "shaders/passthroughFS.glsl");

  glUseProgram(passthroughProgram);
  glActiveTexture(GL_TEXTURE0);

  #ifdef __APPLE__
	  // send into GLFW main loop
	  while(1){
		display();
		if (glfwGetKey(GLFW_KEY_ESC) == GLFW_PRESS || !glfwGetWindowParam( GLFW_OPENED )){
				exit(0);
		}
	  }

	  glfwTerminate();
  #else
	  glutDisplayFunc(display);
	  glutKeyboardFunc(keyboard);
	  glutMouseFunc(mouse);
	  glutMainLoop();
  #endif
  return 0;
}

//-------------------------------
//---------RUNTIME STUFF---------
//-------------------------------

void runCuda(){

  // Map OpenGL buffer object for writing from CUDA on a single GPU
  // No data is moved (Win & Linux). When mapped to CUDA, OpenGL should not use this buffer
  
  if(iterations<renderCam->iterations){
    uchar4 *dptr=NULL;
    iterations++;
    cudaGLMapBufferObject((void**)&dptr, pbo);
  
    //pack geom and material arrays
    geom* geoms = new geom[renderScene->objects.size()];
    material* materials = new material[renderScene->materials.size()];
    map* maps = new map[renderScene->maps.size()];

    for(int i=0; i<renderScene->objects.size(); i++){
      geoms[i] = renderScene->objects[i];
    }
    for(int i=0; i<renderScene->materials.size(); i++){
      materials[i] = renderScene->materials[i];
    }
    
  	for(int i=0; i<renderScene->maps.size(); i++){
      maps[i] = renderScene->maps[i];
    }
    // execute the kernel
	if(!textureMode)
		cudaRaytraceCore(dptr, renderCam, targetFrame, iterations, materials, renderScene->materials.size(),maps,renderScene->maps.size(), geoms, renderScene->objects.size(), mblur,dof);
	else
		cudaRaytraceCoreT(dptr, renderCam, targetFrame, iterations, materials, renderScene->materials.size(),maps,renderScene->maps.size(), geoms, renderScene->objects.size(), mblur,dof);

	// unmap buffer object
    cudaGLUnmapBufferObject(pbo);
  }else{

    if(!finishedRender){
      //output image file
      image outputImage(renderCam->resolution.x, renderCam->resolution.y);
	  image depthImage(renderCam->resolution.x, renderCam->resolution.y);
      for(int x=0; x<renderCam->resolution.x; x++){
        for(int y=0; y<renderCam->resolution.y; y++){
          int index = x + (y * renderCam->resolution.x);
		  glm::vec3 justRGB(renderCam->image[index].x,renderCam->image[index].y,renderCam->image[index].z);
          outputImage.writePixelRGB(renderCam->resolution.x-1-x,y,justRGB);
		  float d = abs(renderCam->image[index].w-renderCam->positions[targetFrame].z)/40.0f;
		  depthImage.writePixelRGB(renderCam->resolution.x-1-x,y,  glm::vec3(d,d,d));
        }
      }
      
      gammaSettings gamma;
      gamma.applyGamma = true;
      gamma.gamma = 1.0/2.2;
      gamma.divisor = renderCam->iterations;
      outputImage.setGammaSettings(gamma);
      string filename = renderCam->imageName;
      string s;
      stringstream out;
      out << targetFrame;
      s = out.str();
      utilityCore::replaceString(filename, ".bmp", "."+s+".bmp");
      utilityCore::replaceString(filename, ".png", "."+s+".png");
      outputImage.saveImageRGB(filename);
	  depthImage.saveImageRGB("depth."+s+".bmp");
      cout << "Saved frame " << s << " to " << filename << endl;
      finishedRender = true;
      if(singleFrameMode==true){
        //cudaDeviceReset(); 
        exit(0);
      }
    }
    if(targetFrame<renderCam->frames-1){

      //clear image buffer and move onto next frame
      targetFrame++;
      iterations = 0;
      for(int i=0; i<renderCam->resolution.x*renderCam->resolution.y; i++){
        renderCam->image[i] = glm::vec4(0,0,0,-1);
      }
      //cudaDeviceReset(); 
      finishedRender = false;
    }
  }
  
}

#ifdef __APPLE__

	void display(){
		runCuda();

		string title = "CIS565 Render | " + utilityCore::convertIntToString(iterations) + " Iterations";
		glfwSetWindowTitle(title.c_str());

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pbo);
		glBindTexture(GL_TEXTURE_2D, displayImage);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 
			  GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glClear(GL_COLOR_BUFFER_BIT);   

		// VAO, shader program, and texture already bound
		glDrawElements(GL_TRIANGLES, 6,  GL_UNSIGNED_SHORT, 0);

		glfwSwapBuffers();
	}

#else

	void display(){
		runCuda();

		string title = "565Raytracer | " + utilityCore::convertIntToString(iterations) + " Iterations";
		glutSetWindowTitle(title.c_str());

		glBindBuffer( GL_PIXEL_UNPACK_BUFFER, pbo);
		glBindTexture(GL_TEXTURE_2D, displayImage);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, 
			  GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glClear(GL_COLOR_BUFFER_BIT);   

		// VAO, shader program, and texture already bound
		glDrawElements(GL_TRIANGLES, 6,  GL_UNSIGNED_SHORT, 0);

		glutPostRedisplay();
		glutSwapBuffers();
	}


	void mouse(int button, int state, int x, int y)
	{
		if(button == GLUT_RIGHT_BUTTON && state== GLUT_DOWN && dof)
		{
			int pixelX = renderCam->resolution.x -1 - x;
			int index = pixelX + renderCam->resolution.x*y;
			float pixelDist = renderCam->image[index].w;
			iterations = 0;
			renderCam->focalDist = pixelDist;
			resetImage(renderCam);
		}
		
		else if(button == GLUT_LEFT_BUTTON && state== GLUT_DOWN)
		{
			int pixelX = renderCam->resolution.x -1 - x;
			int index = pixelX + renderCam->resolution.x*y;
			currentSelectedObjId = renderCam->objIdBuffer[index];
		}
	}


	void keyboard(unsigned char key, int x, int y)
	{
		switch (key) 
		{
		   case(27):
			   exit(1);
			   break;

		   case 'w':
			   iterations = 0;
			   resetImage(renderCam);
			   renderCam->positions[targetFrame].z-=0.2f;
			   break;

		   case 's':
			   iterations = 0;
			   resetImage(renderCam);
			   renderCam->positions[targetFrame].z+= 0.2f;
			   break;

		   case 'a':
			   iterations = 0;
			   resetImage(renderCam);
			   renderCam->positions[targetFrame].x+=0.2f;
			   break;

		   case 'd':
			   iterations = 0;
			   resetImage(renderCam);
			   renderCam->positions[targetFrame].x-=0.2f;
			   break;

		   case 'x':
			   if(currentSelectedObjId != -1)
			   {
			   iterations = 0;
			   resetImage(renderCam);
			   geom g = renderScene->objects[currentSelectedObjId];
			   g.translations[targetFrame].x+=0.2;
			   glm::mat4 transform = utilityCore::buildTransformationMatrix(g.translations[targetFrame],g.rotations[targetFrame],g.scales[targetFrame]);
			   g.transforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(transform);
			   g.inverseTransforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(glm::inverse(transform));
			   }
			   break;

		   case 'X':
			   if(currentSelectedObjId != -1)
			   {
			   iterations = 0;
			   resetImage(renderCam);
			   geom g = renderScene->objects[currentSelectedObjId];
			   g.translations[targetFrame].x-=0.2;
			   glm::mat4 transform = utilityCore::buildTransformationMatrix(g.translations[targetFrame],g.rotations[targetFrame],g.scales[targetFrame]);
			   g.transforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(transform);
			   g.inverseTransforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(glm::inverse(transform));
			   }
			   break;

		   case 'y':
			   if(currentSelectedObjId != -1)
			   {
			   iterations = 0;
			   resetImage(renderCam);
			   geom g = renderScene->objects[currentSelectedObjId];
			   g.translations[targetFrame].y+=0.2;
			   glm::mat4 transform = utilityCore::buildTransformationMatrix(g.translations[targetFrame],g.rotations[targetFrame],g.scales[targetFrame]);
			   g.transforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(transform);
			   g.inverseTransforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(glm::inverse(transform));
			   }
			   break;

		   case 'Y':
			   if(currentSelectedObjId != -1)
			   {
			   iterations = 0;
			   resetImage(renderCam);
			   geom g = renderScene->objects[currentSelectedObjId];
			   g.translations[targetFrame].y-=0.2;
			   glm::mat4 transform = utilityCore::buildTransformationMatrix(g.translations[targetFrame],g.rotations[targetFrame],g.scales[targetFrame]);
			   g.transforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(transform);
			   g.inverseTransforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(glm::inverse(transform));
			   }
			   break;
		   case 'z':
			   if(currentSelectedObjId != -1)
			   {
			   iterations = 0;
			   resetImage(renderCam);
			   geom g = renderScene->objects[currentSelectedObjId];
			   g.translations[targetFrame].z+=0.2;
			   glm::mat4 transform = utilityCore::buildTransformationMatrix(g.translations[targetFrame],g.rotations[targetFrame],g.scales[targetFrame]);
			   g.transforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(transform);
			   g.inverseTransforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(glm::inverse(transform));
			   }
			   break;

		   case 'Z':
			   if(currentSelectedObjId != -1)
			   {
			   iterations = 0;
			   resetImage(renderCam);
			   geom g = renderScene->objects[currentSelectedObjId];
			   g.translations[targetFrame].z-=0.2;
			   glm::mat4 transform = utilityCore::buildTransformationMatrix(g.translations[targetFrame],g.rotations[targetFrame],g.scales[targetFrame]);
			   g.transforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(transform);
			   g.inverseTransforms[targetFrame] = utilityCore::glmMat4ToCudaMat4(glm::inverse(transform));
			   }
			   break;

		   case 't':
			   textureMode = !textureMode;
			   iterations = 0;
			   resetImage(renderCam);
			   break;

		   case 'h':
			   if(currentSelectedObjId != -1 && textureMode)
			   {
			   iterations = 0;
			   resetImage(renderCam);
			   geom g = renderScene->objects[currentSelectedObjId];
			   material mtl = renderScene->materials[g.materialid];
			   if ( (renderScene->maps[mtl.mapID].width1+0.1f) > 0);
				renderScene->maps[mtl.mapID].width1+=0.1f;			   }
			   break;
		   case 'j':
			   if(currentSelectedObjId != -1 && textureMode)
			   {
			   iterations = 0;
			   resetImage(renderCam);
			   geom g = renderScene->objects[currentSelectedObjId];
			   material mtl = renderScene->materials[g.materialid];
			   if ( (renderScene->maps[mtl.mapID].width1-0.1f) > 0);
				renderScene->maps[mtl.mapID].width1-=0.1f;
			   }
			   break;
		   case 'k':
			   if(currentSelectedObjId != -1 && textureMode)
			   {
			   iterations = 0;
			   resetImage(renderCam);
			   geom g = renderScene->objects[currentSelectedObjId];
			   material mtl = renderScene->materials[g.materialid];
			   if ( (renderScene->maps[mtl.mapID].width2+0.1f) > 0);
				renderScene->maps[mtl.mapID].width2+=0.1f;
			   }
			   break;
		   case 'l':
			   if(currentSelectedObjId != -1 && textureMode)
			   {
			   iterations = 0;
			   resetImage(renderCam);
			   geom g = renderScene->objects[currentSelectedObjId];
			   material mtl = renderScene->materials[g.materialid];
			   if ( (renderScene->maps[mtl.mapID].width2-0.1f) > 0);
				renderScene->maps[mtl.mapID].width2-=0.1f;
			   }
			   break;

		}
	}

#endif



//-------------------------------
//----------SETUP STUFF----------
//-------------------------------

#ifdef __APPLE__
	void init(){

		if (glfwInit() != GL_TRUE){
			shut_down(1);      
		}

		// 16 bit color, no depth, alpha or stencil buffers, windowed
		if (glfwOpenWindow(width, height, 5, 6, 5, 0, 0, 0, GLFW_WINDOW) != GL_TRUE){
			shut_down(1);
		}

		// Set up vertex array object, texture stuff
		initVAO();
		initTextures();
	}
#else
	void init(int argc, char* argv[]){
		glutInit(&argc, argv);
		glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
		glutInitWindowSize(width, height);
		glutCreateWindow("565Raytracer");

		// Init GLEW
		glewInit();
		GLenum err = glewInit();
		if (GLEW_OK != err)
		{
			/* Problem: glewInit failed, something is seriously wrong. */
			std::cout << "glewInit failed, aborting." << std::endl;
			exit (1);
		}

		initVAO();
		initTextures();
	}
#endif

void initPBO(GLuint* pbo){
  if (pbo) {
    // set up vertex data parameter
    int num_texels = width*height;
    int num_values = num_texels * 4;
    int size_tex_data = sizeof(GLubyte) * num_values;
    
    // Generate a buffer ID called a PBO (Pixel Buffer Object)
    glGenBuffers(1,pbo);
    // Make this the current UNPACK buffer (OpenGL is state-based)
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, *pbo);
    // Allocate data for the buffer. 4-channel 8-bit image
    glBufferData(GL_PIXEL_UNPACK_BUFFER, size_tex_data, NULL, GL_DYNAMIC_COPY);
    cudaGLRegisterBufferObject( *pbo );
  }
}

void initCuda(){
  // Use device with highest Gflops/s
  cudaGLSetGLDevice( compat_getMaxGflopsDeviceId() );

  initPBO(&pbo);

  // Clean up on program exit
  atexit(cleanupCuda);

  runCuda();
}

void initTextures(){
    glGenTextures(1,&displayImage);
    glBindTexture(GL_TEXTURE_2D, displayImage);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA,
        GL_UNSIGNED_BYTE, NULL);
}

void initVAO(void){
    GLfloat vertices[] =
    { 
        -1.0f, -1.0f, 
         1.0f, -1.0f, 
         1.0f,  1.0f, 
        -1.0f,  1.0f, 
    };

    GLfloat texcoords[] = 
    { 
        1.0f, 1.0f,
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f
    };

    GLushort indices[] = { 0, 1, 3, 3, 1, 2 };

    GLuint vertexBufferObjID[3];
    glGenBuffers(3, vertexBufferObjID);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObjID[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)positionLocation, 2, GL_FLOAT, GL_FALSE, 0, 0); 
    glEnableVertexAttribArray(positionLocation);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObjID[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
    glVertexAttribPointer((GLuint)texcoordsLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(texcoordsLocation);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vertexBufferObjID[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
}

GLuint initShader(const char *vertexShaderPath, const char *fragmentShaderPath){
    GLuint program = glslUtility::createProgram(vertexShaderPath, fragmentShaderPath, attributeLocations, 2);
    GLint location;

    glUseProgram(program);
    
    if ((location = glGetUniformLocation(program, "u_image")) != -1)
    {
        glUniform1i(location, 0);
    }

    return program;
}

//-------------------------------
//---------CLEANUP STUFF---------
//-------------------------------

void cleanupCuda(){
  if(pbo) deletePBO(&pbo);
  if(displayImage) deleteTexture(&displayImage);
}

void deletePBO(GLuint* pbo){
  if (pbo) {
    // unregister this buffer object with CUDA
    cudaGLUnregisterBufferObject(*pbo);
    
    glBindBuffer(GL_ARRAY_BUFFER, *pbo);
    glDeleteBuffers(1, pbo);
    
    *pbo = (GLuint)NULL;
  }
}

void deleteTexture(GLuint* tex){
    glDeleteTextures(1, tex);
    *tex = (GLuint)NULL;
}
 
void shut_down(int return_code){
  #ifdef __APPLE__
	glfwTerminate();
  #endif
  exit(return_code);
}

void resetImage(camera* renderCam)
{
	for (int x=0; x<renderCam->resolution.x;++x)
		for(int y=0; y<renderCam->resolution.y;++y)
		{
			int index = x + (renderCam->resolution.x*y);
			renderCam->image[index] = glm::vec4(0,0,0,-1);
			renderCam->objIdBuffer[index] = -1;
		}
}