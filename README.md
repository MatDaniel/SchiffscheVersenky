# SchiffscheVersenky

So, about this project, uhm where do we start...

This project is a rather shity implementation of a opengl driven 3d gameengine, that doesnt even fuckign support physics, nor animations.\
Like for real, we dont even have entities nor an entity manager.\
This shit runs on a single thread using nonblocking sockets with horribly over engineered netcode (yes its multiplexed).\
The code is kinda well structured, well at least until we got to the point of making the renderer and the server talk with eachother.
To say the least, this is probably a prime example of how to fucking not implement a game engine,
and probably overkill for what it was intended to do.
There are also other heavily cut corner in this project, as for example if one player disconnects th server will shut down,\
but that ain't even the best part.. the server in its current state is unable to disconnect clients..\
so as a quick fix i just shutdown the server as a whole and exit the application resulting in CONNECTRESET,\
yes this should absolutely be fixed and i may or may not (that means i will not) fix this issue in the far future (after i died probably).

What could we have done better?, well for starts maybe not start such a project in the first place, we had other options,..\
options that didnt require us to write 10k lines of fucking rushed and unpolished c++ shitcode.

Where does this project belong now?, to be honest with you, i really dont know, but probably close to the trash bin please.

Sincerely, Lima and Daniel.

## How to build:

1. Recursively clone from github.com
2. Download glad binaries in realease and put it in "3rdparty/glad_rel"
3. Download glfw binaries, do the same as above ("3rdparty/glfw-3.3.5.bin.WIN64")
4. Open the solution in fucking visual studio 2022
5. Press ctrl+alt+f7 and hope it fucking builds for you
6. No, no fucking profit here, only lost time

As an alternative to the above instructions: just dont fucking build it, idiot.\
We have releases for exatcly that fucking reason

## FAQ (short for FUCK and... something else idk):

Q: "Does this even work correctly?"\
A: I hope it fucking does

Q: "Does this contain bugs?"\
A: Yes, yes it does, infact a metric fuck ton of bugs that hopefully never emerge from the ground

Q: "When i run this my firewall asks me for permissions, should i be worried?"\
A: The reason for this is quite simple, as this uses the winsock api it needs access to the web to be able to bind tcp connections,
   this is therefore completely expected behaviour, just whitelist the program.

Q: "Can this executable put my computer at risk?"\
A: Ofcourse it can, it was written by monkeys, and honestly i dont wanna know how many RCE/ACE bugs are in this thing,
   also, there might be a present in this (just kidding,.. or am i ?)

Q: "How to start the server"\
A: Open a terminal (whatever you use on windows, for example cmd.exe), find the executable and give it "s" as an argument,\
   you and also give it optionally a port number behing that, that the server should bind to, note: the port has to be open.

Q: "Can i play this online with my friends?"\
A: Yea sure, just make sure to port forward the port used by the server on the machine running the server application,
   idk why you would want to play this tho...

Q: "Can i run this on linxu/macos?"\
A: NO, fuck you, im not porting this to other os's especially not fucking unix system after the horrible agonizing pain i had to go through,
   trying to make this shitty server run on unix shit sockets.
