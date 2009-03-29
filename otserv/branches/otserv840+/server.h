//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////

#ifndef __OTSERV_SERVER_H__
#define __OTSERV_SERVER_H__

#include "definitions.h"

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/utility.hpp>
#include <boost/enable_shared_from_this.hpp>

class Connection;
class Protocol;
class NetworkMessage;

class ServiceBase;
class ServicePort;

typedef boost::shared_ptr<ServiceBase> Service_ptr;
typedef boost::shared_ptr<ServicePort> ServicePort_ptr;

// The Service class is very thin, it's only real job is to create dynamic
// dispatch of the protocol attributes, which would otherwise be very hard,
// and require templating of many Service functions

class ServiceBase : boost::noncopyable
{
public:
	virtual bool is_single_socket() const = 0;
	virtual bool is_checksummed() const = 0;
	virtual uint8_t get_protocol_identifier() const = 0;

	virtual Protocol* make_protocol(Connection* c) const = 0;
};

template <typename ProtocolType>
class Service : public ServiceBase
{
public:
	bool is_single_socket() const {return ProtocolType::server_sends_first;}
	bool is_checksummed() const {return ProtocolType::use_checksum;}
	uint8_t get_protocol_identifier() const {return ProtocolType::protocol_identifier;}

	Protocol* make_protocol(Connection* c) const {return new ProtocolType(c);}
};

// A Service Port represents a listener on a port.
// It accepts connections, and asks each Service running
// on it if it can accept the connection, and if so passes
// it on to the service
class ServicePort : boost::noncopyable, public boost::enable_shared_from_this<ServicePort>
{
public:
	ServicePort(boost::asio::io_service& io_service);
	~ServicePort();

	void open(uint16_t port);
	void close();

	bool add_service(Service_ptr);
	Protocol* make_protocol(bool checksummed, NetworkMessage& msg) const;

	void onStopServer();
	void onAccept(Connection* connection, const boost::system::error_code& error);

protected:
	void accept();

	boost::asio::io_service& m_io_service;
	boost::asio::ip::tcp::acceptor* m_acceptor;
	std::vector<Service_ptr> m_services;

	uint32_t m_listenErrors;
	uint16_t m_serverPort;
	bool m_pendingStart;
};

typedef boost::shared_ptr<ServicePort> ServicePort_ptr;

// The ServiceManager simply manages all services and handles startup/closing
class ServiceManager : boost::noncopyable
{
	ServiceManager(const ServiceManager&);
public:
	ServiceManager();
	~ServiceManager();

	// Run and start all servers
	void run() { m_io_service.run(); }
	void stop();

	// Adds a new service to be managed
	template <typename ProtocolType>
	bool add(uint16_t port);

	std::list<uint16_t> get_ports() const;
protected:
	std::map<uint16_t, ServicePort_ptr> m_acceptors;

	boost::asio::io_service m_io_service;
};

template <typename ProtocolType>
bool ServiceManager::add(uint16_t port)
{
	ServicePort_ptr service_port;

	std::map<uint16_t, ServicePort_ptr>::iterator finder = 
		m_acceptors.find(port);

	if(finder == m_acceptors.end()){
		service_port.reset(new ServicePort(m_io_service));
		service_port->open(port);
		m_acceptors[port] = service_port;
	}
	else{
		service_port = finder->second;
	}

	return service_port->add_service(Service_ptr(new Service<ProtocolType>()));
}

#endif