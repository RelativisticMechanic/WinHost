#pragma comment(lib, "Ws2_32.lib")

#define WINHOST_VERSION_STRING "0.1.0"
#define WINHOST_CREDIT_STRING "(C) Siddharth Gautam, 2023. This software comes with NO WARRANTY"

#include <iostream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <cassert>
#include <csignal>

#include <winsock2.h>
#include <WS2tcpip.h>
#include <tchar.h>

std::string PrintfToString(std::string format, ...);
void WinHostQuit(int err_code);

#define MAX_REQUEST_SIZE 3000
#define HTTP_FILE_BINARY 0b100000000

typedef enum
{
    HTTP_PLAINTEXT = 0,
    HTTP_JSON = 1,
    HTTP_HTML = 2,
    HTTP_CSS = 3,
    HTTP_JS = 4,
    HTTP_IMAGE_JPEG = 5 | HTTP_FILE_BINARY,
    HTTP_IMAGE_PNG = 6 | HTTP_FILE_BINARY,
    HTTP_FONT_TTF = 7 | HTTP_FILE_BINARY,
    HTTP_FONT_OTF = 8 | HTTP_FILE_BINARY,
    HTTP_KUCHBHI = -1
} HTTPContentType;

std::map<std::string, HTTPContentType> file_formats {
    { ".txt", HTTP_PLAINTEXT },
    { ".json", HTTP_JSON },
    { ".html", HTTP_HTML },
    { ".js", HTTP_JS },
    { ".css", HTTP_CSS },
    { ".jpeg", HTTP_IMAGE_JPEG },
    { ".jpg", HTTP_IMAGE_JPEG },
    { ".png", HTTP_IMAGE_PNG },
    { ".ttf", HTTP_FONT_TTF },
    { ".otf", HTTP_FONT_OTF }
};

std::map<int, std::string> content_type_strings = {
    { HTTP_PLAINTEXT, "text/plain" },
    { HTTP_JSON, "application/json" },
    { HTTP_HTML, "text/html" },
    { HTTP_JS, "text/javascript" },
    { HTTP_CSS, "text/css" },
    { HTTP_IMAGE_JPEG, "image/jpeg" },
    { HTTP_IMAGE_PNG, "image/png" },
    { HTTP_FONT_TTF, "font/ttf" },
    { HTTP_FONT_OTF, "font/otf"}
};

std::map<int, std::string> status_messages = {
        { 200, "OK" },
        { 400, "Bad Request" },
        { 401, "Unauthorized" },
        { 402, "Payment Required" },
        { 403, "Forbidden" },
        { 404, "Not Found" },
        { 405, "Method Not Allowed" }
};

typedef enum
{
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_UNKNOWN
} HTTPRequestMethod;

class HTTPResponse
{
public:
    std::vector<uint8_t> vbuf;

    HTTPResponse()
    {
        /* Do nothing */
    }

    /* Generate an HTML / CSS / JS response */
    HTTPResponse(int status_code, HTTPContentType type, std::string content)
    {
        std::string status_msg, content_type, response_str;
        int content_length = content.length();
        /* Get status message */
        if (status_messages.find(status_code) != status_messages.end())
        {
            status_msg = status_messages[status_code];
        }
        else
        {
            status_msg = "OK";
        }

        content_type = content_type_strings[type];

        response_str = PrintfToString("HTTP/1.1 %d %s\n", status_code, status_msg.c_str());
        response_str += PrintfToString("Content-Type:%s\n", content_type.c_str());
        response_str += PrintfToString("Content-Length: %d\n\n", content_length);
        response_str += content;

        for (int i = 0; i < response_str.length(); i++)
        {
            this->vbuf.push_back(response_str[i]);
        }
    }

    /* Generate a PNG / JPG / TTF (binary) response */
    HTTPResponse(int status_code, HTTPContentType type, uint8_t* content_buffer, int buffer_size)
    {
        std::string status_msg, content_type, response_str;
        /* Get status message */
        if (status_messages.find(status_code) != status_messages.end())
        {
            status_msg = status_messages[status_code];
        }
        else
        {
            status_msg = "OK";
        }

        content_type = content_type_strings[type];

        response_str = PrintfToString("HTTP/1.1 %d %s\n", status_code, status_msg.c_str());
        response_str += PrintfToString("Content-Type:%s\n", content_type.c_str());
        response_str += PrintfToString("Content-Length: %d\n\n", buffer_size);
        for (int i = 0; i < response_str.length(); i++)
        {
            this->vbuf.push_back(response_str[i]);
        }
        for (int i = 0; i < buffer_size; i++)
        {
            this->vbuf.push_back(content_buffer[i]);
        }
    }

    uint8_t* GetBuffer()
    {
        return this->vbuf.data();
    }

    int length(void)
    {
        return this->vbuf.size();
    }

    ~HTTPResponse()
    {
        this->vbuf.clear();
    }
};

class HTTPRequest
{
public:
    std::map<std::string, HTTPRequestMethod> method_from_str_map = {
        { "GET", HTTP_GET },
        { "POST", HTTP_POST },
        { "PUT", HTTP_PUT },
        { "DELETE", HTTP_DELETE }
    };

    std::map<HTTPRequestMethod, std::string> str_from_method_map = {
        { HTTP_GET, "GET" },
        { HTTP_POST, "POST" },
        { HTTP_PUT, "PUT" },
        { HTTP_DELETE, "DELETE" },
        { HTTP_UNKNOWN, "UNKNOWN" }
    };

    HTTPRequestMethod method = HTTP_UNKNOWN;
    std::string path = "";

    HTTPRequest(const char* response_str)
    {
        int cursor = 0;
        std::vector<std::string> tokens;
        std::string current_token = "";
        /* Format of an HTTP Request: Method<SPACE>Path<HTTP Version> */
        while (cursor < strlen(response_str))
        {
            if (response_str[cursor] == '\n')
            {
                if (current_token.length())
                    tokens.push_back(current_token);
                break;
            }
            else if (response_str[cursor] == ' ')
            {
                tokens.push_back(current_token);
                current_token = "";
            }
            else
            {
                current_token += response_str[cursor];
            }

            cursor += 1;
        }

        if (tokens.size() >= 2)
        {
            /* Get method */
            if (method_from_str_map.find(tokens[0]) != method_from_str_map.end())
            {
                this->method = method_from_str_map[tokens[0]];
            }
            /* Get path */
            this->path = std::string(tokens[1]);
        }
    }

    void Display(void)
    {
        std::cout << "[HTTP] Method: " << str_from_method_map[this->method] << " Path: " << this->path << std::endl;
    }
};

/* Some basic responses */
HTTPResponse ResponseBadRequest(400, HTTP_HTML, "<h2>400: Bad Request</h2><p>That's not something we expected.</p><i>Running WinHost " WINHOST_VERSION_STRING "</i>");
HTTPResponse ResponseForbidden(403, HTTP_HTML, "<h2>403: Forbidden</h2><p>You aren't supposed to access this.</p><i>Running WinHost " WINHOST_VERSION_STRING "</i>");
HTTPResponse ResponseNotFound = HTTPResponse(404, HTTP_HTML, "<h2>404: Not found</h2><p>The file was not found on this server.</p><i>Running WinHost " WINHOST_VERSION_STRING "</i>");
HTTPResponse ResponseUnsupportedMethod = HTTPResponse(405, HTTP_HTML, "<h2>405: Method Not Allow</h2><p>This is a simple HTTP web server. It only accepts GET requests.</p><i>Running WinHost " WINHOST_VERSION_STRING "</i>");
HTTPResponse ResponseWinHostDefault = HTTPResponse(200, HTTP_HTML, "<h2>Hello from WinHost!</h2><p>If you are seeing this message, that means WinHost is running!</p>");

typedef HTTPResponse (*HTTPCallBack)(HTTPRequest);

class HTTPConnection
{
public:
    int winsock_err, port;
    SOCKET listener_socket = INVALID_SOCKET;
    WSADATA winsock_data;

    HTTPConnection(int port)
    {
        this->port = port;
        this->winsock_err = WSAStartup(MAKEWORD(2, 2), &winsock_data);
        if (this->winsock_err)
        {
            std::cerr << "[ERROR] winsock 2.2 failed to initialize. WS2_Error_Code: " << WSAGetLastError() << std::endl;
            WinHostQuit(-1);
        }

        struct addrinfo* result = NULL, * ptr = NULL, hints;
        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        this->winsock_err = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &result);
        if (this->winsock_err)
        {
            std::cerr << "[ERROR] getaddrinfo() failed. WS2_Error_Code: " << WSAGetLastError() << std::endl;
            WSACleanup();
            WinHostQuit(-1);
        }

        this->listener_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

        if (listener_socket == INVALID_SOCKET)
        {
            std::cerr << "[ERROR] socket creation failed. WS2_Error_Code: " << WSAGetLastError() << std::endl;
            WSACleanup();
            WinHostQuit(-1);
        }

        this->winsock_err = bind(listener_socket, result->ai_addr, (int)result->ai_addrlen);
        this->winsock_err = listen(listener_socket, AF_INET);

        if (this->winsock_err)
        {
            std::cerr << "[ERROR] failed to bind & listen to socket. WS2_Error_Code: " << WSAGetLastError() << std::endl;
            WinHostQuit(-1);
        }
    }

    void Listen(HTTPCallBack callback)
    {

        std::cout << "[NOTIFY] WinHost is listening on: http://localhost:" << this->port << std::endl;
        /* Get IP on local network */
        char ipbuf[1024];
        if(gethostname(ipbuf, sizeof(ipbuf)) == SOCKET_ERROR)
        {
            std::cerr << "[ERROR] gethostname() failed" << std::endl;
            WinHostQuit(-1);
        }
        struct hostent* host_entries = gethostbyname(ipbuf);
        if(host_entries == NULL)
        {
            std::cerr << "[ERROR] gethostbyname() failed" << std::endl;
            WinHostQuit(-1);
        }

        for(int i = 0; host_entries->h_addr_list[i] != 0; i++)
        {
            struct in_addr addr;
            memcpy(&addr, host_entries->h_addr_list[i], sizeof(struct in_addr));
            std::cout << "[NOTIFY] Alternatively, http://" << inet_ntoa(addr) << ":" << this->port << std::endl;
        }
        
        std::cout << "*** Hit ^C (CTRL+C) to stop hosting anytime. ***" << std::endl;
        while (true)
        {
            SOCKET client = accept(this->listener_socket, NULL, NULL);
            char reply[MAX_REQUEST_SIZE] = { 0 };
            recv(client, reply, sizeof(reply), NULL);
            HTTPRequest request = HTTPRequest((const char*)&reply);

            if (client == SOCKET_ERROR)
            {
                std::cerr << "[ERROR] Accept Failed! WS2_Error_Code: " << WSAGetLastError() << std::endl;
                WinHostQuit(-1);
            }
            else
            {
                HTTPResponse response = ResponseWinHostDefault;
                if (callback)
                {
                    response = callback(request);
                }
                send(client, (const char*)response.GetBuffer(), response.length(), NULL);
            }
            closesocket(client);
        }
    }
};

inline bool endsWith(std::string const& value, std::string const& ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

void WinHostQuit(int err_code)
{
    WSACleanup();
    std::cout << "[NOTIFY] WinHost has shut down.\n" << std::endl;
    /* _exit forces program termination, we are using this function as a signal handler too and exit(3) will fail inside signal handler. */
    _exit(err_code);
}
/* Makes it easy for us to printf to a std::string */
std::string PrintfToString(std::string format, ...)
{
    va_list args, args_copy;
    va_start(args, format);
    va_copy(args_copy, args);

    const auto sz = std::vsnprintf(nullptr, 0, format.c_str(), args) + 1;

    char* result = (char*)calloc(sz, sizeof(char));
    assert(result != NULL);
    std::vsnprintf(result, sz, format.c_str(), args_copy);

    va_end(args_copy);
    va_end(args);

    std::string result_cpp = std::string(result);
    free(result);
    return result_cpp;
}

HTTPResponse SimpleHTTPServer(HTTPRequest request)
{
    request.Display();

    /* Only GET requests sir. */
    if(request.method != HTTP_GET)
    {
        return ResponseUnsupportedMethod;
    }
    /* Default / => index.html */
    if (request.path == "/")
    {
        request.path = "index.html";
    }

    std::string path = "./" + request.path;
    
    /* Get file format */
    HTTPContentType type = HTTP_KUCHBHI;
    for (auto i = file_formats.begin(); i != file_formats.end(); i++)
    {
        if (endsWith(path, i->first))
        {
            type = i->second;
        }
    }

    if (type == HTTP_KUCHBHI)
    {
        /* Yeah, don't read formats we don't support, for security purposes */
        return ResponseForbidden;
    }

    std::ifstream stream;
    /* Check if file is of binary type */
    if (type & HTTP_FILE_BINARY)
        stream = std::ifstream(path, std::ios::binary);
    else
        stream = std::ifstream(path);

    if (stream.fail())
    {
        return ResponseNotFound;
    }

    /* If binary read into a binary buffer */
    if (type & HTTP_FILE_BINARY)
    {
        int size = std::filesystem::file_size(path);
        char* buffer = (char*)calloc(size, sizeof(char));
        stream.read(buffer, size);
        HTTPResponse response = HTTPResponse(200, type, (uint8_t*)buffer, size);
        free(buffer);
        return response;
    }
    /* Else read into a string buffer */
    else
    {
        std::stringstream buffer;
        buffer << stream.rdbuf();
        stream.close();
        return HTTPResponse(200, type, buffer.str());
    }
}

int main(int argc, const char* argv)
{
    // 8772 = 'WH' aka WinHost
    uint16_t port = 'WH';
    std::cout << "WinHost, version: " << WINHOST_VERSION_STRING << std::endl;
    std::cout << WINHOST_CREDIT_STRING << std::endl;
    HTTPConnection connection = HTTPConnection(port);
    connection.Listen(SimpleHTTPServer);
    return 0;
}

