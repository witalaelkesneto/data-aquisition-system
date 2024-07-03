// Trabalho Feito por Witalaelkes Neto e Samuel Henrique (Mesma dupla do seminário)

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
#include <fstream>
#include <filesystem>

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

std::string time_t_to_string(std::time_t time) {
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
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
        boost::asio::async_read_until(socket_, buffer_, "\r\n", [this, self](boost::system::error_code ec, std::size_t length)
                                      {
            if (!ec) {
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            std::cout << "Received: " << message << std::endl;

            // Usar std::istringstream para facilitar o parse
            std::istringstream iss(message);
            std::string token;
            std::vector<std::string> tokens;

            while (std::getline(iss, token, '|')) {
                tokens.push_back(token);
            }

            if (tokens[0] == "LOG") {
                LogRecord log;

                strcpy(log.sensor_id, tokens[1].c_str());
                log.timestamp = string_to_time_t(tokens[2]);
                log.value = std::stod(tokens[3]);

                std::string filePath = "../logs/";
                std::string fileName = std::string(log.sensor_id) + ".dat";

                std::fstream file(filePath + fileName , std::fstream::out | std::fstream::binary | std::fstream::app);

                if (!file.is_open()) {
                    std::cout << "Falha ao criar o arquivo" << std::endl;
                }

                file.write((char*)&log, sizeof(LogRecord));
                file.close();
            }else if(tokens[0] == "GET"){

                int n = std::stoi(tokens[2]);

                std::string filePath = "../logs/";
                std::string fileName = std::string(tokens[1].c_str()) + ".dat";

                if(std::filesystem::exists(filePath + fileName)){
                    std::fstream file(filePath + fileName , std::fstream::in | std::fstream::binary);

                    if (!file.is_open()) {
                        std::cout << "Falha ao abrir o arquivo" << std::endl;
                    }

                    file.seekg(0, std::ios_base::end);

                    int file_size = file.tellg();
                    int numRecords = file_size/sizeof(LogRecord);
                    int startPos = file_size - n * sizeof(LogRecord);

                    if (startPos < 0) {
                        startPos = 0;
                        n = numRecords;
                    }

                    file.seekg(startPos, std::ios_base::beg);

                    LogRecord log;

                    message = std::to_string(n) + ";";

                    while (file.read((char*)&log, sizeof(LogRecord))) {
                        message = message + time_t_to_string(log.timestamp) + "|" + std::to_string(log.value) + ";";
                    }

                    message.append("\r\n");

                    file.close();
                }else{
                    message = "ERROR|INVALID_SENSOR_ID\r\n";
                }
            }
                write_message(message);
            } });
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