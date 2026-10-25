#include "ag_gauge_renderer.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static ag_gauge_renderer_t renderer;
static struct {
    uint16_t pixels[466][466];
    size_t calls,fail_at;
    bool reenter;
} sink;
static bool write_row(void *p,uint16_t y,const uint16_t *pixels,size_t width){
    assert(p==&sink&&y<466&&width==466&&pixels!=NULL);assert(y==sink.calls);
    ++sink.calls;
    if(sink.reenter)assert(ag_gauge_renderer_step(&renderer,1)==AG_RENDER_BUSY);
    if(sink.reenter)assert(ag_gauge_renderer_cancel_pending(&renderer)==AG_RENDER_BUSY);
    if(sink.calls==sink.fail_at)return false;
    memcpy(sink.pixels[y],pixels,width*sizeof(*pixels));return true;
}
static ag_gauge_scene_t scene(void){
    ag_gauge_scene_t s={.configuration_revision=1,.dashboard_id=1,.dashboard_count=1,.widget_count=2,
        .power=AG_GAUGE_POWER_ON,.brightness=AG_GAUGE_BRIGHTNESS_MANUAL,.brightness_percent=50};
    s.widgets[0]=(ag_gauge_widget_t){.id=1,.pid=12,.rect={2000,2000,6000,3000},.quality=AG_SAMPLE_VALID,.unit=AG_UNIT_RPM,.value=0,.has_source=true,.has_value=true};
    s.widgets[1]=(ag_gauge_widget_t){.id=2,.pid=5,.rect={2000,5000,6000,3000},.quality=AG_SAMPLE_VALID,.unit=AG_UNIT_CELSIUS,.value=-12.5,.has_source=true,.has_value=true};
    return s;
}
static void boot(void){
    memset(&sink,0,sizeof(sink));
    ag_gauge_renderer_options_t options={&sink,write_row,466,466};assert(ag_gauge_renderer_init(&renderer,&options));
}
static void complete(void){
    while(renderer.active){size_t before=sink.calls;ag_gauge_render_result_t result=ag_gauge_renderer_step(&renderer,16);
        assert(result==AG_RENDER_OK||result==AG_RENDER_DONE);assert(sink.calls-before<=16);}
    assert(sink.calls==466&&ag_gauge_renderer_step(&renderer,1)==AG_RENDER_DONE);
}
static size_t color_count(uint16_t color){size_t n=0;for(unsigned y=0;y<466;++y)for(unsigned x=0;x<466;++x)if(sink.pixels[y][x]==color)++n;return n;}
static void test_geometry_snapshot_zero_and_clip(void){
    boot();ag_gauge_scene_t s=scene();assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK&&sink.calls==0);
    assert(strcmp(renderer.values[0],"0.0")==0&&strcmp(renderer.values[1],"-12.5")==0&&strcmp(renderer.units[0],"RPM")==0);
    s.widgets[0].value=999;s.widgets[0].rect.width=0;
    assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_BUSY&&renderer.scene.widgets[0].value==0);
    sink.reenter=true;complete();assert(color_count(0xffff)>0&&color_count(0x07ff)>0&&color_count(0xbdf7)>0);
    assert(sink.pixels[0][0]==0&&sink.pixels[465][465]==0&&sink.pixels[93][93]==0x2104);
    for(unsigned y=0;y<466;++y)for(unsigned x=0;x<466;++x){
        double dx=(double)x+0.5-233,dy=(double)y+0.5-233;
        if(dx*dx+dy*dy>233.0*233.0)assert(sink.pixels[y][x]==0);
    }
}
static void test_quality_power_and_no_stale_value(void){
    for(unsigned quality=AG_SAMPLE_UNSUPPORTED;quality<=AG_SAMPLE_UNAVAILABLE;++quality){
        boot();ag_gauge_scene_t s=scene();s.widget_count=1;s.widgets[0].quality=(ag_sample_status_t)quality;
        s.widgets[0].value=9000;assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK);
        assert(strcmp(renderer.values[0],"--")==0&&strcmp(renderer.units[0],"?")==0);complete();
        assert(color_count(0xffff)==0&&color_count(0xffe0)>0);
    }
    for(unsigned which=0;which<4;++which){boot();ag_gauge_scene_t s=scene();
        if(which==0)s.widgets[0].value=NAN;else if(which==1)s.widgets[0].has_value=false;else if(which==2)s.widgets[0].value=INFINITY;else s.widgets[0].has_source=false;
        assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK&&strcmp(renderer.values[0],"--")==0);complete();}
    boot();ag_gauge_scene_t s=scene();s.power=AG_GAUGE_POWER_OFF;
    assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK);complete();assert(color_count(0)==466u*466u);
    boot();s=scene();s.power=AG_GAUGE_POWER_UNAVAILABLE;assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK);
    complete();assert(color_count(0xffe0)>0&&color_count(0xffff)==0);
}
static void test_invalid_input_no_io_and_fault_latch(void){
    for(unsigned which=0;which<10;++which){boot();ag_gauge_scene_t s=scene();
        switch(which){case 0:s.widget_count=AG_MAX_WIDGETS+1;break;case 1:s.widgets[0].rect.x=-1;break;
            case 2:s.widgets[0].rect.width=10001;break;case 3:s.widgets[0].rect.x=INT32_MAX;break;
            case 4:s.widgets[0].rect.height=0;break;case 5:s.widgets[1].id=1;break;
            case 6:s.dashboard_count=0;break;case 7:s.brightness_percent=101;break;
            case 8:s.widgets[0].quality=(ag_sample_status_t)99;break;case 9:s.widgets[0].unit=(ag_unit_t)99;break;}
        assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_INVALID&&sink.calls==0&&!renderer.active);
    }
    boot();ag_gauge_scene_t s=scene();assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK);
    assert(ag_gauge_renderer_step(&renderer,0)==AG_RENDER_INVALID&&ag_gauge_renderer_step(&renderer,17)==AG_RENDER_INVALID&&sink.calls==0);
    sink.fail_at=3;assert(ag_gauge_renderer_step(&renderer,16)==AG_RENDER_FAULT&&sink.calls==3&&renderer.faulted&&!renderer.active);
    assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_FAULT&&ag_gauge_renderer_step(&renderer,1)==AG_RENDER_FAULT&&sink.calls==3);
    assert(ag_gauge_renderer_cancel_pending(&renderer)==AG_RENDER_FAULT);
    boot();s=scene();assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK);
    assert(ag_gauge_renderer_step(&renderer,1)==AG_RENDER_OK&&sink.calls==1);
    assert(ag_gauge_renderer_cancel_pending(&renderer)==AG_RENDER_OK&&!renderer.active&&sink.calls==1);
    s.power=AG_GAUGE_POWER_OFF;sink.calls=0;assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK);complete();
    assert(color_count(0)==466u*466u);
    ag_gauge_renderer_options_t options={&sink,write_row,467,466};assert(!ag_gauge_renderer_init(&renderer,&options));
    options.width=466;options.height=31;assert(!ag_gauge_renderer_init(&renderer,&options));
}
static void test_range_tiny_rect_and_full_circle(void){
    boot();ag_gauge_scene_t s=scene();s.widget_count=1;s.widgets[0].rect=(ag_rect_t){0,0,10000,10000};s.widgets[0].value=1e300;
    assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK&&strcmp(renderer.values[0],"RANGE")==0);complete();
    assert(sink.pixels[0][0]==0&&sink.pixels[233][233]!=0);
    boot();s=scene();s.widget_count=1;s.widgets[0].rect=(ag_rect_t){9999,9999,1,1};
    assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK);complete();assert(color_count(0)==466u*466u);
}
static void preview(void){
    const char *path=getenv("AG_RENDER_PREVIEW");if(path==NULL)return;
    boot();ag_gauge_scene_t s=scene();s.widgets[0].value=2000;assert(ag_gauge_renderer_begin(&renderer,&s)==AG_RENDER_OK);complete();
    FILE *file=fopen(path,"wb");assert(file);assert(fprintf(file,"P6\n466 466\n255\n")>0);
    for(unsigned y=0;y<466;++y)for(unsigned x=0;x<466;++x){uint16_t word=sink.pixels[y][x];
        uint8_t rgb[3]={(uint8_t)(((word>>11)&31)*255/31),(uint8_t)(((word>>5)&63)*255/63),(uint8_t)((word&31)*255/31)};
        assert(fwrite(rgb,1,sizeof(rgb),file)==sizeof(rgb));}
    assert(fclose(file)==0);
}
int main(void){test_geometry_snapshot_zero_and_clip();test_quality_power_and_no_stale_value();test_invalid_input_no_io_and_fault_latch();test_range_tiny_rect_and_full_circle();preview();
    puts("PASS: 4 bounded RGB565 renderer groups; no physical panel IO");return 0;}
