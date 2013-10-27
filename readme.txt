/* CSci4061 F2013 Assignment 2
* date: 10/29/2013
* name: Devon Grandahl, Alex Cook
* id: 4260296, 4123940 */

1. This program is meant to be a simple web-browser. It displays a user interface for creating URL-rendering tabs, assigning a url for a tab to render, and closing the tab/killing the process.

2. To compile this program, type 'make' into the prompt while in the directory containing this file and the wrapper files. This will generate an executable named 'browser'.

3. To use this program from the shell, type './browser'. 

<<<<<<< HEAD
4. What our program does:
	a. Using forks and pipes, we create processes that simulate a simple multi-process browser. 
	b. A “Router” process manages communication between the Controller window and the tab windows. As well as the Controller creation and tab creation. 
	c. The Router is constantly checking for messages from the tabs and the Controller to handle. 
	d. When a tab or the Controller have their window closed, the Router manages killing the processes and reaping the children. 
	e. If a tab is exited, the next opened tab will have the number of the closed tab

	More Detailed: When the user runs the program, a router process is started and it creates a controller child process. From here, messages are sent via pipes to and from the router and it's children when A) The + button is pushed in the controller, which sends a 'CREATE_TAB' message to the router and opens a new url_browser at the lowest free tab number. B) A url and tab index is entered at the top of the router and the user hits 'enter'. This sends a 'NEW_URI_ENTERED' message to the router, which relays the message to the browser process (based on the tab index) and calls a wrapper function to render the page. C) The 'X' button at the top of a tab is clicked and a 'KILL_TAB' function is sent to the router. This kills the process that sent the message. If the controller is the process that sent the message, all tabs are killed along with the controller and the program terminates.

5. For this program we assume that the user will never need to have more than 15 tabs open simultaneously.

6. Error Handling Strategies:
	a. In the case of system call errors, we print an error message and exit.
	b. If user errors are experienced (incorrect tab number, too many tabs, etc) the program does not exit. The user is prompted in the terminal to make a correction. 

