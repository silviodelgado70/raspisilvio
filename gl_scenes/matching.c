/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, Tim Gover
All rights reserved.


Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
 * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "matching.h"
#include "RaspiTex.h"
#include "RaspiTexUtil.h"
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

/* \file matching.c 
 */

#define MATCHING_VSHADER_SOURCE \
    "attribute vec2 vertex;\n" \
    "varying vec2 texcoord;\n" \
    "void main(void) {\n" \
    "   texcoord = 0.5 * (vertex + 1.0);\n" \
    "   gl_Position = vec4(vertex, 0.0, 1.0);\n" \
    "}\n"

#define MATCHING_VSHADER_SOURCE_PREVIEW \
    "attribute vec2 vertex;\n" \
    "attribute vec2 top_left;\n"\
    "varying vec2 texcoord;\n" \
    "void main(void) {\n" \
    "   texcoord = 0.5 * (vertex + 1.0);\n" \
    "   gl_Position = vec4(top_left + 0.5 * (vertex + vec2(1.0, - 1.0)), 0.0, 1.0);\n" \
    "}\n"

#define MATCHING_FSHADER_SOURCE \
    "#extension GL_OES_EGL_image_external : require\n" \
    "uniform samplerExternalOES tex;\n" \
    "uniform samplerExternalOES tex;\n" \
    "varying vec2 texcoord;\n" \
    "void main(void) {\n" \
    "    vec4 rgba_col = texture2D(tex, texcoord);\n" \
    "    vec4 thresh_col = vec4(0.0,0.0,0.0,1.0);\n" \
    "    float temp = max(rgba_col.r,rgba_col.g);\n" \
    "    float v = max(rgba_col.b,temp);\n" \
    "    float temp2 = min(rgba_col.r,rgba_col.g);\n" \
    "    float temp3 = min(rgba_col.b,temp2);\n" \
    "    float s = 0.0;\n" \
    "    float temp4 = v - temp3;\n" \
    "    float h = 0.0;\n" \
    "    if(v != 0.0)\n" \
    "    {\n" \
    "           s = temp4 / v;\n" \
    "    }\n" \
    "    if(v == rgba_col.r)\n" \
    "    {\n" \
    "           h = 60.0 * abs(rgba_col.g - rgba_col.b)/temp4 ;\n" \
    "    }\n" \
    "    else if(v  == rgba_col.g)\n" \
    "    {\n" \
    "           h = 120.0 + 60.0 * abs(rgba_col.b - rgba_col.r)/temp4 ;\n" \
    "    }\n" \
    "    else if(v == rgba_col.b)\n" \
    "    {\n" \
    "           h = 240.0 + 60.0 * abs(rgba_col.r - rgba_col.g)/temp4 ;\n" \
    "    }\n" \
    "    if((h > 165.0) && (h < 180.0) && (s > 60.0/255.0) && (s < 130.0/255.0) && (v > 50.0/255.0) && (v < 105.0/255.0) )\n" \
    "    {\n" \
    "           thresh_col = vec4(1.0) ;\n" \
    "    }\n" \
    "    gl_FragColor = thresh_col;\n" \
    "}\n"

#define MATCHING_FSHADER_SOURCE_PREVIEW \
    "#extension GL_OES_EGL_image_external : require\n" \
    "uniform samplerExternalOES tex;\n" \
    "varying vec2 texcoord;\n" \
    "void main(void) {\n" \
    "    gl_FragColor = texture2D(tex, texcoord);\n" \
    "}\n"

static GLfloat quad_varray[] = {
                                - 1.0f , - 1.0f , 1.0f , 1.0f , 1.0f , - 1.0f ,
                                - 1.0f , 1.0f , 1.0f , 1.0f , - 1.0f , - 1.0f ,
} ;

static GLuint quad_vbo ;

char *SRCfromfile = NULL ;

float h_min = 240.0f , h_max = 285.0f ;
float s_min = 40.0f , s_max = 153.0f ;
float v_min = 41.0f , v_max = 100.0f ;
int stop_flag = 0 ;

static RASPITEXUTIL_SHADER_PROGRAM_T matching_shader_preview = {
                                                                .vertex_source = MATCHING_VSHADER_SOURCE_PREVIEW ,
                                                                .fragment_source = MATCHING_FSHADER_SOURCE_PREVIEW ,
                                                                .uniform_names =
    {"tex" } ,
                                                                .attribute_names =
    {"vertex" , "top_left" } ,
} ;

static RASPITEXUTIL_SHADER_PROGRAM_T matching_shader = {
                                                        .vertex_source = MATCHING_VSHADER_SOURCE_PREVIEW ,
                                                        .fragment_source = "" ,
                                                        .uniform_names =
    {"tex" , "lower" , "upper" } ,
                                                        .attribute_names =
    {"vertex" , "top_left" } ,
} ;

static const EGLint matching_egl_config_attribs[] = {
                                                     EGL_RED_SIZE , 8 ,
                                                     EGL_GREEN_SIZE , 8 ,
                                                     EGL_BLUE_SIZE , 8 ,
                                                     EGL_ALPHA_SIZE , 8 ,
                                                     EGL_RENDERABLE_TYPE , EGL_OPENGL_ES2_BIT ,
                                                     EGL_NONE
} ;

/**
 * Initialisation of shader uniforms.
 *
 * @param width Width of the EGL image.
 * @param width Height of the EGL image.
 */
static int shader_set_uniforms ( RASPITEXUTIL_SHADER_PROGRAM_T *shader ,
                                 int width , int height )
{
    GLCHK ( glUseProgram ( shader->program ) ) ;
    GLCHK ( glUniform1i ( shader->uniform_locations[0] , 0 ) ) ; // Texture unit

    /* Dimensions of a single pixel in texture co-ordinates */
    //    GLCHK ( glUniform2f ( shader->uniform_locations[1] ,
    //                          1.0 / ( float ) width , 1.0 / ( float ) height ) ) ;

    /* Enable attrib 0 as vertex array */
    GLCHK ( glEnableVertexAttribArray ( shader->attribute_locations[0] ) ) ;
    return 0 ;
}

/**
 * Creates the OpenGL ES 2.X context and builds the shaders.
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int matching_init ( RASPITEX_STATE *raspitex_state )
{
    int rc = 0 ;
    int width = raspitex_state->width ;
    int height = raspitex_state->height ;

    vcos_log_trace ( "%s" , VCOS_FUNCTION ) ;
    raspitex_state->egl_config_attribs = matching_egl_config_attribs ;
    rc = raspitexutil_gl_init_2_0 ( raspitex_state ) ;
    if ( rc != 0 )
        goto end ;

    rc = raspitexutil_build_shader_program ( &matching_shader_preview ) ;
    if ( rc != 0 )
        goto end ;

    assert ( ! SRCfromfile ) ;
    FILE* f = fopen ( "gl_scenes/matchingFS.glsl" , "rb" ) ;
    assert ( f ) ;
    fseek ( f , 0 , SEEK_END ) ;
    int sz = ftell ( f ) ;
    fseek ( f , 0 , SEEK_SET ) ;
    SRCfromfile = malloc ( sz + 1 ) ;
    fread ( SRCfromfile , 1 , sz , f ) ;
    SRCfromfile[sz] = 0 ; //null terminate it!
    fclose ( f ) ;

    matching_shader.fragment_source = SRCfromfile ;

    rc = raspitexutil_build_shader_program ( &matching_shader ) ;
    if ( rc != 0 )
        goto end ;

    GLCHK ( glGenBuffers ( 1 , &quad_vbo ) ) ;
    GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
    GLCHK ( glBufferData ( GL_ARRAY_BUFFER , sizeof (quad_varray ) , quad_varray , GL_STATIC_DRAW ) ) ;
    GLCHK ( glClearColor ( 0.0f , 0.0f , 0.0f , 1.0f ) ) ;

end:
    return rc ;
}

/* Redraws the scene with the latest buffer.
 *
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int matching_redraw ( RASPITEX_STATE* state )
{
    glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;
    GLCHK ( glUseProgram ( matching_shader_preview.program ) ) ;
    GLCHK ( glUniform1i ( matching_shader_preview.uniform_locations[0] , 0 ) ) ; // Texture unit    

    GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
    GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
    GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
    GLCHK ( glEnableVertexAttribArray ( matching_shader_preview.attribute_locations[0] ) ) ;
    GLCHK ( glVertexAttribPointer ( matching_shader_preview.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
    GLCHK ( glVertexAttrib2f ( matching_shader_preview.attribute_locations[1] , - 1.0f , 1.0f ) ) ;
    GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;

    GLCHK ( glUseProgram ( matching_shader.program ) ) ;
    GLCHK ( glUniform1i ( matching_shader.uniform_locations[0] , 0 ) ) ; // Texture unit
    GLCHK ( glUniform3f ( matching_shader.uniform_locations[1] , h_min , s_min / 255.0f , v_min / 255.0f ) ) ;
    GLCHK ( glUniform3f ( matching_shader.uniform_locations[2] , h_max , s_max / 255.0f , v_max / 255.0f ) ) ;

    GLCHK ( glActiveTexture ( GL_TEXTURE0 ) ) ;
    GLCHK ( glBindTexture ( GL_TEXTURE_EXTERNAL_OES , state->texture ) ) ;
    GLCHK ( glBindBuffer ( GL_ARRAY_BUFFER , quad_vbo ) ) ;
    GLCHK ( glEnableVertexAttribArray ( matching_shader.attribute_locations[0] ) ) ;
    GLCHK ( glVertexAttribPointer ( matching_shader.attribute_locations[0] , 2 , GL_FLOAT , GL_FALSE , 0 , 0 ) ) ;
    GLCHK ( glVertexAttrib2f ( matching_shader.attribute_locations[1] , 0.0f , 1.0f ) ) ;
    GLCHK ( glDrawArrays ( GL_TRIANGLES , 0 , 6 ) ) ;

    if ( stop_flag )
    {
        state->ops.update_texture = NULL ;
    }

    return 0 ;
}

int matching_open ( RASPITEX_STATE *state )
{
    state->ops.gl_init = matching_init ;
    state->ops.redraw = matching_redraw ;
    state->ops.update_texture = raspitexutil_update_texture ;
    return 0 ;
}
