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
		void		updateAsset(Reading *reading);
		void		addAsset(Reading *reading);
		void		addDatapoint(std::string& assetName, OpcUa::Node& obj,
					std::string& name, DatapointValue& value);
		void		updateDatapoint(std::string& assetName, OpcUa::Node& obj,
					std::string& name, DatapointValue& value);
		OpcUa::UaServer				*m_server;
		std::map<std::string, OpcUa::Node>	m_assets;
		std::string				m_name;
		std::string				m_url;
		std::string				m_uri;
		std::string				m_namespace;
		int					m_idx;
		OpcUa::Node				m_objects;
		Logger					*m_log;
};

#endif
