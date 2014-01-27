__kernel void glk(__write_only image2d_t A, float x) {
	// get work-item Unique ID
	int idx_x = get_global_id(0);
	int idx_y = get_global_id(1);

	int2 coord = (int2)(idx_x,idx_y);
	float4 color = (float4)(x,0,1,1);
	write_imagef(A, coord, color);
}
