//
// async_client.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/locale.hpp>

using boost::asio::ip::tcp;

class client
{
public:
	client(boost::asio::io_service& io_service,
		const std::string& server, const std::string& path , std::string & _contain)
		: resolver_(io_service),
		socket_(io_service),
		contain(_contain)
	{
		// Form the request. We specify the "Connection: close" header so that the
		// server will close the socket after transmitting the response. This will
		// allow us to treat all data up until the EOF as the content.
		std::ostream request_stream(&request_);
		request_stream << "GET " << path << " HTTP/1.0\r\n";
		request_stream << "Host: " << server << "\r\n";
		request_stream << "Accept: */*\r\n";
		request_stream << "Connection: close\r\n\r\n";

		// Start an asynchronous resolve to translate the server and service names
		// into a list of endpoints.
		tcp::resolver::query query(server, "http");
		resolver_.async_resolve(query,
			boost::bind(&client::handle_resolve, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::iterator));
	}

private:
	void handle_resolve(const boost::system::error_code& err,
		tcp::resolver::iterator endpoint_iterator)
	{
		if (!err)
		{
			// Attempt a connection to each endpoint in the list until we
			// successfully establish a connection.
			boost::asio::async_connect(socket_, endpoint_iterator,
				boost::bind(&client::handle_connect, this,
				boost::asio::placeholders::error));
		}
		else
		{
			std::cout << "Error: " << err.message() << "\n";
		}
	}

	void handle_connect(const boost::system::error_code& err)
	{
		if (!err)
		{
			// The connection was successful. Send the request.
			boost::asio::async_write(socket_, request_,
				boost::bind(&client::handle_write_request, this,
				boost::asio::placeholders::error));
		}
		else
		{
			std::cout << "Error: " << err.message() << "\n";
		}
	}

	void handle_write_request(const boost::system::error_code& err)
	{
		if (!err)
		{
			// Read the response status line. The response_ streambuf will
			// automatically grow to accommodate the entire line. The growth may be
			// limited by passing a maximum size to the streambuf constructor.
			boost::asio::async_read_until(socket_, response_, "\r\n",
				boost::bind(&client::handle_read_status_line, this,
				boost::asio::placeholders::error));
		}
		else
		{
			std::cout << "Error: " << err.message() << "\n";
		}
	}

	void handle_read_status_line(const boost::system::error_code& err)
	{
		if (!err)
		{
			// Check that response is OK.
			std::istream response_stream(&response_);
			std::string http_version;
			response_stream >> http_version;
			unsigned int status_code;
			response_stream >> status_code;
			std::string status_message;
			std::getline(response_stream, status_message);
			if (!response_stream || http_version.substr(0, 5) != "HTTP/")
			{
				std::cout << "Invalid response\n";
				return;
			}
			if (status_code != 200)
			{
				std::cout << "Response returned with status code ";
				std::cout << status_code << "\n";
				return;
			}

			// Read the response headers, which are terminated by a blank line.
			boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
				boost::bind(&client::handle_read_headers, this,
				boost::asio::placeholders::error));
		}
		else
		{
			std::cout << "Error: " << err << "\n";
		}
	}

	void handle_read_headers(const boost::system::error_code& err)
	{
		if (!err)
		{
			// Process the response headers.
			std::istream response_stream(&response_);
			std::string header;
			while (std::getline(response_stream, header) && header != "\r")
				//std::cout << header << "\n";
				oss << header << "\n";
			//std::cout << "\n";
			oss << "\n";

			// Write whatever content we already have to output.
			if (response_.size() > 0)
				//std::cout << &response_;
				oss << &response_;

			// Start reading remaining data until EOF.
			boost::asio::async_read(socket_, response_,
				boost::asio::transfer_at_least(1),
				boost::bind(&client::handle_read_content, this,
				boost::asio::placeholders::error));
		}
		else
		{
			std::cout << "Error: " << err << "\n";
		}
	}

	void handle_read_content(const boost::system::error_code& err)
	{
		if (!err)
		{
			// Write all of the data that has been read so far.
			//std::cout << &response_;
			oss << &response_;

			// Continue reading remaining data until EOF.
			boost::asio::async_read(socket_, response_,
				boost::asio::transfer_at_least(1),
				boost::bind(&client::handle_read_content, this,
				boost::asio::placeholders::error));
		}
		else if (err != boost::asio::error::eof)
		{
			std::cout << "Error: " << err << "\n";
		}
		else if(err == boost::asio::error::eof)
		{
			contain = oss.str();
		}
	}

	tcp::resolver resolver_;
	tcp::socket socket_;
	boost::asio::streambuf request_;
	boost::asio::streambuf response_;

	std::ostringstream oss;
	std::string &contain;
};

void toUtf_8( std::string & str );
bool isUtf_8(const char* str,long length);

void toUtf_8( std::string & str )
{
	if(isUtf_8(str.c_str() , str.size()))
		return;
	else
	{
		std::string s = str;
		str = boost::locale::conv::between( s, "UTF-8", "gb2312" );
	}
}
bool isUtf_8(const char* str,long length)
{
	int i;
	int nBytes=0;//UFT8可用1-6个字节编码,ASCII用一个字节
	unsigned char chr;
	bool bAllAscii=true; //如果全部都是ASCII, 说明不是UTF-8
	for(i=0;i<length;i++)
	{
		chr= *(str+i);
		if( (chr&0x80) != 0 ) // 判断是否ASCII编码,如果不是,说明有可能是UTF-8,ASCII用7位编码,但用一个字节存,最高位标记为0,o0xxxxxxx
			bAllAscii= false;
		if(nBytes==0) //如果不是ASCII码,应该是多字节符,计算字节数
		{
			if(chr>=0x80)
			{
				if(chr>=0xFC&&chr<=0xFD)
					nBytes=6;
				else if(chr>=0xF8)
					nBytes=5;
				else if(chr>=0xF0)
					nBytes=4;
				else if(chr>=0xE0)
					nBytes=3;
				else if(chr>=0xC0)
					nBytes=2;
				else
				{
					return false;
				}
				nBytes--;
			}
		}
		else //多字节符的非首字节,应为 10xxxxxx
		{
			if( (chr&0xC0) != 0x80 )
			{
				return false;
			}
			nBytes--;
		}
	}

	if( nBytes > 0 ) //违返规则
	{
		return false;
	}

	if( bAllAscii ) //如果全部都是ASCII, 说明不是UTF-8
	{
		return false;
	}
	return true;
}

int main(int argc, char* argv[])
{
	try
	{

		//http://jinshanjiejc.soufun.com


		char buf[1024] = "";
		std::string url = "";
		std::string host = "";

		std::ifstream fin("input.txt");
		if(!fin)
		{
			std::cout << "no file input" << std::endl;
			return 1;
		}

		while(fin.getline(buf , sizeof(buf)))
		{
			std::stringstream ss (buf);
			ss >> url;
			ss >> host;
			std::cout << url << "	" << host << std::endl;
		}
		
		fin.close();

		if(url.size() == 0 || host.size() == 0)
		{
			url = "http://www.wubaiyi.com/";
			host = "www.wubaiyi.com";
		}

		std::string contain;
		boost::asio::io_service io_service;
		client c(io_service, host, url , contain);

		io_service.run();


		toUtf_8(contain);

		remove("output.txt");
		std::ofstream fout("output.txt");
		fout << contain ;
		fout.close();
	}
	catch (std::exception& e)
	{
		std::cout << "Exception: " << e.what() << "\n";
	}


	getchar();
	return 0;
}