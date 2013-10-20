#include "wrapper.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <gtk/gtk.h>

#define MAX_TABS 15

/*
 * Name:		uri_entered_cb
 * Input arguments:'entry'-address bar where the url was entered
 *			 'data'-auxiliary data sent along with the event
 * Output arguments:void
 * Function:	When the user hits the enter after entering the url
 *			in the address bar, 'activate' event is generated
 *			for the Widget Entry, for which 'uri_entered_cb'
 *			callback is called. Controller-tab captures this event
 *			and sends the browsing request to the router(/parent)
 *			process.
 */
void uri_entered_cb(GtkWidget* entry, gpointer data)
{
	if(data == NULL)
	{	
		return;
	}

	browser_window* b_window = (browser_window*)data;
	comm_channel channel = b_window->channel;
	
	// Get the tab index where the URL is to be rendered
	int tab_index = query_tab_id_for_request(entry, data);

	if(tab_index < 0)
	{
		printf("error\n");
                return;
	}

	// Get the URL.
	char* uri = get_entered_uri(entry);

	// Now you get the URI from the controller.
	// What is next? 
	// Insert code here!!
}

/*
 * Name:		new_tab_created_cb
 * Input arguments:	'button' - whose click generated this callback
 *			'data' - auxillary data passed along for handling
 *			this event.
 * Output arguments:    void
 * Function:		This is the callback function for the 'create_new_tab'
 *			event which is generated when the user clicks the '+'
 *			button in the controller-tab. The controller-tab
 *			redirects the request to the parent (/router) process
 *			which then creates a new child process for creating
 *			and managing this new tab.
 */ 
void new_tab_created_cb(GtkButton *button, gpointer data)
{
	if(data == NULL)
	{
		return;
	}

 	int tab_index = ((browser_window*)data)->tab_index; //this is always zero...
 	if(tab_index != 0){
		printf("Tab index of new tab was not zero!\n");
	}
	
	//This channel have pipes to communicate with router. 
	comm_channel channel = ((browser_window*)data)->channel;

	// Create a new request of type CREATE_TAB
	child_req_to_parent new_req;

	// Users press + button on the control window. 
	// What is next?
	child_req_to_parent create_tab;
	create_tab.type = CREATE_TAB;
	create_tab.req.new_tab_req.tab_index = tab_index;
	
	write(channel.child_to_parent_fd[1], &create_tab, sizeof(child_req_to_parent));
}

/*
 * Name:                run_control
 * Input arguments:     'comm_channel': Includes pipes to communctaion with Router process
 * Output arguments:    void
 * Function:            This function will make a CONTROLLER window and be blocked until the program terminate.
 */
int run_control(comm_channel comm)
{	//char* msg = "Hello from control!";
	browser_window * b_window = NULL;
	
	//write(comm.child_to_parent_fd[1], msg , MAX_MESSAGE);
	
	//Create controler process
	create_browser(CONTROLLER_TAB, 0, G_CALLBACK(new_tab_created_cb), G_CALLBACK(uri_entered_cb), &b_window, comm);

	//go into infinite loop.
	show_browser();
	return 0;
}

/*
* Name:                 run_url_browser
* Input arguments:      'nTabIndex': URL-RENDERING tab index
                        'comm_channel': Includes pipes to communctaion with Router process
* Output arguments:     void
* Function:             This function will make a URL-RENDRERING tab Note.
*                       You need to use below functions to handle tab event. 
*                       1. process_all_gtk_events();
                        2. process_single_gtk_event();
*/
int run_url_browser(int nTabIndex, comm_channel comm)
{
	browser_window * b_window = NULL;
	
	//Create controler window
	create_browser(URL_RENDERING_TAB, nTabIndex, G_CALLBACK(new_tab_created_cb), G_CALLBACK(uri_entered_cb), &b_window, comm);

	child_req_to_parent req;
	
	while (1) 
	{
		process_single_gtk_event();
		usleep(1000);

		//Need to communicate with Router process here.
		//Insert code here!!

	}

	return 0;
}


int main()
{
	//This is Router process
	//Make a controller and URL-RENDERING tab when user request it. 
	//With pipes, this process should communicate with controller and tabs.
	pid_t pid;
	comm_channel controller; //controller communications
	comm_channel tab[MAX_TABS];
	short openTab[MAX_TABS]; //keep track of open/closed tabs
	int flags;
	//char msg[MAX_MESSAGE];
	int r, i, index = -1;
	child_req_to_parent chld_msg;
	
	pipe(controller.parent_to_child_fd);
	pipe(controller.child_to_parent_fd);
	
	for (i = 0; i < MAX_TABS; i++){
		openTab[i] = 0; //initialize all tabs to closed
	}
	
	if((pid = fork() ) < 0 ){
		perror("Failed to fork controller process.\n");
		exit(-1);
	}
	
	if (pid == 0){ //child
		close(controller.parent_to_child_fd[1]); //close parent write
		close(controller.child_to_parent_fd[0]); //close read for child to parent
		run_control(controller);
	}
	else{ //parent
		close(controller.parent_to_child_fd[0]); //close child read
		close(controller.child_to_parent_fd[1]); //close write for child to parent
		flags = fcntl(1 , F_GETFL, 0); //not sure if this call is right
		fcntl(controller.child_to_parent_fd[0], F_SETFL, flags | O_NONBLOCK); //non-blocking read of child
		while(1){
			
			if((r = read(controller.child_to_parent_fd[0], (void*) &chld_msg, sizeof(child_req_to_parent))) > 0){ 
				printf("The message %d:\n", r);
				
				//when + is pushed, handle create tab
				if(chld_msg.type == CREATE_TAB ){
					index++; //= chld_msg.req.new_tab_req.tab_index;
					printf("index: %d\n" , index);
					
					if (index >= 0 && index < 15 && openTab[index] == 0){
						pipe(tab[index].parent_to_child_fd);
						pipe(tab[index].child_to_parent_fd);
						if(fork() == 0) { //then child
							close(tab[index].parent_to_child_fd[1]); //close parent write
							close(tab[index].child_to_parent_fd[0]); //close read for child to parent
							run_url_browser(index+1 , tab[index]);
						}	
						else{ //parent
							close(tab[index].parent_to_child_fd[0]); //close child read
							close(tab[index].child_to_parent_fd[1]); //close write for child to parent
						}
					}
					else {
						perror("Invalid tab entered. Try again.\n New tab index: >=1, <=15, and not currently open.");
					}
				}	
			}
			usleep(10000);
		}
	}
	printf("Please read the instruction and comments on source code provided for the project 2\n");

	return 0;
}

