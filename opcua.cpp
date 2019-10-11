/*
 * FogLAMP OPC UA north plugin.
 *
 * Copyright (c) 2019 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <opcua.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

using namespace std;
using namespace OpcUa;


/**
 * Constructor for the OPCUAServer object
 */
OPCUAServer::OPCUAServer() : m_server(NULL)
{
	m_log = Logger::getLogger();
}

/**
 * Destructor for the OPCUA Server object
 */
OPCUAServer::~OPCUAServer()
{
}

/**
 *
 * @param conf	FogLAMP configuration category
 */
void OPCUAServer::configure(const ConfigCategory *conf)
{
	if (conf->itemExists("url"))
		m_url = conf->getValue("url");
	else
		m_log->error("Missing URL in configuration");
	if (conf->itemExists("uri"))
		m_uri = conf->getValue("uri");
	else
		m_log->error("Missing URI in configuration");
	if (conf->itemExists("namespace"))
		m_namespace = conf->getValue("namespace");
	else
		m_log->error("Missing namespace in configuration");
	if (conf->itemExists("name"))
		m_name = conf->getValue("name");
	else
		m_log->error("Missing name in configuration");
}

/**
 * Send a block of reading to OPCUA Server
 *
 * @param readings	The readings to send
 * @return 		The number of readings sent
 */
uint32_t OPCUAServer::send(const vector<Reading *>& readings)
{
int n;

	if (! m_server)
	{
		m_log->info("Starting OPC UA Server on %s", m_url.c_str());
		try {
			m_server = new UaServer(m_log);
			m_server->SetEndpoint(m_url);
			m_server->SetServerURI(m_uri);
			m_server->SetServerName(m_name);
			m_server->Start();
			m_log->info("Server started");
	
			m_idx = m_server->RegisterNamespace(m_namespace);
			m_objects = m_server->GetObjectsNode();
		} catch (exception& e) {
			m_log->error("Failed to start OPC UA Server: %s", e.what());
		}
	}

	for (auto reading = readings.cbegin(); reading != readings.cend(); reading ++)
	{
		string assetName = (*reading)->getAssetName();
		if (m_assets.find(assetName) == m_assets.end())
		{
			addAsset(*reading);
		}
		else
		{
			updateAsset(*reading);
		}
		n++;
	}
	return n;
}

void OPCUAServer::addAsset(Reading *reading)
{
	string assetName = reading->getAssetName();
	m_log->info("Add new asset: %s", assetName.c_str());
	NodeId	nodeId(assetName.c_str(), m_idx);
	QualifiedName	qn(assetName, m_idx);
	Node obj = m_objects.AddObject(nodeId, qn);

	vector<Datapoint *>& dataPoints = reading->getReadingData();
	for (vector<Datapoint *>::iterator it = dataPoints.begin(); it != dataPoints.end(); ++it)
	{
		// Get the reference to a DataPointValue
		DatapointValue& value = (*it)->getData();
		string name = (*it)->getName();
		addDatapoint(assetName, obj, name, value);
	}
	m_assets.insert(pair<string, Node>(assetName, obj));
}

void OPCUAServer::addDatapoint(string& assetName, Node& obj, string& name, DatapointValue& value)
{
	if (value.getType() == DatapointValue::T_INTEGER)
		Node myvar = obj.AddVariable(m_idx, name, Variant(value.toInt()));
	else if (value.getType() == DatapointValue::T_FLOAT)
		Node myvar = obj.AddVariable(m_idx, name, Variant(value.toDouble()));
	else if (value.getType() == DatapointValue::T_STRING)
		Node myvar = obj.AddVariable(m_idx, name, value.toString());
	else if (value.getType() == DatapointValue::T_DP_DICT)
	{
		NodeId	nodeId(name.c_str(), m_idx);
		QualifiedName	qn(name, m_idx);
		Node child = obj.AddObject(nodeId, qn);
		vector<Datapoint*> *children = value.getDpVec();
		for (auto dpit = children->begin(); dpit != children->end(); dpit++)
		{
			name = (*dpit)->getName();
			DatapointValue& val = (*dpit)->getData();
			addDatapoint(assetName, child, name, val);
		}
	} // TODO add support for arrays (T_DP_LIST)
	else
	{
		m_log->warn("Asset %s, datapoint %s is unknown type %d", assetName.c_str(), name.c_str(), value.getType());
	}
}

void OPCUAServer::updateAsset(Reading *reading)
{
	m_log->info("Update asset: %s", reading->getAssetName().c_str());
	auto it = m_assets.find(reading->getAssetName());
	if (it != m_assets.end())
	{
		vector<OpcUa::Node> variables = it->second.GetVariables();

		vector<Datapoint *>& dataPoints = reading->getReadingData();
		for (vector<Datapoint *>::iterator dpit = dataPoints.begin(); dpit != dataPoints.end(); ++dpit)
		{
			// Get the reference to a DataPointValue
			DatapointValue& value = (*dpit)->getData();
			string name = (*dpit)->getName();
			bool found = false;
			for (auto var : variables)
			{
				OpcUa::QualifiedName qName = var.GetBrowseName();
				if (qName.Name.compare(name) == 0)
				{
					if (value.getType() == DatapointValue::T_INTEGER)
						var.SetValue(Variant(value.toInt()));
					else if (value.getType() == DatapointValue::T_FLOAT)
						var.SetValue(Variant(value.toDouble()));
					else if (value.getType() == DatapointValue::T_STRING)
						var.SetValue(value.toString());
					found = true;
				}
			}
			if (!found)
			{
				if (value.getType() == DatapointValue::T_INTEGER)
					Node myvar = it->second.AddVariable(m_idx, name, Variant(value.toInt()));
				else if (value.getType() == DatapointValue::T_FLOAT)
					Node myvar = it->second.AddVariable(m_idx, name, Variant(value.toDouble()));
				else if (value.getType() == DatapointValue::T_STRING)
					Node myvar = it->second.AddVariable(m_idx, name, value.toString());
			}
		}
	}
}

void OPCUAServer::stop()
{
	if (m_server)
	{
		m_server->Stop();
	}
}
