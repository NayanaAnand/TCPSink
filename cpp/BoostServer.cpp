/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file distributed with this 
 * source distribution.
 * 
 * This file is part of REDHAWK Basic Components sinksocket.
 * 
 * REDHAWK Basic Components sinksocket is free software: you can redistribute it and/or modify it under the terms of 
 * the GNU Lesser General Public License as published by the Free Software Foundation, either 
 * version 3 of the License, or (at your option) any later version.
 * 
 * REDHAWK Basic Components sinksocket is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
 * PURPOSE.  See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License along with this 
 * program.  If not, see http://www.gnu.org/licenses/.
 */

#include <omniORB4/CORBA.h>
#include "BoostServer.h"

void session::start()
{
	socket_.async_read_some(boost::asio::buffer(read_data_, max_length_),
			boost::bind(&session::handle_read, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
}

template<typename T, typename U>
void session::write(std::vector<T, U>& data)
{
	if (socket_.is_open())
	{
		boost::mutex::scoped_lock lock(writeLock_);
		size_t numBytes = data.size()*sizeof(T);
		writeBuffer_.push_back(std::vector<char>(numBytes));
		memcpy(&writeBuffer_.back()[0],&data[0],numBytes);
		if (writeBuffer_.size()==1)
		{
			boost::asio::async_write(socket_,
				boost::asio::buffer(writeBuffer_[0]),
				boost::bind(&session::handle_write, shared_from_this(),
						boost::asio::placeholders::error));
		}
	}
}

void session::handle_read(const boost::system::error_code& error,
		size_t bytes_transferred)
{
	if (!error)
	{
		read_data_.resize(bytes_transferred);
		server_->newSessionData(read_data_);
		read_data_.resize(max_length_);
		socket_.async_read_some(boost::asio::buffer(read_data_, max_length_),
				boost::bind(&session::handle_read, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred));
	}
	else
	{
		std::cerr<<"ERROR reading session data: "<<error<<std::endl;
		server_->closeSession(shared_from_this());
	}
}

void session::handle_write(const boost::system::error_code& error)
{
	boost::mutex::scoped_lock lock(writeLock_);
	writeBuffer_.pop_front();
	if (error)
	{
		std::cerr<<"ERROR writting session data: "<<error<<std::endl;
		server_->closeSession(shared_from_this());
	}
	else if(!writeBuffer_.empty())
	{
		boost::asio::async_write(socket_,
						boost::asio::buffer(writeBuffer_[0]),
						boost::bind(&session::handle_write, shared_from_this(),
								boost::asio::placeholders::error));
	}
}


template<typename T, typename U>
void server::write(std::vector<T, U>& data)
{
	boost::mutex::scoped_lock lock(sessionsLock_);
	for (std::list<session_ptr>::iterator i = sessions_.begin(); i!=sessions_.end(); i++)
	{
		session_ptr thisSession= *i;
		thisSession->write(data);
	}
}
template<typename T>
void server::read(std::vector<char, T> & data, size_t index)
{
	boost::mutex::scoped_lock lock(pendingDataLock_);
	int numRead=std::min(data.size()-index, pendingData_.size());
	data.resize(index+numRead);
	int j=0;
	for (unsigned int i=index; i!=data.size(); i++)
	{
		data[i]=pendingData_[j];
		j++;
	}
	pendingData_.erase(pendingData_.begin(), pendingData_.begin()+numRead);
}

bool server::is_connected()
{
	return !sessions_.empty();
}

template<typename T>
void server::newSessionData(std::vector<char, T>& data)
{
	boost::mutex::scoped_lock lock(pendingDataLock_);
	int oldSize=pendingData_.size();
	pendingData_.resize(oldSize+data.size());
	char* newData= &data[0];
	for (unsigned int i=oldSize; i!=pendingData_.size(); i++)
	{
		pendingData_[i]=*newData;
		newData++;
	}
}
void server::closeSession(session_ptr ptr)
{
	boost::mutex::scoped_lock lock(sessionsLock_);
	for (std::list<session_ptr>::iterator i=sessions_.begin(); i!=sessions_.end(); i++)
	{
		if (ptr==*i)
		{
			sessions_.remove(ptr);
			break;
		}
	}
}


void server::start_accept()
{
	{
		session_ptr new_session(new session(io_service_, this, maxLength_));

		acceptor_.async_accept(new_session->socket(),
				boost::bind(&server::handle_accept, this, new_session,
						boost::asio::placeholders::error));
	}
}

void server::handle_accept(session_ptr new_session,
		const boost::system::error_code& error)
{
		if (!error)
		{
			{
				boost::mutex::scoped_lock lock(sessionsLock_);
				sessions_.push_back(new_session);

				session_ptr new_session(new session(io_service_, this, maxLength_));
				acceptor_.async_accept(new_session->socket(),
								boost::bind(&server::handle_accept, this, new_session,
										boost::asio::placeholders::error));
			}
			new_session->start();
		}
		start_accept();
}

void server::run()
{
	try
	{
		io_service_.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception in thread: " << e.what() << "\n";
		std::exit(1);
	}
}

//need to put these bad boys in here for templates or you get undefined references when linking ...grr...

template void server::read(std::vector<char, std::allocator<char> >&, size_t);
//template void server::read(std::vector<char, _seqVector::seqVectorAllocator<char> >&, size_t);

template void server::write(std::vector<unsigned char, std::allocator<unsigned char> >&);
template void server::write(std::vector<char, std::allocator<char> >&);
template void server::write(std::vector<signed char, std::allocator<signed char> >&);
template void server::write(std::vector<CORBA::Short, std::allocator<CORBA::Short> >&);
template void server::write(std::vector<CORBA::UShort, std::allocator<CORBA::UShort> >&);
template void server::write(std::vector<CORBA::Long, std::allocator<CORBA::Long> >&);
template void server::write(std::vector<CORBA::ULong, std::allocator<CORBA::ULong> >&);
template void server::write(std::vector<CORBA::Float, std::allocator<CORBA::Float> >&);
template void server::write(std::vector<CORBA::Double, std::allocator<CORBA::Double> >&);
