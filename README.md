# Socket-Programming
Project #1 – NetProbe 
A. Overview
In this project, the students will develop a simple C program for network testing and benchmarking. Key concepts applied in this project include: sockets programming basics, blocking I/O, socket configuration and control, simple protocol design and implementation. The NetProbe is a very useful tool for diagnosing network problems, as well a good tool for benchmarking performance of various programming models. 
B. Specification
The NetProbe tool can be operated in three modes, namely Send Mode, Receive Mode, and HostInfo Mode. Both the send and the receive mode are to be implemented using blocking I/O with multi-threading. The HostInfo mode is also to be implemented using blocking mode. Use of thread is optional. The platform is Windows console mode so no GUI is required.
The command line arguments are specified below.
Specification of Command-line Arguments: 
NetProbe [mode] <more parameters depended on mode, see below>
[mode] : 	“s” means sending mode; “r” means receiving mode ; 
“h” means host information mode. In this mode, NetProbe will try to resolve and find out all the IP addresses and names for a given hostname. 
If [mode] = “s” then
NetProbe [mode] [refresh_interval] [remote_host] [remote_port] [protocol] [packet_size] [rate] [num]
else if [mode] = “r” then
NetProbe [mode] [refresh_interval] [local_host] [local_port] [protocol] [packet_size]
else if [mode] = “h” then 
NetProbe [mode] [hostname]
end if
Parameter Definitions:
[refresh interval] : The refreshing interval of statistics display (in ms).
[remote_host | local_host] : Hostname or IP address for remote | local host.
[remote_port | local_port] : Port number for remote | local host.
[protocol] : “tcp” means to use TCP; “udp” means to use UDP.
[packet_size] : Size in bytes of a datagram (UDP), or size of I/O transaction (TCP).
[rate] : The rate (in bytes per second) at which to transmit data (sending mode only). ‘0’ means send as fast as the transport allows.
[num] : Total number of packets to send before disconnecting and terminate. ‘0’ means send forever until break by the user.
[hostname] : Name of host to lookup, assume local host if not specified.
Statistics Display:
Suggested one-line format: “Elapsed [120s] Pkts [1234] Lost [5, 0.1%] Rate [3.5Mbps] Jitter [5.2ms]”
(a) Elapsed: time elapsed for this session
(b) Pkts: accumulated number of packets/message received
(c) Lost: accumulated number of packets lost and the packet loss rate in percentage, i.e., Lost/Pkts*100%
(d) Rate: average throughput since beginning of session
(e) Jitter: average delay jitter of inter-packet transmission/reception time
Except for (b) which is computed and displayed only by the receiver, all other statistics are to be computed and displayed by both the sender and the receiver.
C. Experiments
1.	Measure the throughput, delay jitter, and CPU utilization versus packet size (2n bytes, n=6, 8, 10, 12, 13, 14, 15) using the TCP protocol.
2.	Repeat (1) using the UDP protocol. Plot also the packet loss rate versus packet size.
 
Project #2 – NetProbe Client-Server Edition
A. Overview and Specifications
In this project, we extend the NetProbe application developed in Project #1 into two applications, namely a client and a server application. 
(a)	The NetProbe Server will listen for incoming connections originating from a NetProbe Client, receiving the operating parameters (e.g., packet size, transmission rate, etc.) and then begin transmitting data to the NetProbe Client. Additionally, the NetProbe Server will be extended to become a concurrent server and thus, will be able to establish more than one NetProbe Sessions with multiple NetProbe Clients. The concurrent server model is to be implemented using multi-threading. No GUI is required for the NetProbe Server.
(b)	Both NetProbe Server and NetProbe Client are to be written such that they are portable for compilation and execution in both Linux and Windows, using a common set of source codes.

B. Experiments
1.	Compare the achievable throughput, loss, and CPU utilization in Linux and Windows platforms.
2.	Compare the achievable throughput versus the number of client sessions, under both TCP and UDP.
3.	(Optional) Test the achievable throughput over a wireless network (e.g., Wi-Fi), under both TCP and UDP.

C. Bonus
The submitted NetProbe will be subject to a performance test under a specific test setup by the tutor. The resultant performance of all submitted NetProbes will be ranked and a bonus will be given to students according to their NetProbe’s ranked performance. A tentative bonus will be equal to 5(Ni)/N, where N is the total number of submitted projects and i is the rank of the project.

 
Project #3  NetProbe Web Server
A. Overview
This project extends the NetProbe Server in Project #2 to support basic HTTP protocol processing such that the NetProbe Server can serve HTTP GET requests from HTTP clients (e.g., browser).
Only basic HTTP processing needs to be implemented:
- Only HTTP GET request needs to be supported;
- No need to support advanced HTTP options such as byte-range requests, persistent connection, etc.
The concurrent server is to be implemented using the same multi-threading model as in Project #2, using one-thread per connection. 
The threading model is to be implemented using two methods: (a) on-demand thread creation; and (b) thread-pool models. In model (a) a new thread is created for each incoming connection accepted, and destroyed after the connection ends. In model (b) a preconfigured fixed number of threads are pre-created and assigned to incoming connections. Upon connection completion the thread is returned to the pool rather than destroyed.
B. Experiments
To evaluate HTTP performance it is recommended to use httperf available at http://sourceforge.net/projects/httperf/
1.	Compare the HTTP server response time under the two different threading models.
2.	Compare the HTTP server response time under Linux and Windows (for running the server).
3.	Determine the maximum number of concurrent HTTP sessions that can be supported, record the machine configuration (memory size, etc.), and determine the limiting bottleneck for serving even more connections.
 
Project #4  NetProbe Cloud Storage Application
A. Overview
This project integrates and extends the implementations in Project #2 and #3 into a cloud storage application. The cloud server extends the Web Server in Project #3 while the cloud client extends the NetProbe Client in Project #2 to make HTTP POST request to the cloud server.
The cloud client runs under Windows as a system service. It maintains a local directory configured by the user where all files in the directory are synchronized with the cloud server. The cloud server runs under Linux and maintains a local directory for where the files mirrored the ones in the user’s Windows host running the cloud client.
The application must implement the following functions:
(i)	Allow the user to specify the Windows directory for the local file. The local directory can be an existing directory or a new one.
(ii)	Whenever the cloud client is started it will establish connection with the cloud server and then synchronize the files by comparing the local files with the ones in the cloud server. If the local file is a newer version or a new one, then it will be uploaded to the cloud server. If the cloud server’s version is newer or a file does not exist in the local file system, then it will be downloaded back to the client Windows host.
(iii)	While the cloud client is running it will monitor the local and remote cloud storage by periodically waking up to check if there is any file updates, and if so, perform synchronization accordingly. The cloud client must implement proper file locking to avoid data corruption or race conditions.
(iv)	The cloud-storage application protocol is to be implemented on top of HTTP, where the cloud server will appear to be an HTTP server and the client will appear to be an HTTP client. To enable uploading of data to the server the application can make use of HTTP POST for data upload.
(v)	As HTTP is stateless the application will need to maintain state information using either HTTP Cookies or URL rewrites.
The following features are optional and may score bonus marks:
(vi)	For file synchronization, instead of uploading/downloading the entire file one can just send the delta between the local file and the remote one. This cuts down bandwidth consumption and improves response time. This could be implemented using HTTP byte-range retrieval and also chunked encoding.
(vii)	Implement persistent HTTP connection to improve performance.
(viii)	Implement request pipelining and see if that can improve performance.
(ix)	Implement parallel HTTP connections for file upload/download to improve throughput performance.
(x)	Implement user account management to support cloud storage for multiple users.
(xi)	Implement a web-based interface for users to browse and download/upload files via a web browser instead of the cloud client.
