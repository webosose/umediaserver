#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <pbnjson.hpp>
#include <sstream>
#include <ResourceManagerClient.h>
#include <memory>
#include <vector>
#include <list>
#include <mutex>
#include <stdio.h>
#include <exception>

using namespace pbnjson;
using namespace uMediaServer;

struct resource_struct
{
	std::string type;
	int index, qty;
};

typedef std::list<resource_struct> resources_list;

std::string make_request(const resources_list & resources)
{
	JValue root = Array();
	for (const auto & unit : resources)
	{
		JValue junit = Object();
		junit.put("resource", unit.type);
		junit.put("qty", unit.qty);
		if (unit.index != -1)
			junit.put("index", unit.index);
		root.append(junit);
	}
	JGenerator serializer;
	std::string payload_serialized;
	if (!serializer.toString(root,  pbnjson::JSchema::AllSchema(), payload_serialized))
		std::cerr << "Can't serialize to string" << std::endl;
	return payload_serialized;
}

resources_list parse_request (const std::string & res)
{
	JDomParser parser;
	resources_list res_list;

	if (!parser.parse(res, pbnjson::JSchema::AllSchema()))
	{
		std::cerr << "Can't parse response!" << std::endl;
		return res_list;
	}

	JValue resp = parser.getDom();
	JValue resources = resp.isArray() ? resp : resp["resources"];

	if (!resources.isArray())
		return res_list;

	for (int i  = 0; i < resources.arraySize(); ++i)
	{
		const JValue & _resource = resources[i];
		if (_resource.isObject())
		{
			resource_struct resource;
			_resource["resource"].asString(resource.type);
			_resource["index"].asNumber(resource.index);
			if (!_resource.hasKey("qty"))
				resource.qty = 1;
			else
				_resource["qty"].asNumber(resource.qty);
			res_list.push_back(resource);
		}
	}
}

class RMC : public ResourceManagerClient
{
public:
	RMC(const std::string &, const std::string &);
	virtual ~RMC(){unregisterPipeline();}
	bool resource_acquire(const resources_list &);
	bool resource_release(resources_list &);
	void assign_policy_action();
	bool policyActionHandler(const char * action, const char * resources, const char * requestor_type,
							 const char * requestor_name, const char * connection_id);
	std::recursive_mutex lock_map;

private:
	std::string client_name;
	bool accept_policy_action;
	std::multimap<std::string, int> alloc_resources;
};

std::map<std::string, std::unique_ptr<RMC>> ClientMap;

RMC::RMC(const std:: string & type, const std::string & name)
	: accept_policy_action(true), client_name(name)
{
	registerPipeline(type);
	registerPolicyActionHandler(std::bind(&RMC::policyActionHandler, this,
										  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
										  std::placeholders::_4, std::placeholders::_5));
}

bool RMC::policyActionHandler(const char *action, const char *resources,
							  const char *requestor_type, const char *requestor_name,
							  const char *connection_id)
{
	bool correctRequest = false;
	std::lock_guard<std::recursive_mutex> lock(lock_map);
	std::cout << client_name << ": PA: ";

	if (!accept_policy_action)
	{
		accept_policy_action = true;
		std::cout << "Deny" << std::endl;
		return false;
	}

	std::cout << action << ": " << resources << std::endl;
	std::multimap<std::string, int> temp_map = alloc_resources;
	resources_list ret_list;

	for (auto & res : parse_request(resources))
	{
		for (int i = 0; i < res.qty; ++i)
		{
			auto pair = temp_map.equal_range(res.type);
			auto it = pair.first;
			while (it != pair.second)
			{
				if (res.index == -1 || res.index == it->second)
				{
					ret_list.push_back(resource_struct({ res.type, it->second, 1 }));
					it = temp_map.erase(it);
					correctRequest = true;
				}
				else
					++it;
			}
			if (!correctRequest)
				break;
		}
	}

	return correctRequest && resource_release(ret_list);
}

bool RMC::resource_acquire(const resources_list & res_list)
{
	std::string response;
	std::string res = make_request(res_list);
	std::cout << client_name << ": acquire: " << res << std::endl;

	notifyActivity();
	if (!acquire(res, response))
	{
		std::cout << client_name << ": acquire: failed. Can't allocate resource!" << std::endl;
		std::cout << response << std::endl;
		return false;
	}

	resources_list responce_list = parse_request(response);
	std::cout << client_name << ": acquired: ";

	for (const auto & resp : responce_list)
	{
		alloc_resources.insert(std::pair<std::string, int>(resp.type, resp.index));
		std::cout << resp.type << "[" << resp.index << "] ";
	}
	std::cout << std::endl;

	return true;
}

bool RMC::resource_release(resources_list & resources)
{
	bool correctRequest;
	std::string res = make_request(resources);
	std::lock_guard<std::recursive_mutex> lock(lock_map);
	std::multimap<std::string, int> temp_map = alloc_resources;

	std::cout << client_name << ": release: " << res << std::endl;

	for (auto & resource : resources)
	{
		correctRequest = false;

		for (int i = 0; i < resource.qty; ++i)
		{
			auto pair = temp_map.equal_range(resource.type);
			auto it = pair.first;
			while (it != pair.second)
			{
				if(resource.index < 0 || it->second == resource.index)
				{
					resource.index = it->second;
					it = temp_map.erase(it);
					correctRequest = true;
				}
				else
					++it;
			}
			if (!correctRequest)
				break;
		}
	}

	res = make_request(resources);

	if (correctRequest)
	{
		alloc_resources = temp_map ;
		release(res);
		std::cout << client_name << ": released: " << res << std::endl;
		return true;
	}
	else
	{
		std::cout << client_name << ": release: failed. Wrong release request!" << std::endl;
		return false;
	}
}

void RMC::assign_policy_action()
{
	accept_policy_action = false;
}

class Cmd
{
public:
	RMC & client;

	Cmd(RMC & c)
		: client(c)
	{ }

	virtual void apply() = 0;
};

class ListCmd : public Cmd
{
public:
	resources_list resource;

	ListCmd(RMC & c, const resources_list& rl)
		: resource(rl), Cmd(c)
	{ }
};

class Release : public ListCmd
{
public:
	Release(RMC& c, const resources_list& r) : ListCmd(c, r) { }

	virtual void apply() {
		client.resource_release(resource);
	}
};

class Acquire : public ListCmd
{
public:
	Acquire(RMC& c, const resources_list& r) : ListCmd(c, r) { }

	virtual void apply() {
		client.resource_acquire(resource);
	}
};

class PolicyAction : public Cmd
{
public:
	PolicyAction(RMC & name) : Cmd(name){}

	virtual void apply() {
		client.assign_policy_action();
	}
};

std::vector<std::unique_ptr<Cmd>> command_list;

bool parse_client(const JValue & clients)
{
	if (!clients.isArray())
	{
		return false;
	}

	for (int i  = 0; i < clients.arraySize(); ++i)
	{
		const JValue & client = clients[i];
		std::string name, type;
		if (client.isObject() && client.hasKey("name") && client.hasKey("type"))
		{
			client["name"].asString(name);
			client["type"].asString(type);
			ClientMap.insert(std::make_pair(name, std::unique_ptr<RMC>(new RMC(type, name))));
		}
		else
			return false;
	}

	return true;
}

void parse_commands(const JValue & commands)
{
	if (!commands.isArray())
	{
		std::cerr << "Wrong comands format" << std::endl;
	}

	for (int i  = 0; i < commands.arraySize(); ++i)
	{
		const JValue & command = commands[i];
		std::string _command, client;
		if (!command.isObject())
			continue;

		command["client"].asString(client);
		command["command"].asString(_command);

		if (_command == "policyActionDeny")
			command_list.push_back(std::unique_ptr<PolicyAction>(new PolicyAction(*ClientMap[client])));

		JValue resources = command["resources"];
		if (resources.isArray())
		{
			resources_list res_list;
			std::string type;
			int count, index;
			for (int i  = 0; i < resources.arraySize(); ++i)
			{
				const JValue & resource = resources[i];
				if (resource.isObject())
				{
					resource["type"].asString(type);
					resource.hasKey("count") ? resource["count"].asNumber(count) : count = 1;
					resource.hasKey("index") ? resource["index"].asNumber(index) : index = -1;
					res_list.push_back(resource_struct({ type, index, count }));
				}
			}
			if (_command == "acquire")
				command_list.push_back(std::unique_ptr<Acquire>(new Acquire(*ClientMap[client], res_list)));
			else if (_command == "release")
				command_list.push_back(std::unique_ptr<Release>(new Release(*ClientMap[client], res_list)));
		}

	}
}

int main(int argc, char *argv[])
{
	if (argc <= 1)
	{
		std::cerr << "Enter file name!" << std::endl;
		return 1;
	}

	std::ifstream f(argv [1]);
	if (!f.good())
	{
		std::cerr << "Wrong file path! File doesn't exist!" << std::endl;
		return 1;
	}

	std::ostringstream sout;
	std::copy(std::istreambuf_iterator<char>(f),
			  std::istreambuf_iterator<char>(),
			  std::ostreambuf_iterator<char>(sout));

	JDomParser parser;
	if (!parser.parse(sout.str(), pbnjson::JSchema::AllSchema()))
	{
		std::cerr << "Invalid input file." << std::endl;
		return 1;
	}

	JValue rm = parser.getDom();
	if (!parse_client(rm["client"]))
	{
		std::cerr << "Wrong client registration!" << std::endl;
		return 1;
	}

	parse_commands(rm["simulated_sequence"]);
	for(int i = 0; i < command_list.size(); ++i)
		command_list[i]->apply();
	return 0;
}
