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
OPCUAServer::OPCUAServer() : m_server(NULL), m_write(NULL)
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
	if (conf->itemExists("controlRoot"))
		m_controlRoot = conf->getValue("controlRoot");
	else
		m_log->error("Missing URL in configuration");
	if (conf->itemExists("controlMap"))
	{
		string controlMap = conf->getValue("controlMap");
		rapidjson::Document doc;
		rapidjson::ParseResult result = doc.Parse(controlMap.c_str());
		if (! result)
		{
			Logger::getLogger()->error("Error parsing control map: %s at %u",
						doc.GetParseError(), result.Offset());
		}
		else
		{
			if (doc.HasMember("nodes") && doc["nodes"].IsArray())
			{
				rapidjson::Value& nodes = doc["nodes"];
				for (auto& node : nodes.GetArray())
				{
					string name, type, service, asset, script;
					if (node.HasMember("name"))
					{
						rapidjson::Value& v = node["name"];
						if (v.IsString())
							name = v.GetString();
					}
					if (node.HasMember("type"))
					{
						rapidjson::Value& v = node["type"];
						if (v.IsString())
							type = v.GetString();
					}
					if (node.HasMember("service"))
					{
						rapidjson::Value& v = node["service"];
						if (v.IsString())
							service = v.GetString();
					}
					if (node.HasMember("asset"))
					{
						rapidjson::Value& v = node["asset"];
						if (v.IsString())
							asset = v.GetString();
					}
					if (node.HasMember("script"))
					{
						rapidjson::Value& v = node["script"];
						if (v.IsString())
							script = v.GetString();
					}
					if (name.empty() || type.empty())
					{
						Logger::getLogger()->error("Badly formed control map, both node name and type must be provided");
					}
					else if (!script.empty())
					{
						addControlNode(name, type, DestinationScript, script);
					}
					else if (!asset.empty())
					{
						addControlNode(name, type, DestinationAsset, asset);
					}
					else if (!service.empty())
					{
						addControlNode(name, type, DestinationService, service);
					}
					else
					{
						addControlNode(name, type);
					}
				}
			}
			else
			{
				m_log->error("Missing the nodes element in the control map");
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

			createControlNodes();

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
			Node myvar = obj.AddVariable(m_idx, name, value.toStringValue());
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
		}
		else if (value.getType() == DatapointValue::T_FLOAT_ARRAY)
		{
			vector<double> array = *value.getDpArr();
			Node myvar = obj.AddVariable(m_idx, name, Variant(array));
			DataValue dv = myvar.GetDataValue();
			dv.SourceTimestamp = DateTime::FromTimeT(userTS.tv_sec, userTS.tv_usec);
			dv.Encoding |= DATA_VALUE_SOURCE_TIMESTAMP;
			myvar.SetValue(dv);
		} // TODO add support for arrays (T_DP_LIST)
		else 
		{
			// Log unsupported types just once per run of the plugin
			bool found = false;
			DatapointValue::DatapointTag type = value.getType();
			for (auto& w : m_warned)
				if (w == type)
					found = true;
			if (!found)
			{
				m_log->warn("Asset %s, datapoint %s is unknown type %d", assetName.c_str(), name.c_str(), value.getType());
				m_warned.push_back(value.getType());
			}
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
				dv.Value = Variant(value.toStringValue());
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

void OPCUAServer::registerControl(bool ( *write)(const char *name, const char *value, ControlDestination destination, ...),
                                int (* operation)(char *operation, int paramCount, char *parameters[], ControlDestination destination, ...))
{
	m_write = write;
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
				string svalue = value.toStringValue();
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
				string svalue = value.toStringValue();
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

/**
 * Add a new control node. Since we have two parameters
 * this is a broadcast control
 *
 * @param name	The name of the node
 * @param type	The type of the node
 */
void OPCUAServer::addControlNode(const string& name, const string& type)
{
	m_control.push_back(ControlNode(name, type));
}

/**
 * Add a new control node.
 *
 * @param name	The name of the node
 * @param type	The type of the node
 * @param destination	The control destiantion
 * @param arg	Argument to the control destination
 */
void OPCUAServer::addControlNode(const string& name, const string& type, ControlDestination destination, const string& arg)
{
	m_control.push_back(ControlNode(name, type, destination, arg));
}

/**
 * Create the actual nodes inthe OPCUA server for the control
 */
void OPCUAServer::createControlNodes()
{
	m_subscriptionClient.registerServer(this);
	m_subscription =  m_server->CreateSubscription(100, m_subscriptionClient);
	Node	objects = m_server->GetObjectsNode();
	NodeId nid(99, m_idx);
	QualifiedName qn(m_controlRoot, m_idx);
	Node	parent = objects.AddObject(nid, qn);
	for (auto &n : m_control)
	{
		n.createNode(m_idx, parent);
		m_subscription->SubscribeDataChange(n.getNode());
	}
}

/**
 * One of our nodes has changed value. Find the corresponding
 * ControlNode entry and set the set poitn operation.
 *
 * @param node	The node that has changed
 * @param value	THe new value of the node
 */
void OPCUAServer::nodeChange(const Node& node, const string& value)
{
	if (!m_write)
	{
		m_log->error("Node change has occured but we have no callback registered for the service");
		return;
	}
	for (auto &n : m_control)
	{
		if (n.getNode() == node)
		{
			ControlDestination dest = n.getDestination();
			if (dest != DestinationBroadcast)
			{
				const string arg = n.getArgument();
				(*m_write)(n.getName().c_str(), value.c_str(), dest, arg.c_str());
			}
			else
			{
				(*m_write)(n.getName().c_str(), value.c_str(), DestinationBroadcast, NULL);
			}
			return;
		}
	}
	m_log->warn("Failed to find control node");
}

/**
 * Create the control node
 *
 * @param idx		The namespace index
 * @param parent	The parent node
 */
void OPCUAServer::ControlNode::createNode(uint32_t idx, Node& parent)
{
	if (m_type.compare("integer") == 0)
		m_node = parent.AddVariable(idx, m_name, Variant(32));
	if (m_type.compare("float") == 0)
		m_node = parent.AddVariable(idx, m_name, Variant(32.8));
}

/**
 * Subscription Client handler for data change events
 *
 * @param handle	
 * @param node		The node that has changed
 * @param val		The value the node is beign assigned
 * @param attr		The Attribute ID
 */
void SubClient::DataChange(uint32_t handle,
				const OpcUa::Node & node,
				const OpcUa::Variant & val,
				OpcUa::AttributeId attr)
{
string value;

	if (val.IsNul())
		return;
	if (val.IsScalar())
	{
		switch (val.Type())
		{
			case OpcUa::VariantType::BYTE:
			{
				long lval = static_cast<uint8_t>(val);
				value = to_string(lval);
				break;
			}
			case OpcUa::VariantType::SBYTE:
			{
				long lval = static_cast<int8_t>(val);
				value = to_string(lval);
				break;
			}
			case OpcUa::VariantType::DATE_TIME:
			{
				OpcUa::DateTime timestamp = static_cast<OpcUa::DateTime>(val);
				int64_t raw = static_cast<int64_t>(timestamp);
				struct timeval tm;
				uint64_t micro = raw % 10000000;
				raw -= micro;
				raw = raw / 10000000LL;
				const int64_t daysBetween1601And1970 = 134774;
				const int64_t secsFrom1601To1970 = daysBetween1601And1970 * 24 * 3600LL;
				tm.tv_sec = raw - secsFrom1601To1970;
				tm.tv_usec = micro / 10;

				char date_time[80], usec[10];

				// Populate tm structure with UTC time
				struct tm timeinfo;
				gmtime_r(&tm.tv_sec, &timeinfo);

				// Build date_time with format YYYY-MM-DD HH24:MM:SS.MS+00:00
				// Create datetime with seconds
				std::strftime(date_time, sizeof(date_time),
						"%Y-%m-%d %H:%M:%S", &timeinfo);
				// Add microseconds
				snprintf(usec, sizeof(usec), ".%06lu", tm.tv_usec);
				strcat(date_time, usec);
				strcat(date_time, "+00:00");
				value = date_time;
				break;
			}
			case OpcUa::VariantType::INT16:
			{
				long lval = static_cast<int16_t>(val);
				value = to_string(lval);
				break;
			}
			case OpcUa::VariantType::UINT16:
			{
				long lval = static_cast<uint16_t>(val);
				value = to_string(lval);
				break;
			}
			case OpcUa::VariantType::INT32:
			{
				long lval = static_cast<int32_t>(val);
				value = to_string(lval);
				break;
			}
			case OpcUa::VariantType::UINT32:
			{
				long lval = static_cast<uint32_t>(val);
				value = to_string(lval);
				break;
			}
			case OpcUa::VariantType::INT64:
			{
				long lval = static_cast<int64_t>(val);
				value = to_string(lval);
				break;
			}
			case OpcUa::VariantType::UINT64:
			{
				long lval = static_cast<uint64_t>(val);
				value = to_string(lval);
				break;
			}
			case OpcUa::VariantType::FLOAT:
			{
				double fval = static_cast<float>(val);
				value = to_string(fval);
				break;
			}
			case OpcUa::VariantType::DOUBLE:
			{
				double fval = static_cast<double>(val);
				value = to_string(fval);
				break;
			}
			default:
			{
				value = val.ToString();
				break;
			}
		}
	}
	m_server->nodeChange(node, value);
}
