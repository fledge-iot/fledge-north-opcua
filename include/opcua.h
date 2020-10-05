#ifndef _OPCUASERVER_H
#define _OPCUASERVER_H
#include <reading.h>
#include <config_category.h>
#include <logger.h>
#include <string>
#include <map>
#include <opc/ua/node.h>
#include <opc/ua/server/server.h>

class OPCUAServer {
	public:
		OPCUAServer();
		~OPCUAServer();
		void		configure(const ConfigCategory *conf);
		uint32_t	send(const std::vector<Reading *>& readings);
		void		stop();
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
		void		updateAsset(Reading *reading);
		void		addAsset(Reading *reading);
		void		addDatapoint(std::string& assetName, OpcUa::Node& obj,
					std::string& name, DatapointValue& value);
		void		updateDatapoint(std::string& assetName, OpcUa::Node& obj,
					std::string& name, DatapointValue& value);
		OpcUa::Node&	findParent(const Reading *reading);
		OpcUa::Node&	findParent(const std::vector<NodeTree>& hierarchy, const Reading *reading, OpcUa::Node& root, std::string key);
		void 		parseChildren(NodeTree& parent, const rapidjson::Value& value);
		OpcUa::UaServer				*m_server;
		std::map<std::string, OpcUa::Node>	m_assets;
		std::map<std::string, OpcUa::Node>	m_parents;
		std::string				m_name;
		std::string				m_url;
		std::string				m_uri;
		std::string				m_namespace;
		std::string				m_root;
		int					m_idx;
		OpcUa::Node				m_objects;
		Logger					*m_log;
		std::vector<NodeTree>			m_hierarchy;
};

#endif
