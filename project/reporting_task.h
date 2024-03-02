#ifndef _REPORTING_
#define _REPORTING_

void init_reporting(void);

void reportBMPData(float temperature, float pressure);
void reportTMPData(float temperature);
void reportGPSData(float lat, float lng, float altitude);
void reportWINDData(int counts, int direction_degrees);
void reportRAINData(int tips);

#endif // _REPORTING_