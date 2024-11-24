// the first thing a client should send a server should be a message code, depending on which the server will reply
#define MSG_GET_ID 0 // to get an id from a boot node after initial startup
#define MSG_FIND_NODE 1 // to find the k closest nodes known by a server, to a given hash
#define MSG_STORE 2 // to store the key-value pair sent in bencoded format
#define MSG_FIND_VALUE 3 // to get a stored key-value pair 
#define MSG_PING 4 // to check if a node is online
#define MSG_REHASH 5 // sent by a latest joined node to all its closest k nodes, to rehash their hash keys, and send some keys to me