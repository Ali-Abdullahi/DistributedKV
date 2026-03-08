#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kv_store.h>


void save_to_disk(){     // Saves the current contents of the kv_store into a database.txt file
    FILE *f= fopen("database.txt", "w");

    if(f==NULL){
        perror("Failed to open Database file.");
    }

    for(int i=0; i<TABLE_SIZE; i++){
        Node *curr= kvStore[i];
        while(curr!=NULL){
            fprintf(f, "%s %s\n", curr->key, curr->val);
            curr=curr->next;
        }
    }
    fclose(f);
}