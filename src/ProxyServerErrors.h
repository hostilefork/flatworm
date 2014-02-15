//
// ProxyServerErrors.h
//
// Unmodified code from 3Proxy's error list.
//

#ifndef __PROXYSERVERERRORS_H__
#define __PROXYSERVERERRORS_H__

struct ProxyWorkerERROR {
public:
	int code;
	char* html;
};

// 0
const ProxyWorkerERROR Proxyerror_Bad_Request = {400,	"HTTP/1.0 400 Bad Request\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>400 Bad Request</title></head>\r\n"
	"<body><h2>400 Bad Request</h2></body></html>\r\n"};

// 1
const ProxyWorkerERROR Proxyerror_Host_Not_Found = {502, "HTTP/1.0 502 Bad Gateway\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>502 Bad Gateway</title></head>\r\n"
	"<body><h2>502 Bad Gateway</h2><h3>Host Not Found or connection failed</h3></body></html>\r\n"};

// 2
const ProxyWorkerERROR Proxyerror_Traffic_Limit = {503,	"HTTP/1.0 503 Service Unavailable\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>503 Service Unavailable</title></head>\r\n"
	"<body><h2>503 Service Unavailable</h2><h3>You have exceeded your traffic limit</h3></body></html>\r\n"};

// 3
const ProxyWorkerERROR Proxyerror_Recursion_Detected = {503, "HTTP/1.0 503 Service Unavailable\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>503 Service Unavailable</title></head>\r\n"
	"<body><h2>503 Service Unavailable</h2><h3>Recursion detected</h3></body></html>\r\n"};

// 4
const ProxyWorkerERROR Proxyerror_Not_Implemented = {501, "HTTP/1.0 501 Not Implemented\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>501 Not Implemented</title></head>\r\n"
	"<body><h2>501 Not Implemented</h2><h3>Required action is not supported by proxy server</h3></body></html>\r\n"};

// 5
const ProxyWorkerERROR Proxyerror_No_Parent_Proxy = {502, "HTTP/1.0 502 Bad Gateway\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>502 Bad Gateway</title></head>\r\n"
	"<body><h2>502 Bad Gateway</h2><h3>Failed to connect parent proxy</h3></body></html>\r\n"};

// 6
const ProxyWorkerERROR Proxyerror_Internal = { 500,
	"HTTP/1.0 500 Internal Error\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>500 Internal Error</title></head>\r\n"
	"<body><h2>500 Internal Error</h2><h3>Internal proxy error during processing your request</h3></body></html>\r\n" };

// 7
const ProxyWorkerERROR Proxyerror_Resource_Disallowed = { 407,
	"HTTP/1.0 407 Proxy Authentication Required\r\n"
	"Proxy-Authenticate: Basic realm=\"proxy\"\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>407 Proxy Authentication Required</title></head>\r\n"
	"<body><h2>407 Proxy Authentication Required</h2><h3>Access to requested resource disallowed by administrator or you need valid username/password to use this resource</h3></body></html>\r\n" };

// 8
const ProxyWorkerERROR Proxyerror_Connection_Established = { 200,
	"HTTP/1.0 200 Connection established\r\n\r\n" };

// 9
const ProxyWorkerERROR Proxyerror_Connection_Established_Html = { 200,
	"HTTP/1.0 200 Connection established\r\n"
	"Content-Type: text/html\r\n\r\n" };

// 10
const ProxyWorkerERROR Proxyerror_Not_Found = { 404,
	"HTTP/1.0 404 Not Found\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>404 Not Found</title></head>\r\n"
	"<body><h2>404 Not Found</h2><h3>File not found</body></html>\r\n" };

// 11
const ProxyWorkerERROR Proxyerror_Access_Denied = {403,	"HTTP/1.0 403 Forbidden\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>403 Access Denied</title></head>\r\n"
	"<body><h2>403 Access Denied</h2><h3>Access control list denies you to access this resource</body></html>\r\n" };

// 12
const ProxyWorkerERROR Proxyerror_Authentication_Username = { 407,
"HTTP/1.0 407 Proxy Authentication Required\r\n"
	"Proxy-Authenticate: NTLM\r\n"
	"Proxy-Authenticate: basic realm=\"proxy\"\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>407 Proxy Authentication Required</title></head>\r\n"
	"<body><h2>407 Proxy Authentication Required</h2><h3>Access to requested resource disallowed by administrator or you need valid username/password to use this resource</h3></body></html>\r\n" };

// 13
const ProxyWorkerERROR Proxyerror_Authentication_Required = { 407,
	"HTTP/1.0 407 Proxy Authentication Required\r\n"
	"Proxy-Connection: keep-alive\r\n"
	"Content-Length: 0\r\n"
	"Proxy-Authenticate: NTLM " };

// 14
const ProxyWorkerERROR Proxyerror_Forbidden_Html = {
	403,	"HTTP/1.0 403 Forbidden\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<pre>"
};

// 15
const ProxyWorkerERROR Proxyerror_Violates_Policy = { 
	503, 	"HTTP/1.0 503 Service Unavailable\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>503 Service Unavailable</title></head>\r\n"
	"<body><h2>503 Service Unavailable</h2><h3>Your request violates configured policy</h3></body></html>\r\n" };

// 16
const ProxyWorkerERROR Proxyerror_Ftp_Authentication_Required = {401,
	"HTTP/1.0 401 Authentication Required\r\n"
	"Proxy-Authenticate: basic realm=\"FTP Server\"\r\n"
	"Proxy-Connection: close\r\n"
	"Content-type: text/html; charset=us-ascii\r\n"
	"\r\n"
	"<html><head><title>401 FTP Server requires authentication</title></head>\r\n"
	"<body><h2>401 FTP Server requires authentication</h2><h3>This FTP server rejects anonymous access</h3></body></html>\r\n" };

#endif
