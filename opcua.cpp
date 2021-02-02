/*
 * Fledge OPC UA north plugin.
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
#include <rapidjson/document.h>

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
 * @param conf	Fledge configuration category
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
	if (conf->itemExists("root"))
		m_root = conf->getValue("root");
	else
		m_root = "";
	if (conf->itemExists("hierarchy"))
	{
		string hierarchy = conf->getValue("hierarchy");
		if (hierarchy.length() > 0)
		{
			rapidjson::Document doc;
			rapidjson::ParseResult result = doc.Parse(hierarchy.c_str());
			if (!result)
			{
				Logger::getLogger()->error("Error parsing hierarchy: %s at %u",
						doc.GetParseError(), result.Offset());
			}
			else
			{
				for (rapidjson::Value::ConstMemberIterator itr = doc.MemberBegin();
					itr != doc.MemberEnd(); ++itr)
				{
					string name = itr->name.GetString();
					m_hierarchy.push_back(NodeTree(name));
					parseChildren(m_hierarchy.back(), itr->value);
				}
			}
		}
	}
}

/**
 * Build the tree of nodes for the OPC/UA hierarchy
 *
 * @param parent	The parent node
 * @param value		The JSON children
 */
void OPCUAServer::parseChildren(NodeTree& parent, const rapidjson::Value& value)
{
	if (value.IsObject())
	{
		for (rapidjson::Value::ConstMemberIterator itr = value.MemberBegin();
			itr != value.MemberEnd(); ++itr)
		{
			string name = itr->name.GetString();
			NodeTree child = NodeTree(name);
			parseChildren(child, itr->value);
			parent.addChild(child);
		}
	}
}

/**
 * Send a block of reading to OPCUA Server
 *
 * @param readings	The readings to send
 * @return 		The number of readings sent
 */
uint32_t OPCUAServer::send(const vector<Reading *>& readings)
{
int n = 0;

	if (! m_server)
	{
		m_log->info("Starting OPC UA Server on %s", m_url.c_str());
		try {
		//	m_server = new UaServer(m_log);
			m_server = new UaServer(true);
			m_server->SetEndpoint(m_url);
			m_server->SetServerURI(m_uri);
			m_server->SetServerName(m_name);
			m_server->Start();
			m_log->info("Server started");
	
			m_idx = m_server->RegisterNamespace(m_namespace);
			m_objects = m_server->GetObjectsNode();
			if (m_root.length() > 0)
			{
				NodeId  nodeId(m_root, m_idx);
				QualifiedName   qn(m_root, m_idx);
				m_objects = m_objects.AddObject(nodeId, qn);
			}

			m_server->EnableEventNotification();
		} catch (exception& e) {
			m_log->error("Failed to start OPC UA Server: %s", e.what());
		}
	}
	for (auto reading = readings.cbegin(); reading != readings.cend(); reading++)
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

/**
 * Add a new asset to the object tree
 * Called the first time we see a particular asset
 *
 * @param reading	The reading to add
 */
void OPCUAServer::addAsset(Reading *reading)
{
	string assetName = reading->getAssetName();
	OpcUa::Node parent = findParent(reading);
	NodeId	nodeId(assetName, m_idx);
	QualifiedName	qn(assetName, m_idx);
	Node obj = parent.AddObject(nodeId, qn);
	m_log->info("Add new asset: %s", assetName.c_str());

	struct timeval userTS;
	reading->getUserTimestamp(&userTS);
	vector<Datapoint *>& dataPoints = reading->getReadingData();
	for (vector<Datapoint *>::iterator it = dataPoints.begin(); it != dataPoints.end(); ++it)
	{
		// Get the reference to a DataPointValue
		DatapointValue& value = (*it)->getData();
		string name = (*it)->getName();
		addDatapoint(assetName, obj, name, value, userTS);
	}
	m_assets.insert(pair<string, Node>(assetName, obj));
}

/**
 * Add the variable within an asset object. This may be
 * called recursively for nested objects
 *
 * @param assetName The name of the asset being added
 * @param obj	The parent object
 * @param name	The name of the variable to add
 * @param value	The value of the variable
 */
void OPCUAServer::addDatapoint(string& assetName, Node& obj, string& name, DatapointValue& value, struct timeval userTS)
{
	try {
		if (value.getType() == DatapointValue::T_INTEGER)
		{
			Node myvar = obj.AddVariable(m_idx, name, Variant((int64_t)value.toInt()));
			DataValue dv = myvar.GetDataValue();
			dv.SourceTimestamp = DateTime::FromTimeT(userTS.tv_sec, userTS.tv_usec);
			dv.Encoding |= DATA_VALUE_SOURCE_TIMESTAMP;
			myvar.SetValue(dv);
		}
		else if (value.getType() == DatapointValue::T_FLOAT)
		{
			Node myvar = obj.AddVariable(m_idx, name, Variant(value.toDouble()));
			DataValue dv = myvar.GetDataValue();
			dv.SourceTimestamp = DateTime::FromTimeT(userTS.tv_sec, userTS.tv_usec);
			dv.Encoding |= DATA_VALUE_SOURCE_TIMESTAMP;
			myvar.SetValue(dv);
		}
		else if (value.getType() == DatapointValue::T_STRING)
		{
			Node myvar = obj.AddVariable(m_idx, name, value.toString());
			DataValue dv = myvar.GetDataValue();
			dv.SourceTimestamp = DateTime::FromTimeT(userTS.tv_sec, userTS.tv_usec);
			dv.Encoding |= DATA_VALUE_SOURCE_TIMESTAMP;
			myvar.SetValue(dv);
		}
		else if (value.getType() == DatapointValue::T_DP_DICT)
		{
			string fullname = assetName + "_" + name;
			NodeId	nodeId(fullname, m_idx);
			QualifiedName	qn(name, m_idx);
			Node child = obj.AddObject(nodeId, qn);
			vector<Datapoint*> *children = value.getDpVec();
			for (auto dpit = children->begin(); dpit != children->end(); dpit++)
			{
				name = (*dpit)->getName();
				DatapointValue& val = (*dpit)->getData();
				addDatapoint(assetName, child, name, val, userTS);
			}
		} // TODO add support for arrays (T_DP_LIST)
		else
		{
			m_log->warn("Asset %s, datapoint %s is unknown type %d", assetName.c_str(), name.c_str(), value.getType());
		}
	} catch (runtime_error& r) {
		m_log->error("Failed to add asset %s datapoint %s, %s", assetName.c_str(), name.c_str(), r.what());
	} catch (exception& e) {
		m_log->error("Failed to add asset %s datapoint %s, %s", assetName.c_str(), name.c_str(), e.what());
	}
}

/**
 * Update the value of an asset we have seen previously
 *
 * @param reading	The reading to update
 */
void OPCUAServer::updateAsset(Reading *reading)
{
string assetName = reading->getAssetName();

	m_log->info("Update asset: %s", assetName.c_str());
	auto it = m_assets.find(assetName);
	if (it != m_assets.end())
	{
		Node obj = it->second;

		vector<Datapoint *>& dataPoints = reading->getReadingData();
		struct timeval userTS;
		reading->getUserTimestamp(&userTS);
		for (vector<Datapoint *>::iterator dpit = dataPoints.begin(); dpit != dataPoints.end(); ++dpit)
		{
			// Get the reference to a DataPointValue
			DatapointValue& value = (*dpit)->getData();
			string name = (*dpit)->getName();
			updateDatapoint(assetName, obj, name, value, userTS);
		}
	}
}

/**
 * Update the datapoint for a given asset
 *
 * @param assetName The name of the asset being updates
 * @param obj	The parent object
 * @param name	The name of the variable to update
 * @param value	The value of the variable
 */
void OPCUAServer::updateDatapoint(string& assetName, Node& obj, string& name, DatapointValue& value, struct timeval userTS)
{
	bool found = false;
	vector<OpcUa::Node> variables = obj.GetVariables();
	for (auto var : variables)
	{
		OpcUa::QualifiedName qName = var.GetBrowseName();
		if (qName.Name.compare(name) == 0)
		{
			if (value.getType() == DatapointValue::T_INTEGER)
			{
				DataValue dv = var.GetDataValue();
				dv.Value = Variant((uint64_t)value.toInt());
				dv.SourceTimestamp = DateTime::FromTimeT(userTS.tv_sec, userTS.tv_usec);
				dv.Encoding |= DATA_VALUE_SOURCE_TIMESTAMP;
				var.SetValue(dv);
			}
			else if (value.getType() == DatapointValue::T_FLOAT)
			{
				DataValue dv = var.GetDataValue();
				dv.Value = Variant(value.toDouble());
				dv.SourceTimestamp = DateTime::FromTimeT(userTS.tv_sec, userTS.tv_usec);
				dv.Encoding |= DATA_VALUE_SOURCE_TIMESTAMP;
				var.SetValue(dv);
			}
			else if (value.getType() == DatapointValue::T_STRING)
			{
				DataValue dv = var.GetDataValue();
				dv.Value = Variant(value.toString());
				dv.SourceTimestamp = DateTime::FromTimeT(userTS.tv_sec, userTS.tv_usec);
				dv.Encoding |= DATA_VALUE_SOURCE_TIMESTAMP;
				var.SetValue(dv);
			}
			else if (value.getType() == DatapointValue::T_DP_DICT)
			{
				vector<OpcUa::Node> childNodes = obj.GetChildren();
				vector<Datapoint*> *children = value.getDpVec();
				for (auto dpit = children->begin(); dpit != children->end(); dpit++)
				{
					name = (*dpit)->getName();
					DatapointValue& val = (*dpit)->getData();
					for (auto child : childNodes)
					{
						QualifiedName qName = child.GetBrowseName();
						if (qName.Name.compare(name) == 0)
						{
							updateDatapoint(assetName, child, name, val, userTS);
						}
					}
				}
			} // TODO add support for arrays (T_DP_LIST)
			found = true;
		}
	}
	if (!found)
	{
		addDatapoint(assetName, obj, name, value, userTS);
	}
}

/**
 * Stop the OPCUA server
 */
void OPCUAServer::stop()
{
	if (m_server)
	{
		m_server->Stop();
	}
}

/**
 * Find the parent OPCUA node for this asset
 *
 * @param reading	The reading we are sending
 * @return 		The OPCUA parent node
 */
OpcUa::Node& OPCUAServer::findParent(const Reading *reading)
{
	vector<Datapoint *> datapoints = reading->getReadingData();
	for (int i = 0; i < datapoints.size(); i++)
	{
		string name = datapoints[i]->getName();
		for (int j = 0; j < m_hierarchy.size(); j++)
		{
			if (m_hierarchy[j].getName().compare(name) == 0)
			{
				DatapointValue value = datapoints[i]->getData();
				string svalue = value.toString();
				svalue = svalue.substr(1, svalue.length() - 2);
				OpcUa::Node opcNode;
				auto it = m_parents.find(svalue);
				if (it != m_parents.end())
				{
					opcNode = it->second;
				}
				else
				{
					NodeId  nodeId(svalue, m_idx);
					QualifiedName   qn(svalue, m_idx);
					opcNode = m_objects.AddObject(nodeId, qn);
					m_parents.insert(pair<string, Node>(svalue, opcNode));
				}
				return findParent(m_hierarchy[j].getChildren(), reading, opcNode, svalue);
			}
		}
	}
	return m_objects;
}

/**
 * Find the parent OPCUA node for this asset
 *
 * @param hierarchy	The defintion for the current level of the hierarchy
 * @param reading	The reading we are sending
 * @param root		The current root for the point in the hierarchy we are at
 * @param key		The key to use to lookup the cache of OPCUA nodes
 * @return 		The OPCUA parent node
 */
OpcUa::Node& OPCUAServer::findParent(const vector<NodeTree>& hierarchy, const Reading *reading, OpcUa::Node& root, string key)
{
	vector<Datapoint *> datapoints = reading->getReadingData();
	for (int i = 0; i < datapoints.size(); i++)
	{
		string name = datapoints[i]->getName();
		for (int j = 0; j < hierarchy.size(); j++)
		{
			if (hierarchy[j].getName().compare(name) == 0)
			{
				DatapointValue value = datapoints[i]->getData();
				string svalue = value.toString();
				svalue = svalue.substr(1, svalue.length() - 2);
				string newkey = key.append("/");
				newkey = newkey.append(svalue);
				OpcUa::Node opcNode;
				auto it = m_parents.find(newkey);
				if (it != m_parents.end())
				{
					opcNode = it->second;
				}
				else
				{
					NodeId  nodeId(svalue, m_idx);
					QualifiedName   qn(svalue, m_idx);
					opcNode = root.AddObject(nodeId, qn);
					m_parents.insert(pair<string, Node>(newkey, opcNode));
				}
				return findParent(hierarchy[j].getChildren(), reading, opcNode, svalue);
			}
		}
	}
	return root;
}
