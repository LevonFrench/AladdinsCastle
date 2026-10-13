// SPDX-License-Identifier: MIT
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#include "grip_calibration_draw.hpp"
#include <array>
#include <cstdio>
#include <string>

namespace room {
namespace {
using Glyph=std::array<unsigned char,7>;
Glyph glyph(char c){
    switch(c){
    case 'A':return {14,17,17,31,17,17,17}; case 'B':return {30,17,17,30,17,17,30};
    case 'C':return {14,17,16,16,16,17,14}; case 'D':return {30,17,17,17,17,17,30};
    case 'E':return {31,16,16,30,16,16,31}; case 'F':return {31,16,16,30,16,16,16};
    case 'G':return {14,17,16,23,17,17,15}; case 'H':return {17,17,17,31,17,17,17};
    case 'I':return {14,4,4,4,4,4,14}; case 'L':return {16,16,16,16,16,16,31};
    case 'N':return {17,25,21,19,17,17,17}; case 'O':return {14,17,17,17,17,17,14};
    case 'P':return {30,17,17,30,16,16,16}; case 'R':return {30,17,17,30,20,18,17};
    case 'S':return {15,16,16,14,1,1,30}; case 'T':return {31,4,4,4,4,4,4};
    case 'U':return {17,17,17,17,17,17,14}; case 'V':return {17,17,17,17,17,10,4};
    case 'W':return {17,17,17,21,21,21,10}; case 'Y':return {17,17,10,4,4,4,4};
    case '0':return {14,17,19,21,25,17,14}; case '1':return {4,12,4,4,4,4,14};
    case '2':return {14,17,1,2,4,8,31}; case '3':return {30,1,1,14,1,1,30};
    case '4':return {2,6,10,18,31,2,2}; case '5':return {31,16,16,30,1,1,30};
    case '6':return {14,16,16,30,17,17,14}; case '7':return {31,1,2,4,8,8,8};
    case '8':return {14,17,17,14,17,17,14}; case '9':return {14,17,17,15,1,1,14};
    case '-':return {0,0,0,31,0,0,0}; case '+':return {0,4,4,31,4,4,0};
    case '.':return {0,0,0,0,0,12,12}; case '/':return {1,1,2,4,8,16,16};
    default:return {};
    }
}
void quad(float x,float y,float w,float h){
    glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);
}
void text(float x,float y,const std::string &value){
    glBegin(GL_QUADS);
    for(char c:value){
        const auto rows=glyph(c);
        for(int row=0;row<7;++row)for(int col=0;col<5;++col)
            if(rows[row]&(1<<(4-col)))quad(x+col*2.f,y+row*2.f,2.f,2.f);
        x+=13.f;
    }
    glEnd();
}
const char *status(GripFeedback feedback){
    switch(feedback){
    case GripFeedback::Ready:return "B/Y HOLD EDIT";
    case GripFeedback::Editing:return "EDIT HOLD B/Y SAVE TAP CANCEL";
    case GripFeedback::Saved:return "SAVED";
    case GripFeedback::Cancelled:return "CANCELLED";
    case GripFeedback::Unavailable:return "UNAVAILABLE";
    case GripFeedback::SaveFailed:return "SAVE FAILED";
    }
    return "UNAVAILABLE";
}
}
void draw_grip_status(const GripCalibration &cal,unsigned hand,unsigned width,unsigned height){
    const auto layout=grip_panel_layout(width,height);
    if(layout.scale==0)return;
    GLint previous_mode=GL_MODELVIEW;
    glGetIntegerv(GL_MATRIX_MODE,&previous_mode);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
    glOrtho(0,static_cast<GLdouble>(width),static_cast<GLdouble>(height),0,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    glTranslatef(layout.x,layout.y,0);glScalef(layout.scale,layout.scale,1);
    glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(.02f,.04f,.07f,.84f);glBegin(GL_QUADS);quad(0,0,520,65);glEnd();
    glColor3f(cal.editing()?.95f:.7f,cal.editing()?.8f:.9f,.35f);
    char values[96];
    const auto angles=cal.preview();
    std::snprintf(values,sizeof(values),"%c YAW %+.1f PITCH %+.1f",
                  hand==0?'R':'L',angles.yaw_deg,angles.pitch_deg);
    text(12,8,values);
    text(12,37,status(cal.feedback()));
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();
    glMatrixMode(static_cast<GLenum>(previous_mode));glPopAttrib();
}
}
