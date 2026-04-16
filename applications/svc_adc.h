#ifndef APPLICATIONS_SVC_ADC_H_
#define APPLICATIONS_SVC_ADC_H_

#include "app_types.h"

int svc_adc_init(void);
int svc_adc_task_start(void);
const app_adc_snapshot_t *svc_adc_get_snapshot(void);
rt_bool_t svc_adc_consume_s1_event(void);
rt_bool_t svc_adc_consume_s2_event(void);
rt_bool_t svc_adc_consume_s3_event(void);

#endif /* APPLICATIONS_SVC_ADC_H_ */