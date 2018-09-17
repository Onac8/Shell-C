//JONATHAN CANO PICAZO -- Shell C

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>

//CONSTANTES
#define MAX 20
#define MAXLINE 4*1024

struct cmd_type{
	char* argv[MAX];
	int fd_r;
	int fd_wr;
};

typedef struct cmd_type cmdt;

struct input_type{
	char buf[MAXLINE];
	cmdt command[MAX];
	char* file_path_r;
	char* file_path_wr;
	int fd_pipe_aux;
  int counter;
	int back;
};

typedef struct input_type input;


//READ CMD----------------------------------------------------------------------
//Lee y almacena comandos+args o paths de ficheros (caso ><)--------------------
int read_cmd (input *in, char *cmd){
  char* token;
	char* token_aux;
	char* token_aux2;
	char *cmd_aux;
  int i, cont;

  //OPCIONAL --> COMPROBAR $, =(v.e.), o %
  cont = in->counter;
  token = strtok_r(cmd, " \n\t\r", &cmd_aux);

  i = 0;
  while (token != NULL){
		token_aux = strchr(token,'<');
		token_aux2 = strchr(token,'>');
    if (token_aux != NULL){ //"<"
			if (strcmp(token_aux, "<") != 0){
				in->file_path_r = token_aux+1; //cmd "</tmp/fich1 >..."
				token = strtok_r(NULL, " \n\t\r", &cmd_aux);
			}else{
				token = strtok_r(NULL, " \n\t\r", &cmd_aux);
				in->file_path_r = token; //cmd "< /tmp/fich1 >..."
				token = strtok_r(NULL, " \n\t\r", &cmd_aux);
			}
  	}else if (token_aux2 != NULL){ //">"
			if (strcmp(token_aux2, ">") != 0){
				in->file_path_wr = token_aux2+1; //cmd ">/tmp/fich1 <..."
				token = strtok_r(NULL, " \n\t\r", &cmd_aux);
  		}else{
				token = strtok_r(NULL, " \n\t\r", &cmd_aux);
  			in->file_path_wr = token; //cmd "> /tmp/fich1 <..."
				token = strtok_r(NULL, " \n\t\r", &cmd_aux);
  		}
    }else{ //comando + args
      in->command[cont].argv[i]= token;
      token = strtok_r(NULL, " \n\t\r", &cmd_aux);
			i++;
    }
  }
  in->command[cont].argv[i]= NULL; //ultimo token = null (para execv)
	in->command[cont].fd_r = 0;
	in->command[cont].fd_wr = 0;
	return 0;
}



//READ LINE---------------------------------------------------------------------
//Lee linea a linea. Tokeniza por comandos (pipes)------------------------------
int read_line (input *in){
  char *token;
	char *token_aux;
	char *back;

  //Inicializamos struct
	in->counter = 0;
	in->file_path_r = NULL;
	in->file_path_wr = NULL;
	in->fd_pipe_aux = 0;
	in->back = 0;

  //Leemos linea a linea
	if (fgets(in->buf, sizeof in->buf, stdin) == NULL){
		if (feof(stdin) > 0){ //CASO FIN DE FICHERO
			return 1;
		}
		return -1; //CASO NULL ERROR
	}


	if(strlen(in->buf) >= MAXLINE-1){
		fprintf(stderr, "Line too long. Try again...\n");
		return 1;
	}

	//Caso solo ENTER
	if (strcmp(in->buf, "\n") == 0){
		return -1;
	}

  //read %, &, v.e, []
	// if (read_cmd(line)<0){
	// 	return -1;
	// }

	//Caso background
	back = strchr (in->buf, '&');
	if (back != NULL){
		in->back = 1;
		*back = ' ';
	}

  token= strtok_r (in->buf, "|", &token_aux); //tokenizamos primero los pipes

  while(token != NULL){
		if (in->counter >= MAX){ //tendre max_commands??
			fprintf (stderr, "Max commands = %d. Try again...\n", MAX);
			return -1;
		}
		if (read_cmd (in, token)<0){
			return -1;
		}
    token= strtok_r (NULL, "|", &token_aux);
    in->counter++;
	}
	return 0;
}



//CHANGE CURRENT WORK DIRECTORY-------------------------------------------------
int changecwd (char* argv[]){
	char* cwd;

	if (argv[1] == NULL){
		cwd = getenv("HOME");
	}else if (argv[2] != NULL){ //Solo 0 o 1 arg
		fprintf(stderr, "chd: Usage: {chd} {optional_path}");
		return -1;
	}else{
		cwd = argv[1];
	}
	if (chdir (cwd) < 0){
		warn ("chdir %s", cwd);
		return -1;
	}
	return 0;
}



//SEARCH_CMD_BUILT-IN-----------------------------------------------------------
//Si lo encuentra, devuelve 0 y su path absoluto; -1 si no encuentra------------
int search_cmd (char* cmd){
	// struct stat cmd_stat;
	char* p_aux;
	char path[MAXLINE];
	char buf[1024];

	strcpy(path, getenv("PATH"));
	p_aux = strtok(path, ":");
	while(p_aux != NULL){
		strcpy(buf, p_aux);
		strcat(buf, "/");
		strcat(buf, cmd);

		if (access(buf, X_OK)==0){
			strcpy(cmd,buf);
			p_aux = NULL;
			return 0;
		}
		p_aux = strtok(NULL, ":");
	}
	return -1;
}


//CREATE FILE DESCRIPTORS-------------------------------------------------------
//Crea descriptores de fichero fd_r y fd_wr-------------------------------------
int create_fds(input *in, int i){
	int pipes[2];

	//Abrimos fichero en caso de lectura (abrimos desde comando 0)
	if(i== 0 && in->file_path_r != NULL){
		in->command[i].fd_r = open (in->file_path_r, O_RDONLY);
		if (in->command[i].fd_r < 0){
			warn ("open %s", in->file_path_r);
			return -1;
		}
	}
	//Abrimos /dev/null si tenemos & y no existe file_path_r
	if (i== 0 && in->file_path_r == NULL && in->back != 0){
		in->command[i].fd_r = open ("/dev/null", O_RDONLY);
		if (in->command[i].fd_r < 0){
			warn ("open %s", in->file_path_r);
			return -1;
		}
	}
	//Creamos fich para lectura si estamos en ultimo cmd
	if (i== in->counter-1 && in->file_path_wr != NULL){
		in->command[i].fd_wr = creat (in->file_path_wr, 0664);
		if (in->command[i].fd_wr < 0){
			warn ("creat %s", in->file_path_wr);
			return -1;
		}
	}

	//PIPELINE -------------------------------------------------
	//Redirigimos salida/entrada si comandos > 1 (pipes)--------
	if (in->counter > 1){
		if (i!= 0){ //si i==0 -> no pipe del que leer -> fd_r = open()...
			in->command[i].fd_r = in->fd_pipe_aux;
		}
		if (i < in->counter-1){ //redirigimos si i!=ultimo_comando
			if (pipe (pipes)<0){
				warn ("pipe");
				return -1;
			}
			in->fd_pipe_aux = pipes[0]; //fd_pipe_aux de lectura
			in->command[i].fd_wr = pipes[1]; //nuevo fd_pipe de escritura
		}else{ //caso ultimo comando
			in->fd_pipe_aux = 0;
		}
	}
	//----------------------------------------------------------
	return 0;
}


//REDIRECTIONS------------------------------------------------------------------
//Redirecciona entrada/salida (caso ><) y cierra descriptores de fichero--------
int redirections (input *in, int i){

	//Cambio de descriptor (caso "<" o pipe(0): entrada estandar por fd_r)
	if (in->command[i].fd_r != 0){
		if(dup2 (in->command[i].fd_r, 0)<0){
			warn ("dup2 fd_read: %d", in->command[i].fd_r);
			return -1;
		}
		if (close (in->command[i].fd_r)<0){
			warn ("close fd_read: %d", in->command[i].fd_r);
			return -1;
		}
	}
	//Cambio de descriptor (caso ">" o pipe(1): salida estandar por fd_wr)
	if (in->command[i].fd_wr != 0){
		if (dup2(in->command[i].fd_wr, 1)<0){
			warn ("dup2 fd_write: %d", in->command[i].fd_r);
			return -1;
		}
		if (close(in->command[i].fd_wr)<0){
			warn ("close fd_write: %d", in->command[i].fd_r);
			return -1;
		}
	}

	//Cierro fd_pipe_aux si existe (caso pipes)
	if (in->fd_pipe_aux != 0){
		if (close(in->fd_pipe_aux)<0){
			warn ("close fd_pipe_aux: %d", in->fd_pipe_aux);
			return -1;
		}
	}

	return 0;
}


//EXECUTE LINE------------------------------------------------------------------
//Ejecuta cmd a cmd: built-in o chd si el nombre del comando es correcto--------
int execute_line (input *in){
	char cmd[1024];
	int i, pid, sts;

	i = 0;
	//Comando a comando
	while(i< in->counter){
		strncpy(cmd, in->command[i].argv[0], sizeof (cmd));
	  if (strcmp(cmd,"chd") == 0){
			if (changecwd(in->command[i].argv) < 0){
				return -1;
			}
			return 0;
	    //status = chd (in->command[i].argv); //llamamos a nuestra funcion
		}
		if (search_cmd(cmd)<0){
			fprintf (stderr, "command %s not found\n", cmd);
			return -1;
		}
		// printf("COMANDO GUARDADO (execute_line) VALE: --%s--\n", in->command[i].argv[0]);
		// printf("COMANDO GUARDADO (execute_line) VALE: --%s--\n", in->command[i].argv[1]);
		// printf("FILE_PATH_R VALE: --%s--\n", in->file_path_r);
		// printf("FILE_PATH_WR VALE: --%s--\n", in->file_path_wr);


		//Creamos fd_r, fd_wr y/o fd_pipe_aux para padre e hijo
		if(create_fds(in, i)<0){
			return -1;
		}

		// printf("fd_r TRAS PIPE VALE: --%d--\n", in->command[i].fd_r);
		// printf("Fd_WR TRAS PIPE VALE: --%d--\n", in->command[i].fd_wr);
		// printf("Fdpipe_R TRAS PIPE VALE: --%d--\n", in->fd_pipe_aux);

		//Forking
		pid = fork();
		switch (pid){
		case -1:
			warn ("fork");
			return -1;
		case 0:
			if (redirections(in, i) < 0){
				return -1;
			}
			execv(cmd, in->command[i].argv);
			err(1, "exec %s", cmd);
		default:
			//closing fd_r, fd_wr si existen
			if (in->command[i].fd_r != 0){
				if (close (in->command[i].fd_r)<0){
					warn ("close fd_read: %d", in->command[i].fd_r);
					return -1;
				}
			}
			if (in->command[i].fd_wr != 0){
				if (close(in->command[i].fd_wr)<0){
					warn ("close fd_write: %d", in->command[i].fd_r);
					return -1;
				}
			}
			//------------------------------

			if (in->back == 0){ //Caso no &
				while (wait(&sts) != pid){
					;
				}
			}
		}
		i++;
	}
	return 0;
}


//SHELL PROMPT------------------------------------------------------------------
//Pinta el promp de sh1 igual que el shell de unix------------------------------
int shell_prompt(){
	char *name;
	// char hostname[1024];
	char cwd[1024];

	name = getenv("LOGNAME");
	if(name == NULL){
		warn("getenv 'LOGNAME'\n");
		return -1;
	}

	// if(gethostname(hostname, sizeof(hostname)) < 0){
	// 	warn("gethostname");
	// 	return -1;
	// }

	if (getcwd(cwd, sizeof (cwd)) == NULL){
		warn("getcwd");
		return -1;
	}

	//printf("%s@%s ~%s : ", name, hostname, cwd); //PROMPT LARGA
	printf("%s@ ~ : ", name); //PROMPT CORTA
	fflush(stdout);
	return 0;
}



//MAIN--------------------------------------------------------------------------
//------------------------------------------------------------------------------
int
main (int argc, char *argv[]){
	input line;
	input* pline;
  int finish;
	int result;

	pline = &line;

  //Usage testing
  if (argc > 1){
    errx(1, "Usage: { ./shell } { ; mi_srcipt | shell } { shell < /mi_script }");
  }

  finish = 0;
	while (finish == 0){
		if (shell_prompt() < 0){
			fprintf (stderr, "Prompt failed. Trying again...");
		}else{
			result = read_line(pline);
			if(result < 0){
				continue;
				//fprintf(stderr, "Try again...\n");
			}else if (result == 1){
				printf("\n");
				finish = 1; //Fichero leido. FEOF...
			}
			if(execute_line (pline)<0){
				continue;
				//fprintf(stderr, "Execute command failed. Please, try again...\n");
			}
		}
	}
	exit (0);
}
