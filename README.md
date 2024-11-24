Efficient file sharing software using p2p.

Suppose iitb safe app software for sharing quizzes at the same time, or say the vlab software for sharing a whole lab image file, in a huge class, say cs101 having say 1000 students.

The teacher says students to download a file of say 5 GB. Assume IITB has 1 web server running at a 1GB/s bandwith connection. Now, total data that needs to be sent to 1000 students  is 5000 GB, and it would take about 5000 seconds =  about 83 minutes to transmit that much data from the server.

Hence in a simple client-server architecture, we would need 83 minutes to send that lab image file to all the students.

Whereas if we modify that vlab code so that each student acts as a server as well, not only a client, and can share file to his peers as well, we can exponentially reduce this time required to share this file.

The first node joining the ring becomes the bootstrap node. It has additional responsibilities like approving or rejecting new nodes joining the network, and assigning a GUID to them.

However, there are also trade-offs to consider:

* **Single Point of Failure** : While the system
  was built on the premise of decentralization, the coordinator failing
  can impact new nodes joining or overall message routing efficiency until
  a new coordinator is elected.
* **Increased Load on Coordinator** : The
  coordinator might experience higher load compared to other nodes due to
  managing join requests and potentially additional tasks.

Additional things to add-

Encrypt messages. Suppose A wants to send a private message to B. We want to ensure 3 things-

1. Confidentiality- Eve shouldn't be able to read the message
2. Integrity- Any modifications to the messsage should be detected.
3. Authenticity- Ensure that the message was sent by A and not Malary faking to be A.

For providing these, I can use the SSL/TLS protocol, same as how its used in client-server model. The only difference is that now, the server certificate needs to be generated for each peer, and we don't need a well-established CA to generate the certificate. We can just keep our own one centeral server which doesn't take part in file transfers, but its only role is to generate certificate for each new peer joining the network.

Implementing a trackerless version of bittorrent

The server is just a peer only, which serves a torrent file to any peer which tries to contact it.

But it is also like a permanent seeder in the network which will always stay up no matter what.

also its the bootstrap peer

When a new node joins the network, how does it fill its routing table?

#1

The process of joining a Kademlia network requires discovery of only one peer, whereby the node then broadcasts its appearance. The initiator then collects the NodeID from each response and adds it to its own peer table. (This is where the term ‘distributed hash table’ comes from.)

#2

It first contacts the bootstrap node with FIND_NODE, giving it its own node id. This results in the bootstrap node in returning the k nearest nodes to the given node id which are already a part of the network, from his routing table.

also, bootstrap node is the one giving id to new node, after making sure a node with a given id doesn't already exist

We can recursively call FIND_NODE on those returned nodes to find the globally actually k nearest nodes to the newly added node.

Also, alongside this, we can keep on populating our routing table with the nodes that we are finding along the way.

(My thought- we can ask any closest node for its routing table as well after joining, and fill up our own routing table till the prefix we and our friend had in common, say we are node 1111, and one of our nearest node was 1101. We asked for his routing table, and filled our own routing table's entries for subtrees starting with 0 and 10, because after that he is storing 111 whereas we need 110.)

(Alternatively, we can one by one fill our routing table using bootstrap node as well. say we are 1111, simply ask the bootstrap node for k nodes closest to id 0 and 10 and 110 and 1110)


currently, when a new node joins a network, it broadcasts a hello message via the bootstrap to whole of its routing table, giving new node's id as sender, to which all other nodes in network reply with hello message back, which helps the current node fill its routing table


currently, it only stores a given hash key at one node. I wanted to store them in k closest nodes, and i also wrote a function for that, but sadly that function was not properly working, and i did not have the time to debug it.


But after filling its routing table, how am i going to redistribute the keys? Since some keys which are now closest to this newly added node are currently being stored by some of its neighbours

Suppose if a new node is added to the network, due to which a key held by a given old node is now closest to this new node instead. Then how to handle it?

When a new node joins the network, it announces its k-nearest neighbors about himself. So that the k nearest neighbours can send all their stored keys to this new node. This node then checks which k keys are closest to her, and stores them!

Also, each node periodically rechecks which keys it is responsible for storing. It does that by executing FIND_NODE to find the global k-closest nodes to a given key. If he gets his own node id in response, then he continues to store that key, as well as issues a store instruction for other k closest nodes to store that key as well. otherwise it drops tha key. (This also helps with redistribution of the keys if a node abruptly left the network without telling its neighbors). It would also help if this node could notify the actual k nodes then, so they don't have to check whether they need to store the key or not.

how does kademlia handle collisions of nodes

But for now, for keeping things simple, when a new node joins, it is gonna broadcast its id and ip in the whole network.

Tradeoff- iterating over whole routing table instead of just 1 bucket to improve accuracy but decrease speed
