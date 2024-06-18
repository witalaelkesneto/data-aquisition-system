#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <string>
#include <sstream>
#include <vector>
#include <ctime>
#include <iomanip>

using boost::asio::ip::tcp;

#pragma pack(push, 1)
struct LogRecord
{
    char sensor_id[32];    // supondo um ID de sensor de até 32 caracteres
    std::time_t timestamp; // timestamp UNIX
    double value;          // valor da leitura
};
#pragma pack(pop)

std::time_t string_to_time_t(const std::string &time_string)
{
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        read_message();
    }

private:
    void read_message()
    {
        auto self(shared_from_this());
        boost::asio::async_read_until(socket_, buffer_, "\r\n",
                                      [this, self](boost::system::error_code ec, std::size_t length)
                                      {
                                          if (!ec)
                                          {
                                              std::istream is(&buffer_);
                                              std::string message(std::istreambuf_iterator<char>(is), {});
                                              std::cout << "Received: " << message << std::endl;

                                              // Usar std::istringstream para facilitar o parse
                                              std::istringstream iss(message);
                                              std::string token;
                                              std::vector<std::string> tokens;

                                              while (std::getline(iss, token, '|'))
                                              {
                                                  tokens.push_back(token);
                                              }

                                              LogRecord log;

                                              strcpy(log.sensor_id, tokens[1].c_str());
                                              log.timestamp = string_to_time_t(tokens[2]);
                                              log.value = std::stod(tokens[3]);

                                              std::cout << log.sensor_id << std::endl;
                                              std::cout << log.timestamp << std::endl;
                                              std::cout << log.value << std::endl;

                                              write_message(message);
                                          }
                                      });
    }

    void write_message(const std::string &message)
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(message),
                                 [this, self, message](boost::system::error_code ec, std::size_t /*length*/)
                                 {
                                     if (!ec)
                                     {
                                         read_message();
                                     }
                                 });
    }

    tcp::socket socket_;
    boost::asio::streambuf buffer_;
};

class server
{
public:
    server(boost::asio::io_context &io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        accept();
    }

private:
    void accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    std::make_shared<session>(std::move(socket))->start();
                }

                accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: chat_server <port>\n";
        return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();

    return 0;
}