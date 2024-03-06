#version 450 core

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

uniform vec3 u_SVOCenter;
uniform vec3 u_SVOExtent;
uniform vec3 u_SVOAABBMin;
uniform vec3 u_SVOAABBMax;
uniform int u_SVOMaxLevelCount;
uniform ivec3 u_voxelPoolSize;

struct VoxelFragment
{
	vec4 position; 
};

layout (std430, binding = 0) buffer VoxelFragmentBuffer {
	VoxelFragment voxelFragmentList[];
};

struct VoxelFragmentPointer
{
	int prev;
	int next;
	int padding0;
	int padding1;
};

layout (std430, binding = 2) buffer VoxelFragmentPointerBuffer {
	VoxelFragmentPointer voxelFragmentPointers[];
};

struct SVONode
{
	ivec4 coord;
	uint bSubdivide;
	uint bLeaf;
	uint childIndex;
	uint padding;
};

layout (std430, binding = 1) buffer SVONodeBuffer {
	SVONode nodes[];
};

uniform layout(binding = 0, r32i) iimage3D u_perLeafVoxelFragmentHeadPointer;
uniform layout(binding = 1, r32i) iimage3D u_voxelFragmentLinkedListSemaphore;

// uv should be in normalized 3d uv space
SVONode descendSVO(vec3 uv, int level, inout uint nodeIndex)
{
	SVONode node = nodes[0];
	nodeIndex = 0;

	for (int i = 0; i < level; ++i)
	{
		// stop descending when reaching an empty node
		if (node.bSubdivide < 1 || node.bLeaf > 0)
		{
			break;
		}

		int childLevel = node.coord.w + 1;
		ivec3 childLevelSize = ivec3(pow(2, childLevel));

		ivec3 firstChildCoord = 2 * node.coord.xyz;
		ivec3 childNodeCoord = ivec3(floor(uv * childLevelSize)) - firstChildCoord;
		int childOffset = childNodeCoord.z * 4 + childNodeCoord.y * 2 + childNodeCoord.x;
		uint childIndex = node.childIndex + childOffset; 
		node = nodes[childIndex];
		nodeIndex = childIndex;
	}

	return node;
}

ivec3 calcVoxelCoord(int index, ivec3 voxelPoolSize)
{
	int z = index / (voxelPoolSize.x * voxelPoolSize.y);
	int y = (index % (voxelPoolSize.x * voxelPoolSize.y)) / voxelPoolSize.x;
	int x = (index % (voxelPoolSize.x * voxelPoolSize.y)) % voxelPoolSize.x;
	return ivec3(x, y, z);
}

void main()
{
	// fetch the fragment for this thread
	int voxelFragmentIndex = int(gl_GlobalInvocationID.x);
	VoxelFragment fragment = voxelFragmentList[voxelFragmentIndex];

	// calculate that normalized voxel fragment uv 
	vec3 SVOOrigin = u_SVOCenter + vec3(-u_SVOExtent.xy, u_SVOExtent.z);
	vec3 uv = (fragment.position.xyz - SVOOrigin) / (2.f * u_SVOExtent);
	uv.z *= -1.f;

	// descending into the SVO to the bottom most level
	uint outNodeIndex = 0;
	SVONode node = descendSVO(uv, u_SVOMaxLevelCount - 1, outNodeIndex);

	if (node.bLeaf > 0 && node.bSubdivide > 0)
	{
		ivec3 voxelCoord = calcVoxelCoord(int(node.childIndex), u_voxelPoolSize);

		// handles synchronization between threads
		while (true)
		{
			// grab the virtual semaphore
			int insertingFragment = imageAtomicCompSwap(u_voxelFragmentLinkedListSemaphore, voxelCoord, -1, voxelFragmentIndex);
			if (insertingFragment == voxelFragmentIndex)
			{
			#if 0
				// inserting into the linked list here
				int headPointerIndex = imageLoad(u_perLeafVoxelFragmentHeadPointer, voxelCoord).r;

				// todo: the code never goes into this branch since I never clear header to -1
				if (headPointerIndex == -1)
				{
					imageStore(u_perLeafVoxelFragmentHeadPointer, voxelCoord, ivec4(voxelFragmentIndex));
					voxelFragmentPointers[voxelFragmentIndex].prev = -1;
					voxelFragmentPointers[voxelFragmentIndex].next = -1;
				}
				else
				{
					// traverse the linked list and insert 
					VoxelFragmentPointer pointer = voxelFragmentPointers[headPointerIndex];
					int pointerIndex = headPointerIndex;
					// while (pointer.next > 0)
					{
						pointerIndex = pointer.next;
						pointer = voxelFragmentPointers[pointer.next];
					}

					// pointer now stops at the tail of the linked list
					pointer.next = voxelFragmentIndex;
					VoxelFragmentPointer newTail = voxelFragmentPointers[voxelFragmentIndex];
					newTail.prev = pointerIndex;
				}
			#endif

				// when done, release the virtual semaphore
				imageAtomicCompSwap(u_voxelFragmentLinkedListSemaphore, voxelCoord, voxelFragmentIndex, -1);
				imageAtomicAdd(u_perLeafVoxelFragmentHeadPointer, voxelCoord, 3);
				break;
			}
		}
	}
}
