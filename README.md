# UDP-socket-Programming
The goal of this project is to learn how to use UDP sockets
and to implement a client-server system for le-exchange.
The best way to get started is by learning about UDP
sockets and the manner in which they are used. There
are numerous textbooks as well as web sources to get you
started. Chapter 2 of your course textbook has useful
material on this topic as well. Spend a couple of days on
reading and understanding the basics of UDP sockets. It
will probably serve you better if you rst get your feet wet
by implementing a simple UDP socket between two ma-
chines in a client-server conguration and transfer a few
bytes of data. You can add functionality and aesthetics
later.
The programs must meet the following use-case require-
ments:
1. A server process (let us call it the data server) lis-
tens on UDP port 7777 on a machine of your choice,
waiting for a client to send it UDP data segments.
2. Next, a client process starts execution and gets the
IP address of the remote server, which you can supply
manually from the terminal window, or from a stored
le. The client process may run on the same machine
as the server, or on a dierent machine on a dierent
network.
3. The client then proceeds to transfer ten les to the
server as follows:
A random le (from ten available ones) is selected,
and, from this le, a random number of lines (1 - 3),
starting with line #1 are selected for transmission to
the server. Thus, for example, lines #1 and #2 from
le #5 may be transmitted rst. After those lines are
transmitted, another le is selected at random, and a
random number of lines (1 - 3) are again transmitted.
Thus, for instance, lines #1, #2 and #3 from le #7
may be transmitted, and so on.
An example transmission schedule may look like this:
1. From le #5: lines #1, #2.
2. From le #7: lines #1, #2, #3.
3. From le #1: line #1.
4. From le #10: lines #1, #2, #3.
5. From le #4: line #1.
6. From le #7: lines #4, #5.
7. From le #1: lines #2, #3, #4.
8. From le #2: lines #1, #2.
9. From le #7: line #6.
10. From le #3: line #1.
11. From le #4: lines #2, #3.
etc.
Each line is sent using a separate sendto() socket call
until all of the ten les are completely transmitted.
No line duplicates are sent unless the originals are
lost. Ultimately, when all lines from all ten les are
transmitted and received, the client-to-server trans-
missions are over.
The server does not have any a priori information
about the client's transmission schedule. Therefore,
the client needs to somehow inform the server of the
lename, the total number of lines in that le, and
the line number associated with each transmitted line
of the le. Doing so enables the server to correctly re-
construct the les from the transmissions. Segments
may be lost, damaged, or reordered by UDP, so be
sure to take these possibilities into account, and re-
transmit any lost or damaged segments, if needed.
4. Once the client nishes all transmissions, the server
concatenates all les into a single large le and
sends it back to the client, with all les and (lines
within those les) in order. Again, this being UDP,
lost/damaged/out-of-order deliveries are possible, so
be sure to x any problems before saving the con-
catenated le on the client machine.
