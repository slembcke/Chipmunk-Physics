/* Copyright (c) 2007 Scott Lembcke
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>

#include "GL/glew.h"
#include "GL/glfw.h"

#include "chipmunk_private.h"
#include "ChipmunkDemo.h"
#include "ChipmunkDemoShaderSupport.h"
#include "ChipmunkDemoTextSupport.h"

#include "VeraMoBd.ttf_sdf.h"

//#define Scale 3.0f
#define Scale 0.70f
#define LineHeight (18.0f*Scale)

static GLuint program;
static GLuint texture;

struct v2f {GLfloat x, y;};
typedef struct Vertex {struct v2f vertex, tex_coord; Color color;} Vertex;
typedef struct Triangle {Vertex a, b, c;} Triangle;

static GLuint vao = 0;
static GLuint vbo = 0;

// char -> glyph indexes generated by the lonesock tool.
static int glyph_indexes[256];

void
ChipmunkDemoTextInit(void)
{
	GLint vshader = CompileShader(GL_VERTEX_SHADER, GLSL(
		attribute vec2 vertex;
		attribute vec2 tex_coord;
		attribute vec4 color;
		
		varying vec2 v_tex_coord;
		varying vec4 v_color;
		
		void main(void){
			// TODO get rid of the GL 2.x matrix bit eventually?
			gl_Position = gl_ModelViewProjectionMatrix*vec4(vertex, 0.0, 1.0);
			
			v_color = color;
			v_tex_coord = tex_coord;
		}
	));
	
	GLint fshader = CompileShader(GL_FRAGMENT_SHADER, GLSL(
		uniform sampler2D u_texture;
		
		varying vec2 v_tex_coord;
		varying vec4 v_color;
		
		float aa_step(float t1, float t2, float f)
		{
			//return step(t2, f);
			return smoothstep(t1, t2, f);
		}
		
		void main(void)
		{
			float sdf = texture2D(u_texture, v_tex_coord).a;
			
			//float fw = fwidth(sdf)*0.5;
			float fw = length(vec2(dFdx(sdf), dFdy(sdf)))*0.5;
			
			float alpha = aa_step(0.5 - fw, 0.5 + fw, sdf);
			gl_FragColor = v_color*(v_color.a*alpha);
//			gl_FragColor = vec4(1, 0, 0, 1);
		}
	));
	
	program = LinkProgram(vshader, fshader);
	CHECK_GL_ERRORS();
	
//	GLint index = -1;//glGetUniformLocation(program, "u_texture");
//	glUniform1i(index, 0);
//	CHECK_GL_ERRORS();
	
	// Setu VBO and VAO.
	glGenVertexArraysAPPLE(1, &vao);
	glBindVertexArrayAPPLE(vao);
	
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	
	SET_ATTRIBUTE(program, struct Vertex, vertex, GL_FLOAT);
	SET_ATTRIBUTE(program, struct Vertex, tex_coord, GL_FLOAT);
	SET_ATTRIBUTE(program, struct Vertex, color, GL_FLOAT);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArrayAPPLE(0);
	CHECK_GL_ERRORS();
	
	// Load the SDF font texture.
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, sdf_tex_width, sdf_tex_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, sdf_data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	CHECK_GL_ERRORS();
	
	// Fill in the glyph index array.
	for(int i=0; i<sdf_num_chars; i++){
		int char_index = sdf_spacing[i*8];
		glyph_indexes[char_index] = i;
	}
}

#undef MAX
#define MAX(__a__, __b__) (__a__ > __b__ ? __a__ : __b__)

static size_t triangle_capacity = 0;
static size_t triangle_count = 0;
static Triangle *triangle_buffer = NULL;

static Triangle *PushTriangles(size_t count)
{
	if(triangle_count + count > triangle_capacity){
		triangle_capacity += MAX(triangle_capacity, count);
		triangle_buffer = realloc(triangle_buffer, triangle_capacity*sizeof(Triangle));
	}
	
	Triangle *buffer = triangle_buffer + triangle_count;
	triangle_count += count;
	return buffer;
}

static cpFloat
PushChar(int character, GLfloat x, GLfloat y, Color color)
{
	int i = glyph_indexes[character];
	GLfloat w = (GLfloat)sdf_tex_width;
	GLfloat h = (GLfloat)sdf_tex_height;
	
	GLfloat gw = sdf_spacing[i*8 + 3];
	GLfloat gh = sdf_spacing[i*8 + 4];
	
	GLfloat txmin = sdf_spacing[i*8 + 1]/w;
	GLfloat tymin = sdf_spacing[i*8 + 2]/h;
	GLfloat txmax = txmin + gw/w;
	GLfloat tymax = tymin + gh/h;
	
	GLfloat s = Scale/scale_factor;
	GLfloat xmin = x + sdf_spacing[i*8 + 5]/scale_factor*Scale;
	GLfloat ymin = y + (sdf_spacing[i*8 + 6]/scale_factor - gh)*Scale;
	GLfloat xmax = xmin + gw*Scale;
	GLfloat ymax = ymin + gh*Scale;
	
	Vertex a = {{xmin, ymin}, {txmin, tymax}, color};
	Vertex b = {{xmin, ymax}, {txmin, tymin}, color};
	Vertex c = {{xmax, ymax}, {txmax, tymin}, color};
	Vertex d = {{xmax, ymin}, {txmax, tymax}, color};
	
	Triangle *triangles = PushTriangles(2);
	triangles[0] = (Triangle){a, b, c};
	triangles[1] = (Triangle){a, c, d};
	
	return sdf_spacing[i*8 + 7]*s;
}

#undef ChipmunkDemoTextDrawString

void
ChipmunkDemoTextDrawString(cpVect pos, char *str)
{
	Color c = LAColor(1.0f, 1.0f);
	GLfloat x = pos.x, y = pos.y;
	
	for(int i=0, len=strlen(str); i<len; i++){
		if(str[i] == '\n'){
			y -= LineHeight;
			x = pos.x;
//		} else if(str[i] == '*'){ // print out the last demo key
//			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, 'A' + demoCount - 1);
		} else {
			x += PushChar(str[i], x, y, c);
		}
	}
}

void
ChipmunkDemoTextFlushRenderer(void)
{
//	triangle_count = 0;
//	ChipmunkDemoTextDrawString(cpv(-300, 0), "0.:,'");
	
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Triangle)*triangle_count, triangle_buffer, GL_STREAM_DRAW);
	
	glUseProgram(program);
	
	glBindVertexArrayAPPLE(vao);
	glDrawArrays(GL_TRIANGLES, 0, triangle_count*3);
		
	CHECK_GL_ERRORS();
}

void
ChipmunkDemoTextClearRenderer(void)
{
	triangle_count = 0;
}

static int pushed_triangle_count = 0;
void
ChipmunkDemoTextPushRenderer(void)
{
	pushed_triangle_count = triangle_count;
}

void
ChipmunkDemoTextPopRenderer(void)
{
	triangle_count = pushed_triangle_count;
}


