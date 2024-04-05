#ifndef _OPCUASERVER_H
#define _OPCUASERVER_H
#include <reading.h>
#include <config_category.h>
#include <logger.h>
#include <string>
#include <map>
#include <opc/ua/node.h>
#include <opc/ua/subscription.h>
#include <opc/ua/server/server.h>
#include <plugin_api.h>

class OPCUAServer;

/**
 * The subscription client used to handle the data change events
 */
class SubClient : public OpcUa::SubscriptionHandler
{
	public:
		void	registerServer(OPCUAServer *server) { m_server = server; };
		void 	DataChange(uint32_t handle, const OpcUa::Node & node, const OpcUa::Variant & val, OpcUa::AttributeId attr) override;
	private:
		OPCUAServer		*m_server;
};

/**
 * The OPCUA Server 
 */
class OPCUAServer {
	public:
		OPCUAServer();
		~OPCUAServer();
		void		configure(const ConfigCategory *conf);
		uint32_t	send(const std::vector<Reading *>& readings);
		void		stop();
		void		nodeChange(const OpcUa::Node& node, const std::string& value);
		void		registerControl(bool ( *write)(const char *name, const char *value, ControlDestination destination, ...),
                                int (* operation)(char *operation, int paramCount, char *parameters[], ControlDestination destination, ...));
	private:
		class NodeTree {
			public:
				NodeTree(const std::string& name) : m_name(name) {};
				const std::string& 	getName() const
							{
								return m_name;
							};
				void			addChild(NodeTree child)
							{
								m_children.push_back(child);
							};
				const std::vector<NodeTree>&
							getChildren() const
							{
								return m_children;
							};
			private:
				std::string		m_name;
				std::vector<NodeTree>	m_children;

		};
		class ControlNode {
			public:
				ControlNode(const std::string& name, const std::string& type)
									: m_name(name), m_type(type), m_destination(DestinationBroadcast) {};
				ControlNode(const std::string& name, const std::string& type, ControlDestination dest, const std::string& arg)
									: m_name(name), m_type(type), m_destination(dest), m_arg(arg) {};
				void			createNode(uint32_t idx, OpcUa::Node& parent);
				const std::string&	getName() const { return m_name; };
				const OpcUa::Node	getNode() const { return m_node; };
				ControlDestination	getDestination() const { return m_destination; };
				const std::string&	getArgument() const { return m_arg; };
			private:
				const std::string	m_name;
				const std::string	m_type;
				const ControlDestination
							m_destination;
				const std::string	m_arg;
				OpcUa::Node		m_node;
		};
		void		updateAsset(Reading *reading);
		void		addAsset(Reading *reading);
		void		addDatapoint(std::string& assetName, OpcUa::Node& obj,
					std::string& name, DatapointValue& value, struct timeval userTS);
		void		updateDatapoint(std::string& assetName, OpcUa::Node& obj,
					std::string& name, DatapointValue& value, struct timeval userTS);
		OpcUa::Node		findParent(const Reading *reading);
		OpcUa::Node&	findParent(const std::vector<NodeTree>& hierarchy, const Reading *reading, OpcUa::Node& root, std::string key);
		void 		parseChildren(NodeTree& parent, const rapidjson::Value& value);
		void		addControlNode(const std::string& name, const std::string& type);
		void		addControlNode(const std::string& name, const std::string& type, ControlDestination dest, const std::string& arg);
		void		createControlNodes();
		bool 					(*m_write)(const char *name, const char *value, ControlDestination destination, ...);
		OpcUa::UaServer				*m_server;
		std::map<std::string, OpcUa::Node>	m_assets;
		std::map<std::string, OpcUa::Node>	m_parents;
		std::string				m_name;
		std::string				m_url;
		std::string				m_uri;
		std::string				m_namespace;
		std::string				m_root;
		bool					m_includeAsset;
		int					m_idx;
		OpcUa::Node				m_objects;
		Logger					*m_log;
		std::vector<NodeTree>			m_hierarchy;
		OpcUa::Subscription::SharedPtr		m_subscription;
		SubClient				m_subscriptionClient;
		std::vector<ControlNode>		m_control;
		std::string				m_controlRoot;
		std::vector<DatapointValue::DatapointTag>
							m_warned;
};

#endif
