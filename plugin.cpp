/*
 * Fledge Google Cloud Platform IoT-Core north plugin.
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


using namespace std;
using namespace rapidjson;

extern "C" {

#define PLUGIN_NAME "opcua"

#define CONTROL_MAP QUOTE({						\
				"nodes" : [				\
					{				\
						"name" : "test",	\
						"type" : "integer"	\
					}				\
				]					\
		})

/**
 * Plugin specific default configuration
 */
static const char *default_config = QUOTE({
			"plugin" : {
				"description" : "OPCUA Server",
				"type" : "string",
				"default" : PLUGIN_NAME,
				"readonly" : "true"
			},
			"name" : {
				"description" : "The OPC UA Server name to advertise",
				"type" : "string",
				"default" : "Fledge OPCUA",
				"order" : "1",
				"displayName" : "Server Name"
			},
			"url" : {
				"description" : "The OPC UA Server URL",
				"type" : "string",
				"default" : "opc.tcp://localhost:4840/fledge/server",
				"order" : "2",
				"displayName" : "URL"
			},
			"uri" : {
				"description" : "The OPC UA Service URI",
				"type" : "string",
				"default" : "urn://fledge.dianomic.com",
				"order" : "3",
				"displayName" : "URI"
			},
			"namespace" : {
				"description" : "The OPC UA Namespace",
				"type" : "string",
				"default" : "http://fledge.dianomic.com",
				"order" : "4",
				"displayName" : "Namespace"
			},
			"source" : {
				"description" : "The Source of the data to send",
				"type" : "enumeration",
				"options" : ["readings", "statistics", "audit"],
				"default" : "readings",
				"order" : "5",
				"displayName" : "Source"
			},
			"root" : {
				"description" : "The OPC UA Root node for this service",
				"type" : "string",
				"default" : "",
				"order" : "6",
				"displayName" : "Object Root"
			},
			"IncludeAssetName" : {
				"description" : "If true, create an OPC UA Object named after the Asset",
				"type" : "boolean",
				"default" : "true",
				"displayName" : "Include Asset as Object",
				"order" : "7"
			},
			"ParseAssetName" : {
				"description" : "If true, parse a hierarchy from the Asset Name and use it as the beginning of the path",
				"type" : "boolean",
				"default" : "false",
				"displayName" : "Parse Hierarchy from Asset Name",
				"order" : "8"
			},
			"hierarchy" : {
				"description" : "The object hierarchy to use",
				"type" : "JSON",
				"default" : "{}",
				"order" : "9",
				"displayName" : "Hierarchy"
			},
			"controlRoot" : {
				"description" : "The OPC UA Root node to use for control items for this service",
				"type" : "string",
				"default" : "Control",
				"order" : "10",
				"displayName" : "Control Root"
			},
			"controlMap" : {
				"description" : "The control map to use",
				"type" : "JSON",
				"default" : CONTROL_MAP,
				"order" : "11",
				"displayName" : "Control Map"
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
	SP_CONTROL,			// Flags
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

void plugin_register(PLUGIN_HANDLE handle,
		bool ( *write)(const char *name, const char *value, ControlDestination destination, ...),
		int (* operation)(char *operation, int paramCount, char *parameters[], ControlDestination destination, ...))
{
OPCUAServer	*opcua = (OPCUAServer *)handle;

	opcua->registerControl(write, operation);
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
