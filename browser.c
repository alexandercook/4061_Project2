/* CSci4061 F2013 Assignment 2
* date: 10/23/2013
* name: Devon Grandahl, Alex Cook
* id: 4260296, 4123940 */

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
	int tab_index = query_tab_id_for_request(entry, data) - 1;

	if(tab_index < 0)
	{
		printf("tab indexing error\n");
                return;
	}

	// Get the URL.
	char* uri = get_entered_uri(entry);

	// Now you get the URI from the controller
	child_req_to_parent new_uri;  //Declare new message
	if (!strncpy (new_uri.req.uri_req.uri, uri, 512)){
		printf("error copying url into struct\n");
	} //Add the new uri to the request struct
	new_uri.type = NEW_URI_ENTERED;
	new_uri.req.uri_req.render_in_tab = tab_index;
	
	if(!write(channel.child_to_parent_fd[1], &new_uri, sizeof(child_req_to_parent))){
							printf("Error writing to pipe\n");
						} //Write new uri request into pipe
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
	//child_req_to_parent new_req;

	// Users press + button on the control window. 
	// What is next?
	child_req_to_parent create_tab;
	
	create_tab.type = CREATE_TAB;
	create_tab.req.new_tab_req.tab_index = tab_index;
	
	if(!write(channel.child_to_parent_fd[1], &create_tab, sizeof(child_req_to_parent))){
		printf("Error writing to pipe\n");
	}
}

/*
 * Name:                run_control
 * Input arguments:     'comm_channel': Includes pipes to communctaion with Router process
 * Output arguments:    void
 * Function:            This function will make a CONTROLLER window and be blocked until the program terminate.
 */
int run_control(comm_channel comm)
{
	browser_window * b_window = NULL;
	//Create controler process
	if(create_browser(CONTROLLER_TAB, 0, G_CALLBACK(new_tab_created_cb), G_CALLBACK(uri_entered_cb), &b_window, comm)!=0){
		printf("Error creating controller\n");
	}

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
	int r, flags;
	//Create controler window
	if (create_browser(URL_RENDERING_TAB, nTabIndex, G_CALLBACK(new_tab_created_cb), G_CALLBACK(uri_entered_cb), &b_window, comm)!=0){
		printf("Error creating browser");
	}

	child_req_to_parent req;
	flags = fcntl(1 , F_GETFL, 0);
	if (fcntl(comm.parent_to_child_fd[0], F_SETFL, flags | O_NONBLOCK)==-1){
		perror("Error setting flags");
	} //non-blocking read of child
	
	while (1) 
	{
		process_single_gtk_event();
		usleep(1000);

		//Need to communicate with Router process here.
		if((r = read(comm.parent_to_child_fd[0], (void*) &req, sizeof(child_req_to_parent))) > 0){ 
				printf("Router to tab Message %d:\n", r);
				if(req.type == TAB_KILLED ){
					exit(1);
				}
				if(req.type == NEW_URI_ENTERED ){
					render_web_page_in_tab(req.req.uri_req.uri, b_window); //Call the actual render function
					
				}
			}
				

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
	int r, i, j, index = -1;
	child_req_to_parent chld_msg;
	
	if (pipe(controller.parent_to_child_fd)==-1){
		perror("Error creating pipe from parent to child\n");
		exit(-1);
	}
	if (pipe(controller.child_to_parent_fd)==-1){
		perror("Error creating pipe from child to parent\n");
		exit(-1);
	}
	for (i = 0; i < MAX_TABS; i++){
		openTab[i] = 0; //initialize all tabs to closed
	}
	
	if((pid = fork() ) < 0 ){
		perror("Failed to fork controller process.\n");
		exit(-1);
	}
	
	if (pid == 0){ //child
		if(close(controller.parent_to_child_fd[1])==-1){
		perror("Error closing parent write end\n");
		exit(-1);
	} //close parent write
		if(close(controller.child_to_parent_fd[0])==-1){
		perror("Error closing child read end\n");
		exit(-1);
	} //close read for child to parent
		run_control(controller);
	}
	else{ //parent
		if(close(controller.parent_to_child_fd[0])==-1){
		perror("Error closing child read end\n");
		exit(-1);
	} //close child read
		if(close(controller.child_to_parent_fd[1])==-1){
		perror("Error closing child write end\n");
		exit(-1);
	} //close write for child to parent
		flags = fcntl(1 , F_GETFL, 0); //not sure if this call is right
		if(fcntl(controller.child_to_parent_fd[0], F_SETFL, flags | O_NONBLOCK)==-1){
		perror("Error setting flags");
	} //non-blocking read of child
		while(1){
			if((r = read(controller.child_to_parent_fd[0], (void*) &chld_msg, sizeof(child_req_to_parent))) > 0){ 
				printf("The message %d:\n", r);
				
				//when + is pushed, handle create tab
				if(chld_msg.type == CREATE_TAB ){ // Assign index to lowest free tab
					for (j=0; j<=MAX_TABS; j++){
						if (openTab[j]==0){
							index=j;
							break;
						}
					}
					printf("index: %d\n" , index);
					
					if (index >= 0 && index < MAX_TABS && openTab[index] == 0){
						openTab[index] = 1; //now open and not valid for future open
						if(pipe(tab[index].parent_to_child_fd)==-1){
							perror("Error creating pipe from child to parent\n");
							exit(-1);
						}
						if(pipe(tab[index].child_to_parent_fd)==-1){
							perror("Error creating pipe from child to parent\n");
							exit(-1);
						}
						if(fcntl(tab[index].child_to_parent_fd[0], F_SETFL, flags | O_NONBLOCK)==-1){
							perror("Error setting flags");
						} //non-blocking read of child

						if(fork() == 0) { //then child
							if(close(tab[index].parent_to_child_fd[1])==-1){
								perror("Error closing parent write end\n");
								exit(-1);
							} //close parent write
							if(close(tab[index].child_to_parent_fd[0])==-1){
								perror("Error closing child read end\n");
								exit(-1);
							} //close read for child to parent
							run_url_browser(index+1 , tab[index]);
						}	
						else{ //parent
							if(close(tab[index].parent_to_child_fd[0])==-1){
								perror("Error closing child read end\n");
								exit(-1);
							} //close child read
							if(close(tab[index].child_to_parent_fd[1])==-1){
								perror("Error closing child write end\n");
								exit(-1);
							} //close write for child to parent
						}
					}
					else {
						perror("Invalid tab entered. Try again.\n New tab index: >=1, <=MAX_TABS(15), and not currently open.");
					}
				}
				else if(chld_msg.type == TAB_KILLED){
					for (i = 0; i < MAX_TABS; i++){
								if( openTab[i] == 1){
									printf("Killing all tabs...\n");
									if(!write(tab[i].parent_to_child_fd[1] , (void*) &chld_msg, sizeof(child_req_to_parent))){
										printf("Error writing to pipe\n");
									} //send kill to child
									if(close(tab[i].child_to_parent_fd[0])==-1){
										perror("Error closing child read end\n");
										exit(-1);
									} //close tab pipes
									if(close(tab[i].parent_to_child_fd[1]));
										openTab[i] = 0; //show as closed tab
									}	

					}
					exit(1);
				}
				else if(chld_msg.type == NEW_URI_ENTERED){
						if(!write(tab[chld_msg.req.uri_req.render_in_tab].parent_to_child_fd[1] , (void*) &chld_msg, sizeof(child_req_to_parent))){
							printf("Error writing to pipe\n");
						} //send new_uri_entered to child
				}
				else{
					perror("Invalid Message from controller.\n Expecting 'new tab', 'new uri', or 'tab kill'.");
				}
			}
			
			for (i = 0; i < MAX_TABS; i++){
				if( openTab[i] == 1 && (r = read(tab[i].child_to_parent_fd[0], 
										(void*) &chld_msg, 
										sizeof(child_req_to_parent))) > 0){
					printf("Tab Message %d:\n", r);
					if(chld_msg.type == TAB_KILLED){
						printf("Kill Message from wrapper!");
						if(!write(tab[i].parent_to_child_fd[1] , (void*) &chld_msg, sizeof(child_req_to_parent))){
							printf("Error writing to pipe\n");
						} //send kill to child
						if(close(tab[i].child_to_parent_fd[0])==-1){
							perror("Error closing child read end\n");
							exit(-1);
						} //close tab pipes
						if(close(tab[i].parent_to_child_fd[1])==-1){
							perror("Error closing child read end\n");
							exit(-1);
						}
						openTab[i] = 0; //show as closed tab
					}
				}
			}
			while(waitpid(-1, NULL, WNOHANG) > 0){
				printf("Just reaped a child.\n");//reap all children
			}
			usleep(10000);
		}
	}
	
	//Controller was closed, so close all tabs and then exit


	return 0;
}

