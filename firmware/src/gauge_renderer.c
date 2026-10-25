#include "ag_gauge_renderer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Own compact5x7 glyphs, row bitmasks (bit4 is left). Deliberately limited to
 * the ASCII label/unit/value vocabulary; unknown glyphs render as '?'. */
typedef struct {char c;uint8_t rows[7];} glyph_t;
static const glyph_t glyphs[]={
 {'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}},{'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}},{'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,1,14}},
 {'A',{14,17,17,31,17,17,17}},{'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}},{'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,15}},{'H',{17,17,17,31,17,17,17}},
 {'I',{14,4,4,4,4,4,14}},{'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}},{'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}},{'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}},{'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}},{'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}},{'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}},{'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}},{'Z',{31,1,2,4,8,16,31}},
 {'-',{0,0,0,31,0,0,0}},{'.',{0,0,0,0,0,12,12}},
 {'/',{1,2,2,4,8,8,16}},{'%',{24,25,2,4,8,19,3}},
 {'?',{14,17,1,2,4,0,4}},{' ',{0,0,0,0,0,0,0}}
};
static const uint8_t *glyph(char c){
    for(size_t i=0;i<sizeof(glyphs)/sizeof(glyphs[0]);++i)if(glyphs[i].c==c)return glyphs[i].rows;
    return glyphs[sizeof(glyphs)/sizeof(glyphs[0])-2].rows;
}
static const char *unit(ag_unit_t u){
    static const char *const names[]={"?","%","C","RPM","KM/H","G/S","KPA","V","S","KM","RATIO","COUNT","DEG","MIN","L/H","NM"};
    return (unsigned)u<sizeof(names)/sizeof(names[0])?names[u]:"?";
}
static bool overlap(const void *p,size_t n,const void *q,size_t m){
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)q;
    return a>UINTPTR_MAX-n||b>UINTPTR_MAX-m||(a<b+m&&b<a+n);
}
bool ag_gauge_renderer_init(ag_gauge_renderer_t *r,const ag_gauge_renderer_options_t *options){
    if(r==NULL||options==NULL||options->write_row==NULL||options->width<32||options->height<32
        ||options->width>AG_GAUGE_RENDER_MAX_SIDE||options->height>AG_GAUGE_RENDER_MAX_SIDE)return false;
    ag_gauge_renderer_options_t copy=*options;memset(r,0,sizeof(*r));r->options=copy;r->initialized=true;return true;
}
static bool valid(const ag_gauge_scene_t *s){
    if(s->widget_count>AG_MAX_WIDGETS||s->dashboard_count>AG_MAX_DASHBOARDS
        ||(unsigned)s->power>AG_GAUGE_POWER_ON||(unsigned)s->brightness>AG_GAUGE_BRIGHTNESS_AUTO
        ||s->brightness_percent>100||(s->widget_count&&s->dashboard_count==0))return false;
    for(size_t i=0;i<s->widget_count;++i){const ag_gauge_widget_t *w=&s->widgets[i];const ag_rect_t *p=&w->rect;
        if(w->id<=0||w->pid==0||(unsigned)w->quality>AG_SAMPLE_UNAVAILABLE||(unsigned)w->unit>AG_UNIT_NEWTON_METER
            ||p->x<0||p->y<0||p->width<=0||p->height<=0||p->x>10000||p->y>10000
            ||p->width>10000-p->x||p->height>10000-p->y)return false;
        for(size_t j=0;j<i;++j)if(w->id==s->widgets[j].id)return false;
    }
    return true;
}
ag_gauge_render_result_t ag_gauge_renderer_begin(ag_gauge_renderer_t *r,const ag_gauge_scene_t *scene){
    if(r==NULL||!r->initialized||scene==NULL||overlap(r,sizeof(*r),scene,sizeof(*scene)))return AG_RENDER_INVALID;
    if(r->faulted)return AG_RENDER_FAULT;
    if(r->busy||r->active)return AG_RENDER_BUSY;
    if(!valid(scene))return AG_RENDER_INVALID;
    r->scene=*scene;r->next_y=0;
    for(size_t i=0;i<scene->widget_count;++i){const ag_gauge_widget_t *w=&scene->widgets[i];
        (void)snprintf(r->labels[i],sizeof(r->labels[i]),"PID %02X",(unsigned)w->pid);
        bool available=scene->power==AG_GAUGE_POWER_ON&&w->has_source&&w->has_value&&w->quality==AG_SAMPLE_VALID&&isfinite(w->value);
        if(!available)(void)snprintf(r->values[i],sizeof(r->values[i]),"--");
        else if(fabs(w->value)>999999999.0)(void)snprintf(r->values[i],sizeof(r->values[i]),"RANGE");
        else {
            int n=snprintf(r->values[i],sizeof(r->values[i]),"%.1f",w->value==0?0.0:w->value);
            if(n<0||(size_t)n>=sizeof(r->values[i]))(void)snprintf(r->values[i],sizeof(r->values[i]),"RANGE");
        }
        (void)snprintf(r->units[i],sizeof(r->units[i]),"%s",available?unit(w->unit):"?");
    }
    r->active=true;return AG_RENDER_OK;
}
ag_gauge_render_result_t ag_gauge_renderer_cancel_pending(ag_gauge_renderer_t *r){
    if(r==NULL||!r->initialized)return AG_RENDER_INVALID;
    if(r->faulted)return AG_RENDER_FAULT;
    if(r->busy)return AG_RENDER_BUSY;
    r->active=false;r->next_y=0;memset(&r->scene,0,sizeof(r->scene));
    memset(r->labels,0,sizeof(r->labels));memset(r->values,0,sizeof(r->values));memset(r->units,0,sizeof(r->units));
    return AG_RENDER_OK;
}
static int pixel(int32_t normalized,unsigned side){return (int)((uint32_t)normalized*side/10000u);}
static bool circle(unsigned x,unsigned y,unsigned width,unsigned height){
    int32_t dx=(int32_t)(2*x+1)-(int32_t)width,dy=(int32_t)(2*y+1)-(int32_t)height;
    int32_t diameter=(int32_t)(width<height?width:height);
    return dx*dx+dy*dy<=diameter*diameter;
}
static void text(ag_gauge_renderer_t *r,uint16_t y,const char *value,int x0,int y0,int x1,int y1,
    unsigned desired_scale,uint16_t color){
    size_t count=strlen(value);if(count==0||x1<=x0||y1<=y0)return;
    unsigned scale=desired_scale;
    while(scale>0&&((int)(count*6*scale- scale)>x1-x0||(int)(7*scale)>y1-y0))--scale;
    if(scale==0)return;
    int left=x0+((x1-x0)-(int)(count*6*scale-scale))/2;
    int top=y0+((y1-y0)-(int)(7*scale))/2;
    if((int)y<top||(int)y>=top+(int)(7*scale))return;
    unsigned gy=((unsigned)y-(unsigned)top)/scale;
    for(size_t c=0;c<count;++c){const uint8_t *rows=glyph(value[c]);
        for(unsigned gx=0;gx<5;++gx)if(rows[gy]&(1u<<(4-gx))){
            int start=left+(int)(c*6*scale+gx*scale);
            for(unsigned k=0;k<scale;++k){int x=start+(int)k;
                if(x>=x0&&x<x1&&x>=0&&x<r->options.width&&circle((unsigned)x,y,r->options.width,r->options.height))r->row[x]=color;
            }
        }
    }
}
static void row(ag_gauge_renderer_t *r,uint16_t y){
    const ag_gauge_scene_t *s=&r->scene;memset(r->row,0,r->options.width*sizeof(r->row[0]));
    if(s->power==AG_GAUGE_POWER_OFF)return;
    if(s->power!=AG_GAUGE_POWER_ON){
        text(r,y,"POWER ?",0,0,r->options.width,r->options.height,3,0xffe0);return;
    }
    for(size_t i=0;i<s->widget_count;++i){const ag_gauge_widget_t *w=&s->widgets[i];
        int x0=pixel(w->rect.x,r->options.width),y0=pixel(w->rect.y,r->options.height);
        int x1=pixel(w->rect.x+w->rect.width,r->options.width),y1=pixel(w->rect.y+w->rect.height,r->options.height);
        if(y<y0||y>=y1||x1<=x0||y1<=y0)continue;
        for(int x=x0;x<x1;++x)if(circle((unsigned)x,y,r->options.width,r->options.height)){
            bool border=x==x0||x==x1-1||y==y0||y==y1-1;r->row[x]=border?0x2104:0x0841;
        }
        int h=y1-y0;uint16_t color=w->quality==AG_SAMPLE_VALID&&w->has_source&&w->has_value&&isfinite(w->value)?0xffff:0xffe0;
        text(r,y,r->labels[i],x0+2,y0+2,x1-2,y0+h/4,2,0x07ff);
        text(r,y,r->values[i],x0+2,y0+h/4,x1-2,y0+3*h/4,6,color);
        text(r,y,r->units[i],x0+2,y0+3*h/4,x1-2,y1-2,2,0xbdf7);
    }
}
ag_gauge_render_result_t ag_gauge_renderer_step(ag_gauge_renderer_t *r,size_t budget){
    if(r==NULL||!r->initialized||budget==0||budget>AG_GAUGE_RENDER_ROWS_PER_STEP_MAX)return AG_RENDER_INVALID;
    if(r->faulted)return AG_RENDER_FAULT;
    if(r->busy)return AG_RENDER_BUSY;
    if(!r->active)return AG_RENDER_DONE;
    r->busy=true;
    for(size_t i=0;i<budget&&r->next_y<r->options.height;++i){row(r,r->next_y);
        if(!r->options.write_row(r->options.context,r->next_y,r->row,r->options.width)){
            r->faulted=true;r->active=false;r->busy=false;return AG_RENDER_FAULT;
        }
        ++r->next_y;
    }
    if(r->next_y==r->options.height)r->active=false;
    r->busy=false;return r->active?AG_RENDER_OK:AG_RENDER_DONE;
}
