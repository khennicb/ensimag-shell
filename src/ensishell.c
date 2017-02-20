/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h> 
#include <fcntl.h>

#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

// background process counter
static int nb_bg_process = 0;
struct pid_cell *bg_process_list;

struct pid_cell {
	int pid;
	char *name;
	struct pid_cell *next;
};


#if USE_GUILE == 1
#include <libguile.h>



int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */


	 // TODO : A tester (j'arrive pas a tester sur mon ordi, il faut avoir guile)
	printf("blop\n");

	/* parsecmd free line and set it up to 0 */
	l = parsecmd( & line);

	/* If input stream closed, normal termination */
	if (!l) {
		terminate(0);
	}

	// execute la ligne d'instruction
	execInst(l);


	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}



static void unset_handler(int pid){

	struct pid_cell *ptr = bg_process_list;
	struct pid_cell *before_ptr = NULL;

	while(ptr != NULL && ptr->pid != pid) {
		before_ptr = ptr;
		ptr = ptr->next;
	}

	if(ptr != NULL) {
		nb_bg_process--;
		printf("process %d just died. \n", pid);
		if(before_ptr != NULL) {
			before_ptr->next = ptr->next;
		} else {
			if(ptr->next == NULL) {
				bg_process_list = NULL;
			} else {
				bg_process_list = ptr->next;
			}
		}
		free(ptr->name);
		free(ptr);
		ptr=NULL;

	}


	if (bg_process_list == NULL) {
	    // unset the handler
   		signal(SIGCHLD, SIG_DFL);
	}

}

// Handler SIGCHLD
static void child_handler(int sig)
{
    int status;
    int pid;
    /* EEEEXTEERMINAAATE! */
	pid = waitpid(-1, &status, WNOHANG);
    unset_handler(pid);
}


static void set_handler(int pid, char* name){

	if (nb_bg_process == 0) {
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;
		sa.sa_handler = child_handler;

		sigaction(SIGCHLD, &sa, NULL);
	}

	nb_bg_process++;

	struct pid_cell *new_cell = (struct pid_cell*) malloc(sizeof(struct pid_cell));
	new_cell->pid = pid;
	new_cell->name = malloc(sizeof(char) * strlen(name) + 1);
	strcpy(new_cell->name, name);
	new_cell->next = bg_process_list;
	bg_process_list = new_cell;

}

void execJobs(){
	struct pid_cell *ptr = bg_process_list;

	while(ptr != NULL){
		printf("%d %s\n", ptr->pid, ptr->name);
		ptr = ptr->next;
	}
}

void execPipe(struct cmdline *l){
	int res;
	int tuyau[2];
	pipe(tuyau);

	if((res=fork())==0) {
		dup2(tuyau[0], 0);
		close(tuyau[1]); close(tuyau[0]);
		execvp(*(l->seq[1]), l->seq[1]);
	}
	dup2(tuyau[1], 1);
	close(tuyau[0]); close(tuyau[1]);
	execvp(*(l->seq[0]), l->seq[0]);
}

void execIn(struct cmdline *l){
	int file = open(l->in, O_RDONLY);

	dup2(file, 0);
	close(file);
}

void execOut(struct cmdline *l){
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int file = open(l->out, O_WRONLY | O_CREAT | O_TRUNC, mode);

	dup2(file, 1);
	close(file);
}

void execInst(struct cmdline *l){
	pid_t pid;
	switch( pid = fork() ) {
		case -1:
			l->err = "Error during fork";
			// perror("fork:");
			break;
		case 0:
			if(l->in){
				execIn(l);
			}

			if (l->out)	{
				execOut(l);
			}

			if (strcmp(*(l->seq[0]), "jobs") == 0) {
				execJobs();
			} else if (l->seq[1]!=0) {
				execPipe(l);
			} else {
				execvp(*(l->seq[0]), l->seq[0]);
			}
			break;
		default:
		{ 
			if (l->bg==0) {
				int status;
				waitpid(pid, &status, 0);
			} else {
				set_handler(pid, *(l->seq[0]));
			}
			// TODO : le pere doit-il faire autre chose ? 
			break;
		}
	}
}


int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);
        printf("%d\n", USE_GUILE);
#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {
		struct cmdline *l;
		char *line=0;
		int i, j;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
		  
			terminate(0);
		}
		
		// execute la ligne d'instruction
		execInst(l);

		
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");

		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
                        for (j=0; cmd[j]!=0; j++) {
                                printf("'%s' ", cmd[j]);
                        }
			printf("\n");
		}
	}

}
