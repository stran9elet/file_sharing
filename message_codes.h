// the first thing a client should send a server should be a message code, depending on which the server will reply
#define MSG_GET_ID 0 // to get an id from a boot node after initial startup
#define MSG_FIND_NODE 1 // to find the k closest nodes known by a server, to a given hash
#define MSG_PING 2 // to check if a node is online
#define MSG_PONG 3 // response to ping
#define MSG_STORE 4 // to store the key-value pair sent in selected server
#define MSG_LOAD 5 // to get a stored key-value pair from a selected server

#define MSG_INSERT 6 // to insert a key-value pair into network
#define MSG_GET 7 // to get a key-value pair from the network

#define MSG_REDISTRIBUTE 8 // sent by a latest joined node to all its closest k nodes, to rehash their hash keys, and send some keys to me