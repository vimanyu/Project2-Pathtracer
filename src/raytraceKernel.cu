// CIS565 CUDA Raytracer: A parallel raytracer for Patrick Cozzi's CIS565: GPU Computing at the University of Pennsylvania
// Written by Yining Karl Li, Copyright (c) 2012 University of Pennsylvania
// This file includes code from:
//       Rob Farber for CUDA-GL interop, from CUDA Supercomputing For The Masses: http://www.drdobbs.com/architecture-and-design/cuda-supercomputing-for-the-masses-part/222600097
//       Peter Kutz and Yining Karl Li's GPU Pathtracer: http://gpupathtracer.blogspot.com/
//       Yining Karl Li's TAKUA Render, a massively parallel pathtracing renderer: http://www.yiningkarlli.com

#include <stdio.h>
#include <cuda.h>
#include <cmath>
#include "sceneStructs.h"
#include "utilities.h"
#include "raytraceKernel.h"
#include "intersections.h"
#include "interactions.h"
#include <vector>
#include "glm/glm.hpp"

void checkCUDAError(const char *msg) {
  cudaError_t err = cudaGetLastError();
  if( cudaSuccess != err) {
    fprintf(stderr, "Cuda error: %s: %s.\n", msg, cudaGetErrorString( err) ); 
    exit(EXIT_FAILURE); 
  }
} 

//LOOK: This function demonstrates how to use thrust for random number generation on the GPU!
//Function that generates static.
__host__ __device__ glm::vec3 generateRandomNumberFromThread(glm::vec2 resolution, float time, int x, int y){
  int index = x + (y * resolution.x);
   
  thrust::default_random_engine rng(hash(index*time));
  thrust::uniform_real_distribution<float> u01(0,1);

  return glm::vec3((float) u01(rng), (float) u01(rng), (float) u01(rng));
}

////Kernel that does the initial raycast from the camera.
//__host__ __device__ ray raycastFromCameraKernel(glm::vec2 resolution, float time, int x, int y, glm::vec3 eye, glm::vec3 view, glm::vec3 up, glm::vec2 fov){
//   
//  int index = x + (y * resolution.x);
//   
//  thrust::default_random_engine rng(hash(index*time));
//  thrust::uniform_real_distribution<float> u01(0,1);
//  
//  //standard camera raycast stuff
//  glm::vec3 E = eye;
//  glm::vec3 C = view;
//  glm::vec3 U = up;
//  float fovx = fov.x;
//  float fovy = fov.y;
//  
//  float CD = glm::length(C);
//  
//  glm::vec3 A = glm::cross(C, U);
//  glm::vec3 B = glm::cross(A, C);
//  glm::vec3 M = E+C;
//  glm::vec3 H = (A*float(CD*tan(fovx*(PI/180))))/float(glm::length(A));
//  glm::vec3 V = (B*float(CD*tan(-fovy*(PI/180))))/float(glm::length(B));
//  
//  float sx = (x)/(resolution.x-1);
//  float sy = (y)/(resolution.y-1);
//  
//  glm::vec3 P = M + (((2*sx)-1)*H) + (((2*sy)-1)*V);
//  glm::vec3 PmE = P-E;
//  glm::vec3 R = E + (float(200)*(PmE))/float(glm::length(PmE));
//  
//  glm::vec3 direction = glm::normalize(R);
//  //major performance cliff at this point, TODO: find out why!
//  ray r;
//  r.origin = eye;
//  r.direction = direction;
//  return r;
//}

//Function that does the initial raycast from the camera
__host__ __device__ ray raycastFromCameraKernel(glm::vec2 resolution, float time, int x, int y, glm::vec3 eye, glm::vec3 view, glm::vec3 up, glm::vec2 fov){
  
  ray r;
  float theta = fov.x*PI/180.0f;
  float phi = fov.y*PI/180.0f;

  glm::vec3 A = glm::cross(view,up);
  glm::vec3 B = glm::cross(A,view);
  glm::vec3 M = eye + view;
  glm::vec3 H = glm::normalize(A)*glm::length(view)*tan(theta);
  glm::vec3 V = glm::normalize(B)*glm::length(view)*tan(phi);

  float sx= -1.0f; //(float)x/(resolution.x-1);
  float sy = -1.0f;//1.0 - (float)y/(resolution.y-1);
  
  thrust::default_random_engine rng(hash(43231*time));
  thrust::uniform_real_distribution<float> u01(-0.95,0.95);

  while( sx<=0.0f || sx>=1.0f || sx<=0.0f || sy>=1.0f)
	{
		float xrand = x + u01(rng);
		float yrand = y + u01(rng);
		sx = xrand/(resolution.x-1);
		sy = 1.0f - yrand/ (resolution.y-1);
	}
  glm::vec3 P = M + (2*sx-1)*H + (2*sy - 1)*V;
  r.origin = eye;
  r.direction = glm::normalize(P-eye);
  return r;
}


//Kernel that blacks out a given image buffer
__global__ void clearImage(glm::vec2 resolution, glm::vec3* image){
    int x = (blockIdx.x * blockDim.x) + threadIdx.x;
    int y = (blockIdx.y * blockDim.y) + threadIdx.y;
    int index = x + (y * resolution.x);
    if(x<=resolution.x && y<=resolution.y){
      image[index] = glm::vec3(0,0,0);
    }
}

//Kernel that writes the image to the OpenGL PBO directly. 
__global__ void sendImageToPBO(uchar4* PBOpos, glm::vec2 resolution, glm::vec3* image, int iterations){
  
  int x = (blockIdx.x * blockDim.x) + threadIdx.x;
  int y = (blockIdx.y * blockDim.y) + threadIdx.y;
  int index = x + (y * resolution.x);
  
  if(x<=resolution.x && y<=resolution.y){

      glm::vec3 color;      
      color.x = image[index].x*255.0;
      color.y = image[index].y*255.0;
      color.z = image[index].z*255.0;

      if(color.x>255){
        color.x = 255;
      }

      if(color.y>255){
        color.y = 255;
      }

      if(color.z>255){
        color.z = 255;
      }
      
      // Each thread writes one pixel location in the texture (textel)
	  if (iterations == 0)
	  {
      PBOpos[index].w = 0;
      PBOpos[index].x = color.x;     
      PBOpos[index].y = color.y;
      PBOpos[index].z = color.z;
	  }

	  else
	  {
		  int currentIteration = iterations+1;
		  PBOpos[index].x *= (iterations);
		  PBOpos[index].y *= (iterations);
		  PBOpos[index].z *= (iterations);

		  PBOpos[index].x = (PBOpos[index].x+color.x)/currentIteration;
		  PBOpos[index].y = (PBOpos[index].y+color.y)/currentIteration;
		  PBOpos[index].z = (PBOpos[index].z+color.z)/currentIteration;
	  }
  }
}

__device__ bool isLight(int objId, int* lights, int numberOfLights)
{
	for (int i=0; i<numberOfLights; ++i)
		if (lights[i] == objId)
			return true;
	return false;
}

//Kernel that blacks out a given image buffer
__global__ void clearActiveRays(glm::vec2 resolution, ray* rays, glm::vec3* image){
    int x = (blockIdx.x * blockDim.x) + threadIdx.x;
    int y = (blockIdx.y * blockDim.y) + threadIdx.y;
    int index = x + (y * resolution.x);
    if(x<=resolution.x && y<=resolution.y && rays[index].active){
       image[index] = glm::vec3(0,0,0);
    }
}

//TODO: IMPLEMENT THIS FUNCTION
//Core raytracer kernel
__global__ void raytraceRay(glm::vec2 resolution, float time, float bounce, cameraData cam, int rayDepth, glm::vec3* colors, 
                            staticGeom* geoms, int numberOfGeoms, material* materials, int numberOfMaterials, int* lights, int numberOfLights,ray* rays){

  int x = (blockIdx.x * blockDim.x) + threadIdx.x;
  int y = (blockIdx.y * blockDim.y) + threadIdx.y;
  int index = x + (y * resolution.x);

  ray r;
  if (bounce==1)
  {
	r = raycastFromCameraKernel(resolution, time, x, y, cam.position, cam.view, cam.up, cam.fov);
	r.active = true;
	r.pixelIndex = glm::vec2(x,y);
	colors[index] = glm::vec3(1,1,1); //White initially
	rays[index].origin = r.origin;
	rays[index].direction = r.direction;
	rays[index].active = r.active;
	rays[index].pixelIndex = r.pixelIndex;
  }
  else
  {
	  r = rays[index];
	  x = r.pixelIndex.x;
	  y = r.pixelIndex.y;
	  index = x + (y * resolution.x);
  }

  if((x<=resolution.x && y<=resolution.y && r.active)){
	glm::vec3 intersectionPoint;
	glm::vec3 intersectionNormal;

	int objId = findNearestGeometricIntersection(r,intersectionPoint,intersectionNormal,geoms,numberOfGeoms);

	if (objId == -1)
	{
		r.active = false;
		rays[index].active = false;
		colors[index] = glm::vec3(0,0,0);
		return;
	}
	material mtl = materials[geoms[objId].materialid];
	if (isLight(objId,lights,numberOfLights))
	{
		r.active = false;
		rays[index].active = false;
		colors[index].x *= mtl.color.x;
		colors[index].y *= mtl.color.y;
		colors[index].z *= mtl.color.z;
		return;
	}

	glm::vec3 emittedColor;
	glm::vec3 unabsorbedColor;
	int bsdf = calculateBSDF(r,intersectionPoint,intersectionNormal,emittedColor,colors[index],unabsorbedColor,mtl,time);
	
	if (bsdf == 0)
	{		
		colors[index].x *= mtl.color.x;
		colors[index].y *= mtl.color.y;
		colors[index].z *= mtl.color.z;
	}
	else if(bsdf == 1)
	{
		colors[index].x *= mtl.specularColor.x;
		colors[index].y *= mtl.specularColor.y;
		colors[index].z *= mtl.specularColor.z;
	}
	else if (bsdf == 2)
	{
		colors[index].x = mtl.color.x;
		colors[index].y = mtl.color.y;
		colors[index].z = mtl.color.z;
	}

	rays[index].origin = r.origin;
	rays[index].direction = r.direction;
	rays[index].active = r.active;
	rays[index].pixelIndex = r.pixelIndex;
	//colors[index] = glm::abs(r.direction);
    //colors[index] = generateRandomNumberFromThread(resolution, time, x, y);
   }
	
}


//TODO: FINISH THIS FUNCTION
// Wrapper for the __global__ call that sets up the kernel calls and does a ton of memory management
void cudaRaytraceCore(uchar4* PBOpos, camera* renderCam, int frame, int iterations, material* materials, int numberOfMaterials, geom* geoms, int numberOfGeoms){
  
  int traceDepth = 1; //determines how many bounces the raytracer traces

  // set up crucial magic
  int tileSize = 8;
  dim3 threadsPerBlock(tileSize, tileSize);
  dim3 fullBlocksPerGrid((int)ceil(float(renderCam->resolution.x)/float(tileSize)), (int)ceil(float(renderCam->resolution.y)/float(tileSize)));
  
  //send image to GPU
  glm::vec3* cudaimage = NULL;
  cudaMalloc((void**)&cudaimage, (int)renderCam->resolution.x*(int)renderCam->resolution.y*sizeof(glm::vec3));
  cudaMemcpy( cudaimage, renderCam->image, (int)renderCam->resolution.x*(int)renderCam->resolution.y*sizeof(glm::vec3), cudaMemcpyHostToDevice);
  
   //package lights
  std::vector<int> lightsVector;

  ray* rays = new ray[ (int)renderCam->resolution.x*(int)renderCam->resolution.y];
  ray* cudarays = NULL;
  cudaMalloc((void**)&cudarays, (int)renderCam->resolution.x*(int)renderCam->resolution.y*sizeof(ray));
  cudaMemcpy( cudarays, rays, (int)renderCam->resolution.x*(int)renderCam->resolution.y*sizeof(ray), cudaMemcpyHostToDevice);

  //package geometry and materials and sent to GPU
  staticGeom* geomList = new staticGeom[numberOfGeoms];
  for(int i=0; i<numberOfGeoms; i++){
    staticGeom newStaticGeom;
    newStaticGeom.type = geoms[i].type;
    newStaticGeom.materialid = geoms[i].materialid;
    newStaticGeom.translation = geoms[i].translations[frame];
    newStaticGeom.rotation = geoms[i].rotations[frame];
    newStaticGeom.scale = geoms[i].scales[frame];
    newStaticGeom.transform = geoms[i].transforms[frame];
    newStaticGeom.inverseTransform = geoms[i].inverseTransforms[frame];
    geomList[i] = newStaticGeom;

	if (materials[geoms[i].materialid].emittance > 0.0f)
		lightsVector.push_back(i);
  }
  

  staticGeom* cudageoms = NULL;
  cudaMalloc((void**)&cudageoms, numberOfGeoms*sizeof(staticGeom));
  cudaMemcpy( cudageoms, geomList, numberOfGeoms*sizeof(staticGeom), cudaMemcpyHostToDevice);
  
  material* cudamaterials = NULL;
  cudaMalloc((void**)&cudamaterials, numberOfMaterials*sizeof(material));
  cudaMemcpy( cudamaterials, materials, numberOfMaterials*sizeof(material), cudaMemcpyHostToDevice);

  int numberOfLights = lightsVector.size();
  int* cudalights = NULL;
  cudaMalloc( (void**)&cudalights, numberOfLights*sizeof(int));
  cudaMemcpy(cudalights,&lightsVector,numberOfLights*sizeof(int),cudaMemcpyHostToDevice);
  
  //package camera
  cameraData cam;
  cam.resolution = renderCam->resolution;
  cam.position = renderCam->positions[frame];
  cam.view = renderCam->views[frame];
  cam.up = renderCam->ups[frame];
  cam.fov = renderCam->fov;

  //kernel launches
  for(int bounce = 1; bounce <= 10; ++bounce)
  {
  raytraceRay<<<fullBlocksPerGrid, threadsPerBlock>>>(renderCam->resolution, (float)iterations, (float)bounce, cam, traceDepth, cudaimage, cudageoms, numberOfGeoms, cudamaterials, numberOfMaterials, cudalights,numberOfLights,cudarays);
  }
  //clearActiveRays<<<fullBlocksPerGrid, threadsPerBlock>>>(renderCam->resolution,cudarays, cudaimage);

  sendImageToPBO<<<fullBlocksPerGrid, threadsPerBlock>>>(PBOpos, renderCam->resolution, cudaimage,iterations);

  //retrieve image from GPU
  cudaMemcpy( renderCam->image, cudaimage, (int)renderCam->resolution.x*(int)renderCam->resolution.y*sizeof(glm::vec3), cudaMemcpyDeviceToHost);

  //free up stuff, or else we'll leak memory like a madman
  cudaFree( cudaimage );
  cudaFree( cudageoms );
  cudaFree( cudamaterials );
  cudaFree(cudalights);
  cudaFree(cudarays);
  delete [] geomList;
  delete [] rays;

  // make certain the kernel has completed 
  cudaThreadSynchronize();

  checkCUDAError("Kernel failed!");
}
