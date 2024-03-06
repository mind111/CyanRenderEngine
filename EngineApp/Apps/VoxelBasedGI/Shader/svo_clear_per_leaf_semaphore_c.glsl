#version 450 core

layout (local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

uniform ivec3 u_semaphorePoolSize;
uniform layout(binding = 0, r32i) iimage3D u_perLeafVoxelFragmentHeadPointer;
uniform layout(binding = 1, r32i) iimage3D u_voxelFragmentLinkedListSemaphore;

void main()
{
	if (gl_GlobalInvocationID.x >= u_semaphorePoolSize.x || gl_GlobalInvocationID.y >= u_semaphorePoolSize.y || gl_GlobalInvocationID.z >= u_semaphorePoolSize.z)
	{
		return;
	}

	// clear head pointer to -1
	imageStore(u_perLeafVoxelFragmentHeadPointer, ivec3(gl_GlobalInvocationID), ivec4(-1));
	// clear per leaf node semaphore to -1
	imageStore(u_voxelFragmentLinkedListSemaphore, ivec3(gl_GlobalInvocationID), ivec4(-1));
}
