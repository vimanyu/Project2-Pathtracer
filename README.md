-------------------------------------------------------------------------------
CUDA Pathtracer
-------------------------------------------------------------------------------

-------------------------------------------------------------------------------

-------------------------------------------------------------------------------
INTRODUCTION:
-------------------------------------------------------------------------------
This is an implementation of path tracing rendering algorithm on the GPU, using CUDA.

Path tracing is considered to be the best known solution to the classic rendering equation. 

The scary looking integral in the rendering equation is approximated beautifully by Monte Carlo sampling which also implies that in order to reduce our approximation errors, we need to take a really large number of samples.

-------------------------------------------------------------------------------
CONTENTS:
-------------------------------------------------------------------------------
The Project2 root directory contains the following subdirectories:
	
* src/ contains the source code for the project. Both the Windows Visual Studio solution and the OSX makefile reference this folder for all source; the base source code compiles on OSX and Windows without modification.
* scenes/ contains some example scene files that were used to create the renders which can be found in the "renders/" subdir
* renders/ contains a few renders showcasing the abilities of my path tracer 
* PROJ1_WIN/ contains a Windows Visual Studio 2010 project and all dependencies needed for building and running on Windows 7.
* PROJ1_OSX/ contains a OSX makefile, run script, and all dependencies needed for building and running on Mac OSX 10.8. 
* PROJ1_NIX/ contains a Linux makefile for building and running on Ubuntu 
  12.04 LTS. Note that you will need to set the following environment
  variables: 
    
  - PATH=$PATH:/usr/local/cuda-5.5/bin
  - LD_LIBRARY_PATH=/usr/local/cuda-5.5/lib64:/lib

-------------------------------------------------------------------------------
FEATURES:
-------------------------------------------------------------------------------
Path tracing lends itself naturally to a lot of generally "sought" after features in rendering.
We can implement these features at almost no extra cost. The following is the list of features of my path tracer. 

Features that you get for free with path tracing
* Supersampling
* Area lighting and soft shadows
* Full global illumination

Features that I implemented additionally
* Stream compaction on the GPU
* A generic BRDF model that supports diffuse, specular and transmittive surfaces (or a combination of those)
* Fresnel reflections and refractions
* Translational motion blur
* Depth of field based on aperture and focal plane
* Procedural textures
* User interaction with the scene

-------------------------------------------------------------------------------
FEATURES (in a bit more detail)
-------------------------------------------------------------------------------
* **Stream compaction on the GPU**: This is basically a technique that prunes out inactive rays after every bounce. This helps in reducing the number of threads for the next bounce, and hence can achieve speedups especially when we have a large number of rays that don't hit anything in the scene. Currently I am using "thrust" library for this task.

* **BRDF model:** : Here is an example of results from a scene having surfaces that are both diffuse and specular at the same time.
The way the BRDF deals with it is a russian roulette based on probabilities. Simply put, if a material is 70% diffuse, at every iteration, draw a random number from 0 to 1, and if its less than 0.7, then calculate diffuse reflection, else specular reflection.
![alt tag](https://raw.github.com/vimanyu/Project2-Pathtracer/master/renders/pool.0.bmp)

* **Fresnel reflection and refractions:**
This physically based model tells you the amount of reflection or transmittance that takes place at given incoming directions of rays on the material.
Notice that the sphere at the bottom refracts and reflects based on angles. If we do not use fresnel, then we will see either of those phenomena on the sphere but not both.
![alt tag](https://raw.github.com/vimanyu/Project2-Pathtracer/master/renders/refr_refl_fresnel.bmp)

* **Translational motion blur:**
The idea is to do temporal sampling in addition to spatial sampling. Given a frame, go back in time, and linearly (could be cubic or higher order) interpolate between the positions of geometry now and then.
![alt tag](https://raw.github.com/vimanyu/Project2-Pathtracer/master/renders/motion_blurred.bmp)

* **Depth of field:**
Based on the aperture and focal plane specified as camera attributes in the config file.
The idea is to jitter the rays in such a manner that the jittering is least at the focal plane region.
![alt tag](https://raw.github.com/vimanyu/Project2-Pathtracer/master/renders/dof_image.bmp)

* **Procedural textures:**
I cannot be more fascinated by the beauty of procedural textures (two things that amazed me the most during this assignment are Monte Carlo sampling and Perlin Noise. 
So beautiful!). I have implemented a few basic procedural textures for the path tracer -Vertical stripes texture,Horizontal stripes texture, Checkerboard texture ,Marble texture , and a generic Perlin texture
And then, it was time to play with these parameters and just appreciate mathematics.
![alt tag](https://raw.github.com/vimanyu/Project2-Pathtracer/master/renders/perlin_texture.bmp)

As you can see, this was too much fun. So here is an image with everything textured using perlin textures.
![alt tag](https://raw.github.com/vimanyu/Project2-Pathtracer/master/renders/perlin_earth.bmp)

* **User interaction with the path tracer:**
According to me, this was the coolest part of the assignment. User interaction with photorealistic rendering algorithms are best avoided, but hey, we have the GPU! And we have inherently formulated the path tracing algorithm in a parallel and iterative fashion. This makes it idea for user interaction.
There are three types of user interactions supported.

1. Interactive "look-at" depth of field: 
   The user can focus on any part of the image by using the right mouse button. Just click and let it be in focus! 
2. Interactive positioning of objects on screen: 
   LMB on any object in the scene, and use the following keys to move the object
   + x : move in positive x-direction
   + X : move in negative x-direction
   + y : move in positive y-direction
   + Y : move in negative y-direction
   + z : move in positive z-direction
   + Z : move in negative z-direction
3. Interacitve editing of procedural textures : 
   Press "t" on the keyboard to enter/exit the world of procedural textures. This feature grew out of necessity. It was taking me quite a while to come up with good settings for the "attempted" earth procedural texture above. So, I thought it might be covenient to do this visually.
Finally, I ended up implementing this feature as an integral part of path tracer. We can think of it like this. If you don't like the way, your procedural textures are rendering in your path tracer, you can hit 't' and set your textures as per your liking, and then hit 't' again to path trace with these textures.
A "Texture session"  basically shows a flat color render without any lighting.  The user can tweak values (see the keyboard controls given below) and interactively see if he likes any of the textures. Right now, the interface doesn't cover "all" the parameters, so you will be basically see derivations or variations from an existing configuration. For example, you can't change the colors, but can change the pattern.

Last but not the least, since these are interactive features, images will not be able to describe them completely. Please see the **video of these features**at this link,

https://vimeo.com/76013561

**SECRET SAUCE** behind these user interactive features,
In the first iteration and the first bounce, I am capturing the depth and the object IDs at every pixel. 
	+ "Depth map" : This helps in interactive DOF. We just set the focal plane to the value of the clicked pixel in the depth buffer
	+ "OBJID map" : This is how I identify which pixels correspond to which objects. Basis of both interactive textures and interactive translations features.
Here is an example depth map,
![alt tag](https://raw.github.com/vimanyu/Project2-Pathtracer/master/renders/depthMap_pool.bmp)

-------------------------------------------------------------------------------
BUILDING AND RUNNING THE CODE
-------------------------------------------------------------------------------

New additional command line options:
dof, mblur,textureMode

Eg: 
565Pathtracer.exe scene="../testScene.txt" dof=1 mblur=0 
565Pathtracer.exe scene="../testScene.txt" textureMode=1 

There are a few example scene files under the "scenes/" directory.
Desicription of the scene file format can be found at 
https://github.com/vimanyu/Project1-RayTracer under the heading "TAKUAscene format".

Here are a few things that I added to the existing TAKUAscene format.

* Map : These are basically texture maps which are bound to materials. Materials have an attribute called "MAPID" that links the material to the map.
Different types of maps are supported, and they have the following common attributes (though for some maps, the meanings of the attributes are not self-explanatory).

MAP (int)  	 // Map header

type        	 //Type of map

COL1 (vec3)      // Primary color 

COL2 (vec3)      // Secondary color

WIDTH1 (float)   // Width of the primary color

WIDTH2  (float)  // Width of the secondary color

SMOOTH  (int)    // Smooth interpolation between the two colors

Types of maps:
1.base 
For base maps, the options are more or less non-functional. Base maps are a special kind of map which the materials use to specify that all the properties come from the material, and none from the map.
This is in effect a dummy map

2.hstripe:
For horizontal stripes. The attributes perform expected actions. Width2 is ignored.

3.vstripe:
For vertical stripes. The attributes perform expected actions. Width2 is ignored.

4.checkerboard:
For 2D grid textures. All the attributes are useful.

5.marble
For marble looking patterns. The colors are expected, but the width1 and width2 settings have an effect on the turbulence and the amplitude of noise functions

6.perlin
A generic perlin function.

-------------------------------------------------------------------------------
KEYBOARD CONTROLS
-------------------------------------------------------------------------------
   + x : move in positive x-direction
   + X : move in negative x-direction
   + y : move in positive y-direction
   + Y : move in negative y-direction
   + z : move in positive z-direction
   + Z : move in negative z-direction
   + h : increase width1 of selected object's map
   + j : decrease width1 of selected object's map
   + k : increase width2 of selected object's map
   + l : decrease width2 of selected object's map

-------------------------------------------------------------------------------
PERFORMANCE EVALUATION
-------------------------------------------------------------------------------
For performance tests, I started with testing my code with tiles of different sizes.
Tested this with two image sizes  - 800 X 800 and 1280 X 720 on a machine equipped with 
Nvidia GT 650M.

For resolution 800 X 800, (sampleScene.txt)

Tile Size | Average time per iteration (ms)
--- | --- 
4|447
8|208
16|224
32|245

For resolution 1280 X 720, (sampleScene.txt)

Tile Size | Average time per iteration (ms)
--- | --- 
4|522
8|243
16|264
32|292

So it looked like the tile size of 8 was the most optimum.

Next I went on to see the impact of stream compaction. Got quite a mixed set of results here.

At first, my code without stream compaction ran faster than with stream compaction. This made me think that the 
cost of compacting the array was more than having idle threads lying around. This was done on the provided example file
sampleScene.txt.

Stream Compaction | Average time per iteration (ms)
--- | --- 
ON|208
OFF|184

This made me believe that the benefits of stream compaction are seen when we have a lot of rays without any intersections.
So I removed the left wall from the scene

Stream Compaction | Average time per iteration (ms)
--- | --- 
ON|143
OFF|134

Then I removed the right wall as well,

Stream Compaction | Average time per iteration (ms)
--- | --- 
ON| 91
OFF|101

Final tests were done with different types of memory. My implementation has an array of 512 random integers that must be passed to the gpu for perlin computations.

So, I first moved them to constant memory on gpu, using __constant__declaration and cudaMemcpyToSymbol

Type of memory for perlin array | Average time per iteration (ms)
--- | --- 
Global| 309
Constant|342
Shared|224

Though shared memory implementation was crashing for me after a few iterations. 
But on my slow machine ( 8600m gt), constant memory showed a dramatic improvement for the same scene.
From 5600 ms to 4400 ms.

Apart from these, in the implementation, I still feel there are places where I can avoid normalizing operations if I am sure the input vectors are normalized.
Also, for texture mode implementatoin, I thought it was best to duplicate and trim down the ray tracing code so to avoid branching in gpu and to run only as much as needed.

-------------------------------------------------------------------------------
THIRD PARTY CODE CREDITS
-------------------------------------------------------------------------------
* Perlin noise function: http://www.codermind.com/articles/Raytracer-in-C++-Part-III-Textures.html
