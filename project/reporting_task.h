#ifndef _REPORTING_
#define _REPORTING_

void init_reporting(void);

void reportBMPData(float temperature, float pressure);
void reportTMPData(float temperature);
void reportGPSData(float lat, float lng, float altitude);
void reportWINDData(int counts, int direction_degrees);
void reportRAINData(int tips);
void reportRainScaledData(float rain_hr, float rain_day);
void reportWINDScaledData(float avgSpeed_2m, int avgDirection_2m, float gustSpeed_10m, int gustDirection_10m);

#endif // _REPORTING_