/* Arcan-fe, scriptable front-end engine
 *
 * Arcan-fe is the legal property of its developers, please refer
 * to the COPYRIGHT file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "arcan_math.h"
#include <SDL_opengl.h>

static void mult_matrix_vecf(const float matrix[16], const float in[4], float out[4])
{
	int i;

	for (i=0; i<4; i++) {
		out[i] =
		in[0] * matrix[0*4+i] +
		in[1] * matrix[1*4+i] +
		in[2] * matrix[2*4+i] +
		in[3] * matrix[3*4+i];
	}
}

/* good chance for speedup here using SSE intrisics */
void multiply_matrix(float* dst, float* a, float* b)
{
	for (int i = 0; i < 16; i+= 4)
		for (int j = 0; j < 4; j++)
			dst[i+j] =
				b[i]   * a[j]   +
				b[i+1] * a[j+4] +
				b[i+2] * a[j+8] + 
				b[i+3] * a[j+12];
}

void scale_matrix(float* m, float xs, float ys, float zs)
{
	m[0] *= xs; m[4] *= ys; m[8] *= zs;
	m[1] *= xs; m[5] *= ys; m[9] *= zs;
	m[2] *= xs; m[6] *= ys; m[10] *= zs;
	m[3] *= xs; m[7] *= ys; m[11] *= zs;
}

void translate_matrix(float* m, float xt, float yt, float zt)
{
	m[12] = m[0] * xt + m[4] * yt + m[8] * zt + m[12];
	m[13] = m[1] * xt + m[5] * yt + m[9] * zt + m[13];
	m[14] = m[2] * xt + m[6] * yt + m[10]* zt + m[14];
	m[15] = m[3] * xt + m[7] * yt + m[11]* zt + m[15];
}

static float midentity[] =
 {1.0, 0.0, 0.0, 0.0,
  0.0, 1.0, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.0, 0.0, 0.0, 1.0};
  
void identity_matrix(float* m)
{
		memcpy(m, midentity, 16 * sizeof(float));
}

void build_orthographic_matrix(float* m, const float left, const float right,
							  const float bottom, const float top, const float near, const float far)
{
	float irml = 1.0 / (right - left);
	float itmb = 1.0 / (top - bottom);
	float ifmn = 1.0 / (far - near);
	
	m[0]  = 2.0f * irml;
	m[1]  = 0.0f;
	m[2]  = 0.0f;
	m[3]  = 0.0; 

	m[4]  = 0.0f;
	m[5]  = 2.0f * itmb;
	m[6]  = 0.0f;
	m[7]  = 0.0; 

	m[8]  = 0.0f;
	m[9]  = 0.0f;
	m[10] = 2.0f * ifmn;
	m[11] = 0.0; 

	m[12] = -(right+left) * irml;
	m[13] = -(top+bottom) * itmb;
	m[14] = -(far+near) * ifmn;
	m[15] = 1.0f;
}

void build_projection_matrix(float* m, float nearv, float farv, float aspect, float fov)
{
	const float h = 1.0f / tan(fov * (M_PI / 360.0));
	float neg_depth = nearv - farv;

	m[0]  = h / aspect; m[1]  = 0; m[2]  = 0;  m[3] = 0;
	m[4]  = 0; m[5]  = h; m[6]  = 0;  m[7] = 0;
	m[8]  = 0; m[9]  = 0; m[10] = (farv + nearv) / neg_depth; m[11] =-1;
	m[12] = 0; m[13] = 0; m[14] = 2.0f * (nearv * farv) / neg_depth; m[15] = 0;
}

int project_matrix(float objx, float objy, float objz,
		   const float modelMatrix[16],
		   const float projMatrix[16],
		   const int viewport[4],
		   float *winx, float *winy, float *winz)
{
	float in[4];
	float out[4];

	in[0]=objx;
	in[1]=objy;
	in[2]=objz;
	in[3]=1.0;

	mult_matrix_vecf(modelMatrix, in, out);
	mult_matrix_vecf(projMatrix, out, in);

	if (in[3] == 0.0)
		return 0;
	
	in[0] /= in[3];
	in[1] /= in[3];
	in[2] /= in[3];
	/* Map x, y and z to range 0-1 */
	in[0] = in[0] * 0.5 + 0.5;
	in[1] = in[1] * 0.5 + 0.5;
	in[2] = in[2] * 0.5 + 0.5;

	/* Map x,y to viewport */
	in[0] = in[0] * viewport[2] + viewport[0];
	in[1] = in[1] * viewport[3] + viewport[1];

	*winx=in[0];
	*winy=in[1];
	*winz=in[2];
	return 1;
}

int pinpoly(int nvert, float *vertx, float *verty, float testx, float testy)
{
	int i, j, c = 0;
	for (i = 0, j = nvert-1; i < nvert; j = i++) {
		if ( ((verty[i]>testy) != (verty[j]>testy)) &&
			(testx < (vertx[j]-vertx[i]) * (testy-verty[i]) / (verty[j]-verty[i]) + vertx[i]) )
			c = !c;
	}
	return c;
}

vector build_vect_polar(const float phi, const float theta)
{
	vector res = {.x = sinf(phi) * cosf(theta),
        .y = sinf(phi) * sinf(theta), .z = sinf(phi)};
	return res;
}

vector build_vect(const float x, const float y, const float z)
{
	vector res = {.x = x, .y = y, .z = z};
	return res;
}

vector mul_vectorf(vector a, float f)
{
	vector res = {.x = a.x * f, .y = a.y * f, .z = a.z * f};
	return res;
}

quat build_quat(float angdeg, float vx, float vy, float vz)
{
	quat ret;
	float ang = angdeg / 180.f * M_PI;
	float res = sinf(ang / 2.0f);

	ret.w = cosf(ang / 2.0f);
	ret.x = vx * res;
	ret.y = vy * res;
	ret.z = vz * res;

	return ret;
}

float len_vector(vector invect)
{
	return sqrt(invect.x * invect.x + invect.y * invect.y + invect.z * invect.z);
}

vector crossp_vector(vector a, vector b)
{
	vector res = {
		.x = a.y * b.z - a.z * b.y,
		.y = a.z * b.x - a.x * b.z,
		.z = a.x * b.y - a.y * b.x
	};
	return res;
}

float dotp_vector(vector a, vector b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

vector add_vector(vector a, vector b)
{
	vector res = {.x = a.x + b.x,
	.y = a.y + b.y,
	.z = a.z + b.z};

	return res;
}

vector mul_vector(vector a, vector b)
{
	vector res = {
		.x = a.x * b.x,
		.y = a.y * b.y,
		.z = a.z * b.z
	};

	return res;
}

vector norm_vector(vector invect){
	vector empty = {.x = 0.0, .y = 0.0, .z = 0.0};
	float len = len_vector(invect);
	if (len < 0.0000001)
		return empty;

	vector res = {
		.x = invect.x * len,
		.y = invect.y * len,
		.z = invect.z * len
	};

	return res;
}

quat inv_quat(quat src)
{
	quat res = {.x = -src.x, .y = -src.y, .z = -src.z, .w = src.w };
    return res;
}

float len_quat(quat src)
{
	return sqrt(src.x * src.x + src.y * src.y + src.z * src.z + src.w * src.w);
}

quat norm_quat(quat src)
{
	float val = src.x * src.x + src.y * src.y + src.z * src.z + src.w * src.w;

	if (val > 0.99999 && val < 1.000001)
		return src;

	val = sqrtf(val);
	quat res = {.x = src.x / val, .y = src.y / val, .z = src.z / val, .w = src.w / val};
	return res;
}

quat div_quatf(quat a, float v)
{
	quat res = {
		.x = a.x / v,
		.y = a.y / v,
		.z = a.z / v,
		.w = a.z / v
	};
	return res;
}

quat mul_quatf(quat a, float v)
{
	quat res = {
		.x = a.x * v,
		.y = a.y * v,
		.z = a.z * v,
		.w = a.w * v
	};
	return res;
}

quat mul_quat(quat a, quat b)
{
	quat res;
	res.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
	res.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
	res.y = a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z;
	res.z = a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x;
	return res;
}

quat add_quat(quat a, quat b)
{
	quat res;
	res.x = a.x + b.x;
	res.y = a.y + b.y;
	res.z = a.z + b.z;
	res.w = a.w + b.w;

	return res;
}

float dot_quat(quat a, quat b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

vector angle_quat(quat a)
{
	float sqw = a.w*a.w;
	float sqx = a.x*a.x;
	float sqy = a.y*a.y;
	float sqz = a.z*a.z;

	vector euler;
	euler.x = atan2f(2.f * (a.x*a.y + a.z*a.w), sqx - sqy - sqz + sqw);
	euler.y = asinf(-2.f * (a.x*a.z - a.y*a.w));
	euler.z = atan2f(2.f * (a.y*a.z + a.x*a.w), -sqx - sqy + sqz + sqw);

	float mpi = 180.0 / M_PI;
	euler.x *= mpi;
	euler.y *= mpi;
	euler.z *= mpi;
	
	return euler;
}

vector lerp_vector(vector a, vector b, float fact)
{
	vector res;
	res.x = a.x + fact * (b.x - a.x);
	res.y = a.y + fact * (b.y - a.y);
	res.z = a.z + fact * (b.z - a.z);
	return res;
}

float lerp_val(float a, float b, float fact)
{
	return a + fact * (b - a);
}

quat lerp_quat(quat a, quat b, float fact)
{
//	printf("a (%f, %f, %f, %f) -- b (%f, %f, %f, %f) - %f\n", a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w, fact);
	quat res = add_quat( mul_quatf(a, 1 - fact), mul_quatf(b, fact) );
	return res;
}

quat nlerp_quat(quat a, quat b, float fact)
{
	return norm_quat(lerp_quat(a, b, fact));
}

float* matr_quatf(quat a, float* dmatr)
{
	if (dmatr){
		dmatr[0] = 1.0f - 2.0f * (a.y * a.y + a.z * a.z);
		dmatr[1] = 2.0f * (a.x * a.y + a.z * a.w);
		dmatr[2] = 2.0f * (a.x * a.z - a.y * a.w);
		dmatr[3] = 0.0f;
		dmatr[4] = 2.0f * (a.x * a.y - a.z * a.w);
		dmatr[5] = 1.0f - 2.0f * (a.x * a.x + a.z * a.z);
		dmatr[6] = 2.0f * (a.z * a.y + a.x * a.w);
		dmatr[7] = 0.0f;
		dmatr[8] = 2.0f * (a.x * a.z + a.y * a.w);
		dmatr[9] = 2.0f * (a.y * a.z - a.x * a.w);
		dmatr[10]= 1.0f - 2.0f * (a.x * a.x + a.y * a.y);
		dmatr[11]= 0.0f;
		dmatr[12]= 0.0f;
		dmatr[13]= 0.0f;
		dmatr[14]= 0.0f;
		dmatr[15]= 1.0f;
	}
	return dmatr;
}

double* matr_quat(quat a, double* dmatr)
{
	if (dmatr){
		dmatr[0] = 1.0f - 2.0f * (a.y * a.y + a.z * a.z);
		dmatr[1] = 2.0f * (a.x * a.y + a.z * a.w);
		dmatr[2] = 2.0f * (a.x * a.z - a.y * a.w);
		dmatr[3] = 0.0f;
		dmatr[4] = 2.0f * (a.x * a.y - a.z * a.w);
		dmatr[5] = 1.0f - 2.0f * (a.x * a.x + a.z * a.z);
		dmatr[6] = 2.0f * (a.z * a.y + a.x * a.w);
		dmatr[7] = 0.0f;
		dmatr[8] = 2.0f * (a.x * a.z + a.y * a.w);
		dmatr[9] = 2.0f * (a.y * a.z - a.x * a.w);
		dmatr[10]= 1.0f - 2.0f * (a.x * a.x + a.y * a.y);
		dmatr[11]= 0.0f;
		dmatr[12]= 0.0f;
		dmatr[13]= 0.0f;
		dmatr[14]= 0.0f;
		dmatr[15]= 1.0f;
	}
	return dmatr;
}

quat build_quat_euler(float roll, float pitch, float yaw)
{
	quat res = mul_quat( mul_quat( build_quat(pitch, 1.0, 0.0, 0.0), build_quat(yaw, 0.0, 1.0, 0.0)), build_quat(roll, 0.0, 0.0, 1.0));
	return res;
}

void update_view(orientation* dst, float roll, float pitch, float yaw)
{
	dst->pitchf = pitch;
	dst->rollf = roll;
	dst->yawf = yaw;
	quat pitchq = build_quat(pitch, 1.0, 0.0, 0.0);
	quat rollq  = build_quat(yaw, 0.0, 1.0, 0.0);
	quat yawq   = build_quat(roll, 0.0, 0.0, 1.0);
	quat res = mul_quat( mul_quat(pitchq, yawq), rollq );
	matr_quatf(res, dst->matr);
}

float lerp_fract(unsigned startt, unsigned endt, float ct)
{
	float startf = (float)startt + EPSILON;
	float endf = (float)endt + EPSILON;

	if (ct > endt)
		ct = endt;

	float cf = ((float)ct - startf + EPSILON);

	return cf / (endf - startf);
}


static inline void normalize_plane(float* pl)
{
	float mag = 1.0f / sqrtf(pl[0] * pl[0] + pl[1] * pl[1] + pl[2] * pl[2]);
	pl[0] *= mag;
	pl[1] *= mag;
	pl[2] *= mag;
	pl[3] *= mag;
}

void update_frustum(float* prjm, float* mvm, float frustum[6][4])
{
	float mmr[16];
/* multiply modelview with projection */
	multiply_matrix(mmr, mvm, prjm);

/* extract and normalize planes */
	frustum[0][0] = mmr[3] + mmr[0]; // left
	frustum[0][1] = mmr[7] + mmr[4];
	frustum[0][2] = mmr[11] + mmr[8];
	frustum[0][3] = mmr[15] + mmr[12];
	normalize_plane(frustum[0]);

	frustum[1][0] = mmr[3] - mmr[0]; // right
	frustum[1][1] = mmr[7] - mmr[4];
	frustum[1][2] = mmr[11] - mmr[8];
	frustum[1][3] = mmr[15] - mmr[12];
	normalize_plane(frustum[1]);

	frustum[2][0] = mmr[3] - mmr[1]; // top
	frustum[2][1] = mmr[7] - mmr[5];
	frustum[2][2] = mmr[11] - mmr[9];
	frustum[2][3] = mmr[15] - mmr[13];
	normalize_plane(frustum[2]);

	frustum[3][0] = mmr[3] + mmr[1]; // bottom
	frustum[3][1] = mmr[7] + mmr[5];
	frustum[3][2] = mmr[11] + mmr[9];
	frustum[3][3] = mmr[15] + mmr[13];
	normalize_plane(frustum[3]);

	frustum[4][0] = mmr[3] + mmr[2]; // near
	frustum[4][1] = mmr[7] + mmr[6];
	frustum[4][2] = mmr[11] + mmr[10];
	frustum[4][3] = mmr[15] + mmr[14];
	normalize_plane(frustum[4]);

	frustum[5][0] = mmr[3] - mmr[2]; // far
	frustum[5][1] = mmr[7] - mmr[6];
	frustum[5][2] = mmr[11] - mmr[10];
	frustum[5][3] = mmr[15] - mmr[14];
	normalize_plane(frustum[5]);
}
