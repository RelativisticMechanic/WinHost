#pragma comment(lib, "Ws2_32.lib")

#define WINHOST_VERSION_STRING "0.3.1"
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

#include <atlbase.h>
#include <atlconv.h>

#include "md-min.h"

std::string PrintfToString(std::string format, ...);
void WinHostQuit(int err_code);

#define MAX_REQUEST_SIZE 3000
#define HTTP_FILE_BINARY 0b100000000

#define WINHOST_HTML_HEADER "<!DOCTYPE html>"\
"<html>"\
"<head>"\
"<meta charset=\"UTF-8\">"\
"<title>WinHost " WINHOST_VERSION_STRING "</title>"\
"<link href=\"https://cdn.jsdelivr.net/npm/bootstrap@5.0.2/dist/css/bootstrap.min.css\" rel=\"stylesheet\">"\
"<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.2/css/all.min.css\"/>"\
"</head>"\
"<body>"

#define WINHOST_HTML_FOOTER "<script src=\"https://cdn.jsdelivr.net/npm/bootstrap@5.0.2/dist/js/bootstrap.bundle.min.js\"></script>"\
"</body>"\
"</html>"

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
    HTTP_IMAGE_ICO = 9 | HTTP_FILE_BINARY,
    NONHTTP_DOC_MD = 10,
    HTTP_KUCHBHI = -1
} HTTPContentType;

std::map<std::string, HTTPContentType> file_formats {
    { ".txt", HTTP_PLAINTEXT },
    { ".json", HTTP_JSON },
    { ".html", HTTP_HTML },
    { ".htm", HTTP_HTML },
    { ".js", HTTP_JS },
    { ".css", HTTP_CSS },
    { ".jpeg", HTTP_IMAGE_JPEG },
    { ".jpg", HTTP_IMAGE_JPEG },
    { ".ico", HTTP_IMAGE_ICO },
    { ".png", HTTP_IMAGE_PNG },
    { ".ttf", HTTP_FONT_TTF },
    { ".otf", HTTP_FONT_OTF },
    { ".md", NONHTTP_DOC_MD }
};

std::map<int, std::string> content_type_strings = {
    { HTTP_PLAINTEXT, "text/plain" },
    { HTTP_JSON, "application/json" },
    { HTTP_HTML, "text/html" },
    { HTTP_JS, "text/javascript" },
    { HTTP_CSS, "text/css" },
    { HTTP_IMAGE_JPEG, "image/jpeg" },
    { HTTP_IMAGE_PNG, "image/png" },
    { HTTP_IMAGE_ICO, "image/vnd.microsoft.icon" },
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
    HTTP_HEAD,
    HTTP_CONNECT,
    HTTP_OPTIONS,
    HTTP_TRACE,
    HTTP_PATCH,
    HTTP_UNKNOWN
} HTTPRequestMethod;

class HTTPResponse
{
public:
    std::vector<uint8_t> vbuf;
    int code = 200;

    HTTPResponse()
    {
        /* Do nothing */
    }

    /* Generate an HTML / CSS / JS response */
    HTTPResponse(int status_code, HTTPContentType type, std::string content)
    {
        this->code = status_code;
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

    void Display(void)
    {
        switch(this->code / 100)
        {
            case 2:
                /* Green */
                std::cout << "\x1B[32m";
                break;
            case 3:
                /* Yellow */
                std::cout << "\x1B[33m";
                break;
            case 4:
                /* Red */
                std::cout << "\x1B[31m";
                break;
            default:
                break;
        }

        std::cout << "[HTTPResponse] " << this->code << " " << status_messages[this->code];
        std::cout << "\x1B[39m" << std::endl;
    }

    ~HTTPResponse()
    {
        this->vbuf.clear();
    }
};

class HTTPRequest
{
public:
    typedef enum
    {
        PARSER_STATE_NORMAL,
        PARSER_STATE_PROCESSING_ASCII
    } HTTP_REQUEST_PARSER_STATE;

    std::map<std::string, HTTPRequestMethod> method_from_str_map = {
        { "GET", HTTP_GET },
        { "POST", HTTP_POST },
        { "PUT", HTTP_PUT },
        { "DELETE", HTTP_DELETE },
        { "HEAD", HTTP_HEAD },
        { "CONNECT", HTTP_CONNECT },
        { "OPTIONS", HTTP_OPTIONS },
        { "TRACE", HTTP_TRACE },
        { "PATCH", HTTP_PATCH }
    };

    std::map<HTTPRequestMethod, std::string> str_from_method_map = {
        { HTTP_GET, "GET" },
        { HTTP_POST, "POST" },
        { HTTP_PUT, "PUT" },
        { HTTP_DELETE, "DELETE" },
        { HTTP_HEAD, "HEAD" },
        { HTTP_CONNECT, "CONNECT" },
        { HTTP_OPTIONS, "OPTIONS" },
        { HTTP_TRACE, "TRACE" },
        { HTTP_PATCH, "PATCH" },
        { HTTP_UNKNOWN, "UNKNOWN" }
    };

    HTTPRequestMethod method = HTTP_UNKNOWN;
    std::string path = "";


    int ascii_chars_left = 0;
    char ascii_char = 0;

    HTTP_REQUEST_PARSER_STATE parser_state = PARSER_STATE_NORMAL;
    HTTPRequest(const char* response_str)
    {
        int cursor = 0;
        std::vector<std::string> tokens;
        std::string current_token = "";
        /* Format of an HTTP Request: Method<SPACE>Path<HTTP Version> */
        while (cursor < strlen(response_str))
        {
            /* Are we processing an ASCII character prefixed with a '%'? */
            if(parser_state == PARSER_STATE_PROCESSING_ASCII)
            {
                ascii_chars_left -= 1;
                ascii_char = ascii_char | ((response_str[cursor] - '0') << (4 * ascii_chars_left)); 
                if(ascii_chars_left <= 0)
                {
                    parser_state = PARSER_STATE_NORMAL;
                    current_token += ascii_char;
                }
            }
            else 
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
                else if(response_str[cursor] == '%')
                {
                    parser_state = PARSER_STATE_PROCESSING_ASCII;
                    ascii_chars_left = 2;
                    ascii_char = 0;
                }
                else
                {
                    current_token += response_str[cursor];
                }
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
        std::cout << "\x1B[94m[HTTPRequest] Method: " << str_from_method_map[this->method] << " Path: " << this->path << "\x1B[39m" << std::endl;
    }
};

/* Some basic responses */
HTTPResponse ResponseBadRequest(400, HTTP_HTML, WINHOST_HTML_HEADER "<h2>400: Bad Request</h2><p>That's not something we expected.</p><i>Running WinHost " WINHOST_VERSION_STRING "</i>" WINHOST_HTML_FOOTER);
HTTPResponse ResponseForbidden(403, HTTP_HTML, WINHOST_HTML_HEADER "<h2>403: Forbidden</h2><p>You aren't supposed to access this.</p><i>Running WinHost " WINHOST_VERSION_STRING "</i>" WINHOST_HTML_FOOTER);
HTTPResponse ResponseNotFound = HTTPResponse(404, HTTP_HTML, WINHOST_HTML_HEADER "<h2>404: Not found</h2><p>The file was not found on this server.</p><i>Running WinHost " WINHOST_VERSION_STRING "</i>" WINHOST_HTML_FOOTER);
HTTPResponse ResponseUnsupportedMethod = HTTPResponse(405, HTTP_HTML, WINHOST_HTML_HEADER "<h2>405: Method Not Allow</h2><p>This is a simple HTTP web server. It only accepts GET requests.</p><i>Running WinHost " WINHOST_VERSION_STRING "</i>" WINHOST_HTML_FOOTER);

/* The index page */
HTTPResponse ResponseWinHostDefault = HTTPResponse(200, HTTP_HTML, WINHOST_HTML_HEADER "<head><title>WinHost Index Page</title></head>"
                                                                    "<h2>Hello from WinHost!</h2>"
                                                                    "<p>If you are seeing this message, that means WinHost is running! Please add an index.html to the directory.</p>"
                                                                    "<i>Running WinHost " WINHOST_VERSION_STRING "</i>" WINHOST_HTML_FOOTER);

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

        std::cout << "[NOTIFY] WinHost is listening on: \x1B[7mhttp://localhost:" << this->port << "\x1B[27m" << std::endl;
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
            std::cout << "[NOTIFY] Alternatively, \x1B[7mhttp://" << inet_ntoa(addr) << ":" << this->port << "\x1B[27m"<< std::endl;
        }
        
        std::cout << "\x1B[33m*** Hit ^C (CTRL+C) to stop hosting anytime. ***\x1B[39m" << std::endl;
        while (true)
        {
            SOCKET client = accept(this->listener_socket, NULL, NULL);
            char reply[MAX_REQUEST_SIZE] = { 0 };
            recv(client, reply, sizeof(reply), NULL);
            HTTPRequest request = HTTPRequest((const char*)&reply);
            if(request.method != HTTP_UNKNOWN)
                request.Display();

            if (client == SOCKET_ERROR)
            {
                std::cerr << "[ERROR] Accept Failed! WS2_Error_Code: " << WSAGetLastError() << std::endl;
                WinHostQuit(-1);
            }
            
            if(request.method != HTTP_UNKNOWN)
            {
                HTTPResponse response = ResponseWinHostDefault;
                if (callback)
                {
                    response = callback(request);
                }
                response.Display();
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

HTTPResponse SimpleFileBrowser(std::string path)
{
    std::filesystem::path current_dir = std::filesystem::path("." + path);

    /* Check if directory exists */
    if(!std::filesystem::exists(current_dir))
    {
        return ResponseNotFound;
    }

    /* Check if it is a directory */
    if(!std::filesystem::is_directory(current_dir))
    {
        return ResponseForbidden;
    }

    /* HTML Filebrowser */
    std::string filebrowser_html = "<div class=\"container my-5\">"
    "<h3>Browsing index of: " + path + "</h3>" 
    "<table class=\"table shadow-lg\">";

    filebrowser_html += "<tr><td><a href=\"/" + (current_dir.parent_path()).parent_path().string() + "/\">..</td></a><td></td></tr>";

    for (const auto & entry : std::filesystem::directory_iterator(current_dir))
    {
        std::filesystem::path file_path = entry.path();
        std::string file_size;
        /* Font-Awesome Icon for the path */
        std::string path_icon = "fa-file";
        std::string path_url = file_path.string();
        /* Remove leading dot from path */
        path_url.erase(0, 1);
        /* If directory, add a slash */
        if(std::filesystem::is_directory(file_path))
        {
            path_url += "/";
            path_icon = "fa-folder";
            /* Don't report file sizes for dirs */
            file_size = "";
        }
        else
        {
            file_size = std::to_string(std::filesystem::file_size(file_path)) + " bytes";
        }
        filebrowser_html += "<tr><td><span class=\"fa " + path_icon + "\"></span> " + "<a href=\"" + path_url + "\">" + file_path.filename().string() + "</a></td><td>" +  file_size + "</td></tr>";
    }

    filebrowser_html += "</table><i>Running WinHost " WINHOST_VERSION_STRING "</i></div>";

    return HTTPResponse(200, HTTP_HTML, WINHOST_HTML_HEADER + filebrowser_html + WINHOST_HTML_FOOTER);
}

HTTPResponse SimpleHTTPServer(HTTPRequest request)
{
    /* Only GET requests sir. */
    if(request.method != HTTP_GET && request.method != HTTP_HEAD)
    {
        return ResponseUnsupportedMethod;
    }
    /* Default / => index.html */
    if (request.path == "/")
    {
        request.path = "index.html";
    }

    /* Check if it is a directory */
    if(endsWith(request.path, "/"))
    {
        /* Open the filebrowser */
        return SimpleFileBrowser(request.path);
    }

    std::string path = "." + request.path;

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
        /* If we failed for index.html return the default index page. */
        if(request.path == "index.html")
            return ResponseWinHostDefault;

        return ResponseNotFound;
    }

    /* If HEAD, now is the time to return an OK */
    if(request.method == HTTP_HEAD)
        return HTTPResponse(200, HTTP_HTML, "");

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

        if(type != NONHTTP_DOC_MD)
        {
            return HTTPResponse(200, type, buffer.str());
        }
        else
        {
            /* Return MD parsed file */
            std::shared_ptr<maddy::ParserConfig> config = std::make_shared<maddy::ParserConfig>();
            config->enabledParsers &= ~maddy::types::EMPHASIZED_PARSER;
            config->enabledParsers |= maddy::types::HTML_PARSER;
            /* Enable LaTeX */
            config->enabledParsers |= maddy::types::LATEX_BLOCK_PARSER;

            std::shared_ptr<maddy::Parser> parser = std::make_shared<maddy::Parser>(config);
            std::string html_output = parser->Parse(buffer);
            /* Include basic HTML header */
            std::string html_output_before = "<!DOCTYPE html>"
                                            "<html>"
                                            "<head>"
                                            "<meta charset=\"UTF-8\""
                                            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                                            "<title>" + request.path + "</title>"
                                            /* Include GitHub's markdown */
                                            "<link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/github-markdown-css/5.2.0/github-markdown-light.css\"/>"
                                            "<style>"
                                            /* Markdown Style */
                                            ".markdown-body"
                                            "{"
                                            "box-sizing: border-box;"
                                            "min-width: 200px;"
                                            "max-width: 980px;"
                                            "margin: 0 auto;"
                                            "padding: 45px;"
                                            "}"

                                            "@media (max-width: 767px)"
                                            "{"
                                            ".markdown-body"
                                            "{"
                                            "padding: 15px;"
                                            "}"
                                            "}"
                                            "</style>"
                                            "</head>"
                                            "<body>"
                                            "<article class=\"markdown-body\">";

            std::string html_output_after = "</article>"
                                            /* Include MathJAX */
                                            "<script src=\"https://polyfill.io/v3/polyfill.min.js?features=es6\"></script>"
                                            "<script id=\"MathJax-script\" async src=\"https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-mml-chtml.js\"></script>"
                                            "</body>"
                                            "</html>";
            


            return HTTPResponse(200, HTTP_HTML, html_output_before + html_output + html_output_after);
        }
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

