#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kv_store.h"


void save_to_disk(){     // Saves the current contents of the kv_store into a database.txt file
    FILE *f= fopen("database.txt", "w");

    pthread_rwlock_rdlock(&rwlock);
    if(f==NULL){
        perror("Failed to open Database file.");
        return;
    }

    for(int i=0; i<TABLE_SIZE; i++){
        Node *curr= kvStore[i];
        while(curr!=NULL){
            fprintf(f, "%s %s\n", curr->key, curr->val);
            curr=curr->next;
        }
    }
    fclose(f);
    pthread_rwlock_unlock(&rwlock);
    printf("Data successfully stores in disk.");
}



void pull_from_disk(){  //Reloads the kv_store with the saved values from database.txt
    FILE *f= fopen("database.txt", "r");
    if(f==NULL){
        return;
    }

    char key[30];
    char val[100];


    while (fscanf(f, "%s %s",key, val) == 2){
        kvPut(key,val);
    }

    fclose(f);
    printf("Loaded kv_store data from disk.");

}