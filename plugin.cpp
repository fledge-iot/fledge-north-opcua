/*
 * FogLAMP Google Cloud Platform IoT-Core north plugin.
 *
 * Copyright (c) 2019 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <plugin_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string>
#include <logger.h>
#include <plugin_exception.h>
#include <iostream>
#include <config_category.h>
#include <version.h>
#include <opcua.h>

#define TO_STRING(...) DEFER(TO_STRING_)(__VA_ARGS__)
#define DEFER(x) x
#define TO_STRING_(...) #__VA_ARGS__
#define QUOTE(...) TO_STRING(__VA_ARGS__)

using namespace std;
using namespace rapidjson;

extern "C" {

#define PLUGIN_NAME "OPCUA"

/**
 * Plugin specific default configuration
 */
const char *default_config = QUOTE({
			"plugin" : {
				"description" : "OPCUA Server",
				"type" : "string",
				"default" : PLUGIN_NAME,
				"readonly" : "true"
			},
			"url" : {
				"description" : "The OPC UA Server URL",
				"type" : "string",
				"default" : "opc.tcp://localhost:4840/foglamp/server",
				"order" : "1",
				"displayName" : "URL"
			},
			"uri" : {
				"description" : "The OPC UA Server URI",
				"type" : "string",
				"default" : "urn://foglamp.dianomic.com",
				"order" : "2",
				"displayName" : "URL"
			},
			"namespace" : {
				"description" : "The OPC UA Namespace",
				"type" : "string",
				"default" : "http://foglamp.dianomic.com",
				"order" : "3",
				"displayName" : "Namespace"
			}
		});

/**
 * The OPCUA plugin interface
 */

/**
 * The C API plugin information structure
 */
static PLUGIN_INFORMATION info = {
	PLUGIN_NAME,			// Name
	VERSION,			// Version
	0,				// Flags
	PLUGIN_TYPE_NORTH,		// Type
	"1.0.0",			// Interface version
	default_config			// Configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	return &info;
}

/**
 * Initialise the plugin with configuration.
 *
 * This function is called to get the plugin handle.
 */
PLUGIN_HANDLE plugin_init(ConfigCategory* configData)
{

	OPCUAServer *opcua = new OPCUAServer();
	opcua->configure(configData);

	return (PLUGIN_HANDLE)opcua;
}

/**
 * Send Readings data to historian server
 */
uint32_t plugin_send(const PLUGIN_HANDLE handle,
		     const vector<Reading *>& readings)
{
OPCUAServer	*opcua = (OPCUAServer *)handle;

	return opcua->send(readings);
}

/**
 * Shutdown the plugin
 *
 * Delete allocated data
 *
 * @param handle    The plugin handle
 */
void plugin_shutdown(PLUGIN_HANDLE handle)
{
OPCUAServer	*opcua = (OPCUAServer *)handle;

	opcua->stop();
        delete opcua;
}

// End of extern "C"
};
