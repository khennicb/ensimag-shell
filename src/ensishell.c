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


// compte le numero de la commande
int num_comm = 0;
// Cree un tableau pour contenir les pipes
//int tabPipe[?][2];

void execMultiPipe(struct cmdline *l){
	int i;
	for (i=0; l->seq[i]!=0; i++){}
	int nb_comm = i;
	printf("%d\n", nb_comm);

	// Replir le tableau contenant les pipes
	//int tabPipe[nb_comm][2];

	// Boucle pour creer autant de pipe que necessaire
	for (i=0; l->seq[i+1]!=0; i++){ // Inutile de creer un pipe pour le dernier fils
		int tuyau[2];
		if (pipe(tuyau) ==  -1)
		{
			l->err = "Error during pipe"; return;
		}
		// branchement des pipes
		tabPipe[i][0]=mon_tube[0];//On branche la lecture
    	tabPipe[i][1]=mon_tube[1];//On branche l'écriture
	}

	// boucle de fork pour creer les fils

	// ATTENTION : faire en sorte que les fils s'execute dans le bon ordre
	// IDEE : utiliser un mutex + compteur pour s'avoir quelle commande le fils reveille doit execter
	// utiliser sigaction ?


/*
stdin > commande_0 > === job->tubes[0]=== > commande_1 > ===
job->tubes[1] === > … > === job->tubes[n-1] === > commande_n > stdout

S'il faut éxécuter (n+1) commandes, il faut donc ouvrir (n) tubes
différents, dont les tableaux de "file descriptors" tubes[i] doivent
être stockés dans la structure job_t.

Si le fils n'exécute pas la première commande, (disons qu'il exécute la
commande commande_i) il va devoir brancher son stdin sur job->tubes[i-1][0].
Si le fils n'exécute pas la dernière commande, (disons qu'il exécute la
commande commande_i) il va devoir brancher son stdout sur job->tubes[i][1].
Dans tous les cas, chaque fils doit fermer les accès dont il ne se sert
pas aux tubes qui l'entourent.

Le père doit systématiquement fermer ses accès à chaque tube ouvert : il
ne s'en sert jamais.
*/
	if (num_comm == 0) { // On est le premier
		dup2(tabPipe[num_comm][1], STDOUT_FILENO);
	}else if(num_comm == nb_comm){ // On est le dernier
		dup2(tabPipe[num_comm-1][0], STDIN_FILENO);
	}else{
		dup2(tabPipe[num_comm-1][0], STDIN_FILENO);
      	dup2(tabPipe[num_comm][1], STDOUT_FILENO);
	}

	// On ferme tous les pipes
	for (i=0; l->seq[i+1]!=0; i++){ 
      	close(tabPipe[i-1][0]); 
      	close(tabPipe[i-1][1]); 
	}

	//On execute la commande ici
    int res_e = execvp(*(l->seq[num_comm]), l->seq[num_comm]);
    if (res_e==-1) {l->err = "Error during execvp"; return;}

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
				execMultiPipe(l);
				//execPipe(l);
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
