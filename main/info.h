#ifndef INFO_H
#define INFO_H
/*
 * Function: system_info
 * Purpose:
 * Input:
 * Output:
 */
void system_info();
/*
 * Function:
 * Purpose:
 * Input:
 * Output:
 */
void memory_info(const char * msg);
/*
 * Function:
 * Purpose:
 * Input:
 * Output:
 */
char *get_config_str(const char * namespace,const char *key);
/*
 * Function:
 * Purpose:
 * Input:
 * Output:
 */
bool is_wifi_configured();
int set_config_str(const char *namespace,const char *key,const char * value);
int commit_info(const char *namespace);
#endif