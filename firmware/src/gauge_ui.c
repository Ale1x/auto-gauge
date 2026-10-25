#include "ag_gauge_ui.h"
#include <math.h>
#include <string.h>

static void safe_scene(ag_gauge_scene_t *scene){memset(scene,0,sizeof(*scene));scene->power=AG_GAUGE_POWER_UNAVAILABLE;scene->brightness=AG_GAUGE_BRIGHTNESS_UNAVAILABLE;}
static ag_gauge_power_t power(const ag_can_policy_t *p){
    if(p==NULL)return AG_GAUGE_POWER_UNAVAILABLE;
    if(p->standby_requested||p->power==AG_POWER_NONE||p->power==AG_POWER_USB_ONLY||p->ignition==AG_IGN_OFF)return AG_GAUGE_POWER_OFF;
    if(!p->hardware_validated||!p->rails_valid||(p->power!=AG_POWER_OBD&&p->power!=AG_POWER_OBD_AND_USB)||p->ignition!=AG_IGN_QUALIFIED_ON)return AG_GAUGE_POWER_UNAVAILABLE;
    return AG_GAUGE_POWER_ON;
}
static bool same_configuration(const ag_config_t *a,const ag_config_t *b){
    if(a->schema_version!=b->schema_version||a->revision!=b->revision||a->dashboard_count!=b->dashboard_count
        ||a->brightness_mode!=b->brightness_mode||a->manual_percent!=b->manual_percent)return false;
    for(size_t d=0;d<a->dashboard_count;++d){
        const ag_dashboard_t *x=&a->dashboards[d],*y=&b->dashboards[d];
        if(x->id!=y->id||x->widget_count!=y->widget_count)return false;
        for(size_t w=0;w<x->widget_count;++w){const ag_widget_t *u=&x->widgets[w],*v=&y->widgets[w];
            if(u->id!=v->id||u->pid!=v->pid||u->rect.x!=v->rect.x||u->rect.y!=v->rect.y||u->rect.width!=v->rect.width||u->rect.height!=v->rect.height)return false;
        }
    }
    return true;
}
static void copy_configuration(ag_config_t *out,const ag_config_t *in){
    memset(out,0,sizeof(*out));out->schema_version=in->schema_version;out->revision=in->revision;
    out->dashboard_count=in->dashboard_count;out->brightness_mode=in->brightness_mode;out->manual_percent=in->manual_percent;
    for(size_t d=0;d<in->dashboard_count;++d){out->dashboards[d].id=in->dashboards[d].id;out->dashboards[d].widget_count=in->dashboards[d].widget_count;
        for(size_t w=0;w<in->dashboards[d].widget_count;++w)out->dashboards[d].widgets[w]=in->dashboards[d].widgets[w];}
}
static bool input_valid(const ag_gauge_input_t *in){
    if(in==NULL||in->configuration==NULL||in->supported==NULL||in->binding_count>AG_GAUGE_UI_READING_MAX||in->reading_count>AG_GAUGE_UI_READING_MAX
        ||(in->binding_count&&in->bindings==NULL)||(in->reading_count&&in->readings==NULL)
        ||ag_config_validate(in->configuration,in->supported)!=AG_CONFIG_OK)return false;
    for(size_t i=0;i<in->binding_count;++i){
        if(in->bindings[i].pid==0)return false;
        for(size_t j=0;j<i;++j)if(in->bindings[j].pid==in->bindings[i].pid)return false;
    }
    for(size_t i=0;i<in->reading_count;++i)for(size_t j=0;j<i;++j)
        if(in->readings[i].pid==in->readings[j].pid&&in->readings[i].ecu_source==in->readings[j].ecu_source)return false;
    return true;
}
static ag_gauge_ui_result_t observe(ag_gauge_ui_t *ui,uint64_t now){
    if(ui->clock_fault)return AG_GAUGE_UI_CLOCK_FAULT;
    if(ui->has_time&&now<ui->last_ms){ui->clock_fault=true;return AG_GAUGE_UI_CLOCK_FAULT;}
    ui->last_ms=now;ui->has_time=true;return AG_GAUGE_UI_OK;
}
bool ag_gauge_ui_init(ag_gauge_ui_t *ui,const ag_gauge_ui_options_t *options){
    if(ui==NULL||options==NULL||options->maximum_sample_age_ms==0)return false;
    ag_gauge_ui_options_t copy=*options;memset(ui,0,sizeof(*ui));ui->options=copy;ui->initialized=true;return true;
}
static void project_widget(const ag_gauge_input_t *in,const ag_gauge_ui_t *ui,const ag_widget_t *config,ag_gauge_widget_t *out,bool measurements_allowed){
    out->id=config->id;out->pid=(uint8_t)config->pid;out->rect=config->rect;out->quality=AG_SAMPLE_UNAVAILABLE;out->unit=AG_UNIT_UNKNOWN;out->value=NAN;
    for(size_t i=0;i<in->binding_count;++i)if(in->bindings[i].pid==out->pid){out->has_source=true;out->ecu_source=in->bindings[i].ecu_source;break;}
    if(!ag_obd_pid_implemented(out->pid)){out->quality=AG_SAMPLE_UNSUPPORTED;return;}
    if(!measurements_allowed||!out->has_source)return;
    const ag_measurement_t *reading=NULL;
    for(size_t i=0;i<in->reading_count;++i)if(in->readings[i].pid==out->pid&&in->readings[i].ecu_source==out->ecu_source){reading=&in->readings[i];break;}
    if(reading==NULL)return;
    if((unsigned)reading->status>AG_SAMPLE_UNAVAILABLE||reading->updated_ms>in->now_ms){out->quality=AG_SAMPLE_ERROR;return;}
    out->quality=reading->status;if(reading->status!=AG_SAMPLE_VALID)return;
    if(!reading->has_value||reading->raw_length==0||reading->raw_length>AG_RAW_CAPACITY){out->quality=AG_SAMPLE_ERROR;return;}
    if(in->now_ms-reading->updated_ms>=ui->options.maximum_sample_age_ms){out->quality=AG_SAMPLE_STALE;return;}
    uint8_t payload[AG_RAW_CAPACITY+2]={0x41,out->pid};memcpy(payload+2,reading->raw,reading->raw_length);
    ag_measurement_t decoded;
    if(ag_obd_parse_measurement(payload,(size_t)reading->raw_length+2,out->pid,out->ecu_source,reading->updated_ms,&decoded)!=AG_OBD_OK
        ||!decoded.has_value||!isfinite(decoded.value)){out->quality=AG_SAMPLE_ERROR;return;}
    out->quality=AG_SAMPLE_VALID;out->has_value=true;out->value=decoded.value;out->unit=decoded.unit;
}
ag_gauge_ui_result_t ag_gauge_ui_project(ag_gauge_ui_t *ui,const ag_gauge_input_t *in,ag_gauge_scene_t *out){
    if(out==NULL)return AG_GAUGE_UI_INVALID;
    safe_scene(out);
    if(ui==NULL||!ui->initialized||!input_valid(in))return AG_GAUGE_UI_INVALID;
    ag_gauge_ui_result_t time=observe(ui,in->now_ms);if(time!=AG_GAUGE_UI_OK)return time;
    if(ui->has_configuration){
        if(in->configuration->revision<ui->configuration.revision
            ||(in->configuration->revision==ui->configuration.revision&&!same_configuration(in->configuration,&ui->configuration)))return AG_GAUGE_UI_REVISION_CONFLICT;
        if(in->configuration->revision!=ui->configuration.revision){
            int32_t selected_id=ui->configuration.dashboards[ui->selected].id;size_t selected=0;
            for(size_t d=0;d<in->configuration->dashboard_count;++d)if(in->configuration->dashboards[d].id==selected_id){selected=d;break;}
            ui->selected=selected;copy_configuration(&ui->configuration,in->configuration);
        }
    }else{copy_configuration(&ui->configuration,in->configuration);ui->has_configuration=true;ui->selected=0;}
    out->configuration_revision=(uint32_t)ui->configuration.revision;out->dashboard_count=(uint8_t)ui->configuration.dashboard_count;
    const ag_dashboard_t *dashboard=&ui->configuration.dashboards[ui->selected];out->dashboard_id=dashboard->id;out->widget_count=(uint8_t)dashboard->widget_count;
    out->power=power(in->power_policy);
    if(out->power==AG_GAUGE_POWER_ON){
        if(ui->configuration.brightness_mode==AG_BRIGHTNESS_MANUAL){out->brightness=AG_GAUGE_BRIGHTNESS_MANUAL;out->brightness_percent=(uint8_t)ui->configuration.manual_percent;}
        else if(in->als_valid&&in->als_percent<=100){out->brightness=AG_GAUGE_BRIGHTNESS_AUTO;out->brightness_percent=in->als_percent;}
    }
    bool measurements_allowed=out->power==AG_GAUGE_POWER_ON&&ag_can_tx_allowed(in->power_policy);
    for(size_t w=0;w<dashboard->widget_count;++w)project_widget(in,ui,&dashboard->widgets[w],&out->widgets[w],measurements_allowed);
    return AG_GAUGE_UI_OK;
}
ag_gauge_ui_result_t ag_gauge_ui_tap(ag_gauge_ui_t *ui,bool qualified,const ag_can_policy_t *policy,uint64_t now){
    if(ui==NULL||!ui->initialized)return AG_GAUGE_UI_INVALID;
    if(!qualified)return AG_GAUGE_UI_DENIED;
    ag_gauge_ui_result_t time=observe(ui,now);if(time!=AG_GAUGE_UI_OK)return time;
    if(!ui->has_configuration)return AG_GAUGE_UI_NOT_READY;
    if(power(policy)!=AG_GAUGE_POWER_ON)return AG_GAUGE_UI_DENIED;
    if(ui->has_tap&&now-ui->last_tap_ms<ui->options.tap_debounce_ms)return AG_GAUGE_UI_DEBOUNCED;
    ui->selected=(ui->selected+1)%ui->configuration.dashboard_count;ui->last_tap_ms=now;ui->has_tap=true;return AG_GAUGE_UI_OK;
}
