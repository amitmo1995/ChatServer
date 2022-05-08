# ChatServer
Chat server Using C language

Personal details : 
Name : Amit Moshe
Email: amitmo1995@gmail.com

Exercise name : chat server

Submitted files :
chatServer.c : the chat server simulator code.
READMY : the readme of the exercise.

Remarks :
I used the head of the pool linked list to contain the server wellcome socket.

I created the var : iterator to check that i do not run more then needed iterations , means i checked at each iteratin if iterator is equals to the nReady that select function returned in each iteration.

My program does not use threads at all but uses select function.

This project is simulates a chat server that handles conections of clients and manage the messeges between the clients.

to run the chat server use the command : gcc chatServer.c -o chatServer
                                         ./chatServer <port-number>
and then start telnets (localhost) and connect to the port-number , now you can communicate between the telnets ... 
Have fun!
