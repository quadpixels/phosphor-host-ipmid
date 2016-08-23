#include "chassishandler.h"
#include "ipmid-api.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <mapper.h>


//Defines
#define SET_PARM_VERSION 1
#define SET_PARM_BOOT_FLAGS_PERMANENT 0x40 //boot flags data1 7th bit on
#define SET_PARM_BOOT_FLAGS_VALID_ONE_TIME   0x80 //boot flags data1 8th bit on
#define SET_PARM_BOOT_FLAGS_VALID_PERMANENT  0xC0 //boot flags data1 7 & 8 bit on 



// OpenBMC Chassis Manager dbus framework
const char  *chassis_object_name   =  "/org/openbmc/control/chassis0";
const char  *chassis_intf_name     =  "org.openbmc.control.Chassis";


void register_netfn_chassis_functions() __attribute__((constructor));

// Host settings in dbus
// Service name should be referenced by connection name got via object mapper
const char *settings_object_name  =  "/org/openbmc/settings/host0";
const char *settings_intf_name    =  "org.freedesktop.DBus.Properties";
const char *host_intf_name        =  "org.openbmc.settings.Host";

int dbus_get_property(const char *name, char **buf)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    sd_bus *bus = NULL;
    char *temp_buf = NULL;
    char *connection = NULL;
    int r;

    // Get the system bus where most system services are provided.
    bus = ipmid_get_sd_bus_connection();

    r = mapper_get_service(bus, settings_object_name, &connection);
    if (r < 0) {
        fprintf(stderr, "Failed to get connection, return value: %s.\n", strerror(-r));
        goto finish;
    }

    /*
     * Bus, service, object path, interface and method are provided to call
     * the method.
     * Signatures and input arguments are provided by the arguments at the
     * end.
     */
    r = sd_bus_call_method(bus,
                           connection,                                 /* service to contact */
                           settings_object_name,                       /* object path */
                           settings_intf_name,                         /* interface name */
                           "Get",                                      /* method name */
                           &error,                                     /* object to return error in */
                           &m,                                         /* return message on success */
                           "ss",                                       /* input signature */
                           host_intf_name,                             /* first argument */
                           name);                                      /* second argument */

    if (r < 0) {
        fprintf(stderr, "Failed to issue method call: %s\n", error.message);
        goto finish;
    }

    /*
     * The output should be parsed exactly the same as the output formatting
     * specified.
     */
    r = sd_bus_message_read(m, "v", "s", &temp_buf);
    if (r < 0) {
        fprintf(stderr, "Failed to parse response message: %s\n", strerror(-r));
        goto finish;
    }

    asprintf(buf, "%s", temp_buf);
/*    *buf = (char*) malloc(strlen(temp_buf));
    if (*buf) {
        strcpy(*buf, temp_buf);
    }
*/
    printf("IPMID boot option property get: {%s}.\n", (char *) temp_buf);

finish:
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    free(connection);

    return r;
}

int dbus_set_property(const char * name, const char *value)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *m = NULL;
    sd_bus *bus = NULL;
    char *connection = NULL;
    int r;

    // Get the system bus where most system services are provided.
    bus = ipmid_get_sd_bus_connection();

    r = mapper_get_service(bus, settings_object_name, &connection);
    if (r < 0) {
        fprintf(stderr, "Failed to get connection, return value: %s.\n", strerror(-r));
        goto finish;
    }

    /*
     * Bus, service, object path, interface and method are provided to call
     * the method.
     * Signatures and input arguments are provided by the arguments at the
     * end.
     */
    r = sd_bus_call_method(bus,
                           connection,                                 /* service to contact */
                           settings_object_name,                       /* object path */
                           settings_intf_name,                         /* interface name */
                           "Set",                                      /* method name */
                           &error,                                     /* object to return error in */
                           &m,                                         /* return message on success */
                           "ssv",                                      /* input signature */
                           host_intf_name,                             /* first argument */
                           name,                                       /* second argument */
                           "s",                                        /* third argument */
                           value);                                     /* fourth argument */

    if (r < 0) {
        fprintf(stderr, "Failed to issue method call: %s\n", error.message);
        goto finish;
    }

    printf("IPMID boot option property set: {%s}.\n", value);

finish:
    sd_bus_error_free(&error);
    sd_bus_message_unref(m);
    free(connection);

    return r;
}

struct get_sys_boot_options_t {
    uint8_t parameter;
    uint8_t set;
    uint8_t block;
}  __attribute__ ((packed));

struct get_sys_boot_options_response_t {
    uint8_t version;
    uint8_t parm;
    uint8_t data[5];
}  __attribute__ ((packed));

struct set_sys_boot_options_t {
    uint8_t parameter;
    uint8_t data[8];
}  __attribute__ ((packed));

ipmi_ret_t ipmi_chassis_wildcard(ipmi_netfn_t netfn, ipmi_cmd_t cmd, 
                              ipmi_request_t request, ipmi_response_t response, 
                              ipmi_data_len_t data_len, ipmi_context_t context)
{
    printf("Handling CHASSIS WILDCARD Netfn:[0x%X], Cmd:[0x%X]\n",netfn, cmd);
    // Status code.
    ipmi_ret_t rc = IPMI_CC_OK;
    *data_len = 0;
    return rc;
}

//------------------------------------------------------------
// Calls into Chassis Control Dbus object to do the power off
//------------------------------------------------------------
int ipmi_chassis_power_control(const char *method)
{
	// sd_bus error
	int rc = 0;
	char  *busname = NULL;

	// SD Bus error report mechanism.
	sd_bus_error bus_error = SD_BUS_ERROR_NULL;

	// Response from the call. Although there is no response for this call,
	// obligated to mention this to make compiler happy.
	sd_bus_message *response = NULL;

	// Gets a hook onto either a SYSTEM or SESSION bus
	sd_bus *bus_type = ipmid_get_sd_bus_connection();
	rc = mapper_get_service(bus_type, chassis_object_name, &busname);
	if (rc < 0) {
		fprintf(stderr, "Failed to get bus name, return value: %s.\n", strerror(-rc));
	goto finish;
	}
	rc = sd_bus_call_method(bus_type,        		 // On the System Bus
							busname,        // Service to contact
							chassis_object_name,     // Object path 
							chassis_intf_name,       // Interface name
							method,      		 // Method to be called
							&bus_error,      		 // object to return error
							&response,		 		 // Response buffer if any
							NULL);			 		 // No input arguments
	if(rc < 0)
	{
		fprintf(stderr,"ERROR initiating Power Off:[%s]\n",bus_error.message);
	}
	else
	{
		printf("Chassis Power Off initiated successfully\n");
	}

finish:
    sd_bus_error_free(&bus_error);
    sd_bus_message_unref(response);
    free(busname);

    return rc;
}


//----------------------------------------------------------------------
// Chassis Control commands
//----------------------------------------------------------------------
ipmi_ret_t ipmi_chassis_control(ipmi_netfn_t netfn, ipmi_cmd_t cmd, 
                        ipmi_request_t request, ipmi_response_t response, 
                        ipmi_data_len_t data_len, ipmi_context_t context)
{
	// Error from power off.
	int rc = 0;

	// No response for this command.
    *data_len = 0;

	// Catch the actual operaton by peeking into request buffer
	uint8_t chassis_ctrl_cmd = *(uint8_t *)request;
	printf("Chassis Control Command: Operation:[0x%X]\n",chassis_ctrl_cmd);

	switch(chassis_ctrl_cmd)
	{
		case CMD_POWER_OFF:
			rc = ipmi_chassis_power_control("powerOff");
			break;
		case CMD_HARD_RESET:
			rc = ipmi_chassis_power_control("reboot");
			break;
		default:
		{
			fprintf(stderr, "Invalid Chassis Control command:[0x%X] received\n",chassis_ctrl_cmd);
			rc = -1;
		}
	}

	return ( (rc < 0) ? IPMI_CC_INVALID : IPMI_CC_OK);
}

struct bootOptionTypeMap_t {
    uint8_t ipmibootflag;
    char    dbusname[8];
};

#define INVALID_STRING "Invalid"
// dbus supports this list of boot devices.
bootOptionTypeMap_t g_bootOptionTypeMap_t[] = {

    {0x01, "Network"},
    {0x02, "Disk"},
    {0x03, "Safe"},
    {0x05, "CDROM"},
    {0x06, "Setup"},
    {0x00, "Default"},
    {0xFF, INVALID_STRING}
};

uint8_t get_ipmi_boot_option(char *p) {

    bootOptionTypeMap_t *s = g_bootOptionTypeMap_t;

    while (s->ipmibootflag != 0xFF) {
        if (!strcmp(s->dbusname,p))
            break;
        s++;
    }

    if (!s->ipmibootflag)
        printf("Failed to find Sensor Type %s\n", p);

    return s->ipmibootflag;
}

char* get_boot_option_by_ipmi(uint8_t p) {

    bootOptionTypeMap_t *s = g_bootOptionTypeMap_t;

    while (s->ipmibootflag != 0xFF) {

        if (s->ipmibootflag == p)
            break;

        s++;
    }


    if (!s->ipmibootflag)
        printf("Failed to find Sensor Type 0x%x\n", p);

    return s->dbusname;
}

ipmi_ret_t ipmi_chassis_get_sys_boot_options(ipmi_netfn_t netfn, ipmi_cmd_t cmd, 
                              ipmi_request_t request, ipmi_response_t response, 
                              ipmi_data_len_t data_len, ipmi_context_t context)
{
    ipmi_ret_t rc = IPMI_CC_PARM_NOT_SUPPORTED;
    char *p = NULL;
    get_sys_boot_options_response_t *resp = (get_sys_boot_options_response_t *) response;
    get_sys_boot_options_t *reqptr = (get_sys_boot_options_t*) request;
    uint8_t s;

    printf("IPMI GET_SYS_BOOT_OPTIONS\n");

    memset(resp,0,sizeof(*resp));
    resp->version   = SET_PARM_VERSION;
    resp->parm      = 5;
    resp->data[0]   = SET_PARM_BOOT_FLAGS_VALID_ONE_TIME;

    *data_len = sizeof(*resp);

    /*
     * Parameter #5 means boot flags. Please refer to 28.13 of ipmi doc.
     * This is the only parameter used by petitboot.
     */
    if (reqptr->parameter == 5) {

        /* Get the boot device */
        int r = dbus_get_property("boot_flags",&p);

        if (r < 0) {
            fprintf(stderr, "Dbus get property(boot_flags) failed for get_sys_boot_options.\n");
            rc = IPMI_CC_UNSPECIFIED_ERROR;

        } else {

            s = get_ipmi_boot_option(p);
            resp->data[1] = (s << 2);
            rc = IPMI_CC_OK;

        }

        if (p)
        {
          free(p);
          p = NULL;
        }

        /* Get the boot policy */
        r = dbus_get_property("boot_policy",&p);

        if (r < 0) {
            fprintf(stderr, "Dbus get property(boot_policy) failed for get_sys_boot_options.\n");
            rc = IPMI_CC_UNSPECIFIED_ERROR;

        } else {

            printf("BootPolicy is[%s]", p); 
            resp->data[0] = (strncmp(p,"ONETIME",strlen("ONETIME"))==0) ? 
                            SET_PARM_BOOT_FLAGS_VALID_ONE_TIME:
                            SET_PARM_BOOT_FLAGS_VALID_PERMANENT;
            rc = IPMI_CC_OK;

        }


    } else {
        fprintf(stderr, "Unsupported parameter 0x%x\n", reqptr->parameter);
    }

    if (p)
        free(p);

    return rc;
}



ipmi_ret_t ipmi_chassis_set_sys_boot_options(ipmi_netfn_t netfn, ipmi_cmd_t cmd,
                              ipmi_request_t request, ipmi_response_t response,
                              ipmi_data_len_t data_len, ipmi_context_t context)
{
    ipmi_ret_t rc = IPMI_CC_OK;
    char *s;

    printf("IPMI SET_SYS_BOOT_OPTIONS\n");

    set_sys_boot_options_t *reqptr = (set_sys_boot_options_t *) request;

    // This IPMI command does not have any resposne data
    *data_len = 0;

    /*  000101
     * Parameter #5 means boot flags. Please refer to 28.13 of ipmi doc.
     * This is the only parameter used by petitboot.
     */
    if (reqptr->parameter == 5) {

        s = get_boot_option_by_ipmi(((reqptr->data[1] & 0x3C) >> 2));

        printf("%d: %s\n", __LINE__, s);
        if (!strcmp(s,INVALID_STRING)) {

            rc = IPMI_CC_PARM_NOT_SUPPORTED;

        } else {

            int r = dbus_set_property("boot_flags",s);

            if (r < 0) {
                fprintf(stderr, "Dbus set property(boot_flags) failed for set_sys_boot_options.\n");
                rc = IPMI_CC_UNSPECIFIED_ERROR;
            }
        }
      
        /* setting the boot policy */
        s = (char *)(((reqptr->data[0] & SET_PARM_BOOT_FLAGS_PERMANENT) == 
                       SET_PARM_BOOT_FLAGS_PERMANENT) ?"PERMANENT":"ONETIME");

        printf ( "\nBoot Policy is %s",s); 
        int r = dbus_set_property("boot_policy",s);

        if (r < 0) {
            fprintf(stderr, "Dbus set property(boot_policy) failed for set_sys_boot_options.\n");
            rc = IPMI_CC_UNSPECIFIED_ERROR;
        }

    } else {
        fprintf(stderr, "Unsupported parameter 0x%x\n", reqptr->parameter);
        rc = IPMI_CC_PARM_NOT_SUPPORTED;
    }

    return rc;
}

void register_netfn_chassis_functions()
{
    printf("Registering NetFn:[0x%X], Cmd:[0x%X]\n",NETFUN_CHASSIS, IPMI_CMD_WILDCARD);
    ipmi_register_callback(NETFUN_CHASSIS, IPMI_CMD_WILDCARD, NULL, ipmi_chassis_wildcard);

    printf("Registering NetFn:[0x%X], Cmd:[0x%X]\n",NETFUN_CHASSIS, IPMI_CMD_GET_SYS_BOOT_OPTIONS);
    ipmi_register_callback(NETFUN_CHASSIS, IPMI_CMD_GET_SYS_BOOT_OPTIONS, NULL, ipmi_chassis_get_sys_boot_options);

    printf("Registering NetFn:[0x%X], Cmd:[0x%X]\n",NETFUN_CHASSIS, IPMI_CMD_CHASSIS_CONTROL);
    ipmi_register_callback(NETFUN_CHASSIS, IPMI_CMD_CHASSIS_CONTROL, NULL, ipmi_chassis_control);

    printf("Registering NetFn:[0x%X], Cmd:[0x%X]\n", NETFUN_CHASSIS, IPMI_CMD_SET_SYS_BOOT_OPTIONS);
    ipmi_register_callback(NETFUN_CHASSIS, IPMI_CMD_SET_SYS_BOOT_OPTIONS, NULL, ipmi_chassis_set_sys_boot_options);
}
