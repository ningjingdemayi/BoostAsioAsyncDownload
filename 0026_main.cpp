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
		const std::string& server, const std::string& path , std::string & _contain , long timer_seconds = 3)
		:
		ioserv(io_service),
		resolver_(io_service),
		socket_(io_service),
		contain(_contain),
		timer_(io_service)

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

		/*===add for test timer =======*/
		timer_.expires_from_now(boost::posix_time::seconds(timer_seconds));
		timer_.async_wait(boost::bind(&client::close_socket, this));
		/*===add for test timer =======*/
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
			ioserv.stop();
		}
	}

	tcp::resolver resolver_;
	tcp::socket socket_;
	boost::asio::streambuf request_;
	boost::asio::streambuf response_;

	std::ostringstream oss;
	std::string &contain;



	/*===add for test timer===*/
	boost::asio::io_service & ioserv;
	boost::asio::deadline_timer timer_;  
	void close_socket()  
	{  
		std::cout << "链接超时" << std::endl;
		ioserv.stop();
		socket_.close();  
	}
	/*==add for test timer=====*/
};



int main(int argc, char* argv[])
{
	try
	{
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
			url = "http://www.baidu.com/";
			host = "www.baidu.com";
		}

		std::string contain;
		boost::asio::io_service io_service;

		//这里传入 要下载的网址 域名 包含下载内容的 字符串 和超时时间
		client c(io_service, host, url , contain , 10);

		io_service.run();


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
