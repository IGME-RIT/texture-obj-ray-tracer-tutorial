Documentation Author: Niko Procopi 2019

This tutorial was designed for Visual Studio 2017 / 2019
If the solution does not compile, retarget the solution
to a different version of the Windows SDK. If you do not
have any version of the Windows SDK, it can be installed
from the Visual Studio Installer Tool

Welcome to the Ray Tracing Textures Tutorial!
Prerequesites: 
	Ray Tracing Smooth Normals
	Materials ("More Graphics" section)

In this tutorial, we take the method of interpolating normals 
in the fragment shader, and apply it to interpolating UV coordinates
for textures. We add texture coordinates to each triangle in main.cpp
(for cube and plane), then we load texture coordinates for the car that
are inside the OBJ. On line 517 on main.cpp, you can change the 
texture from texture.jpg (template) to CarColor.png (color of the car).
If you want to use the car's proper texture, you can change the car's
polygon color to white on line 466.

In the compute shader and fragment shader, we now have UV added to
the triangle structure. The compute shader just exports the UV to the 
fragment shader, without adjustments. The Fragment Shader now has
a function called GetInterpolatedUV, which works exactly the same as
GetInterpolatedNormal. There might be a way to combine these functions
into one function.

We also now have a function called getSurfaceColor, which multiplies
the color of the polygon (triangle color), and the color that we get from
the texture, with the interpolated UV
