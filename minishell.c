# include <stdio.h>
# include <unistd.h>
# include <stdlib.h>
# include <sys/wait.h>
# include <string.h>
# include <limits.h>
# include "readcmd.h"
# include <stdbool.h>
# define L 20
# define NB_fils 1


char cwd[PATH_MAX];
int codeTerm;
int nb_fils_termine;
struct cmdline * cmd; // commande à lire

/*Définition du type Etat qui contient les différents états possible des processus*/
enum Etat {A, S};
typedef enum Etat Etat;

/*Définition du type liste_proc qui contient tous les processus en cours d'éxecution avec leurs informations*/
typedef struct liste_proc *liste_proc;
struct liste_proc {
  int id;
  int pid;
  Etat etat;
  char cmd[L];
  liste_proc suivant;
};

pid_t pidFils; // pid du fils
liste_proc liste = NULL; // initialisation de la liste;


/*Fonction qui permet d'ajouter un processus à la liste*/
void ajouter(int pid, Etat etat, char *cmd, liste_proc *liste) {
  liste_proc nv_liste = malloc(sizeof(struct liste_proc));
  if ((*liste) == NULL) {
    nv_liste -> id = 1;
  } else {
    nv_liste -> id = ((*liste) -> id) + 1;
  }

  nv_liste -> pid = pid;
  nv_liste -> etat = etat;
  strncpy(nv_liste -> cmd, cmd, L);
  nv_liste -> suivant = NULL;
  nv_liste -> suivant = (*liste);
  (*liste) = nv_liste;
}

/*Fonction qui renvoie un booléen qui indique si un processus est présent ou non dans la liste*/
bool estPresent(int pid, liste_proc liste) {
  bool present = false;

  if (liste != NULL) {
    if ((liste -> pid) == pid) {
      present = true;
    } else {
      present = estPresent(pid, liste -> suivant);
    }
  }
  return present;
}

/*Fonction qui retire un processus de la liste*/
void retirer(int pid, liste_proc *liste) {
  if (estPresent(pid, *liste)) {
    if ((*liste) -> pid == pid) {
      (*liste) = (*liste) -> suivant;
    } else {
      while ((*liste) -> suivant != NULL && (*liste) -> suivant -> pid != pid) {
        (*liste) = (*liste) -> suivant;
      }

      if ((*liste) -> suivant -> suivant != NULL) {
        (*liste) -> suivant = (*liste) -> suivant -> suivant;
      } else {
        (*liste) -> suivant = NULL;
      }
    }
  }
}

/*Fonction qui affiche la liste des processus*/
void afficherListeProc(liste_proc liste, bool prem) {
  if (prem) {
    printf("IND     PID      ETAT         CMD\n");
  }
  if (liste == NULL) {
    printf("------------------------------------------\n");
  } else {
    char *etat;
    etat = (liste -> etat == A) ? "actif   " : "suspendu";
    printf("  %d  %d      %s     %3s\n", liste -> id, liste -> pid, etat, liste -> cmd);
    afficherListeProc(liste -> suivant, false);
  }
}

/*Fonction qui permet de changer l'état d'un processus*/
void changerEtat(int pid, Etat etat, liste_proc *liste) {
  if (estPresent(pid, *liste)) {
    if ((*liste) -> pid == pid) {
      (*liste) -> etat = etat;
    } else {
      changerEtat(pid, etat, &((*liste) -> suivant));
    }
  }
}

/*Fonction qui retourne le pid d'un processus de la liste*/
int getPid(int id, liste_proc liste) {
  int pid = -1;
  while (liste -> id != id && liste -> suivant != NULL) {
    liste = liste -> suivant;
  }
  if (liste != NULL) {
    pid = liste -> pid;
  }
  return pid;
}

/*Fonction qui stop un processus*/
void stop(int id, liste_proc (*liste)) {
  int pid = getPid(id, *liste);
  if (pid > 0) {
    kill(pid, SIGSTOP);
  }
}

/*Fonction qui reprend un processus*/
void reprendre(int id, liste_proc (*liste)) {
  int pid = getPid(id, *liste);
  if (pid > 0) {
    kill(pid, SIGCONT);
  }
}

/*Gestion SIGCHLD*/
void handler_chld(int signal_num) {

  int fils_termine, wstatus;
  nb_fils_termine++;

  if (signal_num == SIGCHLD) {
    while ((fils_termine = (int) waitpid(-1, &wstatus, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {

      /*Processus terminé naturellement*/
      if (WIFEXITED(wstatus)) {
        nb_fils_termine++;
        retirer(fils_termine, &liste);

      /*Processus terminé par un signal*/
      } else if (WIFSIGNALED(wstatus)) {
        nb_fils_termine++;
        retirer(fils_termine, &liste);

      /*Réception d'un SIGCONT*/
      } else if (WIFCONTINUED(wstatus)) {
        changerEtat(fils_termine, A, &liste);

      /*Réception d'un SIGSTOP*/
      } else if (WIFSTOPPED(wstatus)) {
        changerEtat(fils_termine, S, &liste);
      }
    }
  }
}

void handler_int(int signal_num) {
  
  if (estPresent(pidFils,liste)) {
    retirer(pidFils,liste);
  }

  kill(pidFils, SIGSTOP);
  printf("\n");
}

void handler_stp(int signal_num) {
  ajouter(pidFils, S, cmd->seq[0][0], &list);
  kill(pidFils, SIGSTOP);
  printf("\n");
}

/* Fonction qui affiche la tête de l'invite */
void afficherTete() {
  getcwd(cwd, sizeof(cwd));
  printf("cwu3@minishell:~%s$ ", cwd);
  fflush(stdout);
}

int main() {

  signal(SIGCHLD, handler_chld);

  while (1) {

    // Affichage de l'invite
    afficherTete();

    // Lecture de cmd en utlisant readcmd.h
    cmd = readcmd();
  
    if (cmd != NULL) {
      if ((cmd->seq)[0] != NULL) {

        /* Commande exit ne nécessitant pas de fils */
        if (!strcmp((cmd->seq)[0][0], "exit") && !(cmd->backgrounded)) {
          exit(0);

        /* Commande cd ne nécessitant pas de fils */
        } else if (!strcmp((cmd->seq)[0][0], "cd") && !(cmd->backgrounded)) {
          chdir((cmd->seq)[0][1]);

        /* Commande lj qui affiche la liste des processus en cours */
        } else if (!strcmp((cmd->seq)[0][0], "lj") && !(cmd->backgrounded)) {
          afficherListeProc(liste, true);

        /* Commande sj qui suspend un processus */
        } else if (!strcmp((cmd->seq)[0][0], "sj") && !(cmd->backgrounded)) {
          stop(atoi(cmd->seq[0][1]), &liste);

        /* Commande bg qui reprend un processus suspendu en arrière plan */
        } else if (!strcmp((cmd->seq)[0][0], "bg") && !(cmd->backgrounded)) {
          if (liste == NULL) {
            printf("Pas de processus en cours\n");
          } else if ((cmd->seq)[0][1] == NULL) {
            printf("Pas assez d'argument\n");
          } else {
            reprendre(atoi(cmd->seq[0][1]), &liste);
          }

        /* Commande fg qui reprend un processus suspendu au premier plan */
        } else if (!strcmp((cmd->seq)[0][0], "fg") && !(cmd->backgrounded)) {
          if (liste == NULL) {
            printf("Pas de processus en cours\n");
          } else if ((cmd->seq)[0][1] == NULL) {
            printf("Pas assez d'argument\n");
          } else {
            int pid = getPid(atoi(cmd->seq[0][1]), liste);
            if (pid != -1) {
              kill(pid, SIGCONT);
              retirer(pid, &liste);
              waitpid(pid, NULL, WUNTRACED);
            }
          }
          
        } else {


        pidFils = fork();

        // Erreur
        if (pidFils < 0) {
          exit(1);
        }

        if (pidFils == 0) {

          int ret;
          if ((cmd->seq)[0][0] != NULL) {
            ret = execvp((cmd->seq)[0][0], (cmd->seq)[0]);
          }

          if (ret == -1) {
            perror("Erreur");
            exit(0);
          }
          
          exit(1);

        } else {
          // Gestion du chevauchement
          if ((cmd->backgrounded) == NULL) {
            pid_t idFils = wait(&codeTerm);
            if (idFils == -1) {
              perror("wait...");
              exit(2);
            }
            // if (WEXITSTATUS(codeTerm) == 1) {
            //     printf("Echec d'éxecution de la commande\n");
            // }
          } else {
            ajouter(pidFils, A, cmd->seq[0][0], &liste);
          }
        }
        }
      }
    }
  }
  return EXIT_SUCCESS;
}
