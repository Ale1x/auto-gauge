#include "ag_gauge_ui.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static ag_gauge_ui_t ui;
static ag_config_t config;
static ag_pid_mask_t supported;
static ag_can_policy_t policy;
static ag_gauge_binding_t bindings[AG_GAUGE_UI_READING_MAX];
static ag_measurement_t readings[AG_GAUGE_UI_READING_MAX];
static ag_gauge_input_t input;
static ag_gauge_scene_t scene;
static void boot(void){
    ag_gauge_ui_options_t options={1000,100};assert(ag_gauge_ui_init(&ui,&options));
    memset(&config,0,sizeof(config));config.schema_version=AG_CONFIG_VERSION;config.revision=1;config.dashboard_count=3;config.manual_percent=60;
    const uint8_t pids[]={0x0c,0x0d,0x05};
    for(size_t d=0;d<3;++d){config.dashboards[d].id=(int32_t)(d+1)*10;config.dashboards[d].widget_count=3;
        for(size_t w=0;w<3;++w)config.dashboards[d].widgets[w]=(ag_widget_t){(int32_t)w+1,pids[w],{2000,2000+(int32_t)w*2000,6000,2000}};}
    ag_pid_mask_clear(&supported);for(unsigned pid=1;pid<256;++pid)if(ag_obd_pid_implemented((uint8_t)pid))ag_pid_mask_set(&supported,(uint8_t)pid);
    policy=(ag_can_policy_t){.power=AG_POWER_OBD,.ignition=AG_IGN_QUALIFIED_ON,.hardware_validated=true,.rails_valid=true,.diagnostic_session_active=true};
    memset(bindings,0,sizeof(bindings));memset(readings,0,sizeof(readings));
    const uint8_t rpm[]={0x41,0x0c,0,0},speed[]={0x41,0x0d,0},temp[]={0x41,0x05,40};
    assert(ag_obd_parse_measurement(rpm,sizeof(rpm),0x0c,0x7e8,0,&readings[0])==AG_OBD_OK);
    assert(ag_obd_parse_measurement(speed,sizeof(speed),0x0d,0x7e8,0,&readings[1])==AG_OBD_OK);
    assert(ag_obd_parse_measurement(temp,sizeof(temp),0x05,0x7e8,0,&readings[2])==AG_OBD_OK);
    for(size_t i=0;i<3;++i)bindings[i]=(ag_gauge_binding_t){pids[i],0x7e8};
    input=(ag_gauge_input_t){&config,&supported,bindings,3,readings,3,&policy,0,false,0};
}
static void unavailable(const ag_gauge_widget_t *w,ag_sample_status_t quality){assert(!w->has_value&&isnan(w->value)&&w->quality==quality&&w->unit==AG_UNIT_UNKNOWN);}
static void empty_scene(void){assert(scene.widget_count==0&&scene.power==AG_GAUGE_POWER_UNAVAILABLE&&scene.brightness==AG_GAUGE_BRIGHTNESS_UNAVAILABLE);}
static void test_zero_decode_and_scene_geometry(void){
    boot();assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK);
    assert(scene.power==AG_GAUGE_POWER_ON&&scene.brightness==AG_GAUGE_BRIGHTNESS_MANUAL&&scene.brightness_percent==60&&scene.dashboard_id==10&&scene.widget_count==3);
    for(size_t w=0;w<3;++w){assert(scene.widgets[w].has_value&&scene.widgets[w].value==0&&scene.widgets[w].quality==AG_SAMPLE_VALID&&scene.widgets[w].has_source&&scene.widgets[w].ecu_source==0x7e8);
        assert(!memcmp(&scene.widgets[w].rect,&config.dashboards[0].widgets[w].rect,sizeof(ag_rect_t)));}
    assert(scene.widgets[0].unit==AG_UNIT_RPM&&scene.widgets[1].unit==AG_UNIT_KM_PER_HOUR&&scene.widgets[2].unit==AG_UNIT_CELSIUS);
    readings[0].raw[0]=0x1f;readings[0].raw[1]=0x40;readings[0].value=-999;readings[0].unit=AG_UNIT_PERCENT;
    assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.widgets[0].value==2000&&scene.widgets[0].unit==AG_UNIT_RPM);
}
static void test_quality_stale_and_no_previous_value(void){
    for(unsigned quality=AG_SAMPLE_UNSUPPORTED;quality<=AG_SAMPLE_UNAVAILABLE;++quality){
        boot();readings[0].status=(ag_sample_status_t)quality;readings[0].has_value=true;readings[0].value=2000;
        assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK);unavailable(&scene.widgets[0],(ag_sample_status_t)quality);
    }
    boot();input.now_ms=999;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.widgets[0].has_value);
    input.now_ms=1000;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK);unavailable(&scene.widgets[0],AG_SAMPLE_STALE);
    for(unsigned error=0;error<5;++error){
        boot();switch(error){case 0:readings[0].updated_ms=1;break;case 1:readings[0].has_value=false;break;case 2:readings[0].raw_length=0;break;case 3:readings[0].raw_length=8;break;case 4:readings[0].status=(ag_sample_status_t)99;break;}
        assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK);unavailable(&scene.widgets[0],AG_SAMPLE_ERROR);
    }
    boot();input.reading_count=0;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK);unavailable(&scene.widgets[0],AG_SAMPLE_UNAVAILABLE);
    boot();input.binding_count=0;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&!scene.widgets[0].has_source);unavailable(&scene.widgets[0],AG_SAMPLE_UNAVAILABLE);
}
static void test_source_binding_and_bounds(void){
    boot();readings[3]=readings[0];readings[3].ecu_source=0x7e9;readings[3].raw[0]=0x0f;readings[3].raw[1]=0xa0;
    input.reading_count=4;bindings[0].ecu_source=0x7e9;
    assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.widgets[0].value==1000&&scene.widgets[0].ecu_source==0x7e9);
    readings[3].ecu_source=0x7e8;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_INVALID);empty_scene();
    boot();bindings[1]=bindings[0];assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_INVALID);empty_scene();
    boot();input.reading_count=AG_GAUGE_UI_READING_MAX+1;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_INVALID);empty_scene();
    boot();input.binding_count=AG_GAUGE_UI_READING_MAX+1;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_INVALID);empty_scene();
    boot();config.dashboards[0].widgets[0].rect.width=10001;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_INVALID);empty_scene();
    boot();ag_pid_mask_set(&supported,255);config.dashboards[0].widgets[0].pid=255;bindings[0].pid=255;
    assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK);unavailable(&scene.widgets[0],AG_SAMPLE_UNSUPPORTED);
}
static void test_tap_cycles_and_revision_changes(void){
    boot();assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK);ag_gauge_ui_t before=ui;
    assert(ag_gauge_ui_tap(&ui,false,&policy,99)==AG_GAUGE_UI_DENIED&&!memcmp(&ui,&before,sizeof(ui)));
    assert(ag_gauge_ui_tap(&ui,true,&policy,1)==AG_GAUGE_UI_OK);input.now_ms=1;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.dashboard_id==20);
    assert(ag_gauge_ui_tap(&ui,true,&policy,100)==AG_GAUGE_UI_DEBOUNCED);assert(ag_gauge_ui_tap(&ui,true,&policy,101)==AG_GAUGE_UI_OK);
    input.now_ms=101;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.dashboard_id==30);
    assert(ag_gauge_ui_tap(&ui,true,&policy,201)==AG_GAUGE_UI_OK);input.now_ms=201;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.dashboard_id==10);
    config.revision=2;ag_dashboard_t temp=config.dashboards[0];config.dashboards[0]=config.dashboards[2];config.dashboards[2]=temp;
    assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.dashboard_id==10);
    config.revision=3;config.dashboard_count=2;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.dashboard_id==30);
    config.manual_percent=20;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_REVISION_CONFLICT);empty_scene();
    config.manual_percent=60;config.revision=2;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_REVISION_CONFLICT);empty_scene();
    boot();config.dashboard_count=1;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK);
    assert(ag_gauge_ui_tap(&ui,true,&policy,0)==AG_GAUGE_UI_OK);assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.dashboard_id==10);
}
static void test_power_and_brightness_no_guessed_fallback(void){
    for(unsigned state=0;state<7;++state){
        boot();ag_gauge_power_t wanted=AG_GAUGE_POWER_OFF;
        switch(state){case 0:policy.ignition=AG_IGN_OFF;break;case 1:policy.power=AG_POWER_USB_ONLY;break;case 2:policy.standby_requested=true;break;case 3:policy.power=AG_POWER_NONE;break;
            case 4:input.power_policy=NULL;wanted=AG_GAUGE_POWER_UNAVAILABLE;break;case 5:policy.ignition=AG_IGN_UNKNOWN;wanted=AG_GAUGE_POWER_UNAVAILABLE;break;case 6:policy.rails_valid=false;wanted=AG_GAUGE_POWER_UNAVAILABLE;break;}
        assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.power==wanted&&scene.brightness==AG_GAUGE_BRIGHTNESS_UNAVAILABLE);
        for(size_t w=0;w<scene.widget_count;++w)unavailable(&scene.widgets[w],AG_SAMPLE_UNAVAILABLE);
        assert(ag_gauge_ui_tap(&ui,true,input.power_policy,0)==AG_GAUGE_UI_DENIED);
    }
    boot();policy.bus_off=true;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.power==AG_GAUGE_POWER_ON);unavailable(&scene.widgets[0],AG_SAMPLE_UNAVAILABLE);
    boot();config.brightness_mode=AG_BRIGHTNESS_AUTO;config.manual_percent=71;
    assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.brightness==AG_GAUGE_BRIGHTNESS_UNAVAILABLE&&scene.brightness_percent==0);
    input.als_valid=true;input.als_percent=101;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.brightness==AG_GAUGE_BRIGHTNESS_UNAVAILABLE);
    input.als_percent=0;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.brightness==AG_GAUGE_BRIGHTNESS_AUTO&&scene.brightness_percent==0);
    input.als_percent=100;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.brightness_percent==100);
    boot();config.manual_percent=0;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.brightness==AG_GAUGE_BRIGHTNESS_MANUAL&&scene.brightness_percent==0);
}
static void test_clock_and_all_scalar_decoders(void){
    boot();input.now_ms=UINT64_MAX-100;readings[0].updated_ms=input.now_ms;readings[1].updated_ms=input.now_ms;readings[2].updated_ms=input.now_ms;
    assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK);assert(ag_gauge_ui_tap(&ui,true,&policy,UINT64_MAX)==AG_GAUGE_UI_OK);
    input.now_ms=UINT64_MAX;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK);
    input.now_ms=UINT64_MAX-1;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_CLOCK_FAULT);empty_scene();
    input.now_ms=UINT64_MAX;assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_CLOCK_FAULT);empty_scene();
    for(unsigned pid=1;pid<256;++pid)if(ag_obd_pid_implemented((uint8_t)pid)){
        boot();config.dashboard_count=1;config.dashboards[0].widget_count=1;config.dashboards[0].widgets[0].pid=(int32_t)pid;
        bindings[0].pid=(uint8_t)pid;input.binding_count=input.reading_count=1;
        uint8_t payload[10]={0x41,(uint8_t)pid,255,255,255,255,255,255,255,255};bool decoded=false;
        for(size_t length=3;length<=sizeof(payload);++length)if(ag_obd_parse_measurement(payload,length,(uint8_t)pid,0x7e8,0,&readings[0])==AG_OBD_OK){decoded=true;break;}
        assert(decoded);double value=readings[0].value;ag_unit_t unit=readings[0].unit;
        assert(ag_gauge_ui_project(&ui,&input,&scene)==AG_GAUGE_UI_OK&&scene.widgets[0].has_value&&scene.widgets[0].value==value&&scene.widgets[0].unit==unit);
    }
}
int main(void){test_zero_decode_and_scene_geometry();test_quality_stale_and_no_previous_value();test_source_binding_and_bounds();test_tap_cycles_and_revision_changes();test_power_and_brightness_no_guessed_fallback();test_clock_and_all_scalar_decoders();puts("PASS: 6 gauge UI scene/control groups; no physical display IO");return 0;}
