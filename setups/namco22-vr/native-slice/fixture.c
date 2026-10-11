/* Formula-built native display list. No content loader or game assets. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include <SDL2/SDL.h>
#include "eng_gl.h"
#include "eng.h"
#include "geo_hw.h"
#include "ss22_gl.h"
#include "eye.h"

static int32_t points[1024];
static uint32_t poly[0x8000];
static uint8_t palette[3*0x8000], mixer[0x400], cg[0x1e000], text[0x2000];
static uint16_t cz[4][256];
static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); failures++; } } while(0)

static uint32_t word(int index) { return poly[index & 0x7fff]; }
static void identity_words(int p, int offset)
{
    for (int c=0;c<3;c++) for(int r=0;r<3;r++)
        poly[p+offset+c*3+r] = (r==c)?0x7fff:0;
}
static void add_quad(int index, int group, int x, int y, int z, int w, int h, int priority)
{
    int p=0x101+index*25;
    points[p]=0x18;
    points[p+1]=(index==5)?0x800000:0;
    points[p+2]=0;
    points[p+3]=(64<<16)|(group<<8);
    points[p+4]=priority?(0x200000|priority):0;
    int a=p+5;
    for(int i=0;i<4;i++) {
        points[a+2*i]=(i==1||i==2)?63:0;
        points[a+2*i+1]=(i>=2)?63:0;
        points[a+8+i*3]=x+((i==1||i==2)?w:-w);
        points[a+9+i*3]=y+((i>=2)?-h:h);
        points[a+10+i*3]=z;
    }
}
static void fixture(void)
{
    static const uint8_t colours[6][3]={{65,82,103},{30,155,245},{240,160,35},{65,215,125},{230,65,165},{255,25,25}};
    for(int g=0;g<6;g++) for(int pen=0;pen<256;pen++) for(int c=0;c<3;c++)
        palette[c*0x8000+g*256+pen]=(uint8_t)(colours[g][c]*(pen==1?0.62:1));
    mixer[0]=mixer[1]=mixer[2]=255;
    mixer[8]=12; mixer[9]=18; mixer[10]=28;
    for(int c=0;c<3;c++) for(int i=0;i<256;i++) mixer[0x100+c*256+i]=(uint8_t)i;
    /* No fog contribution. Gamma identity; text and sprite planes excluded. */
    for(int b=0;b<4;b++) for(int i=0;i<256;i++) cz[b][i]=(uint16_t)i;
    int s=0x304,p=s+2;
    poly[s]=0; poly[s+1]=0x15;
    poly[p+6]=(0x2e<<16)|400;
    /* -0.8, +0.8, -0.6, +0.6, rounded DSP floats: full native raster. */
    poly[p+8]=(0x20<<16)|(uint16_t)-13108;
    poly[p+7]=(0x20<<16)|(uint16_t)-13108;
    poly[p+9]=(0x20<<16)|(uint16_t)-9831;
    poly[p+10]=(0x20<<16)|(uint16_t)-9831;
    identity_words(p,0x0c);
    int next=s+0x15+4;
    poly[s+0x15+3]=(uint32_t)next;
    poly[next]=0x45; poly[next+1]=0x0d;
    identity_words(next+2,1);
    points[0x45]=0x80; points[0x80]=0x100; points[0x81]=-1;
    points[0x100]=6*25;
    add_quad(0,0,0,0,6000,4200,3000,0);
    add_quad(1,1,-650,-250,1800,700,650,0);
    add_quad(2,2,550,50,3200,1300,950,0);
    add_quad(3,3,1200,1150,2800,600,300,0);
    add_quad(4,4,1200,1150,2800,600,300,0);
    /* Physically nearest but forced FAR: native painter wins, no GL_LESS. */
    add_quad(5,5,600,-650,1600,240,240,8000);
    g_eng_pointrom=points; g_eng_pointrom_n=1024;
    ss22_regs regs={0}; regs.walk=true; regs.poly_word=word;
    regs.pal=palette; regs.mixer=mixer; regs.cgram=cg; regs.textram=text;
    for(int b=0;b<4;b++) regs.czram[b]=cz[b];
    ss22_prepare(&regs);
}
static acvr_eye eye(int index)
{
    acvr_eye e={0}; ACVR_INIT(&e);
    e.eye_index=(uint32_t)index; e.rect_x=index*640; e.rect_width=640; e.rect_height=480;
    for(int i=0;i<16;i++) e.view_from_scene[i]=(i%5==0)?1:0;
    e.view_from_scene[12]=index?-32:32; /* 64mm at provisional 1000 units/m */
    /* Focal400 from formula native camera; positive near100, far12000. */
    e.projection_from_view[0]=400.0f/320;
    e.projection_from_view[5]=400.0f/240;
    e.projection_from_view[10]=-(12000.0f+100)/(12000-100);
    e.projection_from_view[11]=-1;
    e.projection_from_view[14]=-(2.0f*12000*100)/(12000-100);
    return e;
}
static void checks(void)
{
    int n; const geo_quad *q=acn22_prepared(&n);
    CHECK(n==6); CHECK(g_eng_frame==1); CHECK(ss22_sprites()==0);
    if(n!=6) return;
    const int expected[]={5,0,2,4,3,1};
    for(int i=0;i<n;i++) {
        CHECK((int)((q[i].color>>8)&127)==expected[i]);
        CHECK(q[i].nrv==4); CHECK(q[i].ndv==0);
        for(int k=0;k<4;k++) CHECK(q[i].rv[k].acn_view[2]==q[i].rv[k].z);
    }
    geo_quad snapshot[6]; memcpy(snapshot,q,sizeof snapshot);
    acvr_eye l=eye(0),r=eye(1); geo_quad left,right;
    for(int i=0;i<n;i++) {
        CHECK(acn22_project(q+i,&l,&left)); CHECK(acn22_project(q+i,&r,&right));
        if(!acn22_project(q+i,&l,&left)||!acn22_project(q+i,&r,&right))continue;
        double measured=(left.rv[0].sx16-right.rv[0].sx16)/16.0;
        double expected_disparity=400*64.0/q[i].rv[0].z;
        CHECK(fabs(measured-expected_disparity)<0.063);
        CHECK(left.zsort==q[i].zsort && left.order==q[i].order);
        CHECK(memcmp(left.rv[0].acn_view,q[i].rv[0].acn_view,sizeof left.rv[0].acn_view)==0);
        printf("fixture group=%d native_order=%d key=%d native_z=%d disparity=%.4f analytic=%.4f\n",
               expected[i],q[i].order,q[i].zsort,q[i].rv[0].z,measured,expected_disparity);
    }
    CHECK(acn22_begin_eye(&l)==ACVR_OK); acn22_end_eye();
    CHECK(acn22_begin_eye(&r)==ACVR_OK); acn22_end_eye();
    /* Known XYZ remains independent of the native quantized raster. */
    geo_quad altered=q[5]; altered.rv[0].sx16=999999;
    CHECK(acn22_project(&altered,&l,&left));
    CHECK(acn22_project(q+5,&l,&right)); CHECK(left.rv[0].sx16==right.rv[0].sx16);
    r.projection_from_view[8]=0.1f;
    CHECK(acn22_project(q+2,&r,&right));
    CHECK(fabs((right.rv[0].sx16/16.0)-((1+(1.25*(q[2].rv[0].acn_view[0]-32)/q[2].rv[0].z)-0.1)*320))<0.032);
    r=eye(1); r.view_from_scene[14]=1; CHECK(acn22_begin_eye(&r)==ACVR_UNSUPPORTED);
    r=eye(1); r.view_from_scene[0]=0; CHECK(acn22_begin_eye(&r)==ACVR_UNSUPPORTED);
    r=eye(1); r.view_from_scene[12]=1e8f; CHECK(acn22_begin_eye(&r)==ACVR_UNSUPPORTED);
    r=eye(1); r.projection_from_view[8]=NAN; CHECK(acn22_begin_eye(&r)==ACVR_UNSUPPORTED);
    r=eye(1); r.size=8; CHECK(acn22_begin_eye(&r)==ACVR_UNSUPPORTED);
    altered=q[0]; altered.direct=1; CHECK(!acn22_project(&altered,&l,&left));
    altered=q[0]; altered.behind=1; CHECK(!acn22_project(&altered,&l,&left));
    altered=q[0]; altered.ndv=4; CHECK(!acn22_project(&altered,&l,&left));
    CHECK(memcmp(snapshot,q,sizeof snapshot)==0); CHECK(g_eng_frame==1);
}
static int draw(const char *path)
{
    /* Refuse overwrite before creating the GL window. */
    char resolved[_MAX_PATH];
    if (strlen(path)<3 || path[1]!=':' || (path[2]!='/' && path[2]!='\\') ||
        !_fullpath(resolved,path,sizeof resolved) || !strstr(resolved,"\\.local\\")) {
        fprintf(stderr,"Absolute private .local output required\n"); return 2;
    }
    int fd=_open(resolved,_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_S_IREAD|_S_IWRITE);
    if(fd<0){fprintf(stderr,"Fresh output required\n");return 2;}
    FILE *out=_fdopen(fd,"wb"); if(!out){_close(fd);return 2;}
    SDL_SetMainReady();
    if(SDL_Init(SDL_INIT_VIDEO)!=0){fclose(out);return 3;}
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    SDL_Window *w=SDL_CreateWindow("AladdinsCastle - SYNTHETIC native System22 SBS",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1280,480,SDL_WINDOW_OPENGL);
    SDL_GLContext ctx=w?SDL_GL_CreateContext(w):NULL;
    if(!ctx){fprintf(stderr,"SDL GL: %s\n",SDL_GetError());fclose(out);if(w)SDL_DestroyWindow(w);SDL_Quit();return 3;}
    printf("GL vendor=%s renderer=%s version=%s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
    int width,height; SDL_GL_GetDrawableSize(w,&width,&height);
    CHECK(width==1280 && height==480);
    g_texture_data=calloc(TEXTURE_TOTAL_SIZE,1); g_texture_tilemap=calloc(TEXTUREMAP_SIZE,1);
    CHECK(g_texture_data && g_texture_tilemap);
    if(g_texture_data) for(int y=0;y<16;y++)for(int x=0;x<16;x++)g_texture_data[y*16+x]=(uint8_t)(((x/4+y/4)%2)?1:2);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    for(int i=0;i<2;i++) { acvr_eye e=eye(i); CHECK(acn22_begin_eye(&e)==ACVR_OK); if(acn22_eye)ss22_draw(640,480); acn22_end_eye(); }
    glFinish(); CHECK(glGetError()==GL_NO_ERROR); CHECK(g_eng_frame==1);
    uint8_t *rgb=malloc(1280*480*3); CHECK(rgb!=NULL);
    if(rgb) {
        glPixelStorei(GL_PACK_ALIGNMENT,1); glReadBuffer(GL_BACK);
        glReadPixels(0,0,1280,480,GL_RGB,GL_UNSIGNED_BYTE,rgb);
        /* Equal-key witness is green (earlier emission), not magenta.
         * Painter-priority near red is absent behind the far background.
         * Check both halves after the second clear/draw. */
        for(int i=0;i<2;i++) {
            const uint8_t *tie=rgb+((480-1-76)*1280+i*640+491)*3;
            CHECK(tie[1]>tie[0]*2 && tie[1]>tie[2]);
            const uint8_t *forced=rgb+((480-1-403)*1280+i*640+470)*3;
            CHECK(forced[0]<100 && forced[2]>forced[0]);
            const uint8_t *bg=rgb+((480-1-10)*1280+i*640+10)*3;
            CHECK(bg[0]==12 && bg[1]==18 && bg[2]==28);
        }
        fprintf(out,"P6\n# SYNTHETIC FORMULA FIXTURE; native prepare once / draw twice\n1280 480\n255\n");
        for(int y=479;y>=0;y--) CHECK(fwrite(rgb+y*1280*3,1,1280*3,out)==1280*3);
        free(rgb);
    }
    CHECK(fclose(out)==0);
    SDL_GL_SwapWindow(w);
    uint32_t end=SDL_GetTicks()+3000;
    while(SDL_GetTicks()<end){SDL_Event ev;while(SDL_PollEvent(&ev))if(ev.type==SDL_QUIT)end=0;SDL_Delay(10);}
    SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(w); SDL_Quit();
    free(g_texture_data); free(g_texture_tilemap);
    return failures?1:0;
}
int main(int argc,char **argv)
{
    if(argc!=2 && argc!=3)return 2;
    fixture(); checks();
    if(failures)return 1;
    if(argc==2 && strcmp(argv[1],"--check")==0){puts("CPU native fixture passed; no GL context created");return 0;}
    if(argc==3 && strcmp(argv[1],"--draw")==0)return draw(argv[2]);
    return 2;
}
